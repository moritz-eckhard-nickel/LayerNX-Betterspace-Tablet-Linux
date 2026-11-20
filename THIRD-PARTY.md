# Drittanbieter-Dateien / Third-party files

> **Diese Dateien gehören nicht dem Projekt.** Sie sind enthalten, weil sie für den Port technisch
> notwendig bzw. zur Nachvollziehbarkeit erforderlich sind. **Entfernung auf Anfrage der
> Rechteinhaber per E-Mail an [moritz@onlymoritz.de](mailto:moritz@onlymoritz.de) – unverzüglich.**

| Datei(en) | Herkunft | Rechteinhaber / Lizenz |
|---|---|---|
| `pmaports-device/linux-betterspace-v5x/stock-Image.gz` | boot-Partition, Android 10 `Pad_X8_TELCEL_SW_02` | Linux-Kernel (GPL-2.0), Build des Herstellers BND/LANIX; Quelltext nicht veröffentlicht |
| `pmaports-device/linux-betterspace-v5x/stock-dt-table.img` | boot-Partition | Hersteller / MediaTek |
| `pmaports-device/linux-betterspace-v5x/*.ko` | `/vendor/lib/modules` | MediaTek (CONNSYS-Treiber) |
| `pmaports-device/linux-betterspace-v5x/config-stock.aarch64` | `/proc/config.gz` | Linux-Kernel-Konfiguration |
| `pmaports-device/firmware-betterspace-v5x/*` | `/vendor/firmware` | MediaTek, proprietär |
| `device-dumps/android-stock/img/*.img` | Partitionen boot, recovery, dtbo, vbmeta*, lk, lk2, logo, para, items | MediaTek, LANIX, BND, Google |
| `device-dumps/android-stock/vendor/*` | `/vendor`, `/apex`, `/system/lib64` | MediaTek, LANIX, BND, Google |
| `device-dumps/android-stock/unpack/*` | entpacktes Original-boot.img | wie oben |
| `images/*` | gebaute Images; enthalten die oben genannten Dateien sowie Alpine-/postmarketOS-Pakete | jeweilige Lizenzen |
| `build/tests/*.img` | Test-boot.img mit Stock-Kernel | wie oben |

Geschwärzt wurden: Geräteseriennummer, WLAN-MAC-Adressen, WLAN-Netzname/BSSID.
