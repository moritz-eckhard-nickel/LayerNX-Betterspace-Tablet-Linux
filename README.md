# LayerNX-Tablet-Linux – Betterspace V5x / LANIX Ilium Pad X8

> ## ⚠️ **WICHTIGER RECHTLICHER HINWEIS / LEGAL DISCLAIMER**
>
> **Dieses Repository enthält proprietäre Dateien Dritter** (Hersteller-Kernel als Binärdatei,
> Kernelmodule, WLAN/Bluetooth-Firmware von MediaTek, Bootloader- und Partitions-Dumps sowie
> Android-Bibliotheken des Herstellers). **Diese Dateien sind NICHT Eigentum des Projekts.**
> Alle Rechte liegen bei den jeweiligen Rechteinhabern (u. a. MediaTek Inc., LANIX, BND, Google).
>
> **Sie werden ausschließlich bereitgestellt, weil sie für den Betrieb und die Nachvollziehbarkeit
> dieses Ports technisch notwendig sind** (es gibt für dieses Gerät keine öffentlichen Kernelquellen
> und keine freie Firmware).
>
> **Rechteinhaber, die eine Entfernung wünschen, schreiben bitte eine E-Mail an
> [moritz@onlymoritz.de](mailto:moritz@onlymoritz.de) – betroffene Dateien werden unverzüglich entfernt.**
>
> **This repository contains third-party proprietary files that are technically required for this
> port. All rights belong to their respective owners. On request by a rights holder, sent to
> [moritz@onlymoritz.de](mailto:moritz@onlymoritz.de), the affected files will be removed immediately.**

---

**LayerNX-Tablet-Linux** ist ein schlankes, natives Linux für das Android-Tablet
**Betterspace V5x** (baugleich **LANIX Ilium Pad X8**, MediaTek **MT8168B**, Board `tb8168p1_64_bsp`).

Es basiert auf **postmarketOS / Nura** und **Alpine Linux**, ist aber **kein offizielles
postmarketOS/Nura-Projekt** und wird von diesen Projekten weder unterstützt noch geprüft.
„postmarketOS“ und „Nura“ sind Namen bzw. Marken ihrer jeweiligen Projekte.

## Was funktioniert

