# LayerNX-Tablet-Linux – Wiki

Handbuch für **LayerNX-Tablet-Linux** auf dem **Betterspace V5x / LANIX Ilium Pad X8**
(MediaTek MT8168B, Board `tb8168p1_64_bsp`). Hier steht, was das System kann und wie man es bedient –
per Touch und per Kommandozeile.

> Installation: [README.md](../README.md#installation-kurzfassung) bzw. Schritt für Schritt mit einem
> KI-Agenten: [INSTALL-CLAUDE.md](../INSTALL-CLAUDE.md). Entwicklungsgeschichte: [PROTOKOLL.md](PROTOKOLL.md).

## Inhalt

1. [Überblick](#1-überblick)
2. [Erster Start und Zugang](#2-erster-start-und-zugang)
3. [Bedienung: Touch, Tasten, Oberfläche](#3-bedienung-touch-tasten-oberfläche)
4. [Bildschirmtastatur](#4-bildschirmtastatur)
5. [Einstellungen-App](#5-einstellungen-app)
6. [WLAN und Netzwerk](#6-wlan-und-netzwerk)
7. [Web-Apps und Vollbild-Kiosk](#7-web-apps-und-vollbild-kiosk)
8. [Power-Taste und PIN](#8-power-taste-und-pin)
9. [Display und Helligkeit](#9-display-und-helligkeit)
10. [Ton und Mikrofon](#10-ton-und-mikrofon)
11. [Second Screen (Win+K)](#11-second-screen-wink)
12. [Akku, Akkuschutz, Tiefentladeschutz](#12-akku-akkuschutz-tiefentladeschutz)
13. [Fernzugriff: SSH, Tailscale, cloudflared](#13-fernzugriff-ssh-tailscale-cloudflared)
14. [Updates und Pakete](#14-updates-und-pakete)
15. [Benutzer, Passwörter, Rechte](#15-benutzer-passwörter-rechte)
16. [Dienste und Konfigurationsdateien](#16-dienste-und-konfigurationsdateien)
17. [Systemtest und Fehlersuche](#17-systemtest-und-fehlersuche)
18. [Rettung, Neu-Flashen, zurück zu Android](#18-rettung-neu-flashen-zurück-zu-android)
19. [Selbst anpassen und bauen](#19-selbst-anpassen-und-bauen)
20. [Bekannte Grenzen](#20-bekannte-grenzen)

---

## 1. Überblick

| | |
|---|---|
| Basis | postmarketOS / Alpine Linux (kein offizielles postmarketOS-Projekt), OpenRC |
| Kernel | originaler Hersteller-Kernel 4.14.141 (Binär, keine Quellen veröffentlicht) |
| Oberfläche | Xorg + Openbox, Leiste tint2, Menü jgmenu, Design-Farbe `#4931fd` |
| Display | 1280×800 quer, eigener schneller Dreh-Treiber (~3,5× mehr Bilder/s) |
| Standard-Zugang | nur `root`, Passwort `lnx` (**sofort ändern**) |
| Hostname | `LNXTL-XXXX`, beim ersten Start zufällig vergeben |
| Speicher | Rootfs liegt in der Android-Partition `userdata` (~11 GB nutzbar) |

Startablauf: **LayerNX-Bootlogo → Scribbles-Boot-Animation → Desktop** (nach ca. 30–60 s).

## 2. Erster Start und Zugang

### Per USB-Kabel (immer verfügbar)

Das Tablet meldet sich am PC als USB-Netzwerkgerät (`18d1:d001`). Das Tablet hat die Adresse
`172.16.42.1`, der PC braucht eine Adresse im selben Netz:

```sh
IF=$(ip -br link | awk '/^enx/{print $1; exit}')
sudo ip link set $IF up && sudo ip addr add 172.16.42.2/24 dev $IF
ssh root@172.16.42.1          # Passwort ab Werk: lnx
```

### Erste Schritte

1. **root-Passwort ändern**: Einstellungen → Gerät → Passwort ändern (oder `passwd`).
   Alle LayerNX-Tablets haben ab Werk denselben Zugang `root` / `lnx` – auch per SSH.
2. **WLAN verbinden**: Einstellungen → WLAN.
3. **Updates**: Einstellungen → System → „Alles aktualisieren“ (oder `layernx-update`).
4. Optional: Hostname, Helligkeit, PIN, Akkuschutz, Tailscale.

> **Hinweis zu Release v0.0.4:** Dort ist root nach der Installation gesperrt, `lnx` funktioniert
> nicht. Abhilfe am Tablet: Einstellungen → Gerät → Passwort ändern. Ab dem nächsten Release setzt
> der Erststart-Dienst `lnx` zuverlässig.

Beim **ersten Start** einer Neuinstallation läuft einmalig `layernx-erststart`: Der beim Bauen
angelegte Benutzer wird entfernt (seine SSH-Schlüssel gehen an root), root bekommt `bash` und das
Standardpasswort, und `layernx-hostname` vergibt den Namen `LNXTL-XXXX`.

## 3. Bedienung: Touch, Tasten, Oberfläche

### Touch

| Geste | Wirkung |
|---|---|
| Tippen | Linksklick |
| Lange drücken | Rechtsklick |
| Mit 2 Fingern wischen | Scrollen |

Es gibt keinen Mauszeiger. Programme bekommen echte Multitouch-Events (Touch-Brücke `moritz-v5x-touch`).

### Oberfläche

- **Leiste (unten)**: Menü, offene Fenster, Knopf „Tastatur“, IP-Adresse, Strom/Akku, Uhr.
- **Menü**: Terminal, Bildschirmtastatur, „Web-App hinzufuegen …“, eigene Web-Apps,
  **Einstellungen**, **System ▸** (Display aus, Neu starten, Ausschalten).
- **Terminal**: lxterminal mit bash; beim Öffnen zeigt fastfetch das Maskottchen Scribbles.
- Mit angeschlossener Tastatur: **Alt+Tab** (Fenster wechseln), **Alt+F4** (Fenster schließen).

### Tasten

| Taste | Lage | Wirkung im System |
|---|---|---|
| Power | rechts | siehe [Power-Taste und PIN](#8-power-taste-und-pin) |
| Volume UP | **äußere** Taste rechts | beim Einschalten halten → Bootmenü |
| Volume DOWN | innere Taste neben Power | im Bootmenü: bestätigen |

## 4. Bildschirmtastatur

`layernx-tastatur` – QWERTZ mit Umlauten.

- Geht bei Textfeldern **automatisch auf und zu** (über AT-SPI). In Web-Apps klappt das nicht –
  dort den Knopf **„Tastatur“** in der Leiste antippen (oder Menü → Bildschirmtastatur).
- **Umschalt**: 1× = ein Großbuchstabe, 2× = feststellen.
- Eigene **Symbol-Ebene** für Ziffern und Sonderzeichen.
- Fenster rücken nach oben, damit die Tastatur nichts verdeckt.

## 5. Einstellungen-App

Menü → **Einstellungen** (`layernx-einstellungen`, GTK3, für Touch gebaut). Aktionen, die
root-Rechte brauchen, laufen über den Helfer `layernx-einstellungen-helfer`.

| Seite | Inhalt |
|---|---|
| **WLAN** | Netzwerke suchen, antippen und verbinden; „Firmen-WLAN mit Zertifikat (PEAP/TTLS/TLS) …“ |
| **Display** | Helligkeit 0–99, „Display jetzt ausschalten“ |
| **Ton** | Lautstärke Lautsprecher 0–100 %, Stumm, Testton, „Mikrofon für Web-Apps erlauben“ |
| **Second Screen** | ein/aus, „Aktuelles vergessen“ (gekoppelten PC löschen) |
| **Gerät** | Gerätename (Hostname), Passwort ändern (root oder Benutzer), PIN ändern/entfernen |
| **Akku & Strom** | Akkuschutz an/aus, Ober- und Untergrenze in % |
| **Dienste** | SSH an/aus, Tailscale (starten, Anmelde-Link holen, Status), Hinweis zu cloudflared |
| **Bluetooth** | Hinweis: auf diesem Gerät nicht verfügbar (Kernel ohne `CONFIG_BT`) |
| **System** | „Alles aktualisieren“, Neu starten |

**Hostname**: erlaubt sind Buchstaben, Ziffern und Bindestrich. Wirkt sofort, wird nach einem
Neustart überall angezeigt (auch als Name für Second Screen in Windows).

## 6. WLAN und Netzwerk

- 2,4 und 5 GHz, Treiber und Firmware vom Hersteller (`wlan_drv_gen4m`, MediaTek CONNSYS).
- Gestartet von `moritz-v5x-wifi` (lädt Firmware über `moritz-wmt-launcher`).

### Per Touch

Einstellungen → WLAN → „Netzwerke suchen“ → Netz antippen → Passwort eingeben.

### Per Kommandozeile

```sh
wpa_passphrase "SSID" "PASSWORT" >> /etc/wpa_supplicant/wpa_supplicant.conf
rc-service wpa_supplicant restart && rc-service networking restart
```

### Feste MAC-Adresse

Ohne Eintrag wählt der Treiber bei jedem Start eine zufällige MAC – der Router vergibt dann jedes Mal
eine andere IP. Feste MAC (z. B. die Original-MAC aus Android, `adb shell ip link show wlan0`):

```sh
sed -i 's/^WLAN_MAC=.*/WLAN_MAC="aa:bb:cc:dd:ee:ff"/' /etc/conf.d/moritz-v5x-wifi
rc-service moritz-v5x-wifi restart
```

### Firmen-WLAN (WPA2/3-Enterprise)

Einstellungen → WLAN → **„Firmen-WLAN mit Zertifikat“**:

- **PEAP** oder **TTLS**: Identität/Benutzer + Passwort, CA-Zertifikat empfohlen.
- **TLS (EAP-TLS)**: Client-Zertifikat (`.p12/.pfx` oder `.pem` + privater Schlüssel).
- Zertifikate (`.pem .crt .cer .der .key .p12 .pfx`) per USB-Stick oder `scp` ins Home-Verzeichnis
  legen und im Dialog auswählen. Sie werden nach `/etc/wpa_supplicant/certs/` kopiert (nur root lesbar).

> ⚠️ Die Oberfläche ist getestet, die Anmeldung an einem echten Firmen-WLAN noch nicht.

### USB-Netzwerk

Dienst `moritz-v5x-usbnet`: Tablet `172.16.42.1`, PC `172.16.42.2/24` (siehe [Abschnitt 2](#2-erster-start-und-zugang)).

## 7. Web-Apps und Vollbild-Kiosk

Web-Seiten als eigene Apps im Menü, auf Wunsch im Vollbild – z. B. als Wand-Panel oder Kiosk.

### Anlegen

Per Touch: Menü → **„Web-App hinzufuegen …“** (Name, URL, Fenster/Vollbild, selbst signiertes
Zertifikat ja/nein). Per Kommandozeile:

```sh
layernx-webapp add NAME URL [vollbild] [chromium] [tls-ignorieren] [kein-mikrofon]
layernx-webapp list               # alle Web-Apps
layernx-webapp start NAME         # starten
layernx-webapp remove NAME        # entfernen (Profil bleibt erhalten)
layernx-webapp mikrofon NAME an|aus
```

Beispiel: `layernx-webapp add meinewebui http://192.168.0.10:5555 vollbild`

| Option | Bedeutung |
|---|---|
| `vollbild` | ohne Fensterrahmen über den ganzen Bildschirm (Kiosk) |
| `chromium` | Chromium statt WebKitGTK (Standard `layernx-webview` ist auf dem Gerät flüssiger) |
| `tls-ignorieren` | selbst signierte https-Zertifikate akzeptieren (z. B. Server im LAN) |
| `kein-mikrofon` | dieser App das Mikrofon verbieten |

- Jede Web-App hat ein **eigenes dauerhaftes Profil** (Cookies, Logins bleiben):
  `~/.local/share/layernx-webapps/NAME-profil`.
- Name: nur Buchstaben, Ziffern, `-` und `_`.
- Rechte: Web-Apps dürfen **nur das Mikrofon** benutzen; Kamera, Standort und Benachrichtigungen
  werden abgelehnt. Ton läuft ohne vorheriges Tippen (Autoplay).
- **Vollbild-Apps verlassen**: 5× schnell die Power-Taste → PIN (siehe [nächster Abschnitt](#8-power-taste-und-pin)).

> ⚠️ Browser laufen **ohne Sandbox** (der Hersteller-Kernel hat keine Namespaces). Nur
> vertrauenswürdige Seiten öffnen.

## 8. Power-Taste und PIN

| Zustand | Kurz drücken | Halten | 5× schnell |
|---|---|---|---|
| Display aus | Display an | Display an | Display an |
| Homescreen (keine App offen) | Display aus | Power-Menü | – |
| App offen | **gesperrt** | **gesperrt** | PIN-Abfrage → App schließen |

- **Power-Menü**: Display aus, Neu starten, Ausschalten, Abbrechen.
- Die Sperre bei offener App verhindert, dass jemand ein Kiosk-Tablet einfach ausschaltet.
- **PIN** (4 Ziffern, Ziffernfeld mit zufälliger Anordnung):
  - Beim **ersten** 5× Drücken mit offener App legt man die PIN selbst fest.
  - Ändern/entfernen: Einstellungen → Gerät oder `layernx-pin aendern|entfernen`
    (dafür wird die alte PIN abgefragt).
  - Gespeichert als SHA-256-Hash in `/etc/layernx/pin.sha256`. Drei Fehlversuche pro Abfrage.
- Protokoll der Tastendrücke: `/tmp/layernx-powerkey.log`.

## 9. Display und Helligkeit

```sh
layernx-display on | off | toggle | status
layernx-display brightness 0-99      # 0 = dunkelste Stufe, nicht aus
```

- Die Helligkeit wird in `/etc/conf.d/layernx-display` gespeichert und beim Start gesetzt.
- Das Display dimmt nicht und schaltet sich **nie von selbst** ab (für Dauerbetrieb gedacht).
- Nach „Display aus“ aus dem Menü schaltet Tippen das Display **nicht** wieder ein – Power-Taste
  drücken oder per SSH `layernx-display on`.

## 10. Ton und Mikrofon

- **Lautsprecher** und **eingebautes Mikrofon** laufen über einen systemweiten PulseAudio-Dienst
  (`layernx-pulse`); ALSA-Programme werden an PulseAudio weitergeleitet.
- Einstellungen → Ton: Lautstärke, Stumm, Testton.
- Kommandozeile: `arecord -d 5 test.wav`, `aplay test.wav`, `pactl …`.
- Mikrofon-Feinabstimmung in `/etc/conf.d/layernx-audio` (`MIC_MODUS`, `MIC_PGA` 0–24 dB),
  Software-Verstärkung in `/etc/pulse/layernx.pa`.

**In Web-Apps:**

- Mikrofon gibt der Browser nur über **https** oder **localhost** frei (WebSocket dann `wss://`).
  Bei selbst signiertem Zertifikat die Web-App mit `tls-ignorieren` anlegen.
- Abschalten für alle Web-Apps: Einstellungen → Ton → „Mikrofon für Web-Apps erlauben“;
  für eine App: `layernx-webapp mikrofon NAME aus`. Gilt ab dem nächsten Start der Web-App.
- Keine Echo-Unterdrückung: Was der Lautsprecher spielt, hört das Mikrofon mit.

Kein Kopfhörer-Ausgang (nicht eingerichtet).

## 11. Second Screen (Win+K)

Das Tablet wird zum **drahtlosen Zweitbildschirm für Windows** (Miracast).

1. Tablet: Einstellungen → **Second Screen** einschalten (ab Werk **aus**).
2. Windows: **Win+K** → Tablet-Namen (= Hostname, z. B. `LNXTL-XXXX`) wählen.
3. Windows: **Win+P** → „Erweitern“ oder „Duplizieren“.

- Läuft über **Wi-Fi Direct** – PC und Tablet müssen **nicht** im selben WLAN sein; das normale
  WLAN des Tablets bleibt verbunden.
- 1280×800, ~30 Bilder/s, kaum Verzögerung. **Nur Bild**, der Ton bleibt am PC.
- Der **erste** PC wird gekoppelt und künftig ohne Nachfrage angenommen, alle anderen werden
  abgelehnt. Anderen PC erlauben: Einstellungen → Second Screen → „Aktuelles vergessen“.
- Während der Verbindung ist die Oberfläche abgedeckt. Das Dekodieren braucht ~2 von 4 Kernen –
  am besten mit Netzteil betreiben.
- Verbindungsmodus in `/etc/conf.d/layernx-second-screen`:
  `MODUS=client` (Standard, Windows bestimmt den Kanal) oder `MODUS=go` (Rückfall, Tablet ist Gruppenleiter).
- Protokolle: `/run/layernx-second-screen/`.

## 12. Akku, Akkuschutz, Tiefentladeschutz

### Akkuanzeige

Der Kernel liefert keinen Ladestand (`capacity=-1`). Die Prozentzahl in der Leiste ist eine
**Schätzung aus der Spannung**.

### Akkuschutz für Dauerbetrieb am Netzteil

Für Tablets, die ständig am Netzteil hängen (Wand-Panel, Server): Laden stoppt bei einer Obergrenze
und startet wieder bei einer Untergrenze. Das schont den Akku.

```sh
layernx-akku schutz an 50         # stoppt bei 50 %, lädt wieder ab 45 %
layernx-akku schutz an 80 70      # eigene Grenzen
layernx-akku schutz aus
layernx-akku status
```

Oder Einstellungen → Akku & Strom. Konfiguration: `/etc/conf.d/layernx-akkuschutz`.

> ⚠️ Schalten getestet, Pendeln zwischen den Grenzen über Stunden noch ungetestet.

### Tiefentladeschutz (Akku-Wächter)

Dienst `layernx-akkuwaechter`: Fällt die Spannung **ohne Netzteil** 90 s lang unter `AUS_MV`
(Standard 3450 mV ≈ 3–5 % Rest), fährt das Tablet sauber herunter. Einstellbar in
`/etc/conf.d/layernx-akkuwaechter`.

> ⚠️ Logik getestet, echte Abschaltung bei leerem Akku noch ungetestet.

## 13. Fernzugriff: SSH, Tailscale, cloudflared

### SSH

Läuft ab Werk (USB und WLAN), root-Login mit Passwort ist erlaubt
(`/etc/ssh/sshd_config.d/10-layernx.conf`). An/aus: Einstellungen → Dienste.
Sicherer: eigenen Schlüssel hinterlegen und Passwort ändern:

```sh
ssh-copy-id root@TABLET-IP
```

### Tailscale

Vorinstalliert, aber nicht aktiviert. Einstellungen → Dienste → Tailscale starten → Anmelde-Link
holen. Oder:

```sh
rc-update add tailscale default && rc-service tailscale start && tailscale up
```

### cloudflared

Vorinstalliert, Einrichtung im Terminal: `cloudflared tunnel login`.

## 14. Updates und Pakete

```sh
layernx-update        # apk update + apk upgrade --available, danach cloudflared update
apk add PAKET         # weitere Pakete aus Alpine/postmarketOS
apk search BEGRIFF
```

Oder Einstellungen → System → „Alles aktualisieren“. Der Kernel ist der feste Hersteller-Kernel und
wird nicht über `apk` ausgetauscht – neue boot.img-Versionen kommen über die
[Releases](https://github.com/moritz-eckhard-nickel/LayerNX-Betterspace-Tablet-Linux/releases).

## 15. Benutzer, Passwörter, Rechte

- Ab Werk gibt es **nur `root`** (Passwort `lnx`). Die Oberfläche läuft als root.
- Passwort ändern: Einstellungen → Gerät → Passwort ändern, oder `passwd`.
- Weitere Benutzer: `adduser NAME`. Ist ein Benutzer (UID ≥ 1000) vorhanden, laufen Oberfläche und
  Werkzeuge als dieser Benutzer; root-Aktionen gehen dann über `doas`.
- Gruppen für Video, Ton, Eingabe und Netzwerk setzt `moritz-v5x-groups` beim Start automatisch.

## 16. Dienste und Konfigurationsdateien

| Dienst (OpenRC) | Aufgabe |
|---|---|
| `layernx-erststart` | einmalig beim ersten Start: nur root, Standardpasswort |
| `layernx-hostname` | Hostname `LNXTL-XXXX` vergeben |
| `moritz-v5x-devnodes` | Gerätedateien des Hersteller-Kernels anlegen |
| `moritz-v5x-wifi` | WLAN-Firmware laden, `wlan0` hochfahren |
| `moritz-v5x-usbnet` | USB-Netzwerk `172.16.42.1` |
| `moritz-v5x-bootanim` | Scribbles-Boot-Animation |
| `moritz-v5x-fbrefresh` | Framebuffer regelmäßig aufs Display bringen |
| `moritz-v5x-touch` | Touch-Brücke mit Gesten |
| `moritz-v5x-keys` | Power-Taste auswerten (→ `layernx-powerkey`) |
| `moritz-v5x-xsession` | grafische Oberfläche |
| `layernx-audio`, `layernx-pulse` | Mixer-Signalwege, PulseAudio |
| `layernx-akkuwaechter` | Tiefentladeschutz |
| `layernx-akkuschutz` | Ladegrenzen (nur wenn eingeschaltet) |
| `layernx-second-screen` | Miracast-Empfänger (nur wenn eingeschaltet) |
| `sshd`, `tailscale` | Fernzugriff |

Status aller Dienste: `rc-status`. Einzelner Dienst: `rc-service NAME status|restart`.

| Datei | Inhalt |
|---|---|
| `/etc/conf.d/moritz-v5x-wifi` | `WLAN_MAC` |
| `/etc/conf.d/layernx-display` | gespeicherte Helligkeit |
| `/etc/conf.d/layernx-akkuschutz` | `AKKUSCHUTZ`, `MAX`, `MIN` |
| `/etc/conf.d/layernx-akkuwaechter` | `AUS_MV` |
| `/etc/conf.d/layernx-second-screen` | `MODUS` |
| `/etc/conf.d/layernx-audio` | Mikrofon-Modus und -Verstärkung |
| `/etc/wpa_supplicant/wpa_supplicant.conf` | WLAN-Netze |
| `/etc/layernx/` | PIN-Hash, gekoppelter Second-Screen-PC, Erststart-Marker |
| `~/.local/share/layernx-webapps/` | Web-Apps und ihre Profile |

## 17. Systemtest und Fehlersuche

Automatischer Test (Boot, Dienste, Netzwerk, Hardware, Grafik, Programme, Branding, Zugang):

```sh
scp scripts/moritz-systemtest.sh root@172.16.42.1:/tmp/ && ssh root@172.16.42.1 sh /tmp/moritz-systemtest.sh
```

Ergebnisse früherer Läufe: [SYSTEMTEST-aktuell.txt](SYSTEMTEST-aktuell.txt).

| Problem | Lösung |
|---|---|
| SSH: „Permission denied“ mit `lnx` (Release v0.0.4) | am Tablet Einstellungen → Gerät → Passwort ändern |
| Kein USB-Netz am PC | `lsusb \| grep 18d1`; Interface `enx…` hochfahren und `172.16.42.2/24` setzen |
| Jedes Mal andere WLAN-IP | feste MAC setzen ([Abschnitt 6](#6-wlan-und-netzwerk)) |
| Display bleibt nach „Display aus“ dunkel | Power-Taste drücken oder `layernx-display on` |
| Vollbild-App lässt sich nicht schließen | 5× Power-Taste → PIN |
| PIN vergessen | per SSH `rm /etc/layernx/pin.sha256` |
| Kein Ton | `rc-service layernx-pulse status`, Testton in Einstellungen → Ton |
| Second Screen wird nicht gefunden | eingeschaltet? Anderer PC schon gekoppelt? Protokolle in `/run/layernx-second-screen/` |
| Uhrzeit falsch | stellt sich nach WLAN-Verbindung über das Netz |

## 18. Rettung, Neu-Flashen, zurück zu Android

### Fastboot ohne Tasten (z. B. bei Bootloop)

```sh
sudo python3 scripts/moritz-preloader-fastboot.py 120
# Tablet neu starten lassen -> Antwort "READY…TOOBTSAF" -> Fastboot
fastboot devices
```

### Fastboot mit Tasten

Beim Einschalten **Volume UP** (äußere Taste rechts) halten → Bootmenü → mit Volume UP auf
„Fastboot“, mit Volume DOWN bestätigen.

### Neu flashen

```sh
fastboot flash userdata release/layernx-v5x-rootfs.sparse.img
fastboot flash boot     release/layernx-v5x-boot.img
fastboot reboot
```

`boot.img` und Rootfs **müssen aus demselben Release** stammen.

### Zurück zu Android

```sh
fastboot flash boot   device-dumps/android-stock/img/boot.img
fastboot flash vbmeta device-dumps/android-stock/img/vbmeta.img
fastboot flash logo   device-dumps/android-stock/img/logo.img
fastboot erase userdata
fastboot reboot
```

> **Nie** `preloader`, `lk`, `lk2`, `nvram`, `nvdata`, `proinfo`, `seccfg`, `protect1/2` oder
> `persist` beschreiben.

## 19. Selbst anpassen und bauen

- Pakete: `pmaports-device/` (ein Ordner je Paket, siehe Tabelle „Aufbau“ im [README](../README.md#aufbau)).
- Branding (Boot-Animation, Hintergrund, fastfetch-Logo): `pmaports-device/layernx-branding/`.
- Bootlogo: `scripts/moritz-mklogo.py ORIGINAL-logo.img LOGO.png AUSGABE.img`.
- Bauen: lokal siehe [README „Selbst bauen“](../README.md#selbst-bauen), in GitHub Actions
  `.github/workflows/build.yml` (manuell oder Tag `v*` → Release mit Images).
- Nach Änderungen an einem Paket: `pkgrel` im `APKBUILD` erhöhen und `sha512sums` aktualisieren.

## 20. Bekannte Grenzen

- **Kein Bluetooth** (Hersteller-Kernel ohne `CONFIG_BT`).
- **Keine Kameras, keine GPU-Beschleunigung, keine Sensoren, kein Kopfhörer-Ausgang.**
- **Kein Vibrationsmotor** (der Gerätebaum nennt einen, er reagiert nie).
- Akkustand nur geschätzt.
- Browser/Web-Apps ohne Sandbox – nur vertrauenswürdige Seiten.
- Kernel nicht austauschbar (keine Quellen vom Hersteller).
