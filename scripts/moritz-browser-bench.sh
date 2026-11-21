#!/bin/sh
# moritz-browser-bench.sh - automatischer Browser-Vergleich auf dem Tablet (als root starten)
# Faelle: chromium/badwolf jeweils mit Xorg-Drehung (quer) und ohne (hochkant)
U=moritz; URL="${URL:-https://en.wikipedia.org/wiki/Linux}"; OUT=/tmp/bench; mkdir -p $OUT; : > $OUT/ergebnis.txt
CONF=/etc/X11/xorg.conf.d/10-moritz-v5x.conf
asu() { su $U -c "DISPLAY=:0 $*"; }
xrestart() { rc-service moritz-v5x-xsession restart >/dev/null 2>&1; sleep 8; }
cpu_sample() { # $1 Sekunden -> Durchschnitt CPU% je Prozessgruppe
	top -b -d 1 -n "$1" | awk '
	/^ *[0-9]+ / { c=$9; n=$NF; g="";
		if (n ~ /Xorg/) g="Xorg"; else if (n ~ /chrom/) g="chromium"; else if (n ~ /WebKit|badwolf/) g="webkit";
		else if (n ~ /fbrefresh/) g="fbrefresh"; else if (n ~ /moritz-v5x-touch/) g="touch";
		if (g!="") s[g]+=c }
	/^top -/ { f++ }
	END { for (g in s) printf "%s=%.0f%% ", g, s[g]/(f?f:1); print "" }'
}
run_case() { # name startcmd killpattern scrollx scrolly
	name=$1; start=$2; kp=$3; sx=$4; sy=$5
	pkill -f "$kp" 2>/dev/null; sleep 2
	t0=$(date +%s.%N)
	asu "$start" >/dev/null 2>&1 </dev/null &
	lt="timeout"
	for i in $(seq 1 120); do
		asu "xdotool search --onlyvisible --name 'Linux'" >/dev/null 2>&1 && { lt=$(echo "$(date +%s.%N) - $t0" | bc); break; }
		sleep 0.5
	done
	sleep 6
	idle=$(cpu_sample 5)
	asu "xdotool mousemove $sx $sy" ; 
	( for i in $(seq 1 60); do asu "xdotool click 5"; sleep 0.25; done ) &
	sp=$!
	scroll=$(cpu_sample 15)
	wait $sp
	asu "xwd -root -silent" > $OUT/$name.xwd
	mem=$(free -m | awk '/Mem:/{print $3"MB belegt"}')
	echo "$name | Ladezeit=${lt}s | Leerlauf: $idle| Scrollen: $scroll| $mem" | tee -a $OUT/ergebnis.txt
	pkill -f "$kp" 2>/dev/null; sleep 2
}
CHR="chromium --no-sandbox --test-type --no-first-run --password-store=basic --disable-gpu --user-data-dir=/tmp/bench-chr --start-maximized $URL"
BW="env WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1 badwolf $URL"
cp $CONF $OUT/conf.orig
[ -z "$NUR_WEBKIT" ] && run_case chromium-quer "$CHR" "bench-chr" 640 400
run_case badwolf-quer "$BW" "badwolf" 640 400
sed -i '/Option "Rotate"/d' $CONF; xrestart
[ -z "$NUR_WEBKIT" ] && run_case chromium-hochkant "$CHR" "bench-chr" 400 600
run_case badwolf-hochkant "$BW" "badwolf" 400 600
cp $OUT/conf.orig $CONF; xrestart
echo FERTIG >> $OUT/ergebnis.txt
