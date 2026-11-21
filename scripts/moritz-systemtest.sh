#!/bin/sh
# moritz-systemtest.sh - automatischer Funktionstest fuer LayerNX-Tablet-Linux (auf dem Tablet als root)
# Ausgabe: PASS/FAIL/WARN je Pruefung, am Ende Zusammenfassung. Exitcode = Anzahl FAIL.
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
[ "$(id -u)" = 0 ] || { echo "bitte als root starten (doas sh $0)"; exit 99; }
U="$(awk -F: '$3 >= 1000 && $3 < 60000 {print $1; exit}' /etc/passwd)"; [ -n "$U" ] || U=root
P=0; F=0; W=0
ok()   { P=$((P+1)); printf 'PASS  %-34s %s\n' "$1" "$2"; }
bad()  { F=$((F+1)); printf 'FAIL  %-34s %s\n' "$1" "$2"; }
warn() { W=$((W+1)); printf 'WARN  %-34s %s\n' "$1" "$2"; }
chk()  { name=$1; shift; if "$@" >/dev/null 2>&1; then ok "$name"; else bad "$name" "($*)"; fi; }
asu()  { su "$U" -c "DISPLAY=:0 $*"; }
svc()  { rc-service "$1" status >/dev/null 2>&1; }

echo "== LayerNX-Systemtest $(date '+%F %T') auf $(hostname)"
echo "-- Boot & Kernel"
c=$(cat /proc/cmdline)
case "$c" in *bootopt=64S3*) ok "cmdline bootopt" ;; *) bad "cmdline bootopt" ;; esac
case "$c" in *initrd=0x55000000,*) ok "cmdline initrd" ;; *) bad "cmdline initrd" ;; esac
sz=$(stat -c %s /boot/boot.img 2>/dev/null)
if [ -n "$sz" ] && cmp -s -n "$sz" /boot/boot.img /dev/disk/by-partlabel/boot; then ok "boot-Partition = /boot/boot.img"; else bad "boot-Partition = /boot/boot.img" "(Update nicht geflasht?)"; fi
if head -c 512 /dev/disk/by-partlabel/logo | grep -q logo; then ok "logo-Partition gueltig"; else bad "logo-Partition gueltig"; fi
chk "Kernel 4.14.141" sh -c 'uname -r | grep -q 4.14.141'

echo "-- Dienste"
for s in mdev moritz-v5x-devnodes moritz-v5x-wifi moritz-v5x-usbnet wpa_supplicant networking sshd \
	moritz-v5x-touch moritz-v5x-fbrefresh moritz-v5x-keys moritz-v5x-xsession layernx-hostname; do
	if svc "$s"; then ok "Dienst $s"; else bad "Dienst $s"; fi
done
if svc layernx-akkuschutz; then ok "Dienst layernx-akkuschutz" "(an)"; else ok "Dienst layernx-akkuschutz" "(aus, Standard)"; fi
for s in tailscale cloudflared; do svc "$s" && warn "Dienst $s" "laeuft (Standard: aus)" || ok "Dienst $s" "(nicht aktiv, Standard)"; done

echo "-- Netzwerk"
ip4=$(ip -4 addr show wlan0 2>/dev/null | awk '/inet /{print $2}')
[ -n "$ip4" ] && ok "WLAN-IP" || bad "WLAN-IP"
chk "Internet (ping 1.1.1.1)" ping -c1 -W3 1.1.1.1
chk "DNS (dl-cdn.alpinelinux.org)" sh -c 'getent hosts dl-cdn.alpinelinux.org || nslookup dl-cdn.alpinelinux.org'
chk "apk update" apk update -q
ip -4 addr show usb0 2>/dev/null | grep -q 172.16.42.1 || ip -4 addr show rndis0 2>/dev/null | grep -q 172.16.42.1 && ok "USB-Netz 172.16.42.1" || warn "USB-Netz 172.16.42.1" "(kein USB-Host?)"

echo "-- Hardware"
chk "WLAN-Chip (/dev/wmtWifi)" test -e /dev/wmtWifi
chk "Touch-Bruecke /dev/input/moritz-touch" test -e /dev/input/moritz-touch
chk "Gestengeraet /dev/input/moritz-gestures" test -e /dev/input/moritz-gestures
kpd=""; for n in /sys/class/input/event*; do [ "$(cat $n/device/name 2>/dev/null)" = mtk-kpd ] && kpd=/dev/input/$(basename $n); done
holder=""; for p in /proc/[0-9]*; do ls -la "$p/fd" 2>/dev/null | grep -q " $kpd\$" && holder="$holder $(cat $p/comm)"; done
case "$holder" in *moritz-v5x-key*) [ "$(echo $holder | wc -w)" = 1 ] && ok "Power-Taste nur von moritz-v5x-keys" || bad "Power-Taste exklusiv" "($holder)";; *) bad "Power-Taste exklusiv" "($holder)";; esac
b=$(cat /sys/class/leds/lcd-backlight/brightness); [ "$b" -gt 0 ] && ok "Hintergrundbeleuchtung" "($b)" || warn "Hintergrundbeleuchtung" "(aus)"
v=$(cat /sys/class/power_supply/battery/voltage_now); [ "$v" -gt 3300000 ] && ok "Akkuspannung" "($v uV)" || bad "Akkuspannung" "($v)"
pc=$(moritz-v5x-status prozent); [ "$pc" -ge 0 ] 2>/dev/null && ok "Akku-Schaetzung" "(~$pc%)" || bad "Akku-Schaetzung"
chk "Ladesteuerung erreichbar" test -w /proc/mtk_battery_cmd/current_cmd

