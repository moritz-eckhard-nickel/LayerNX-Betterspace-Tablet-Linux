# INSTALL-CLAUDE.md – Anleitung für einen KI-Agenten (z. B. Claude Code)

> **An den Agenten:** Du installierst **LayerNX-Tablet-Linux** interaktiv auf ein
> **Betterspace V5x / LANIX Ilium Pad X8 (MediaTek MT8168B, Board `tb8168p1_64_bsp`)**.
> Arbeite Schritt für Schritt, prüfe nach jedem Schritt das Ergebnis und erfinde keine
> Erfolge. Der Mensch am Gerät drückt Tasten, wenn du ihn dazu aufforderst. Sag ihm
> immer **genau**, welche Taste er wann drücken soll, und warte auf seine Rückmeldung.
>
> **Datenverlust:** Entsperren und Flashen löschen alle Android-Daten. Hol dir vor
> Schritt 3 ausdrücklich das OK des Menschen.
>
> **Niemals** Partitionen wie `preloader`, `lk`, `lk2`, `nvram`, `nvdata`, `proinfo`,
> `seccfg`, `protect1/2`, `persist` beschreiben. Geflasht werden nur `vbmeta`, `boot`,
> `userdata` und optional `logo`.

---

> **Tastenlage (wichtig, dem Menschen so sagen):** Die **Lauter-Taste (Volume UP)** ist die
> **äußere Taste rechts** – **nicht** die innere Taste direkt neben dem Power-Button
> (das ist Leiser/Volume DOWN).

## 0. Voraussetzungen auf dem Host (Linux)

```sh
sudo apt-get install -y android-tools-adb android-tools-fastboot android-sdk-libsparse-utils python3-venv
# Ubuntu 24.04: Pakete heißen "adb" "fastboot"
git clone https://github.com/moritz-eckhard-nickel/LayerNX-Betterspace-Tablet-Linux.git
cd LayerNX-Betterspace-Tablet-Linux
```

Images aus dem neuesten Release laden (Rootfs ist zu groß für Git):

```sh
gh release download --pattern 'layernx-v5x-*' --pattern 'layernx-logo.img' --pattern 'SHA256SUMS.txt' -D release/
cd release && sha256sum -c SHA256SUMS.txt && cd ..
```

Brich ab, wenn die Prüfsummen nicht stimmen.

## 1. Gerät identifizieren (Android läuft noch)

Mensch: *Einstellungen → Über das Tablet → 7× auf „Build-Nummer“; Entwickleroptionen →
„OEM-Entsperrung“ und „USB-Debugging“ an; Kabel anstecken; „USB-Debugging zulassen“ bestätigen.*

```sh
adb devices -l
adb shell getprop ro.board.platform    # muss mt8168 sein
adb shell getprop ro.product.board     # muss tb8168p1_64_bsp sein
adb shell getprop sys.oem_unlock_allowed   # muss 1 sein
```

**Abbruch**, wenn Plattform/Board nicht passen (es gibt Ilium-Pad-X8-Varianten mit MT6580 o. ä. –
für die ist dieses Image **nicht** geeignet).

Optional (empfohlen): die Original-WLAN-MAC notieren, damit das Tablet später immer dieselbe IP bekommt:
`adb shell ip link show wlan0`.

## 2. Bootloader entsperren (Pflicht, einmalig)

Ohne entsperrten Bootloader lässt sich nichts flashen. Der Vorgang löscht alle Android-Daten.

```sh
adb reboot bootloader
fastboot devices
fastboot getvar unlocked          # "no" -> entsperren
fastboot flashing unlock          # NICHT abbrechen, bis OKAY kommt
```

Mensch: *Auf dem Tablet erscheint „Unlock bootloader?“ mit
„Yes (Volume UP)“ / „No (Volume Down)“ → **einmal Lauter (Volume UP)** drücken –
das ist die **äußere Taste rechts**, nicht die innere neben dem Power-Button.*
Falls der Dialog nicht sichtbar ist oder der Bootloader hängt: nicht den Befehl am Host
abbrechen; mit dem Reset-Loch bzw. langem Power-Druck neu starten und Schritt 2 wiederholen.
(**Achtung:** Volume UP = Ja. Lauter/Leiser im *Bootmenü* bedeuten etwas anderes.)

Prüfen: `fastboot getvar unlocked` → `yes`. Falls nicht: Befehl wiederholen.

## 3. Flashen

```sh
fastboot --disable-verity --disable-verification flash vbmeta device-dumps/android-stock/img/vbmeta.img
fastboot flash userdata release/layernx-v5x-rootfs.sparse.img
fastboot flash boot     release/layernx-v5x-boot.img
fastboot flash logo     release/layernx-logo.img        # optional: LayerNX-Bootlogo
fastboot reboot
```

