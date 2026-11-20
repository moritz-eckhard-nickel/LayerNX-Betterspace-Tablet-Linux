#!/usr/bin/env python3
# moritz-preloader-fastboot.py: schickt dem MTK-Preloader ueber dessen USB-CDC-Port
# "FASTBOOT", damit LK direkt in den Fastboot-Modus startet (kein Tastendruck noetig).
import glob, os, sys, time, termios
deadline = time.time() + float(sys.argv[1] if len(sys.argv) > 1 else 120)
while time.time() < deadline:
    for dev in glob.glob('/dev/ttyACM*'):
        try:
            fd = os.open(dev, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        except OSError:
            continue
        try:
            a = termios.tcgetattr(fd)
            a[0] = a[1] = a[3] = 0
            termios.tcsetattr(fd, termios.TCSANOW, a)
            for _ in range(30):
                os.write(fd, b'FASTBOOT')
                time.sleep(0.05)
                try:
                    r = os.read(fd, 64)
                except BlockingIOError:
                    r = b''
                if r:
                    print(dev, 'Antwort:', r)
                    if b'TOOBTSAF' in r:
                        sys.exit(0)
        except OSError:
            pass
        finally:
            os.close(fd)
    time.sleep(0.05)
print('kein Erfolg')
sys.exit(1)
