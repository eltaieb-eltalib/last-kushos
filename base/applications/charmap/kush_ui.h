/* Layout and colors only. Native character selection, fonts and clipboard stay intact. */
#ifndef KUSH_CHARMAP_UI_H
#define KUSH_CHARMAP_UI_H

#include <winreg.h>

static BOOL KushCharmapEnabled(HWND window)
{
    const WCHAR *names[] = {L"ClassicCharacterMap", L"ClassicWindows"};
    HIGHCONTRASTW contrast = {sizeof(contrast)};
    HKEY key;
    HDC dc;
    INT dpi;
    UINT i;
    if (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) return FALSE;
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
        (contrast.dwFlags & HCF_HIGHCONTRASTON)) return FALSE;
    dc = GetDC(window);
    if (!dc) return FALSE;
    dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(window, dc);
    if (dpi != 96) return FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
    {
        for (i = 0; i < SIZEOF(names); ++i)
        {
            DWORD type = 0, value = 0, size = sizeof(value);
            if (RegQueryValueExW(key, names[i], NULL, &type, (BYTE*)&value, &size) == ERROR_SUCCESS &&
                type == REG_DWORD && size == sizeof(value) && value)
            {
                RegCloseKey(key);
                return FALSE;
            }
        }
        RegCloseKey(key);
    }
    return TRUE;
}

static BOOL KushCharmapWorkArea(HWND window, RECT *work)
{
    MONITORINFO info;
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info)) return FALSE;
    *work = info.rcWork;
    return TRUE;
}

