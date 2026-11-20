# Arbeitsprotokoll – LayerNX-Tablet-Linux auf dem Betterspace V5x

Chronologie der Portierung vom 20.11.2026, inklusive aller Sackgassen. Ziel: Wer das
Gerät (oder ein ähnliches MT8168-Tablet) portieren will, soll nachvollziehen können,
**warum** der Port so aussieht, wie er aussieht.

Legende: **[GETESTET]** am Gerät nachgewiesen · **[UNGETESTET]** gebaut, nicht nachgewiesen

---

## 1. Host-Umgebung

- Ubuntu 24.04, pmbootstrap 3.12.1 (aus Git), pmaports-Kanal **v26.06** (Alpine 3.24).
  postmarketOS heißt seit 09/2026 offiziell „Nura“ (nura.eco); technisch unverändert.
- pmbootstrap läuft mit `--as-root` über `scripts/moritz-pmb` (Arbeitsverzeichnis im Projekt).
- Kernel-Testbuilds: AOSP GCC 4.9 (android10-release). Für den finalen Port **nicht** nötig.

## 2. Hardwareanalyse (Android, root, nur lesend)

`scripts/moritz-dump-android.sh` → `device-dumps/android-stock/info/` (Seriennummer/MACs geschwärzt).

| Komponente | Ergebnis |
|---|---|
| SoC | MediaTek MT8168B, 4× Cortex-A53, Board `tb8168p1_64_bsp` |
| Kernel | 4.14.141, gebaut mit **Clang 9** (Hersteller „BND“) |
| Display | `mtkfb`, Panel **ILI9881C** (`ili9881c_wd101hbm40ab_d4_wxga_hsd`), nativ **800×1280 hochkant**, Android dreht um 270° |
| Touch | **Hynitron CST1xx** (`cst_mjk_800x1280_fw`), Treiber `mtk-tpd`, I²C 2-005d, Typ-A-Multitouch, Koordinaten **quer** 1280×800 |
| WLAN/BT | CONNSYS (MT6631-A-Die), Module `wmt_drv`, `wmt_chrdev_wifi`, `wlan_drv_gen4m`, `bt_drv`, Firmware `soc1_0_*` |
| Panel-/Touch-Auswahl | zur Laufzeit aus der Partition `items` (`ro.odm.LCM`, `ro.odm.touchpanel.*`) |

## 3. Kernelentscheidung

- Öffentliche Quelle gefunden: `kernel_casper_mt8168` (Casper VIA S48, gleiche BSP, **anderes Board**).
  Panel ILI9881C-Variante und **CST1xx-Touchtreiber fehlen** dort → Display & Touch (Pflicht) wären Neuportierung.
- Stock-Kernel fehlen `DEVTMPFS`, `VT`, `SYSVIPC`, `FHANDLE`, `NAMESPACES`; `ANDROID_PARANOID_NETWORK=y`.
- **Entscheidung:** Stock-Kernel + Stock-Module behalten, fehlende Kernel-Features im Userspace umgehen:
  - `/dev`: tmpfs + **mdev** statt devtmpfs/eudev (Initramfs-Patch + mdev-Dienst im Rootfs)
  - Netzwerk-Sockets: Benutzer in GID 3003 (`inet`) / 3004 (`net_raw`)
  - Xorg ohne VT: `vt1 -sharevts -novtswitch -keeptty`, `/dev/tty1` → `/dev/null`

## 4. Bootloader

- `fastboot flashing unlock`: Dialog „Yes (Volume UP) / No (Volume Down)“. **Volume UP = Ja.**
  (Der erste Versuch hing, weil der Befehl am Host abgebrochen wurde.) → `unlocked: yes` **[GETESTET]**
- **Preloader-Trick:** Während der Preloader (USB `0e8d:2000`, `/dev/ttyACM*`) aktiv ist, „FASTBOOT“
  senden → Antwort `READY…TOOBTSAF` → LK startet direkt in Fastboot. Rettet jeden Bootloop ohne Tasten.
  `scripts/moritz-preloader-fastboot.py` **[GETESTET]**

## 5. Der lange Weg zum ersten Boot (alle Tests in `build/`)

Symptom aller Fehlversuche: `initrd not fully accessible via the linear mapping` → `VFS: Unable to mount root fs`.

