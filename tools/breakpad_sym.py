#!/usr/bin/env python3
"""Kush 1.1: resolve an RVA against a Mozilla breakpad .sym file.
Usage: breakpad_sym.py xul.sym 0x1234 [0x5678 ...]"""
import sys, bisect
funcs, lines = [], []
cur = None
for l in open(sys.argv[1], errors='replace'):
    if l.startswith('FUNC '):
        p = l.split(None, 4)
        if p[1] == 'm': p = l.split(None, 5)[1:]
        a, sz, name = int(p[1], 16), int(p[2], 16), p[4].strip()
        funcs.append((a, sz, name)); cur = name
    elif l.startswith('PUBLIC '):
        p = l.split(None, 3)
        if p[1] == 'm': p = l.split(None, 4)[1:]
        funcs.append((int(p[1], 16), 0, p[3].strip()))
    elif l.startswith('FILE '):
        pass
    elif l[:1] in '0123456789abcdef' and cur:
        p = l.split()
        if len(p) == 4:
            lines.append((int(p[0], 16), int(p[1], 16), int(p[2]), int(p[3])))
files = {}
for l in open(sys.argv[1], errors='replace'):
    if l.startswith('FILE '):
        p = l.split(None, 2)
        if p[1] == 'm': p = l.split(None, 3)[1:]
        files[int(p[1])] = p[2].strip()
funcs.sort(); lines.sort()
fa = [f[0] for f in funcs]; la = [x[0] for x in lines]
for a in sys.argv[2:]:
    r = int(a, 16)
    i = bisect.bisect_right(fa, r) - 1
    j = bisect.bisect_right(la, r) - 1
    src = ''
    if j >= 0 and lines[j][0] <= r < lines[j][0] + lines[j][1]:
        src = ' %s:%d' % (files.get(lines[j][3], '?'), lines[j][2])
    print(hex(r), funcs[i][2] + '+' + hex(r - funcs[i][0]) if i >= 0 else '?', src)
