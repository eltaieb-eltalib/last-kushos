/* Presentation for the pre-shell LiveCD dialogs. Native commands and controls
 * keep their IDs, data and behavior; no theme-service startup is moved. */
#ifndef KUSH_LIVECD_FLUENT_H
#define KUSH_LIVECD_FLUENT_H

static INT KushLivePixels(HWND hwnd, INT value)
{
    HDC dc = GetDC(hwnd);
    INT dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(hwnd, dc);
    return MulDiv(value, dpi > 0 ? dpi : 96, 96);
}

static BOOL KushLiveEnabled(VOID)
{
    HIGHCONTRASTW contrast = {sizeof(contrast)};
    LPCWSTR names[] = {L"ClassicLiveCD", L"ClassicWindows"};
    HKEY key;
    UINT i;
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
        (contrast.dwFlags & HCF_HIGHCONTRASTON)) return FALSE;
    if (KushLivePixels(NULL, 640) > GetSystemMetrics(SM_CXSCREEN) ||
        KushLivePixels(NULL, 480) > GetSystemMetrics(SM_CYSCREEN)) return FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
    {
        for (i = 0; i < ARRAYSIZE(names); ++i)
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

static VOID KushLiveMove(HWND dialog, INT id, INT x, INT y, INT width, INT height)
{
    SetWindowPos(GetDlgItem(dialog, id), NULL, KushLivePixels(dialog, x), KushLivePixels(dialog, y),
        KushLivePixels(dialog, width), KushLivePixels(dialog, height), SWP_NOZORDER | SWP_NOACTIVATE);
}

static VOID KushLiveLayout(HWND dialog, PSTATE state, BOOL locale)
{
    HWND child;
    RECT window, client;
    INT labels = 0;
    if (GetWindowLongPtrW(dialog, GWL_EXSTYLE) & WS_EX_LAYOUTRTL)
        state->KushModern = FALSE;
    if (!state->KushModern) return;
    if (!state->KushFont)
    {
        LOGFONTW font;
        HFONT original = (HFONT)SendMessageW(dialog, WM_GETFONT, 0, 0);
        if (!original) original = GetStockObject(DEFAULT_GUI_FONT);
        if (GetObjectW(original, sizeof(font), &font))
        {
            font.lfHeight = -KushLivePixels(dialog, 14);
            font.lfWeight = FW_NORMAL;
            state->KushFont = CreateFontIndirectW(&font);
        }
    }
    GetWindowRect(dialog, &window);
    GetClientRect(dialog, &client);
    SetWindowPos(dialog, NULL, 0, 0,
        window.right - window.left - client.right + KushLivePixels(dialog, 600),
        window.bottom - window.top - client.bottom + KushLivePixels(dialog, 420),
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    for (child = GetWindow(dialog, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        WCHAR name[32];
        LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
        if (state->KushFont) SendMessageW(child, WM_SETFONT, (WPARAM)state->KushFont, FALSE);
        GetClassNameW(child, name, ARRAYSIZE(name));
        if (!_wcsicmp(name, L"Button"))
            SetWindowLongPtrW(child, GWL_STYLE, (style & ~BS_TYPEMASK) | BS_OWNERDRAW);
        else if (!_wcsicmp(name, L"Static") && GetDlgCtrlID(child) == IDC_STATIC)
        {
            if ((style & SS_TYPEMASK) == SS_ETCHEDHORZ) ShowWindow(child, SW_HIDE);
            else if ((style & SS_TYPEMASK) == SS_RIGHT && labels < 2)
            {
                SetWindowPos(child, NULL, KushLivePixels(dialog, 24), KushLivePixels(dialog, 194 + labels * 44),
                    KushLivePixels(dialog, 140), KushLivePixels(dialog, 24), SWP_NOZORDER | SWP_NOACTIVATE);
                ++labels;
            }
            else if ((style & SS_TYPEMASK) == SS_LEFT)
                SetWindowPos(child, NULL, KushLivePixels(dialog, 32), KushLivePixels(dialog, 286),
                    KushLivePixels(dialog, 536), KushLivePixels(dialog, 44), SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    KushLiveMove(dialog, locale ? IDC_LOCALELOGO : IDC_STARTLOGO, 0, 22, 600, 158);
    if (locale)
    {
        KushLiveMove(dialog, IDC_LANGUAGELIST, 180, 190, 388, 200);
        KushLiveMove(dialog, IDC_LAYOUTLIST, 180, 234, 388, 160);
    }
    else
    {
        KushLiveMove(dialog, IDC_RUN, 64, 204, 472, 48);
        KushLiveMove(dialog, IDC_INSTALL, 64, 268, 472, 48);
    }
    KushLiveMove(dialog, IDOK, 356, 366, 100, 36);
    KushLiveMove(dialog, IDCANCEL, 468, 366, 100, 36);
    SendMessageW(dialog, DM_SETDEFID, locale ? IDOK : IDC_RUN, 0);
}

static BOOL KushLiveDrawButton(LPDRAWITEMSTRUCT item, PSTATE state)
{
    RECT rect = item->rcItem, text = rect;
    WCHAR caption[256];
    BOOL primary, disabled, pressed;
    HBRUSH brush, oldBrush;
    HPEN pen, oldPen;
    HFONT oldFont = NULL;
    INT radius;
    if (item->CtlType != ODT_BUTTON) return FALSE;
    primary = item->CtlID == IDC_RUN ||
        (item->CtlID == IDOK && GetDlgItem(item->hwndItem ? GetParent(item->hwndItem) : NULL, IDC_LANGUAGELIST));
    disabled = !!(item->itemState & ODS_DISABLED);
    pressed = !!(item->itemState & ODS_SELECTED);
    FillRect(item->hDC, &rect, GetStockObject(WHITE_BRUSH));
    brush = CreateSolidBrush(disabled ? RGB(239, 241, 243) :
        primary ? (pressed ? RGB(0, 94, 102) : RGB(0, 125, 131)) :
        pressed ? RGB(234, 240, 243) : RGB(255, 255, 255));
    pen = CreatePen(PS_SOLID, 1, primary && !disabled ? RGB(0, 125, 131) : RGB(201, 211, 219));
    oldBrush = SelectObject(item->hDC, brush);
    oldPen = SelectObject(item->hDC, pen);
    radius = KushLivePixels(item->hwndItem, 10);
    RoundRect(item->hDC, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(item->hDC, oldPen); SelectObject(item->hDC, oldBrush);
    DeleteObject(pen); DeleteObject(brush);
    if (state->KushFont) oldFont = SelectObject(item->hDC, state->KushFont);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, disabled ? RGB(142, 151, 158) : primary ? RGB(255, 255, 255) : RGB(35, 54, 67));
    GetWindowTextW(item->hwndItem, caption, ARRAYSIZE(caption));
    if (pressed) OffsetRect(&text, 0, 1);
    DrawTextW(item->hDC, caption, -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (item->itemState & ODS_FOCUS)
    {
        InflateRect(&rect, -4, -4);
        DrawFocusRect(item->hDC, &rect);
    }
    if (oldFont) SelectObject(item->hDC, oldFont);
    return TRUE;
}

/* Paint only: native caption geometry, system menu, hit testing and commands stay intact. */
static VOID KushLiveCaption(HWND dialog)
{
    RECT window, caption, text;
    WCHAR title[128];
    HDC dc = GetWindowDC(dialog);
    INT border = GetSystemMetrics(SM_CXDLGFRAME), bw = GetSystemMetrics(SM_CXSIZE);
    INT bh = GetSystemMetrics(SM_CYSIZE), x, y, i;
    HPEN pen, oldPen;
    HFONT oldFont;
    if (!dc) return;
    GetWindowRect(dialog, &window);
    SetRect(&caption, border, GetSystemMetrics(SM_CYDLGFRAME),
        window.right - window.left - border, GetSystemMetrics(SM_CYDLGFRAME) + GetSystemMetrics(SM_CYCAPTION) - 1);
    FillRect(dc, &caption, GetStockObject(WHITE_BRUSH));
    text = caption; text.left += 8; text.right -= bw * 3 + 8;
    oldFont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(48, 65, 78));
    GetWindowTextW(dialog, title, ARRAYSIZE(title));
    DrawTextW(dc, title, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, oldFont);
    for (i = 0; i < 3; ++i)
    {
        x = caption.right - bw / 2 - 1 - i * (bw - 1);
        y = caption.top + bh / 2;
        pen = CreatePen(PS_SOLID, 1, i == 1 ? RGB(184, 193, 200) : RGB(48, 65, 78));
        oldPen = SelectObject(dc, pen);
        if (i == 0)
        {
            MoveToEx(dc, x - 3, y - 3, NULL); LineTo(dc, x + 4, y + 4);
            MoveToEx(dc, x + 3, y - 3, NULL); LineTo(dc, x - 4, y + 4);
        }
        else if (i == 1)
        {
            MoveToEx(dc, x - 3, y - 3, NULL); LineTo(dc, x + 4, y - 3);
            LineTo(dc, x + 4, y + 4); LineTo(dc, x - 3, y + 4); LineTo(dc, x - 3, y - 3);
        }
        else { MoveToEx(dc, x - 3, y + 3, NULL); LineTo(dc, x + 4, y + 3); }
        SelectObject(dc, oldPen); DeleteObject(pen);
    }
    ReleaseDC(dialog, dc);
}

static BOOL KushLiveMessage(HWND dialog, PSTATE state, UINT msg, WPARAM wp, LPARAM lp, INT_PTR *result)
{
    if (!state || !state->KushModern) return FALSE;
    if (msg == WM_NCPAINT || msg == WM_NCACTIVATE)
    {
        DefWindowProcW(dialog, msg, wp, lp);
        KushLiveCaption(dialog);
        *result = TRUE; return TRUE;
    }
    if (msg == WM_CTLCOLORDLG || msg == WM_CTLCOLORSTATIC)
    {
        SetBkColor((HDC)wp, RGB(255, 255, 255));
        SetTextColor((HDC)wp, RGB(48, 65, 78));
        *result = (INT_PTR)GetStockObject(WHITE_BRUSH); return TRUE;
    }
    if (msg == WM_PAINT || msg == WM_ERASEBKGND)
    {
        PAINTSTRUCT ps;
        RECT rect;
        HDC dc = msg == WM_PAINT ? BeginPaint(dialog, &ps) : (HDC)wp;
        GetClientRect(dialog, &rect);
        FillRect(dc, &rect, GetStockObject(WHITE_BRUSH));
        rect.top = rect.bottom - KushLivePixels(dialog, 72);
        SetDCBrushColor(dc, RGB(247, 249, 251));
        FillRect(dc, &rect, GetStockObject(DC_BRUSH));
        rect.bottom = rect.top + 1;
        SetDCBrushColor(dc, RGB(229, 235, 239));
        FillRect(dc, &rect, GetStockObject(DC_BRUSH));
        if (msg == WM_PAINT) EndPaint(dialog, &ps);
        *result = TRUE; return TRUE;
    }
    return FALSE;
}
#endif
