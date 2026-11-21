# Aufgabenliste – LayerNX-Tablet-Linux (Stand 20.11.2026)

Start erst, wenn Moritz „fang an“ sagt. Reihenfolge einhalten. Nach jedem Punkt: am Gerät testen,
Ergebnis in docs/PROTOKOLL.md, ungetestetes als [UNGETESTET] markieren.

## Quellen (Uploads)
- Bootlogo: `4.png` aus `~/.claude/uploads/0fbc023b-f929-51f8-9712-2038afca219c/f2b9b743-LayerNX-transp.zip`
  (LayerNX-Logo, transparenter Hintergrund; restliche Dateien der ZIP irrelevant)
- Boot-Animation (Scribbles): `~/.claude/uploads/0fbc023b-f929-51f8-9712-2038afca219c/170ccc9a-moritz-bootanimation-tablet-1280x800.zip` (Android-bootanimation-Format)
- Hintergrundbild „Tablet Linux“ (1280×800): Bild aus dem Chat (`/tmp/claude-1000/-home-moritz/0fbc023b-f929-51f8-9712-2038afca219c/images/2.png`, ggf. neu anfordern, falls gelöscht)
- Markenfarbe Lila: **#4931fd**, Name: **LayerNX-Tablet-Linux**

## 1. Strom & Akku
- [x] Am Gerät prüfen, welche sysfs-Einträge (`usb`, `ac`, `charger` `/online`) bei microUSB, Hohlstecker und Dockingstation gesetzt werden
- [x] Ladeanzeige (⚡/Netz) in der tint2-Leiste
- [x] Akkustand in % (Plan A, Schätzung): Plan A Schätzung aus `voltage_now` (+ Gauge-Rohwerte); Plan B Android-`fuelgauged` aus Dump (bionic) testen

## 2. Branding (Paket `layernx-branding`)
- [x] **Bootlogo** (ersetzt „Betterspace“): `4.png` **verkleinert, zentriert auf schwarzem Hintergrund**, 800×1280 hochkant (Panel nativ, später 270° gedreht → Ausrichtung prüfen); Format des Original-`logo.img` analysieren (MTK-Logo, Backup in device-dumps), erst dann in Partition `logo` schreiben
- [x] **Boot-Animation** aus der Scribbles-ZIP während des Linux-Starts (pbsplash oder eigener fb-Abspieler; Panel hochkant → Frames drehen)
- [x] **Hintergrundbild** „Tablet Linux“ als Desktop
- [x] `os-release`, Hostname-Vorschlag, Openbox/tint2/jgmenu/svkbd-Farben auf #4931fd + Schwarz

## 3. Leiste
- [x] IP-Adresse (WLAN, sonst USB) unten in der Leiste (tint2-Executor); SSH bleibt Autostart

## 4. Echter Touch
- [x] Mauszeiger ausblenden
- [x] Touch-Brücke auf echtes Touch-Gerät (MT-Slots, INPUT_PROP_DIRECT) umbauen → Scrollen/Wischen im Browser

## 5. Browser
- [x] Ziel: spätere WebUI (Echo-Show-artig) zuverlässig laden, klickbar per Touch
- [x] Chromium vs. WebKitGTK gemessen → WebKitGTK + eigener Dreh-Treiber (Software-Rendering, Touch-Flags, zram) testen; falls zu träge: WebKitGTK-Browser

## 6. Bluetooth
- [x] geprüft: kein CONFIG_BT → nur Userspace-Stack (BTstack) möglich, offen
- [ ] blueman (Maus, Tastatur, Lautsprecher; Lautsprecher braucht PipeWire)

## 7. Abschluss
- [ ] Gesamtimage frisch flashen und komplett testen (vorher WLAN-Daten/MAC notieren)
- [ ] Repo + Images aktualisieren, Disclaimer beibehalten, nichts Privates

## 8. Zweites Tablet (bootloopend)
- [ ] Am Ende fertiges Image per Preloader-Trick (`scripts/moritz-preloader-fastboot.py`) flashen: vbmeta (disabled), userdata, boot, logo