static UINT KushCharmapLabels(HWND dialog, HWND *labels, UINT maximum)
{
    HWND child;
    UINT count = 0;
    for (child = GetWindow(dialog, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        WCHAR name[32];
        GetClassNameW(child, name, SIZEOF(name));
        if (GetDlgCtrlID(child) == -1 && lstrcmpiW(name, L"Static") == 0)
        {
            if (count == maximum) return 0;
            labels[count++] = child;
        }
    }
    return count;
}

static BOOL KushCharmapTextFits(HWND control, INT width)
{
    WCHAR text[256];
    HDC dc;
    HFONT font, old = NULL;
    SIZE size;
    BOOL result;
    INT length = GetWindowTextW(control, text, SIZEOF(text));
    if (!length || length >= SIZEOF(text) - 1) return FALSE;
    dc = GetDC(control);
    if (!dc) return FALSE;
    font = (HFONT)SendMessageW(control, WM_GETFONT, 0, 0);
    if (font) old = SelectObject(dc, font);
    result = GetTextExtentPoint32W(dc, text, length, &size) && size.cx <= width;
    if (old) SelectObject(dc, old);
    ReleaseDC(control, dc);
    return result;
}

static VOID KushCharmapPlace(HWND control, INT x, INT y, INT width, INT height)
{
    SetWindowPos(control, NULL, x, y, width, height,
                 SWP_NOACTIVATE | SWP_NOZORDER);
}

static VOID KushCharmapPrepare(HWND window)
{
    HWND labels[2];
#ifndef REMOVE_ADVANCED
    HWND advanced[4];
    HWND sorted[4] = {0};
#endif
    RECT work, status, outer = {0, 0, 530, 388};
    UINT i;
    if (!KushCharacterMap) return;
    if (KushCharmapLabels(hCharmapDlg, labels, 2) != 2) goto fallback;
#ifndef REMOVE_ADVANCED
    if (KushCharmapLabels(hAdvancedDlg, advanced, 4) != 4) goto fallback;
#endif
    if (!KushCharmapWorkArea(window, &work)) goto fallback;
    SendMessageW(hStatusWnd, WM_SIZE, 0, 0);
    GetClientRect(hStatusWnd, &status);
    outer.bottom += status.bottom;
#ifndef REMOVE_ADVANCED
    outer.bottom += 86;
#endif
    if (!AdjustWindowRectEx(&outer, GetWindowLongPtrW(window, GWL_STYLE), FALSE,
                            GetWindowLongPtrW(window, GWL_EXSTYLE))) goto fallback;
    if (outer.right - outer.left > work.right - work.left ||
        outer.bottom - outer.top > work.bottom - work.top) goto fallback;
    if (!KushCharmapTextFits(labels[0], 48) ||
        !KushCharmapTextFits(labels[1], 120) ||
        !KushCharmapTextFits(GetDlgItem(hCharmapDlg, IDC_SELECT), 60) ||
        !KushCharmapTextFits(GetDlgItem(hCharmapDlg, IDC_COPY), 60) ||
        !KushCharmapTextFits(GetDlgItem(hCharmapDlg, IDC_CMHELP), 60)) goto fallback;
#ifndef REMOVE_ADVANCED
    for (i = 0; i < 4; ++i)
    {
        RECT r;
        UINT slot;
        GetWindowRect(advanced[i], &r);
        MapWindowPoints(NULL, hAdvancedDlg, (POINT*)&r, 2);
        slot = r.left > 200 ? 3 : (r.top < 25 ? 0 : (r.top < 65 ? 1 : 2));
        if (sorted[slot]) goto fallback;
        sorted[slot] = advanced[i];
        if (!KushCharmapTextFits(advanced[i], slot == 3 ? 68 : 84)) goto fallback;
    }
#endif
    KushCharmapPlace(hCharmapDlg, 0, 0, 530, 388);
    KushCharmapPlace(labels[0], 16, 22, 48, 20);
    KushCharmapPlace(GetDlgItem(hCharmapDlg, IDC_FONTCOMBO), 80, 14, 346, 260);
    SendDlgItemMessageW(hCharmapDlg, IDC_FONTCOMBO, CB_SETITEMHEIGHT, (WPARAM)-1, 24);
    SendDlgItemMessageW(hCharmapDlg, IDC_FONTCOMBO, CB_SETITEMHEIGHT, 0, 22);
    KushCharmapPlace(GetDlgItem(hCharmapDlg, IDC_CMHELP), 442, 14, 72, 30);
    KushCharmapPlace(GetDlgItem(hCharmapDlg, IDC_FONTMAP), 16, 56, 498, 232);
    KushCharmapPlace(labels[1], 16, 310, 120, 22);
    KushCharmapPlace(GetDlgItem(hCharmapDlg, IDC_TEXTBOX), 144, 304, 194, 30);
    KushCharmapPlace(GetDlgItem(hCharmapDlg, IDC_SELECT), 354, 304, 72, 30);
    KushCharmapPlace(GetDlgItem(hCharmapDlg, IDC_COPY), 442, 304, 72, 30);
    KushCharmapPlace(GetDlgItem(hCharmapDlg, IDC_CHECK_ADVANCED), 16, 352, 498, 24);
#ifndef REMOVE_ADVANCED
    KushCharmapPlace(hAdvancedDlg, 0, 388, 530, 86);
    for (i = 0; i < 3; ++i)
        KushCharmapPlace(sorted[i], 16, 8 + 26 * i, 84, 20);
    KushCharmapPlace(sorted[3], 344, 8, 68, 20);
    KushCharmapPlace(GetDlgItem(hAdvancedDlg, IDC_COMBO_CHARSET), 112, 4, 204, 160);
    KushCharmapPlace(GetDlgItem(hAdvancedDlg, IDC_COMBO_GROUPBY), 112, 30, 204, 160);
    KushCharmapPlace(GetDlgItem(hAdvancedDlg, IDC_EDIT_SEARCH), 112, 56, 204, 24);
    KushCharmapPlace(GetDlgItem(hAdvancedDlg, IDC_EDIT_UNICODE), 416, 4, 98, 24);
    KushCharmapPlace(GetDlgItem(hAdvancedDlg, IDC_BUTTON_SEARCH), 344, 56, 170, 24);
#endif
    return;
fallback:
    KushCharacterMap = FALSE;
}

static INT_PTR KushCharmapBackground(HDC dc)
{
    SetBkColor(dc, RGB(255, 255, 255));
    SetTextColor(dc, RGB(35, 45, 56));
    return (INT_PTR)GetStockObject(WHITE_BRUSH);
}

#endif
