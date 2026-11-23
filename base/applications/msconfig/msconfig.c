/*
 * PROJECT:     ReactOS Applications
 * LICENSE:     LGPL - See COPYING in the top level directory
 * FILE:        base/applications/msconfig/msconfig.c
 * PURPOSE:     msconfig main dialog
 * COPYRIGHT:   Copyright 2005-2006 Christoph von Wittich <Christoph@ApiViewer.de>
 *
 */

#include "precomp.h"
#include <wingdi.h>
#include <winnls.h>

#include "toolspage.h"
#include "srvpage.h"
#include "startuppage.h"
#include "freeldrpage.h"
#include "systempage.h"
#include "generalpage.h"

HINSTANCE hInst = 0;

HWND hMainWnd;                   /* Main Window */
HWND hTabWnd;                    /* Tab Control Window */
UINT uXIcon = 0, uYIcon = 0;     /* Icon sizes */
HICON hDialogIconBig = NULL;
HICON hDialogIconSmall = NULL;

void MsConfig_OnTabWndSelChange(void);

/* Presentation only: retain the original dialog controls and command paths. */
static BOOL bKushSystemConfiguration;

static BOOL
KushSystemConfigurationEnabled(VOID)
{
    const WCHAR *names[] = {L"ClassicSystemConfiguration", L"ClassicWindows"};
    DWORD layout = 0, value, size, type;
    LANGID language = GetUserDefaultUILanguage();
    HIGHCONTRAST hc = {sizeof(hc)};
    RECT work;
    HKEY key;
    HDC dc;
    INT dpi;
    UINT i;

    if (PRIMARYLANGID(language) == LANG_ARABIC ||
        PRIMARYLANGID(language) == LANG_HEBREW ||
        (GetProcessDefaultLayout(&layout) && (layout & LAYOUT_RTL))) return FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell", 0,
                      KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
    {
        for (i = 0; i < ARRAYSIZE(names); ++i)
        {
            value = 0; size = sizeof(value); type = 0;
            if (RegQueryValueExW(key, names[i], NULL, &type, (LPBYTE)&value,
                                &size) == ERROR_SUCCESS && type == REG_DWORD && value)
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
    if (dpi != 96) return FALSE;
    return SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0) &&
           work.right - work.left >= 720 && work.bottom - work.top >= 500;
}

static LRESULT CALLBACK
KushConfigSurfaceProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                      UINT_PTR id, DWORD_PTR data)
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
        SetTextColor((HDC)wp, IsWindowEnabled((HWND)lp) ? RGB(40, 48, 56) :
                     GetSysColor(COLOR_GRAYTEXT));
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, KushConfigSurfaceProc, id);
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* Scale positions and hit areas, not fonts, IDs, tab order or enabled states. */
static VOID
KushConfigScale(HWND parent, INT width, INT oldWidth, INT height, INT oldHeight)
{
    HWND child;
    RECT rc;
    for (child = GetWindow(parent, GW_CHILD); child;
         child = GetWindow(child, GW_HWNDNEXT))
    {
        GetWindowRect(child, &rc);
        MapWindowPoints(NULL, parent, (POINT *)&rc, 2);
        KushConfigScale(child, width, oldWidth, height, oldHeight);
        MoveWindow(child, MulDiv(rc.left, width, oldWidth),
                   MulDiv(rc.top, height, oldHeight),
                   MulDiv(rc.right - rc.left, width, oldWidth),
                   MulDiv(rc.bottom - rc.top, height, oldHeight), FALSE);
    }
}

