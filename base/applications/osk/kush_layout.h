/* Presentation-only window sizing/placement. Key scan codes, input, focus,
 * timers, fonts, menus and saved-setting mechanics remain native. */
#ifndef KUSH_OSK_LAYOUT_H
#define KUSH_OSK_LAYOUT_H

static BOOL KushOskLayout;

static BOOL KushOskLayoutEnabled(HWND window)
{
    const WCHAR *names[] = {L"ClassicOnScreenKeyboard", L"ClassicWindows"};
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
        for (i = 0; i < _countof(names); ++i)
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

static BOOL KushOskWorkArea(HWND window, RECT *work)
{
    MONITORINFO info;
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info)) return FALSE;
    *work = info.rcWork;
    return TRUE;
}

static VOID KushOskWindowSize(HWND window, INT *width, INT *height)
{
    RECT rect, client, work;
    INT w, h;
    if (!KushOskLayout || !KushOskWorkArea(window, &work)) return;
    GetWindowRect(window, &rect);
    GetClientRect(window, &client);
    /* Native widths deliberately exclude the Standard layout numpad.
     * Keep that width; only height needs the full nonclient delta. */
    w = *width;
    h = Globals.Keyboard->Size.cy + rect.bottom - rect.top - client.bottom;
    if (w > work.right - work.left || h > work.bottom - work.top) return;
    *width = w;
    *height = h;
}

static VOID KushOskClamp(HWND window)
{
    RECT rect, work;
    INT x, y, width, height;
    if (!KushOskLayout || !KushOskWorkArea(window, &work)) return;
    GetWindowRect(window, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    if (width > work.right - work.left || height > work.bottom - work.top) return;
    x = min(max(rect.left, work.left), work.right - width);
    y = min(max(rect.top, work.top), work.bottom - height);
    Globals.PosX = x;
    Globals.PosY = y;
    SetWindowPos(window, NULL, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

#endif