`boot.img` und Rootfs **müssen aus demselben Release** stammen (Partitions-UUIDs).
Ohne das vbmeta mit abgeschalteter Prüfung stürzt der Bootloader ab (`bootreason=lk_crash`).

## 4. Ersten Start prüfen

Erwartet: LayerNX-Logo → Scribbles-Animation → Desktop mit Leiste (nach ~30–60 s).
Am Host erscheint ein USB-Netzwerk-Gerät (`18d1:d001`):

```sh
lsusb | grep 18d1
IF=$(ip -br link | awk '/^enx/{print $1; exit}')
sudo ip link set $IF up && sudo ip addr add 172.16.42.2/24 dev $IF
ssh root@172.16.42.1            # Passwort: lnx (Standard, nur root)
```

**Sofort das root-Passwort ändern lassen** (Einstellungen → Gerät → Passwort ändern, oder `passwd`):
alle Tablets haben ab Werk `root` / `lnx`, auch per SSH. Standardmäßig existiert nur `root`
(der Build-Benutzer wird beim ersten Start entfernt). Hostname ist automatisch `LNXTL-XXXX`.

## 5. Ersteinrichtung

Am einfachsten per Touch: **Menü → Einstellungen** (WLAN suchen und verbinden, Hostname,
Helligkeit, Ton, PIN, Akkuschutz, SSH, Tailscale, Updates). Die Bildschirmtastatur geht
bei Textfeldern von selbst auf; sonst Knopf „Tastatur“ in der Leiste. Alternativ per SSH:

```sh
# WLAN
sh -c 'wpa_passphrase "SSID" "PASSWORT" >> /etc/wpa_supplicant/wpa_supplicant.conf'
rc-service wpa_supplicant restart && rc-service networking restart
# feste WLAN-MAC (optional, aus Schritt 1)
sed -i 's/^WLAN_MAC=.*/WLAN_MAC="aa:bb:cc:dd:ee:ff"/' /etc/conf.d/moritz-v5x-wifi
# Updates
layernx-update
# optional: Tailscale / cloudflared
rc-update add tailscale default && rc-service tailscale start && tailscale up
cloudflared tunnel login
# optional: Web-App (z. B. eigene WebUI im Vollbild)
layernx-webapp add meinewebui http://192.168.x.y:5555 vollbild
# optional: Akkuschutz für Dauerbetrieb am Netzteil
layernx-akku schutz an 50
```

Der Mensch legt die PIN für das Verlassen von Vollbild-Apps beim ersten
**5× Power-Taste** (bei offener App) selbst fest.

## 6. Rettung bei Bootloop (ohne Tasten)

```sh
sudo python3 scripts/moritz-preloader-fastboot.py 120
# Tablet neu starten lassen (Bootloop reicht) -> Antwort "READY…TOOBTSAF" -> Fastboot
fastboot devices
```

Danach Schritt 3 wiederholen. Mit Tasten: beim Start **Volume UP (äußere Taste rechts) halten** → Bootmenü →
mit Volume UP auf „Fastboot“, mit Volume DOWN bestätigen.

## 7. Zurück zu Android

```sh
fastboot flash boot   device-dumps/android-stock/img/boot.img
fastboot flash vbmeta device-dumps/android-stock/img/vbmeta.img
fastboot flash logo   device-dumps/android-stock/img/logo.img
fastboot erase userdata
fastboot reboot
```

## 8. Selbst anpassen / bauen

Alles ist Quelltext in `pmaports-device/` (Pakete) und `patches/`. Bauen:
lokal siehe README „Selbst bauen“, in CI siehe `.github/workflows/build.yml`.
Eigene Branding-Dateien: `pmaports-device/layernx-branding/` (Animation, Hintergrund,
fastfetch-Logo), Bootlogo: `scripts/moritz-mklogo.py ORIGINAL-logo.img LOGO.png AUSGABE.img`.

## Bekannte Grenzen (dem Menschen ehrlich sagen)

- Kein Bluetooth (Stock-Kernel ohne `CONFIG_BT`), keine Kameras, keine GPU, kein Vibrationsmotor.
- Ton: Lautsprecher und eingebautes Mikrofon gehen (PulseAudio). Mikrofon in Web-Apps nur über https/localhost.
- Akkustand ist eine Schätzung aus der Spannung.
- Browser/Web-Apps ohne Sandbox (Kernel ohne Namespaces) – nur vertrauenswürdige Seiten.
