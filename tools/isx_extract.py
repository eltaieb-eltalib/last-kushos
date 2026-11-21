# Minimal ISSetupStream extractor (port of ISx by lifenjoiner, MIT)
import struct, sys, os, zlib
src, out = sys.argv[1], sys.argv[2]
f = open(src, 'rb')
hdr = f.read(46)
assert hdr[:13] == b'ISSetupStream', hdr[:14]
num, typ = struct.unpack_from('<HI', hdr, 14)
print('files', num, 'type', typ)
MAGIC = [0x13, 0x35, 0x86, 0x07]
def dec(buf, key):
    kl = len(key); b = bytearray(buf)
    for i in range(len(b)):
        x = b[i]; b[i] = (~(key[i % kl] ^ (((x << 4) | (x >> 4)) & 0xff))) & 0xff
    return bytes(b)
os.makedirs(out, exist_ok=True)
for n in range(num):
    a = f.read(24)
    fnlen, flags = struct.unpack_from('<II', a, 0)
    flen, = struct.unpack_from('<I', a, 10)
    launcher, = struct.unpack_from('<H', a, 22)
    if typ == 4: f.read(24)
    name = f.read(fnlen).decode('utf-16le').rstrip('\0')
    seed = name.encode('utf-8')
    key = bytes(seed[i] ^ MAGIC[i % 4] for i in range(len(seed)))
    print(n, name, flen, hex(flags), launcher, flush=True)
    path = os.path.join(out, name.replace('\\', '/'))
    os.makedirs(os.path.dirname(path) or out, exist_ok=True)
    left = flen
    with open(path, 'wb') as w:
        if (flags & 4):
            while left:
                c = f.read(min(1024, left)); left -= len(c)
                w.write(dec(c, key))
        else:
            while left:
                c = f.read(min(1 << 20, left)); left -= len(c); w.write(c)
    if launcher:
        d = open(path, 'rb').read()
        try:
            open(path, 'wb').write(zlib.decompress(d)); print('  inflated')
        except Exception as e: print('  inflate failed', e)