| | Status |
|---|---|
| Booten ohne Host-PC, LayerNX-Bootlogo + Scribbles-Boot-Animation | ✅ getestet |
| SSH über USB-Netzwerk und WLAN, Hostname `LNXTL-XXXX` (zufällig beim ersten Start) | ✅ getestet |
| WLAN (2,4/5 GHz), feste MAC einstellbar | ✅ getestet |
| Paketinstallation mit `apk`, Gesamt-Update mit `layernx-update` | ✅ getestet |
| Display 1280×800 quer, eigener schneller Dreh-Treiber (`moritz-xf86-video-fbdev`, ~3,5× mehr FPS) | ✅ getestet |
| Touch: echte Touch-Events (MT), 2-Finger-Scrollen, Langdruck = Rechtsklick, kein Mauszeiger | ✅ getestet |
| Openbox-Oberfläche im LayerNX-Design (#4931fd), Leiste mit IP, Strom/Akku, Uhr | ✅ getestet |
| Terminal (lxterminal + bash + fastfetch mit Scribbles), Bildschirmtastatur | ✅ getestet |
| Web-Apps (`layernx-webapp`, WebKitGTK oder Chromium), Vollbild, eigene Profile/Cookies, selbst signierte Zertifikate optional | ✅ getestet |
| Einstellungen-App (WLAN suchen/verbinden, Display, Hostname, PIN, Akkuschutz, SSH, Tailscale, Updates) | ✅ installiert, Bedienung per Touch noch **ungetestet** |
| Power-Taste: kurz = Display, halten = Power-Menü; bei offener App gesperrt, 5× = PIN-Pad (Scrambling) | ✅ getestet |
| Helligkeit `layernx-display brightness 0-99`, Display aus/an | ✅ getestet |
| Akkuschutz für Serverbetrieb (`layernx-akku schutz an 50`) | ⚠️ Schalten getestet, Pendeln 45–50 % noch **ungetestet** |
| Tailscale, cloudflared (vorinstalliert, nicht aktiviert) | ✅ installiert, Aktivierung ungetestet |
| Akkuanzeige | ⚠️ nur Schätzung aus der Spannung (Kernel liefert `capacity=-1`) |
| Bluetooth | ❌ Stock-Kernel ohne `CONFIG_BT` (nur Userspace-Stack wie BTstack denkbar) |
| Audio, Kameras, Sensoren, GPU | ❌ / nicht eingerichtet |

Details: [docs/PROTOKOLL.md](docs/PROTOKOLL.md) (vollständige Entwicklungsgeschichte inkl. aller Fehlversuche).

## Aufbau

| Pfad | Inhalt |
|---|---|
| `pmaports-device/device-betterspace-v5x/` | Device-Paket: `deviceinfo`, OpenRC-Dienste (Gerätedateien, WLAN, USB-Netz), boot-deploy-Hook |
| `pmaports-device/linux-betterspace-v5x/` | **Stock-Kernel 4.14.141** + DT-Tabelle + WLAN/BT-Module (proprietär/Binär) |
| `pmaports-device/firmware-betterspace-v5x/` | **MediaTek-CONNSYS-Firmware** (proprietär) |
| `pmaports-device/moritz-wmt-launcher/` | freier Nachbau von `wmt_loader`/`wmt_launcher` (C, GPL-2.0) |
| `pmaports-device/moritz-v5x-tools/` | Touch-Brücke mit Gesten, Framebuffer-Refresher, Boot-Animation, Power-Tasten-Dienst (C, MIT) |
| `pmaports-device/moritz-v5x-desktop/` | Oberfläche: Xorg-Konfiguration, Openbox, tint2, jgmenu, svkbd, lxterminal |
| `pmaports-device/moritz-xf86-video-fbdev/` | fbdev-Treiber mit eigener gekachelter NEON-Drehung (MIT, Patch auf xf86-video-fbdev) |
| `pmaports-device/layernx-branding/` | Bootanimation, Hintergrund, os-release, fastfetch, Hostname `LNXTL-XXXX` |
| `pmaports-device/layernx-server/` | bash & Werkzeuge, Tailscale, cloudflared, `layernx-update`, `layernx-display`, `layernx-akku` |
| `pmaports-device/layernx-einstellungen/` | Touch-Einstellungen (GTK3) + Root-Helfer |
| `pmaports-device/layernx-webapps/` | `layernx-webapp`, `layernx-webview` (WebKitGTK), `layernx-pinpad`, `layernx-pin`, Power-Tasten-Regeln |
| `patches/` | Patch für `postmarketos-initramfs` (devtmpfs-Fallback, kleine Stufe 1) |
| `scripts/` | Build-Wrapper, Android-Dump, Preloader-Fastboot-Trick, Diagnose |
| `device-dumps/android-stock/` | vollständige Dumps des Original-Androids (**proprietär**, Seriennummer/MACs geschwärzt) |
| `images/` | `layernx-v5x-boot.img` (passend zum neuesten Release); das Rootfs-Image liegt unter **[Releases](https://github.com/moritz-eckhard-nickel/LayerNX-Betterspace-Tablet-Linux/releases)** (zu groß für Git) |
| `build/` | Testprotokolle (pstore/ramoops), Screenshots, Touch-Messungen |
| `docs/` | Protokoll |

## Installation mit KI-Agent

Repo klonen und **[INSTALL-CLAUDE.md](INSTALL-CLAUDE.md)** einem Agenten wie Claude Code geben – er führt
interaktiv durch Prüfung, Entsperren, Flashen und Ersteinrichtung (inkl. Treiber, Branding usw.).

## Installation (Kurzfassung)

> **Datenverlust:** Entsperren und Flashen löschen alle Android-Daten. Auf eigenes Risiko.

1. Android: Entwickleroptionen → **OEM-Entsperrung** und **USB-Debugging** an.
2. `adb reboot bootloader`, dann `fastboot flashing unlock` → am Tablet **Volume UP** (= Ja).
3. vbmeta-Prüfung abschalten (sonst stürzt der Bootloader ab):
   `fastboot --disable-verity --disable-verification flash vbmeta device-dumps/android-stock/img/vbmeta.img`
4. Rootfs und Kernel flashen:
   ```sh
   # alle Images aus DEMSELBEN Release laden (boot.img und Rootfs gehören zusammen)
   gh release download --pattern '*.img' --pattern SHA256SUMS.txt -D release/ && (cd release && sha256sum -c SHA256SUMS.txt)
   fastboot flash userdata release/layernx-v5x-rootfs.sparse.img
   fastboot flash boot     release/layernx-v5x-boot.img
   fastboot flash logo     release/layernx-logo.img   # optional: LayerNX-Bootlogo
   fastboot reboot
   ```
   > Hinweis: Alle Bestandteile dieser Images wurden am Gerät getestet; das fertige Gesamtimage als
   > frischer Flash ist **[UNGETESTET]**.
5. Login per USB: Host-Interface `172.16.42.2/24`, dann `ssh root@172.16.42.1` (Passwort: **`lnx`**).
   > ⚠️ **Alle LayerNX-Tablets haben ab Werk denselben Zugang `root` / `lnx`, auch per SSH.**
   > Passwort sofort ändern: **Einstellungen → Gerät → Passwort ändern** (oder `passwd`).
   > Standardmäßig gibt es nur `root`; weitere Benutzer bei Bedarf selbst anlegen (`adduser`).
6. WLAN einrichten: `/etc/wpa_supplicant/wpa_supplicant.conf` (z. B. `wpa_passphrase SSID PASSWORT`), dann `rc-service wpa_supplicant restart`.
   Feste MAC: `WLAN_MAC` in `/etc/conf.d/moritz-v5x-wifi`.

**Rettung bei Bootloop:** `sudo python3 scripts/moritz-preloader-fastboot.py` ausführen und das Tablet
neu starten lassen – es landet ohne Tastendruck im Fastboot-Modus.

**Zurück zu Android:** `fastboot flash boot device-dumps/android-stock/img/boot.img`,
`fastboot flash vbmeta device-dumps/android-stock/img/vbmeta.img`, danach Werksreset (userdata).

## Wichtige Befehle (auf dem Tablet)

| Befehl | Zweck |
|---|---|
| `layernx-update` | alles aktualisieren (apk inkl. Tailscale, cloudflared) |
| `layernx-display on\|off\|brightness 0-99` | Display / Helligkeit |
| `layernx-akku schutz an [MAX [MIN]]` / `schutz aus` / `status` | Akkuschutz für Serverbetrieb |
| `layernx-webapp add NAME URL [vollbild] [tls-ignorieren] [chromium]` | Web-App anlegen (oder Menü → „Web-App hinzufuegen …“) |
| `layernx-pin aendern\|entfernen` | PIN für Vollbild-Apps (oder Menü → Einstellungen) |
| `doas rc-update add tailscale default && doas rc-service tailscale start && doas tailscale up` | Tailscale aktivieren |
| `cloudflared tunnel login` | cloudflared einrichten |

## Selbst bauen

**In GitHub Actions:** `.github/workflows/build.yml` (manuell starten oder Tag `v*` pushen → Release mit Images). *Noch nicht in CI gelaufen – ungetestet.*

**Lokal:**

```sh
git clone https://gitlab.postmarketos.org/postmarketOS/pmbootstrap.git sources/pmbootstrap
git clone -b v26.06 https://gitlab.postmarketos.org/postmarketOS/pmaports.git pmaports
cd pmaports && git apply ../patches/postmarketos-initramfs.patch && cd ..
for p in pmaports-device/*/; do ln -sfn ../../../$p pmaports/device/testing/$(basename $p); done
MORITZ_SUDO_PW=... scripts/moritz-pmb init      # Gerät: betterspace-v5x, UI: none, OpenRC
MORITZ_SUDO_PW=... scripts/moritz-pmb install --password <passwort>
```
Extra-Pakete in `pmbootstrap.cfg`: `moritz-v5x-desktop,evtest,iw,nano,htop,e2fsprogs,util-linux`.

## Lizenzen

| Teil | Lizenz |
|---|---|
| Eigene Skripte, Konfigurationen, Pakete, Doku, `moritz-v5x-tools` | **MIT** ([LICENSES/MIT.txt](LICENSES/MIT.txt)) |
| `moritz-xf86-video-fbdev` (Patch) | **MIT** (wie xf86-video-fbdev) |
| LayerNX-Logo, Scribbles-Boot-Animation, Hintergrundbild | **© LayerNX, alle Rechte vorbehalten** – nur für dieses Projekt |
| cloudflared (Binary von Cloudflare) | Apache-2.0 |
| `moritz-wmt-launcher` | **GPL-2.0-only** ([LICENSES/GPL-2.0.txt](LICENSES/GPL-2.0.txt)), Ansatz nach Evergo-Mainline/initramfs |
| Patch für `postmarketos-initramfs` | **GPL-2.0-or-later** (wie das Original-Paket) |
| Stock-Kernel 4.14.141 und Kernelmodule | GPL-2.0 (Linux) bzw. Hersteller; **Binärdateien des Herstellers BND/LANIX, Quelltext wurde vom Hersteller nicht veröffentlicht** |
| WLAN/BT-Firmware, Vendor-Dateien, Bootloader-/Partitions-Dumps, Android-Bibliotheken | **proprietär**, Rechte bei MediaTek, LANIX, BND, Google u. a. – siehe Disclaimer oben |
| postmarketOS/Nura, Alpine Linux und alle Pakete im Rootfs-Image | jeweilige Lizenzen der Pakete |

Siehe auch [THIRD-PARTY.md](THIRD-PARTY.md).
