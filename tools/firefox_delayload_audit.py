#!/usr/bin/env python3
"""Kush 1.1: list delay-load imports of PE files and check them against Kush exports.

Usage: firefox_delayload_audit.py <firefox_dir> <kush_build_dir>
A missing delay-loaded function raises 0xC06D007F at first call, which Firefox does
not catch, so CSRSS kills the process with STATUS_ABANDONED (exit code 128).
"""
import struct, sys, os, subprocess, glob

def sections(d):
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    so = pe + 24 + opt
    secs = []
    for i in range(nsec):
        va, vs, ra, rs = struct.unpack_from('<IIII', d, so + i * 40 + 8)[:4]
        vs, va, rs, ra = struct.unpack_from('<IIII', d, so + i * 40 + 8)
        secs.append((va, max(vs, rs), ra))
    return pe, secs

def r2o(secs, rva):
    for va, sz, ra in secs:
        if va <= rva < va + sz:
            return rva - va + ra
    return None

def cstr(d, o):
    return d[o:d.index(b'\0', o)].decode('latin1')

def delay_imports(path):
    d = open(path, 'rb').read()
    pe, secs = sections(d)
    magic = struct.unpack_from('<H', d, pe + 24)[0]
    ddo = pe + 24 + (112 if magic == 0x20b else 96)
    rva, size = struct.unpack_from('<II', d, ddo + 13 * 8)
    out = {}
    if not rva:
        return out
    o = r2o(secs, rva)
    while True:
        f = struct.unpack_from('<8I', d, o)
        if not f[1]:
            break
        dll = cstr(d, r2o(secs, f[1]))
        io = r2o(secs, f[4])
        names = []
        while True:
            t = struct.unpack_from('<Q', d, io)[0]
            if not t:
                break
            names.append('#%d' % (t & 0xffff) if t >> 63 else cstr(d, r2o(secs, t & 0x7fffffff) + 2))
            io += 8
        out[dll] = names
        o += 32
    return out

_exp = {}
def kush_exports(build, dll):
    key = dll.lower()
    if key in _exp:
        return _exp[key]
    hits = [p for p in glob.glob(os.path.join(build, '**', '*'), recursive=True)
            if os.path.basename(p).lower() == key]
    if not hits:
        _exp[key] = None
        return None
    txt = subprocess.run(['x86_64-w64-mingw32-objdump', '-p', hits[0]],
                         capture_output=True, text=True).stdout
    names, on = set(), False
    for line in txt.splitlines():
        if line.startswith('[Ordinal/Name Pointer] Table'):
            on = True
            continue
        if on:
            s = line.strip()
            if not s:
                break
            names.add(s.split(']')[-1].strip())
    _exp[key] = names
    return names

if __name__ == '__main__':
    ff, build = sys.argv[1], sys.argv[2]
    for f in sorted(glob.glob(os.path.join(ff, '*.dll')) + glob.glob(os.path.join(ff, '*.exe'))):
        try:
            di = delay_imports(f)
        except Exception as e:
            print('ERR', f, e)
            continue
        for dll, fns in di.items():
            if dll.lower().startswith(('api-ms-', 'ext-ms-')):
                print('%s: %s (API set, %d funcs) -> check apisets table' % (os.path.basename(f), dll, len(fns)))
                continue
            ex = kush_exports(build, dll)
            if ex is None:
                print('%s: %s MISSING DLL (%d funcs)' % (os.path.basename(f), dll, len(fns)))
                continue
            miss = [n for n in fns if not n.startswith('#') and n not in ex]
            if miss:
                print('%s: %s missing %d/%d: %s' % (os.path.basename(f), dll, len(miss), len(fns), ' '.join(miss)))
