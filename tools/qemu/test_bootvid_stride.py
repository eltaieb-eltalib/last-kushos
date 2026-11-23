#!/usr/bin/env python3
"""Exercise the production bootvid BitBlt with 32-bit signed row offsets.

Run on a 64-bit POSIX host with a C compiler. The old unsigned signature is
compiled separately as a regression control and must fail the bottom-up case.
No guest, private files, or installed disks are touched.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'drivers/base/bootvid/common.c').read_text()
function = source[source.index('static VOID\nBitBlt('):source.index('\nstatic VOID\nRleBitBlt(')]
prefix = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
typedef void VOID;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef unsigned char UCHAR, *PUCHAR;
#define _In_
#define _In_reads_bytes_(x)
#define DBG 0
static unsigned count, values[4];
static void PrepareForSetPixel(void) {}
static void SetPixel(ULONG x, ULONG y, UCHAR c)
{
    (void)x; (void)y;
    if (count < 4) values[count++] = c;
}
'''
suffix = r'''
int main(int argc, char **argv)
{
    size_t page = (size_t)sysconf(_SC_PAGESIZE);
    /* Reserve the incorrect +4GB target as inaccessible for a safe control. */
    size_t size = ((size_t)1 << 32) + 2 * page;
    unsigned char *base = mmap(NULL, size, PROT_NONE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED || mprotect(base, page, PROT_READ|PROT_WRITE)) return 77;
    base[0] = 0x12; base[4] = 0x34;
    int bottom = argc > 1 && argv[1][0] == 'b';
    BitBlt(0, 0, 2, 2, base + (bottom ? 4 : 0), 4, bottom ? -4 : 4);
    printf("pixels=%u %u,%u,%u,%u\n", count, values[0], values[1], values[2], values[3]);
    return !(count == 4 && values[0] == (bottom ? 3 : 1) &&
             values[1] == (bottom ? 4 : 2) && values[2] == (bottom ? 1 : 3) &&
             values[3] == (bottom ? 2 : 4));
}
'''
if os.sysconf('SC_LONG_BIT') != 64:
    raise SystemExit('This regression test requires a 64-bit host')
assert '_In_ LONG Stride)' in function
results = []
with tempfile.TemporaryDirectory(prefix='bootvid-stride-') as directory:
    directory = Path(directory)
    for variant in ('production', 'unsigned-control'):
        body = function if variant == 'production' else function.replace('_In_ LONG Stride)', '_In_ ULONG Stride)')
        cfile = directory / (variant + '.c')
        executable = directory / variant
        cfile.write_text(prefix + body + suffix)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-D_GNU_SOURCE', '-O2', '-Wall', '-Wextra', str(cfile), '-o', str(executable)], check=True)
        for order in ('top', 'bottom'):
            result = subprocess.run([str(executable), order], capture_output=True, text=True)
            expected_failure = variant == 'unsigned-control' and order == 'bottom'
            passed = result.returncode < 0 if expected_failure else result.returncode == 0
            results.append(dict(variant=variant, order=order, returncode=result.returncode, stdout=result.stdout.strip(), passed=passed))
print(json.dumps(results, indent=2))
assert all(result['passed'] for result in results)
