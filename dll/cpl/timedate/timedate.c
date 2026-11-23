/*
 * PROJECT:     ReactOS Timedate Control Panel
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        dll/cpl/timedate/timedate.c
 * PURPOSE:     ReactOS Timedate Control Panel
 * COPYRIGHT:   Copyright 2004-2005 Eric Kohl
 *              Copyright 2006 Ged Murphy <gedmurphy@gmail.com>
 *
 */

#include "timedate.h"

#define NUM_APPLETS 1

static LONG APIENTRY Applet(HWND hwnd, UINT uMsg, LPARAM wParam, LPARAM lParam);

HINSTANCE hApplet;

/* Presentation only. Native date, zone, synchronization and notification paths stay intact. */
static BOOL bKushDateTime;

static BOOL
KushDateTimeEnabled(VOID)
{
    const WCHAR *names[] = {L"ClassicDateTime", L"ClassicWindows"};
    LANGID language = GetUserDefaultUILanguage();
    HIGHCONTRAST hc = {sizeof(hc)};
    DWORD layout = 0, value, size, type;
    HKEY key;
    HDC dc;
    INT dpi;
    UINT i;
    if (PRIMARYLANGID(language) == LANG_ARABIC || PRIMARYLANGID(language) == LANG_HEBREW ||
        (GetProcessDefaultLayout(&layout) && (layout & LAYOUT_RTL))) return FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
    {
        for (i = 0; i < ARRAYSIZE(names); ++i)
        {
            value = 0; size = sizeof(value); type = 0;
            if (RegQueryValueExW(key, names[i], NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS &&
                type == REG_DWORD && value)
            {
                RegCloseKey(key);
                return FALSE;
            }
        }
        RegCloseKey(key);
    }
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) &&
        (hc.dwFlags & HCF_HIGHCONTRASTON)) return FALSE;
    dc = GetDC(NULL);
    if (!dc) return FALSE;
    dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return dpi == 96;
}

BOOL KushDateTimeModern(VOID)
{
    return bKushDateTime;
}

static LRESULT CALLBACK
KushDateSurfaceProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
{
    UNREFERENCED_PARAMETER(data);
    if (msg == WM_ERASEBKGND)
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
        return 1;
    }
    if (msg == WM_CTLCOLORDLG || msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLORBTN)
    {
        SetBkMode((HDC)wp, TRANSPARENT);
        SetTextColor((HDC)wp, IsWindowEnabled((HWND)lp) ? RGB(40, 48, 56) : GetSysColor(COLOR_GRAYTEXT));
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, KushDateSurfaceProc, id);
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static INT_PTR
KushDatePage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, DLGPROC original)
{
    INT_PTR result = original(hwnd, msg, wp, lp);
    if (msg == WM_INITDIALOG && bKushDateTime)
        SetWindowSubclass(hwnd, KushDateSurfaceProc, 1, 0);
    return result;
}

static INT_PTR CALLBACK KushDatePageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return KushDatePage(hwnd, msg, wp, lp, DateTimePageProc);
}
static INT_PTR CALLBACK KushZonePageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return KushDatePage(hwnd, msg, wp, lp, TimeZonePageProc);
}
static INT_PTR CALLBACK KushInternetPageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return KushDatePage(hwnd, msg, wp, lp, InetTimePageProc);
}

/* Applets */
APPLET Applets[NUM_APPLETS] =
{
    {IDC_CPLICON, IDS_CPLNAME, IDS_CPLDESCRIPTION, Applet}
};

#if DBG
VOID DisplayWin32ErrorDbg(DWORD dwErrorCode, const char *file, int line)
#else
VOID DisplayWin32Error(DWORD dwErrorCode)
#endif
{
    PWSTR lpMsgBuf;
#if DBG
    WCHAR szMsg[255];
#endif

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL,
                   dwErrorCode,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR) &lpMsgBuf,
                   0,
                   NULL );

#if DBG
    if (_swprintf(szMsg, L"%hs:%d: %s", file, line, (PWSTR)lpMsgBuf))
    {
        MessageBoxW(NULL, szMsg, NULL, MB_OK | MB_ICONERROR);
    }
#else
    MessageBox(NULL, lpMsgBuf, NULL, MB_OK | MB_ICONERROR);
#endif

    LocalFree(lpMsgBuf);
}


