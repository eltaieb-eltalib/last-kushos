/*
 * Kush 1.1: list top-level windows (pid, class, title, visible, rect).
 *   i686-w64-mingw32-gcc -O2 -static -o winlist32.exe winlist.c
 *   winlist32.exe [pid]
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static DWORD g_pid;

static BOOL CALLBACK cb(HWND h, LPARAM l)
{
    DWORD pid = 0; char cls[64] = "", title[96] = ""; RECT r = {0};
    GetWindowThreadProcessId(h, &pid);
    if (g_pid && pid != g_pid) return TRUE;
    GetClassNameA(h, cls, sizeof(cls));
    GetWindowTextA(h, title, sizeof(title));
    GetWindowRect(h, &r);
    printf("%p pid %lu vis %d min %d style %08lx ex %08lx (%ld,%ld)-(%ld,%ld) [%s] \"%s\"\n",
           (void *)h, pid, IsWindowVisible(h), IsIconic(h), GetWindowLongA(h, GWL_STYLE),
           GetWindowLongA(h, GWL_EXSTYLE), r.left, r.top, r.right, r.bottom, cls, title);
    return TRUE;
}

int main(int argc, char **argv)
{
    if (argc > 1) g_pid = strtoul(argv[1], NULL, 0);
    EnumWindows(cb, 0);
    return 0;
}