| Test | Änderung | Ergebnis |
|---|---|---|
| 1 | Ramdisk-Adresse 0x48000000 | Panik |
| 2 | Original-Android-Ramdisk | ok (bootet Android) |
| 3/4/5 | kleine pmOS-Initramfs / `bootopt` / Original-cmdline | Panik |
| 6 | Original-Ramdisk mit Nullen aufgefüllt | ok |
| 7/8 | gzip -9 / Null-Padding (ISIZE=0) | Panik |
| 9 | Original-Ramdisk + 1 MB Nullen **im cpio** | Panik |
| 10 | pmOS-Initramfs auf 1,71 MB entpackt geschrumpft | Panik |
| **11** | **`initrd=0x55000000,<Größe>` auf der Kernel-Kommandozeile** | **pmOS bootet** (temporär) |
| 12 | + `bootopt=64S3,32N2,64N2` | bootet temporär |
| Normalstart | aus boot-Partition | `bootreason=lk_crash` – **LK stürzt bei AVB-Prüfung ab** |
| 13/14 | **vbmeta mit `--disable-verity --disable-verification`** | **Normalstart funktioniert** (mit und ohne AVB-Footer) |

**Lösung (alle drei nötig):**
1. `bootopt=64S3,32N2,64N2` in der Kernel-Kommandozeile (`/usr/lib/kernel-cmdline.d/50-betterspace-v5x.conf`)
2. `initrd=0x55000000,<exakte Größe>` – erzeugt ein **boot-deploy-Hook** bei jedem Initramfs-Build
3. `vbmeta` mit abgeschalteter Verifikation

Zusätzlich: `deviceinfo_create_initfs_extra="true"` + Busybox-Shims (Initramfs-Stufe 1 bleibt klein),
`deviceinfo_flash_kernel_on_update="true"` (Updates schreiben boot.img selbst) **[GETESTET]**.

## 6. Hardware-Inbetriebnahme im Rootfs

| Thema | Problem | Lösung |
|---|---|---|
| WLAN | `wmt_loader`/`wmt_launcher` sind Android-Binaries | Nachbau `moritz-wmt-launcher` (C, ioctl-Sequenz per Disassembly ermittelt) **[GETESTET]** |
| WLAN-MAC | ohne Android-NVRAM-Dienst zufällig | feste MAC per `/etc/conf.d/moritz-v5x-wifi` **[GETESTET]** |
| Hintergrundbeleuchtung | stand auf 0 (Android-Light-HAL fehlt) | `echo 200 > /sys/class/leds/lcd-backlight/brightness` beim Boot **[GETESTET]** |
| Bild „eingefroren“, Bootlogo bleibt stehen | `mtkfb` überträgt nur bei `FBIOPAN_DISPLAY` | `moritz-v5x-fbrefresh` (30 Hz ioctl, ~0 % CPU) **[GETESTET]** |
| Ausrichtung | Panel hochkant | Xorg fbdev `Rotate "CCW"` → 1280×800 quer **[GETESTET]** |
| Touch | Typ-A über mtdev unzuverlässig | `moritz-v5x-touch`: uinput-Brücke, Position vor Klick **[GETESTET]** |
| Klicks auf Leiste verschluckt | Openbox-„Frame“-Bindung auf Linksklick | nur noch Alt+Klick **[GETESTET]** |
| Leiste unvollständig | tint2: `B`=Akku, `P`=Knopf verwechselt | `panel_items = PPTSC` **[GETESTET]** |
| „Absturz“ beim Menü | Menü öffnete unter dem Finger, Loslassen löste „Ausschalten“ aus | Untermenü „System“ + Ja/Nein-Rückfrage **[GETESTET]** |

## 7. Offene Punkte

- Akkuanzeige: `capacity=-1` (MediaTek GM3.0-Fuel-Gauge braucht den Android-Daemon `fuelgauged`).
- Bluetooth, Audio, Kameras, Sensoren, GPU (Mali, kein Mesa-Treiber für diesen Kernel): nicht eingerichtet.
- `rc-service wpa_supplicant restart` meldet eine udev-Fehlermeldung (kosmetisch).
- Eigener Kernel aus Quellen (würde GPL-Quelltextfrage lösen): ILI9881C- und CST1xx-Treiber müssten portiert werden.
