/*
 * Kush 1.1: screen geometry as a 32-bit program sees it, and where a
 * DS_CENTER dialog lands (the Firefox installer dialog appeared at y=1045769672).
 *   i686-w64-mingw32-gcc -O2 -static -o geom32.exe geom32.c -luser32 -lgdi32
 */
#include <windows.h>
#include <stdio.h>

static INT_PTR CALLBACK dlgproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_INITDIALOG)
    {
        RECT r; GetWindowRect(h, &r);
        printf("DS_CENTER dialog rect: (%ld,%ld)-(%ld,%ld)\n", r.left, r.top, r.right, r.bottom);
        EndDialog(h, 0);
    }
    return FALSE;
}

int main(void)
{
    RECT wa = {0}; MONITORINFO mi = { sizeof(mi) }; POINT pt = {0, 0};
    HMONITOR hm;
    WORD tmpl[64] = {0}; DLGTEMPLATE *dt = (DLGTEMPLATE *)tmpl;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("SM_CXSCREEN %d SM_CYSCREEN %d SM_CXFULLSCREEN %d SM_CYFULLSCREEN %d\n",
           GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
           GetSystemMetrics(SM_CXFULLSCREEN), GetSystemMetrics(SM_CYFULLSCREEN));
    printf("SPI_GETWORKAREA ret %d: (%ld,%ld)-(%ld,%ld)\n", SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0),
           wa.left, wa.top, wa.right, wa.bottom);
    hm = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    printf("MonitorFromPoint %p\n", (void *)hm);
    printf("GetMonitorInfo ret %d: mon (%ld,%ld)-(%ld,%ld) work (%ld,%ld)-(%ld,%ld) flags %lx\n",
           GetMonitorInfoA(hm, &mi), mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right, mi.rcMonitor.bottom,
           mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom, mi.dwFlags);
    hm = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
    printf("MonitorFromWindow(desktop) %p\n", (void *)hm);

    dt->style = DS_CENTER | WS_POPUP | WS_CAPTION | DS_SETFONT * 0;
    dt->cx = 300; dt->cy = 200;
    DialogBoxIndirectParamA(GetModuleHandleA(NULL), dt, NULL, dlgproc, 0);
    return 0;
}