echo "-- Grafik"
chk "Xorg laeuft" pgrep -x Xorg
chk "Openbox laeuft" pgrep -x openbox
chk "tint2 laeuft" pgrep -x tint2
chk "Dreh-Treiber moritz-xf86-video-fbdev" apk info -e moritz-xf86-video-fbdev
chk "Xorg: Rotate CCW" grep -q '"Rotate" "CCW"' /etc/X11/xorg.conf.d/10-moritz-v5x.conf
chk "Xorg -nocursor" sh -c 'cat /proc/$(pgrep -x Xorg)/cmdline | tr "\0" " " | grep -q nocursor'
ee=$(grep "^\[.*(EE)" /var/log/Xorg.0.log | grep -v -E 'module "(kbd|libinput)"|matching .kbd.' | wc -l)
[ "$ee" = 0 ] && ok "Xorg-Log ohne echte (EE)" "(kbd/libinput-Suche ist erwartet)" || bad "Xorg-Log ohne echte (EE)" "($ee)"
geo=$(asu "xdotool getdisplaygeometry"); [ "$geo" = "1280 800" ] && ok "Bildschirm 1280x800 quer" || bad "Bildschirm 1280x800 quer" "($geo)"
fb=$(head -c 4096000 /dev/fb0 | od -An -tx1 -j $(( (1279*800+799)*4 )) -N 4 | tr -d ' '); [ -n "$fb" ] && ok "Framebuffer lesbar" || bad "Framebuffer lesbar"

echo "-- Programme (startet, Fenster erscheint, kein Absturz)"
gui() { # name befehl fenstertitel-regex prozessname
	lg=/tmp/st-$(echo "$1" | tr -c 'A-Za-z0-9\n' '_').log; rm -f "$lg"
	asu "$2 >$lg 2>&1 &"; i=0; found=""
	while [ $i -lt 20 ]; do asu "xdotool search --onlyvisible --name '$3'" >/dev/null 2>&1 && { found=1; break; }; sleep 1; i=$((i+1)); done
	if [ -n "$found" ] && ! grep -qi "segmentation\|core dumped" "$lg"; then ok "$1" "(${i}s)"; else bad "$1" "($(tail -1 "$lg" 2>/dev/null))"; fi
	pkill -x "$4" 2>/dev/null; sleep 1
}
gui "Einstellungen" "env -i HOME=\$HOME DISPLAY=:0 PATH=/usr/bin:/bin layernx-einstellungen" '^Einstellungen$' layernx-einstel
gui "PIN-Pad" "layernx-pinpad --titel Test" '^LayerNX PIN$' layernx-pinpad
printf '<html><head><title>LNXTEST</title></head><body>ok</body></html>' > /tmp/lnxtest.html; chmod 644 /tmp/lnxtest.html
gui "Webview (WebKitGTK)" "layernx-webview --titel LNXTEST --profil /tmp/st-profil file:///tmp/lnxtest.html" '^LNXTEST$' layernx-webview
chk "Chromium vorhanden" chromium --version
chk "Terminal lxterminal" which lxterminal
chk "fastfetch mit LayerNX-Konfig" sh -c 'fastfetch --config /usr/share/layernx/fastfetch.jsonc --pipe true | grep -q LayerNX'
for t in layernx-update layernx-display layernx-akku layernx-webapp layernx-pin layernx-powerkey layernx-einstellungen-helfer \
	moritz-wmt-launcher moritz-v5x-status moritz-v5x-bootanim cloudflared tailscale bash; do chk "Befehl $t" which $t; done
chk "layernx-display status" layernx-display status
chk "layernx-akku status" layernx-akku status

echo "-- Branding"
chk "os-release LayerNX" grep -q 'NAME="LayerNX-Tablet-Linux"' /etc/os-release
hostname | grep -Eq '^LNXTL-[A-Z0-9]{4}$' && ok "Hostname LNXTL-XXXX" "($(hostname))" || warn "Hostname LNXTL-XXXX" "($(hostname), manuell geaendert?)"
chk "Hintergrundbild" test -s /usr/share/layernx/background.png
n=$(ls /usr/share/layernx/bootanim/*.png 2>/dev/null | wc -l); [ "$n" -ge 100 ] && ok "Boot-Animation" "($n Bilder)" || bad "Boot-Animation" "($n)"
chk "Openbox-Theme LayerNX" test -f /usr/share/themes/LayerNX/openbox-3/themerc

echo "-- Sicherheit & Zugang"
chk "sshd erlaubt root" sh -c '$(ls /usr/sbin/sshd /usr/sbin/sshd.pam 2>/dev/null | head -1) -T -C user=root,host=test,addr=127.0.0.1 2>/dev/null | grep -qi "^permitrootlogin yes"'
s=$(grep '^root:' /etc/shadow | cut -d: -f2); case "$s" in '$'*) ok "root-Passwort gesetzt" ;; *) bad "root-Passwort gesetzt" ;; esac
[ -f /etc/layernx/nur-root-ausstehend ] && warn "Erststart ausstehend" "(Build-Benutzer noch nicht entfernt)" || ok "Erststart erledigt/nicht noetig"

echo "-- System"
fr=$(df -m / | awk 'NR==2{print $4}'); [ "$fr" -gt 1000 ] && ok "Speicher frei" "(${fr} MB)" || warn "Speicher frei" "(${fr} MB)"
m=$(free -m | awk '/Mem:/{print $7}'); [ "$m" -gt 300 ] && ok "RAM verfuegbar" "(${m} MB)" || warn "RAM verfuegbar" "(${m} MB)"
dmesg | grep -qi "Kernel panic\|Oops:" && bad "dmesg ohne Oops/Panic" || ok "dmesg ohne Oops/Panic"

echo "== Ergebnis: $P PASS, $F FAIL, $W WARN"
exit $F
