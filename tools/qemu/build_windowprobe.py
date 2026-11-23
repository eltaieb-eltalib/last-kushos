#!/usr/bin/env python3
"""Compile the real Kush appearance helper into a small QEMU settings probe.

Usage: python3 tools/qemu/build_windowprobe.py /tmp/window.exe
Inside the guest: window.exe classic, then window.exe modern.
This intentionally runs the production helper, not a separate implementation.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'base/shell/explorer/kushtypography.cpp').read_text()
source = source.replace('#include "precomp.h"', '''#include <windows.h>
#include <strsafe.h>
static void DbgPrint(const char*,...) {}''')
source += r'''
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR args, int)
{
    HKEY key;
    DWORD classic = lstrcmpiA(args, "classic") == 0, applied = 0;
    BOOL fontsOnly = lstrcmpiA(args, "fonts-classic") == 0 || lstrcmpiA(args, "fonts-modern") == 0;
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell", 0, NULL, 0,
        KEY_SET_VALUE, NULL, &key, NULL);
    if (fontsOnly)
    {
        DWORD fontClassic = lstrcmpiA(args, "fonts-classic") == 0;
        RegSetValueExW(key, L"ClassicFonts", 0, REG_DWORD, (BYTE*)&fontClassic, sizeof(fontClassic));
    }
    else
    {
        RegSetValueExW(key, L"ClassicWindows", 0, REG_DWORD, (BYTE*)&classic, sizeof(classic));
        RegSetValueExW(key, L"WindowThemeApplied", 0, REG_DWORD, (BYTE*)&applied, sizeof(applied));
    }
    RegCloseKey(key);
    if (!fontsOnly) KushApplyWindowTheme();
    KushApplyTypography();
    NONCLIENTMETRICSW metrics = {sizeof(metrics)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    COLORREF color = GetSysColor(COLOR_BTNFACE);
    WCHAR text[256];
    StringCchPrintfW(text, 256, L"Mode: %hs\nCaption: %d x %d\nFont: %s (%d)\nDialog RGB: %u %u %u",
        args, metrics.iCaptionWidth, metrics.iCaptionHeight,
        metrics.lfCaptionFont.lfFaceName, metrics.lfCaptionFont.lfHeight,
        GetRValue(color), GetGValue(color), GetBValue(color));
    MessageBoxW(NULL, text, L"Kush window settings probe", MB_OK);
    return 0;
}
'''
out = Path(sys.argv[1] if len(sys.argv) > 1 else '/tmp/window.exe').resolve()
compiler = os.environ.get('CXX', 'x86_64-w64-mingw32-g++')
with tempfile.TemporaryDirectory() as work:
    path = Path(work) / 'windowprobe.cpp'
    path.write_text(source)
    subprocess.run([compiler, '-static', '-s', '-Os', '-mwindows', str(path),
                    '-o', str(out), '-luser32', '-lgdi32', '-ladvapi32'], check=True)
print(out)
