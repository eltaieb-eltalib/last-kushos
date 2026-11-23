/*
 * PROJECT:     ReactOS System Control Panel Applet
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        dll/cpl/sysdm/sysdm.c
 * PURPOSE:     dll entry file
 * COPYRIGHT:   Copyright Thomas Weidenmueller <w3seek@reactos.org>
 *
 */

#include "precomp.h"

#include <regstr.h>
#include <winnls.h>
#include <uxtheme.h>

static BOOL bKushSystemProperties;

static BOOL
KushSystemPropertiesEnabled(VOID)
{
    const WCHAR *names[] = {L"ClassicSystemProperties", L"ClassicWindows"};
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

static LRESULT CALLBACK
KushSystemPropertiesSurfaceProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
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
        SetBkColor((HDC)wp, RGB(255, 255, 255));
        SetTextColor((HDC)wp, IsWindowEnabled((HWND)lp) ? RGB(40, 48, 56) : GetSysColor(COLOR_GRAYTEXT));
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, KushSystemPropertiesSurfaceProc, id);
    return DefSubclassProc(hwnd, msg, wp, lp);
}


static BOOL CALLBACK
KushSystemPropertiesChildTheme(HWND hwnd, LPARAM data)
{
    WCHAR name[32];
    DWORD style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    UNREFERENCED_PARAMETER(data);
    if (GetClassNameW(hwnd, name, ARRAYSIZE(name)))
    {
        if ((!lstrcmpiW(name, L"Static") &&
             (style & SS_TYPEMASK) != SS_OWNERDRAW && (style & SS_TYPEMASK) != SS_BITMAP &&
             (style & SS_TYPEMASK) != SS_ICON) ||
            (!lstrcmpiW(name, L"Button") && (style & BS_TYPEMASK) == BS_GROUPBOX))
            SetWindowTheme(hwnd, L"", L"");
    }
    return TRUE;
}

static VOID
KushSystemPropertiesPageSurface(HWND hwnd)
{
    SetWindowSubclass(hwnd, KushSystemPropertiesSurfaceProc, 1, 0);
    EnumChildWindows(hwnd, KushSystemPropertiesChildTheme, 0);
}



static LONG APIENTRY SystemApplet(HWND hwnd, UINT uMsg, LPARAM wParam, LPARAM lParam);
HINSTANCE hApplet = 0;

/* Applets */
APPLET Applets[NUM_APPLETS] =
{
    {IDI_CPLSYSTEM, IDS_CPLSYSTEMNAME, IDS_CPLSYSTEMDESCRIPTION, SystemApplet}
};

#define MAX_SYSTEM_PAGES    32


INT __cdecl
ResourceMessageBox(
    _In_opt_ HINSTANCE hInstance,
    _In_opt_ HWND hwnd,
    _In_ UINT uType,
    _In_ UINT uCaption,
    _In_ UINT uText,
    ...)
{
    va_list args;
    WCHAR szCaption[MAX_STR_LENGTH];
    WCHAR szText[MAX_STR_LENGTH];
    WCHAR szCookedText[2*MAX_STR_LENGTH];

    LoadStringW(hInstance, uCaption, szCaption, _countof(szCaption));
    LoadStringW(hInstance, uText, szText, _countof(szText));

    va_start(args, uText);
    StringCchVPrintfW(szCookedText, _countof(szCookedText),
                      szText, args);
    va_end(args);

    return MessageBoxW(hwnd,
                       szCookedText,
                       szCaption,
                       uType);
}


static BOOL CALLBACK
PropSheetAddPage(HPROPSHEETPAGE hpage, LPARAM lParam)
{
    PROPSHEETHEADER *ppsh = (PROPSHEETHEADER *)lParam;
    if (ppsh != NULL && ppsh->nPages < MAX_SYSTEM_PAGES)
    {
        ppsh->phpage[ppsh->nPages++] = hpage;
        return TRUE;
    }

    return FALSE;
}

static INT_PTR CALLBACK
KushGeneralPage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    INT_PTR result = GeneralPageProc(hwnd, msg, wp, lp);
    if (msg == WM_INITDIALOG && bKushSystemProperties)
        KushSystemPropertiesPageSurface(hwnd);
    return result;
}

static INT_PTR CALLBACK
KushHardwarePage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    INT_PTR result = HardwarePageProc(hwnd, msg, wp, lp);
    if (msg == WM_INITDIALOG && bKushSystemProperties)
        KushSystemPropertiesPageSurface(hwnd);
    return result;
}

static INT_PTR CALLBACK
KushAdvancedPage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    INT_PTR result = AdvancedPageProc(hwnd, msg, wp, lp);
    if (msg == WM_INITDIALOG && bKushSystemProperties)
        KushSystemPropertiesPageSurface(hwnd);
    return result;
}

static BOOL
InitPropSheetPage(PROPSHEETHEADER *ppsh, WORD idDlg, DLGPROC DlgProc)
{
    HPROPSHEETPAGE hPage;
    PROPSHEETPAGE psp;

    if (ppsh->nPages < MAX_SYSTEM_PAGES)
    {
        ZeroMemory(&psp, sizeof(psp));
        psp.dwSize = sizeof(psp);
        psp.dwFlags = PSP_DEFAULT;
        psp.hInstance = hApplet;
        psp.pszTemplate = MAKEINTRESOURCE(idDlg);
        psp.pfnDlgProc = DlgProc;

        hPage = CreatePropertySheetPage(&psp);
        if (hPage != NULL)
        {
            return PropSheetAddPage(hPage, (LPARAM)ppsh);
        }
    }

    return FALSE;
}