static VOID
InitPropSheetPage(PROPSHEETPAGEW *psp, WORD idDlg, DLGPROC DlgProc)
{
    ZeroMemory(psp, sizeof(PROPSHEETPAGEW));
    psp->dwSize = sizeof(PROPSHEETPAGEW);
    psp->dwFlags = PSP_DEFAULT;
    psp->hInstance = hApplet;
    psp->pszTemplate = MAKEINTRESOURCEW(idDlg);
    psp->pfnDlgProc = DlgProc;
}

static int CALLBACK
PropSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam)
{
    // NOTE: This callback is needed to set large icon correctly.
    HICON hIcon;
    switch (uMsg)
    {
        case PSCB_INITIALIZED:
        {
            if (bKushDateTime) SetWindowSubclass(hwndDlg, KushDateSurfaceProc, 1, 0);
            hIcon = LoadIconW(hApplet, MAKEINTRESOURCEW(IDC_CPLICON));
            SendMessageW(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            break;
        }
    }
    return 0;
}

static LONG APIENTRY
Applet(HWND hwnd, UINT uMsg, LPARAM wParam, LPARAM lParam)
{
    PROPSHEETHEADER psh;
    PROPSHEETPAGEW psp[3];
    LONG Ret = 0;

    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    bKushDateTime = KushDateTimeEnabled();

    if (RegisterMonthCalControl(hApplet) &&
        RegisterClockControl())
    {
        ZeroMemory(&psh, sizeof(PROPSHEETHEADERW));
        psh.dwSize = sizeof(PROPSHEETHEADERW);
        psh.dwFlags =  PSH_PROPSHEETPAGE | PSH_PROPTITLE | PSH_USEICONID | PSH_USECALLBACK;
        psh.hwndParent = hwnd;
        psh.hInstance = hApplet;
        psh.pszIcon = MAKEINTRESOURCEW(KushOwnIconId(IDC_CPLICON));
        psh.pszCaption = MAKEINTRESOURCEW(IDS_CPLNAME);
        psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGEW);
        psh.nStartPage = 0;
        psh.ppsp = psp;
        psh.pfnCallback = PropSheetProc;

        InitPropSheetPage(&psp[0], IDD_DATETIMEPAGE, KushDatePageProc);
        InitPropSheetPage(&psp[1], IDD_TIMEZONEPAGE, KushZonePageProc);
        InitPropSheetPage(&psp[2], IDD_INETTIMEPAGE, KushInternetPageProc);

        Ret = (LONG)(PropertySheetW(&psh) != -1);

        UnregisterMonthCalControl(hApplet);
        UnregisterClockControl();
    }

    return Ret;
}


/* Control Panel Callback */
LONG CALLBACK
CPlApplet(HWND hwndCpl,
          UINT uMsg,
          LPARAM lParam1,
          LPARAM lParam2)
{
    UINT i = (UINT)lParam1;

    switch (uMsg)
    {
        case CPL_INIT:
            return TRUE;

        case CPL_GETCOUNT:
            return NUM_APPLETS;

        case CPL_INQUIRE:
            if (i < NUM_APPLETS)
            {
                CPLINFO *CPlInfo = (CPLINFO*)lParam2;
                CPlInfo->lData = 0;
                CPlInfo->idIcon = KushOwnIconId(Applets[i].idIcon);
                CPlInfo->idName = Applets[i].idName;
                CPlInfo->idInfo = Applets[i].idDescription;
            }
            else
            {
                return TRUE;
            }
            break;

        case CPL_DBLCLK:
            if (i < NUM_APPLETS)
                Applets[i].AppletProc(hwndCpl, uMsg, lParam1, lParam2);
            else
                return TRUE;
            break;
    }
    return FALSE;
}


BOOL WINAPI
DllMain(HINSTANCE hinstDLL,
        DWORD dwReason,
        LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);

    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            INITCOMMONCONTROLSEX InitControls;

            InitControls.dwSize = sizeof(INITCOMMONCONTROLSEX);
            InitControls.dwICC = ICC_DATE_CLASSES | ICC_PROGRESS_CLASS | ICC_UPDOWN_CLASS;
            InitCommonControlsEx(&InitControls);

            hApplet = hinstDLL;
        }
        break;
    }

    return TRUE;
}