static VOID
KushConfigList(HWND list, const INT *columns, UINT count)
{
    LONG_PTR style;
    HIMAGELIST rows;
    RECT rc;
    INT used = 0;
    UINT i;
    if (!list) return;
    style = GetWindowLongPtr(list, GWL_STYLE) & ~WS_BORDER;
    SetWindowLongPtr(list, GWL_STYLE, style);
    style = GetWindowLongPtr(list, GWL_EXSTYLE) & ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLongPtr(list, GWL_EXSTYLE, style);
    SetWindowPos(list, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    ListView_SetBkColor(list, RGB(255, 255, 255));
    ListView_SetTextBkColor(list, RGB(255, 255, 255));
    ListView_SetTextColor(list, RGB(40, 48, 56));
    if (!ListView_GetImageList(list, LVSIL_SMALL))
    {
        rows = ImageList_Create(1, 24, ILC_COLOR32, 1, 0);
        if (rows) ListView_SetImageList(list, rows, LVSIL_SMALL);
    }
    GetClientRect(list, &rc);
    for (i = 0; i < count; ++i)
    {
        INT cx = (i + 1 == count) ? max(60, rc.right - used) : columns[i];
        ListView_SetColumnWidth(list, i, cx);
        used += cx;
    }
}

static VOID
KushConfigLayout(HWND hwnd)
{
    HWND pages[] = {hGeneralPage, hSystemPage, hFreeLdrPage,
                    hServicesPage, hStartupPage, hToolsPage};
    const INT serviceColumns[] = {250, 84, 180, 0};
    const INT startupColumns[] = {180, 220, 0};
    const INT toolColumns[] = {180, 0};
    RECT old, frame = {0, 0, 664, 416}, work;
    HWND tree;
    UINT i;

    GetClientRect(hwnd, &old);
    if (!old.right || !old.bottom) return;
    AdjustWindowRectEx(&frame, GetWindowLongPtr(hwnd, GWL_STYLE), FALSE,
                      GetWindowLongPtr(hwnd, GWL_EXSTYLE));
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    SetWindowPos(hwnd, NULL,
                 work.left + ((work.right-work.left) - (frame.right-frame.left))/2,
                 work.top + ((work.bottom-work.top) - (frame.bottom-frame.top))/2,
                 frame.right-frame.left, frame.bottom-frame.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    KushConfigScale(hwnd, 664, old.right, 416, old.bottom);
    SetWindowSubclass(hwnd, KushConfigSurfaceProc, 1, 0);
    for (i = 0; i < ARRAYSIZE(pages); ++i)
        SetWindowSubclass(pages[i], KushConfigSurfaceProc, 1, 0);
    TabCtrl_SetItemSize(hTabWnd, 0, 30);
    KushConfigList(hServicesListCtrl, serviceColumns, ARRAYSIZE(serviceColumns));
    KushConfigList(hStartupListCtrl, startupColumns, ARRAYSIZE(startupColumns));
    KushConfigList(hToolsListCtrl, toolColumns, ARRAYSIZE(toolColumns));
    tree = GetDlgItem(hSystemPage, IDC_SYSTEM_TREE);
    TreeView_SetItemHeight(tree, 26);
    TreeView_SetBkColor(tree, RGB(255, 255, 255));
    TreeView_SetTextColor(tree, RGB(40, 48, 56));
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}



////////////////////////////////////////////////////////////////////////////////
// Taken from WinSpy++ 1.7
// http://www.catch22.net/software/winspy
// Copyright (c) 2002 by J Brown
//

//
//	Copied from uxtheme.h
//  If you have this new header, then delete these and
//  #include <uxtheme.h> instead!
//
#define ETDT_DISABLE        0x00000001
#define ETDT_ENABLE         0x00000002
#define ETDT_USETABTEXTURE  0x00000004
#define ETDT_ENABLETAB      (ETDT_ENABLE  | ETDT_USETABTEXTURE)

//
typedef HRESULT (WINAPI * ETDTProc) (HWND, DWORD);

//
//	Try to call EnableThemeDialogTexture, if uxtheme.dll is present
//
BOOL EnableDialogTheme(HWND hwnd)
{
    HMODULE hUXTheme;
    ETDTProc fnEnableThemeDialogTexture;

    if (bKushSystemConfiguration) return TRUE;

    hUXTheme = LoadLibrary(_T("uxtheme.dll"));

    if(hUXTheme)
    {
        fnEnableThemeDialogTexture =
            (ETDTProc)GetProcAddress(hUXTheme, "EnableThemeDialogTexture");

        if(fnEnableThemeDialogTexture)
        {
            fnEnableThemeDialogTexture(hwnd, ETDT_ENABLETAB);

            FreeLibrary(hUXTheme);
            return TRUE;
        }
        else
        {
            // Failed to locate API!
            FreeLibrary(hUXTheme);
            return FALSE;
        }
    }
    else
    {
        // Not running under XP? Just fail gracefully
        return FALSE;
    }
}

VOID DisableAllExcept(HWND hTabDlg, UINT idExcept)
{
    HWND hSkip = GetDlgItem(hTabDlg, idExcept);
    for (HWND hWnd = NULL; (hWnd = FindWindowExW(hTabDlg, hWnd, NULL, NULL)) != NULL;)
        EnableWindow(hWnd, hWnd == hSkip);
    if (idExcept == IDC_SYSTEM_TREE && hSkip)
    {
        LONG_PTR style = GetWindowLongPtrW(hSkip, GWL_STYLE);
        style &= ~TVS_EDITLABELS;
        SetWindowLongPtrW(hSkip, GWL_STYLE, style);
    }
}

BOOL OnCreate(HWND hWnd)
{
    TCHAR   szTemp[256];
    TCITEM  item;

    hTabWnd = GetDlgItem(hWnd, IDC_TAB);
    hGeneralPage = CreateDialog(hInst, MAKEINTRESOURCE(IDD_GENERAL_PAGE), hWnd,  GeneralPageWndProc); EnableDialogTheme(hGeneralPage);
    hSystemPage = CreateDialog(hInst, MAKEINTRESOURCE(IDD_SYSTEM_PAGE), hWnd,  SystemPageWndProc); EnableDialogTheme(hSystemPage);
    hFreeLdrPage = CreateDialog(hInst, MAKEINTRESOURCE(IDD_FREELDR_PAGE), hWnd,  FreeLdrPageWndProc); EnableDialogTheme(hFreeLdrPage);
    hServicesPage = CreateDialog(hInst, MAKEINTRESOURCE(IDD_SERVICES_PAGE), hWnd,  ServicesPageWndProc); EnableDialogTheme(hServicesPage);
    hStartupPage = CreateDialog(hInst, MAKEINTRESOURCE(IDD_STARTUP_PAGE), hWnd,  StartupPageWndProc); EnableDialogTheme(hStartupPage);
    hToolsPage = CreateDialog(hInst, MAKEINTRESOURCE(IDD_TOOLS_PAGE), hWnd,  ToolsPageWndProc); EnableDialogTheme(hToolsPage);

    LoadString(hInst, IDS_MSCONFIG, szTemp, 256);
    SetWindowText(hWnd, szTemp);

    // Insert Tab Pages
    LoadString(hInst, IDS_TAB_GENERAL, szTemp, 256);
    memset(&item, 0, sizeof(TCITEM));
    item.mask = TCIF_TEXT;
    item.pszText = szTemp;
    (void)TabCtrl_InsertItem(hTabWnd, 0, &item);

    LoadString(hInst, IDS_TAB_SYSTEM, szTemp, 256);
    memset(&item, 0, sizeof(TCITEM));
    item.mask = TCIF_TEXT;
    item.pszText = szTemp;
    (void)TabCtrl_InsertItem(hTabWnd, 1, &item);

    LoadString(hInst, IDS_TAB_FREELDR, szTemp, 256);
    memset(&item, 0, sizeof(TCITEM));
    item.mask = TCIF_TEXT;
    item.pszText = szTemp;
    (void)TabCtrl_InsertItem(hTabWnd, 2, &item);

    LoadString(hInst, IDS_TAB_SERVICES, szTemp, 256);
    memset(&item, 0, sizeof(TCITEM));
    item.mask = TCIF_TEXT;
    item.pszText = szTemp;
    (void)TabCtrl_InsertItem(hTabWnd, 3, &item);

    LoadString(hInst, IDS_TAB_STARTUP, szTemp, 256);
    memset(&item, 0, sizeof(TCITEM));
    item.mask = TCIF_TEXT;
    item.pszText = szTemp;
    (void)TabCtrl_InsertItem(hTabWnd, 4, &item);

    LoadString(hInst, IDS_TAB_TOOLS, szTemp, 256);
    memset(&item, 0, sizeof(TCITEM));
    item.mask = TCIF_TEXT;
    item.pszText = szTemp;
    (void)TabCtrl_InsertItem(hTabWnd, 5, &item);

    if (bKushSystemConfiguration) KushConfigLayout(hWnd);
    MsConfig_OnTabWndSelChange();

    // FIXME: We don't support applying anything
    EnableWindow(GetDlgItem(hWnd, IDOK), FALSE);
    EnableWindow(GetDlgItem(hWnd, IDC_BTN_APPLY), FALSE);
    EnableWindow(GetDlgItem(hWnd, IDC_BTN_HELP), FALSE);
    return TRUE;
}


void MsConfig_OnTabWndSelChange(void)
{
    switch (TabCtrl_GetCurSel(hTabWnd)) {
    case 0: //General
        ShowWindow(hGeneralPage, SW_SHOW);
        ShowWindow(hSystemPage, SW_HIDE);
        ShowWindow(hFreeLdrPage, SW_HIDE);
        ShowWindow(hServicesPage, SW_HIDE);
        ShowWindow(hStartupPage, SW_HIDE);
        ShowWindow(hToolsPage, SW_HIDE);
        BringWindowToTop(hGeneralPage);
        break;
    case 1: //SYSTEM.INI
        ShowWindow(hGeneralPage, SW_HIDE);
        ShowWindow(hSystemPage, SW_SHOW);
        ShowWindow(hToolsPage, SW_HIDE);
        ShowWindow(hStartupPage, SW_HIDE);
        ShowWindow(hFreeLdrPage, SW_HIDE);
        ShowWindow(hServicesPage, SW_HIDE);
        BringWindowToTop(hSystemPage);
        break;
    case 2: //Freeldr
        ShowWindow(hGeneralPage, SW_HIDE);
        ShowWindow(hSystemPage, SW_HIDE);
        ShowWindow(hFreeLdrPage, SW_SHOW);
        ShowWindow(hServicesPage, SW_HIDE);
        ShowWindow(hStartupPage, SW_HIDE);
        ShowWindow(hToolsPage, SW_HIDE);
        BringWindowToTop(hFreeLdrPage);
        break;
    case 3: //Services
        ShowWindow(hGeneralPage, SW_HIDE);
        ShowWindow(hSystemPage, SW_HIDE);
        ShowWindow(hFreeLdrPage, SW_HIDE);
        ShowWindow(hServicesPage, SW_SHOW);
        ShowWindow(hStartupPage, SW_HIDE);
        ShowWindow(hToolsPage, SW_HIDE);
        BringWindowToTop(hServicesPage);
        break;
    case 4: //startup
        ShowWindow(hGeneralPage, SW_HIDE);
        ShowWindow(hSystemPage, SW_HIDE);
        ShowWindow(hFreeLdrPage, SW_HIDE);
        ShowWindow(hServicesPage, SW_HIDE);
        ShowWindow(hStartupPage, SW_SHOW);
        ShowWindow(hToolsPage, SW_HIDE);
        BringWindowToTop(hStartupPage);
        break;
    case 5: //Tools
        ShowWindow(hGeneralPage, SW_HIDE);
        ShowWindow(hSystemPage, SW_HIDE);
        ShowWindow(hFreeLdrPage, SW_HIDE);
        ShowWindow(hServicesPage, SW_HIDE);
        ShowWindow(hStartupPage, SW_HIDE);
        ShowWindow(hToolsPage, SW_SHOW);
        BringWindowToTop(hToolsPage);
        break;
    }
}


static
VOID
SetDialogIcon(HWND hDlg)
{
    if (hDialogIconBig) DestroyIcon(hDialogIconBig);
    if (hDialogIconSmall) DestroyIcon(hDialogIconSmall);

    hDialogIconBig = LoadIconW(GetModuleHandle(NULL),
                               MAKEINTRESOURCE(IDI_APPICON));
    hDialogIconSmall = LoadImage(GetModuleHandle(NULL),
                                 MAKEINTRESOURCE(IDI_APPICON),
                                 IMAGE_ICON,
                                 uXIcon,
                                 uYIcon,
                                 0);
    SendMessage(hDlg,
                WM_SETICON,
                ICON_BIG,
                (LPARAM)hDialogIconBig);
    SendMessage(hDlg,
                WM_SETICON,
                ICON_SMALL,
                (LPARAM)hDialogIconSmall);
}


/* Message handler for dialog box. */
INT_PTR CALLBACK
MsConfigWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    LPNMHDR         pnmh;
    UINT            uXIconNew, uYIconNew;

    switch (message)
    {
        case WM_INITDIALOG:
            hMainWnd = hDlg;

            uXIcon = GetSystemMetrics(SM_CXSMICON);
            uYIcon = GetSystemMetrics(SM_CYSMICON);

            SetDialogIcon(hDlg);

            return OnCreate(hDlg);

        case WM_SETTINGCHANGE:
            uXIconNew = GetSystemMetrics(SM_CXSMICON);
            uYIconNew = GetSystemMetrics(SM_CYSMICON);

            if ((uXIcon != uXIconNew) || (uYIcon != uYIconNew))
            {
                uXIcon = uXIconNew;
                uYIcon = uYIconNew;
                SetDialogIcon(hDlg);
            }
            break;

        case WM_COMMAND:

            if (LOWORD(wParam) == IDOK)
            {
                //MsConfig_OnSaveChanges();
            }

            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
                return TRUE;
            }
            break;

        case WM_NOTIFY:
            pnmh = (LPNMHDR)lParam;
            if ((pnmh->hwndFrom == hTabWnd) &&
                (pnmh->idFrom == IDC_TAB) &&
                (pnmh->code == TCN_SELCHANGE))
            {
                MsConfig_OnTabWndSelChange();
            }
            break;

        case WM_SYSCOLORCHANGE:
            /* Forward WM_SYSCOLORCHANGE to common controls */
            SendMessage(hServicesListCtrl, WM_SYSCOLORCHANGE, 0, 0);
            SendMessage(hStartupListCtrl, WM_SYSCOLORCHANGE, 0, 0);
            SendMessage(hToolsListCtrl, WM_SYSCOLORCHANGE, 0, 0);
            break;

        case WM_DESTROY:
            if (hToolsPage)
                DestroyWindow(hToolsPage);
            if (hGeneralPage)
                DestroyWindow(hGeneralPage);
            if (hServicesPage)
                DestroyWindow(hServicesPage);
            if (hStartupPage)
                DestroyWindow(hStartupPage);
            if (hFreeLdrPage)
                DestroyWindow(hFreeLdrPage);
            if (hSystemPage)
                DestroyWindow(hSystemPage);
            if (hDialogIconBig)
                DestroyIcon(hDialogIconBig);
            if (hDialogIconSmall)
                DestroyIcon(hDialogIconSmall);
            return DefWindowProc(hDlg, message, wParam, lParam);
    }

    return 0;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPTSTR    lpCmdLine,
                       int       nCmdShow)
{

    INITCOMMONCONTROLSEX InitControls;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    InitControls.dwSize = sizeof(INITCOMMONCONTROLSEX);
    InitControls.dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&InitControls);

    hInst = hInstance;
    bKushSystemConfiguration = KushSystemConfigurationEnabled();

    DialogBox(hInst, (LPCTSTR)IDD_MSCONFIG_DIALOG, NULL,  MsConfigWndProc);

    return 0;
}

/* EOF */