typedef HPROPSHEETPAGE (WINAPI *PCreateNetIDPropertyPage)(VOID);

static HMODULE
AddNetIdPage(PROPSHEETHEADER *ppsh)
{
    HPROPSHEETPAGE hPage;
    HMODULE hMod;
    PCreateNetIDPropertyPage pCreateNetIdPage;

    hMod = LoadLibrary(TEXT("netid.dll"));
    if (hMod != NULL)
    {
        pCreateNetIdPage = (PCreateNetIDPropertyPage)GetProcAddress(hMod,
                                                                    "CreateNetIDPropertyPage");
        if (pCreateNetIdPage != NULL)
        {
            hPage = pCreateNetIdPage();
            if (hPage == NULL)
                goto Fail;

            if (!PropSheetAddPage(hPage, (LPARAM)ppsh))
            {
                DestroyPropertySheetPage(hPage);
                goto Fail;
            }
        }
        else
        {
Fail:
            FreeLibrary(hMod);
            hMod = NULL;
        }
    }

    return hMod;
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
            if (bKushSystemProperties) SetWindowSubclass(hwndDlg, KushSystemPropertiesSurfaceProc, 1, 0);
            hIcon = LoadIconW(hApplet, MAKEINTRESOURCEW(IDI_CPLSYSTEM));
            SendMessageW(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            break;
        }
    }
    return 0;
}

/* First Applet */
LONG CALLBACK
SystemApplet(HWND hwnd, UINT uMsg, LPARAM wParam, LPARAM lParam)
{
    HPROPSHEETPAGE hpsp[MAX_SYSTEM_PAGES];
    PROPSHEETHEADER psh;
    HMODULE hNetIdDll;
    HPSXA hpsxa = NULL;
    INT nPage = 0;
    LONG Ret;
    static INITCOMMONCONTROLSEX icc = {sizeof(INITCOMMONCONTROLSEX), ICC_LINK_CLASS};

    bKushSystemProperties = KushSystemPropertiesEnabled();

    if (!InitCommonControlsEx(&icc))
        return 0;

    if (uMsg == CPL_STARTWPARMSW && lParam != 0)
    {
        nPage = _wtoi((PWSTR)lParam);
    }

    if (nPage == -1)
    {
        ShowPerformanceOptions(hwnd);
        return TRUE;
    }

    ZeroMemory(&psh, sizeof(PROPSHEETHEADER));
    psh.dwSize = sizeof(PROPSHEETHEADER);
    psh.dwFlags =  PSH_PROPTITLE | PSH_USEICONID | PSH_USECALLBACK;
    psh.hwndParent = hwnd;
    psh.hInstance = hApplet;
    psh.pszIcon = MAKEINTRESOURCEW(KushOwnIconId(IDI_CPLSYSTEM));
    psh.pszCaption = MAKEINTRESOURCE(IDS_CPLSYSTEMNAME);
    psh.nPages = 0;
    psh.nStartPage = 0;
    psh.phpage = hpsp;
    psh.pfnCallback = PropSheetProc;

    InitPropSheetPage(&psh, IDD_PROPPAGEGENERAL, KushGeneralPage);
    hNetIdDll = AddNetIdPage(&psh);
    InitPropSheetPage(&psh, IDD_PROPPAGEHARDWARE, KushHardwarePage);
    InitPropSheetPage(&psh, IDD_PROPPAGEADVANCED, KushAdvancedPage);

    /* Load additional pages provided by shell extensions */
    hpsxa = SHCreatePropSheetExtArray(HKEY_LOCAL_MACHINE, REGSTR_PATH_CONTROLSFOLDER TEXT("\\System"), MAX_SYSTEM_PAGES - psh.nPages);
    if (hpsxa != NULL)
    {
        SHAddFromPropSheetExtArray(hpsxa, PropSheetAddPage, (LPARAM)&psh);
    }

    if (nPage != 0 && nPage <= psh.nPages)
        psh.nStartPage = nPage;

    Ret = (LONG)(PropertySheet(&psh) != -1);

    if (hpsxa != NULL)
    {
        SHDestroyPropSheetExtArray(hpsxa);
    }

    if (hNetIdDll != NULL)
        FreeLibrary(hNetIdDll);

    return Ret;
}

/* Control Panel Callback */
LONG CALLBACK
CPlApplet(HWND hwndCPl,
          UINT uMsg,
          LPARAM lParam1,
          LPARAM lParam2)
{
    UINT i = (UINT)lParam1;

    UNREFERENCED_PARAMETER(hwndCPl);

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
                Applets[i].AppletProc(hwndCPl, uMsg, lParam1, lParam2);
            else
                return TRUE;
            break;

        case CPL_STARTWPARMSW:
            if (i < NUM_APPLETS)
                return Applets[i].AppletProc(hwndCPl, uMsg, lParam1, lParam2);
            break;
    }

    return FALSE;
}


BOOL WINAPI
DllMain(HINSTANCE hinstDLL,
        DWORD dwReason,
        LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(lpvReserved);

    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
            hApplet = hinstDLL;
            break;
    }

    return TRUE;
}
