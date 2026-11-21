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
- Bluetooth, Kopfhörer, Kameras, Sensoren, GPU (Mali, kein Mesa-Treiber für diesen Kernel): nicht eingerichtet (Ton: siehe Abschnitt 9).
- `rc-service wpa_supplicant restart` meldet eine udev-Fehlermeldung (kosmetisch).
- Eigener Kernel aus Quellen (würde GPL-Quelltextfrage lösen): ILI9881C- und CST1xx-Treiber müssten portiert werden.

---

## 8. Ausbau zum Tablet-System (20./21.11.2026)

| Thema | Problem / Ziel | Lösung | Status |
|---|---|---|---|
| Strom/Akku | `capacity=-1` | Schätzung aus `voltage_now` (I·R-Korrektur beim Laden), Quelle aus `usb`/`ac` | **[GETESTET]** microUSB, Netz, Akku |
| Bootlogo | „betterspace“ | MTK-`logo.img` analysiert (42 Bilder 1280×800 BGRA, zlib), Bild 0 ersetzt (`scripts/moritz-mklogo.py`) | **[GETESTET]** |
| Rotes „NX“ beim Übergang | LK schreibt BGRA, `mtkfb` liest RGBA | Boot-Animation überdeckt den Moment | **[GETESTET]** |
| Boot-Animation | Scribbles, min. 1 Durchlauf | `moritz-v5x-bootanim` (libpng, dreht in den Hochkant-Puffer), Xorg wartet | **[GETESTET]** |
| Branding | Name, Farben #4931fd | `layernx-branding`, Openbox-Theme LayerNX, os-release, Hostname `LNXTL-XXXX` | **[GETESTET]** |
| Touch | wirkte wie Maus | Brücke liefert MT Typ B (XI2-Touch), 120-ms-Fenster für 2-Finger-Scrollen, Langdruck = Rechtsklick, `-nocursor` | **[GETESTET]** |
| Langdruck ohne Wirkung | Zeitprüfung lief nie (Touch sendet ständig) | `tick()` nach jedem Frame | **[GETESTET]** |
| Power-Taste | kurz/halten/5× + PIN | `moritz-v5x-keys` + `layernx-powerkey` + `layernx-pinpad` (Scrambling) | **[GETESTET]** |
| Power-Taste nach 1× tot | Kindprozess (supervise-daemon) erbte den EVIOCGRAB-fd | `O_CLOEXEC` | **[GETESTET]** |
| Browser | Ruckeln | Messung: Xorg-Drehung kostet bis 98 % eines Kerns; WebKitGTK flüssiger als Chromium | **[GETESTET]** |
| Hardware-Drehung | gewünscht | `CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW` aus → nicht möglich | geprüft |
| Schnelle Drehung | ShadowFB-Rotate generisch | Patch in xf86-video-fbdev: nur Damage-Rechtecke, 32×32-Kacheln, NEON-4×4-Transponieren | **[GETESTET]** 5 → 17 FPS |
| Web-Apps | WebUI wie Echo Show | `layernx-webview` (WebKitGTK, Profil je App, Vollbild, TLS-Ausnahme optional) | **[GETESTET]** inkl. OPNsense |
| Server | bash, Tailscale, cloudflared, Updates | `layernx-server`, `layernx-update` | installiert **[GETESTET]**, Aktivierung **[UNGETESTET]** |
| Akkuschutz | max. 50 % | `/proc/mtk_battery_cmd/current_cmd` („0 1“ = Laden aus) + Dienst | Schalten **[GETESTET]**, Pendeln **[UNGETESTET]** |
| Bluetooth | Maus/Tastatur/Lautsprecher | Stock-Kernel ohne `CONFIG_BT` → BlueZ unmöglich; nur Userspace-Stack (BTstack) denkbar | offen |
| Sandbox | Chromium/WebKit | Kernel ohne Namespaces → Sandbox aus (`--no-sandbox` bzw. `WEBKIT_DISABLE_SANDBOX…`) | bekannt |

---

## 9. Ton, Mikrofon in Web-Apps, Bildschirmtastatur (21.11.2026)

