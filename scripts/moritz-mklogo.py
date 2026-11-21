#!/usr/bin/env python3
# moritz-mklogo.py - ersetzt Bild 0 (Bootlogo) in einer MediaTek-logo.img.
# Format (am Original verifiziert): 512-Byte-Partitionsheader (Magic 0x58881688, Name "logo"),
# danach: uint32 Anzahl, uint32 Gesamtgroesse, uint32 Offsets[Anzahl], zlib-komprimierte
# Rohbilder 1280x800 BGRA (32 bpp). Alle anderen Bilder bleiben unveraendert.
# Aufruf: moritz-mklogo.py ORIGINAL.img LOGO.png AUSGABE.img [Logobreite]
import io, struct, subprocess, sys, zlib

src, logo, out = sys.argv[1:4]
width = int(sys.argv[4]) if len(sys.argv) > 4 else 520
W, H = 1280, 800

d = open(src, 'rb').read()
magic, size = struct.unpack('<II', d[:8])
assert magic == 0x58881688 and d[8:12] == b'logo', 'kein MTK-logo.img'
body = d[512:512 + size]
cnt, total = struct.unpack('<II', body[:8])
offs = list(struct.unpack('<%dI' % cnt, body[8:8 + 4 * cnt]))
imgs = [body[offs[i]:(offs[i + 1] if i + 1 < cnt else total)] for i in range(cnt)]

# neues Bild 0: schwarzer Hintergrund, Logo zentriert, BGRA mit voller Deckkraft
raw = subprocess.run(['convert', '-size', '%dx%d' % (W, H), 'xc:black',
                      '(', logo, '-resize', '%dx' % width, ')',
                      '-gravity', 'center', '-composite', '-alpha', 'off',
                      '-alpha', 'set', '-channel', 'A', '-evaluate', 'set', '100%', '+channel',
                      '-depth', '8', 'bgra:-'], check=True, capture_output=True).stdout
assert len(raw) == W * H * 4, len(raw)
imgs[0] = zlib.compress(raw, 9)

# Container neu aufbauen
hdr_len = 8 + 4 * cnt
new_offs, pos = [], hdr_len
for im in imgs:
    new_offs.append(pos)
    pos += len(im)
newbody = struct.pack('<II', cnt, pos) + struct.pack('<%dI' % cnt, *new_offs) + b''.join(imgs)
header = bytearray(d[:512])
header[4:8] = struct.pack('<I', len(newbody))
res = bytes(header) + newbody
part = len(d)  # Partitionsgroesse (Dump = ganze Partition)
assert len(res) <= part, 'zu gross fuer die Partition'
open(out, 'wb').write(res + b'\0' * (part - len(res)))
print('ok: %s, Bild 0 neu (%d Byte komprimiert), gesamt %d von %d Byte' % (out, len(imgs[0]), len(res), part))
