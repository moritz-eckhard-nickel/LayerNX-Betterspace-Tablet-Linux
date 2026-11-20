#!/usr/bin/env python3
# moritz-fb-raw.py: liest/schreibt das Fastboot-Interface roh (Diagnose bei haengendem LK)
import sys, usb.core, usb.util
d = usb.core.find(idVendor=0x0e8d, idProduct=0x201c)
if d is None: sys.exit("kein Geraet")
try:
    if d.is_kernel_driver_active(0): d.detach_kernel_driver(0)
except Exception: pass
usb.util.claim_interface(d, 0)
def rd(t=1500):
    try: return bytes(d.read(0x81, 512, timeout=t))
    except usb.core.USBTimeoutError: return None
print("vorher anstehend:", rd())
for cmd in sys.argv[1:]:
    print(">>", cmd)
    try: d.write(0x01, cmd.encode(), timeout=2000)
    except Exception as e: print("write-fehler:", e); continue
    for _ in range(5):
        r = rd(3000)
        print("<<", r)
        if r is None or not r.startswith(b"INFO"): break