| Thema | Problem / Ziel | Lösung | Status |
|---|---|---|---|
| Soundkarte | unter Linux „kein Audio“ angenommen | `mt-snd-card` (MT8168-AFE + MT6357) läuft im Stock-Kernel, 13 PCMs | geprüft |
| Aufnahme | `ASoC: no backend DAIs enabled for VUL_FE` | Signalweg wie der Android-HAL schalten: `O09 I03`/`O10 I04`, `AIN Mux=INT ADC`, MICBIAS0, Preamp `IN_ADC1`, ADC über Preamp, PGA 24 dB (`/etc/init.d/layernx-audio`) | **[GETESTET]** |
| Welcher Eingang? | 3 Preamp-Eingänge, Vibration als Prüfsignal half nicht | mit Musik/Stille gemessen: nur `IN_ADC1` reagiert (≈39 → 300–440 RMS), `DCCECMSINGLEMODE` am lautesten | **[GETESTET]** |
| Mehrere Programme / dsnoop | Kernel ohne `CONFIG_SYSVIPC` → dsnoop/dmix unmöglich | PulseAudio systemweit (`layernx-pulse`), ALSA-Standard = Pulse | **[GETESTET]** |
| WebKit nimmt falsches Gerät | GStreamer-ALSA listet 13 gleichnamige Geräte, WebKit nimmt das erste (FM) | `layernx-webview` setzt `GST_PLUGIN_FEATURE_RANK=alsadeviceprovider:NONE` → nur Pulse-Quellen | **[GETESTET]** |
| Rohes `hw:0,3` | `not-negotiated`: Hardware nimmt nur feste Raten | Pulse hält 48 kHz und rechnet um | **[GETESTET]** |
| Mikrofon-Pegel | Analog-Mikrofon sehr leise | Pulse-Quellenlautstärke 250 % | **[GETESTET]** |
| Lautsprecher | stumm | `O03 I05`/`O04 I06`, `INT ADDA O03_O04`, `Speaker_Amp` + **`Ext_Speaker_Amp`** (am Gerät per Pieptöne ermittelt: erst mit externem Verstärker hörbar) | **[GETESTET]** gehört |
| Web-Apps Mikrofon | soll immer gehen | `--mikrofon`: nur Audio-`getUserMedia` erlaubt, alle anderen Rechte abgelehnt; Standard an, aus in Einstellungen → Ton | **[GETESTET]** |
| Ton ohne Tippen | AudioContext blieb `suspended` | `WebKitWebsitePolicies` Autoplay erlaubt | **[GETESTET]** |
| Sprach-Web-App | Mikrofon → WebSocket → Antwort abspielen | Testseite mit WebSocket-Echo-Server: 162/162 Pakete hin, zurück, abgespielt; Rückkopplung ohne Echo-Unterdrückung | **[GETESTET]** |
| Vibration | Tastatur-Haptik | Regler `vibr` schaltet (2,8 V), Motor reagiert nie, unter Android nie vibriert → nicht verbaut; Code entfernt | geprüft |
| Bildschirmtastatur | svkbd hässlich, keine echte Großstellung | `layernx-tastatur` (GTK3): QWERTZ, Umschalt 1×/2×, Symbol-Ebene, ⌫ wiederholt, `_NET_WM_STRUT` | **[GETESTET]** |
| Falsche Zeichen | xdotool belegt fehlende Zeichen beim Tippen um (Wettlauf mit MappingNotify) | fehlende Zeichen beim Start einmal fest auf freie Tastencodes, dann XTest direkt | **[GETESTET]** |
| Falsche Tasten per Touch | 120-ms-Fenster, 2 Finger = Scrollen | Touch-Brücke: im Tastaturbereich (`/tmp/layernx-tastatur.bereich`) jeder Finger sofort ein eigener Druck | **[GETESTET]** von Hand |
| Automatisch auf/zu | soll überall außer Web-Apps | AT-SPI (`dbus-run-session` in der Sitzung), Fokus auf editierbares Feld/Terminal → auf, sonst zu; Web-Apps ausgenommen | **[GETESTET]** Terminal |
| Feste WLAN-MAC | nach Neustart neue IP | `WLAN_MAC` in `/etc/conf.d/moritz-v5x-wifi` war leer (lokal gesetzt, nicht im Repo) | behoben |
