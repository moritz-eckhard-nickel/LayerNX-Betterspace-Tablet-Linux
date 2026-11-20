#!/bin/bash
# moritz-dump-android.sh - liest Hardware-Informationen und Images vom laufenden
# (gerooteten) Android des Betterspace V5x aus. Schreibt NICHTS auf das Geraet
# ausser temporaeren Dateien unter /data/local/tmp/moritz.
set -u
OUT="${1:-/home/moritz/moritz-betterspace-v5x/device-dumps/android-$(date +%Y%m%d-%H%M%S)}"
mkdir -p "$OUT"/{img,info,vendor}
cd "$OUT" || exit 1

A() { adb shell "$@"; }
S() { adb shell "su -c '$*'"; }

echo "== Identitaet pruefen"
A getprop > info/getprop.txt
for p in ro.product.board ro.board.platform ro.hardware ro.product.model ro.product.brand \
         ro.product.manufacturer ro.boot.verifiedbootstate ro.boot.flash.locked \
         ro.build.fingerprint ro.oem_unlock_supported sys.oem_unlock_allowed; do
  printf '%-30s %s\n' "$p" "$(grep "\[$p\]" info/getprop.txt | cut -d: -f2-)"
done | tee info/identity.txt
grep -q 'mt8168' info/getprop.txt || { echo "ACHTUNG: kein mt8168 in getprop - Abbruch"; exit 2; }
S id | tee info/su-id.txt

echo "== Kernel / System"
S cat /proc/cmdline > info/cmdline.txt
S cat /proc/version > info/version.txt
adb exec-out "su -c 'cat /proc/config.gz'" > info/config.gz && gunzip -kf info/config.gz
S dmesg > info/dmesg.txt
S cat /proc/partitions > info/partitions.txt
S cat /proc/mounts > info/mounts.txt
S lsmod > info/lsmod.txt
S cat /proc/cpuinfo > info/cpuinfo.txt
S cat /proc/meminfo > info/meminfo.txt
S cat /proc/bus/input/devices > info/input-devices.txt
S "ls -l /dev/block/by-name/" > info/by-name.txt
S "for f in /dev/block/by-name/*; do echo \$(basename \$f) \$(readlink -f \$f) \$(blockdev --getsize64 \$f); done" > info/partition-sizes.txt
S "ls -la /dev/" > info/dev.txt
S "ls -la /dev/graphics /dev/input /dev/dri 2>&1" > info/dev-gfx-input.txt
S "for f in /sys/class/graphics/fb0/*; do [ -f \$f ] && echo \$f: \$(cat \$f 2>/dev/null | head -3); done" > info/fb0-sysfs.txt
S "for d in /sys/bus/i2c/devices/*; do echo \$d \$(cat \$d/name 2>/dev/null) \$(readlink \$d/driver); done" > info/i2c.txt
S "ls /sys/bus/platform/drivers/" > info/platform-drivers.txt
S "getevent -lp" > info/getevent-lp.txt 2>&1
S "ls -l /dev/wmt* /dev/stp* /dev/conn* /dev/fm /dev/btif 2>&1" > info/wmt-dev.txt
S "ls -la /sys/class/net/; ip addr" > info/net.txt
S "cat /proc/fb" > info/proc-fb.txt
S "cat /sys/kernel/debug/clk/clk_summary 2>/dev/null | head -400" > info/clk.txt

echo "== Device-Tree (live)"
S "cd /proc/device-tree && tar -cf /data/local/tmp/moritz-dt.tar ." && adb pull /data/local/tmp/moritz-dt.tar info/ >/dev/null
mkdir -p info/dt && tar -xf info/moritz-dt.tar -C info/dt 2>/dev/null
dtc -q -I fs -O dts -o info/live-devicetree.dts info/dt 2>/dev/null && echo "  live-devicetree.dts ok"

echo "== Partitionen lesen (nur lesen)"
S "mkdir -p /data/local/tmp/moritz"
for p in boot recovery dtbo vbmeta vbmeta_system vbmeta_vendor lk lk2 logo para misc; do
  if S "test -e /dev/block/by-name/$p" ; then
    S "dd if=/dev/block/by-name/$p of=/data/local/tmp/moritz/$p.img bs=1M 2>/dev/null; chmod 644 /data/local/tmp/moritz/$p.img"
    adb pull /data/local/tmp/moritz/$p.img img/ >/dev/null && echo "  $p.img $(stat -c %s img/$p.img)"
    S "rm -f /data/local/tmp/moritz/$p.img"
  fi
done

echo "== Vendor-Module und Firmware (vom Geraet selbst)"
S "tar -cf /data/local/tmp/moritz/vendor-fw.tar /vendor/lib/modules /vendor/firmware /vendor/etc/firmware /lib/modules 2>/dev/null; chmod 644 /data/local/tmp/moritz/vendor-fw.tar"
adb pull /data/local/tmp/moritz/vendor-fw.tar vendor/ >/dev/null && tar -tf vendor/vendor-fw.tar > vendor/filelist.txt
S "ls -la /vendor/bin /vendor/etc/init /vendor/etc/init/hw" > vendor/vendor-bin-init.txt
S "cat /vendor/etc/init/hw/*.rc /vendor/etc/init/*conn*.rc /vendor/etc/init/*wifi*.rc /vendor/etc/init/*wmt*.rc 2>/dev/null" > vendor/init-rc.txt
S "rm -rf /data/local/tmp/moritz /data/local/tmp/moritz-dt.tar"

echo "== Fertig: $OUT"
ls -la img info | head -60
