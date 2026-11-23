/* Presentation-only standard Calculator layout. Original commands, arithmetic,
 * resources, keyboard handling and other modes remain untouched. */
#ifndef KUSH_CALCULATOR_FLUENT_H
#define KUSH_CALCULATOR_FLUENT_H

static BOOL KushCalcModern;
static HFONT KushCalcFont, KushCalcDisplayFont, KushCalcOriginalFont;
static HBRUSH KushCalcBackground;
static WNDPROC KushCalcOriginalOutputProc;

static LRESULT CALLBACK KushCalcOutputProc(HWND window, UINT message, WPARAM wp, LPARAM lp)
{
    LRESULT result = CallWindowProcW(KushCalcOriginalOutputProc, window, message, wp, lp);
    if (message == WM_SETTEXT)
    {
        WCHAR text[MAX_CALC_SIZE * 2];
        RECT client;
        SIZE size;
        HDC dc = GetDC(window);
        HFONT candidates[3] = {KushCalcDisplayFont, KushCalcFont, KushCalcOriginalFont};
        HFONT previous;
        UINT i;
        GetWindowTextW(window, text, ARRAYSIZE(text));
        GetClientRect(window, &client);
        if (dc)
        {
            previous = SelectObject(dc, candidates[0]);
            for (i = 0; i < ARRAYSIZE(candidates); ++i)
            {
                SelectObject(dc, candidates[i]);
                GetTextExtentPoint32W(dc, text, lstrlenW(text), &size);
                if (size.cx <= client.right - 8 || i == ARRAYSIZE(candidates) - 1)
                {
                    SendMessageW(window, WM_SETFONT, (WPARAM)candidates[i], FALSE);
                    break;
                }
            }
            SelectObject(dc, previous);
            ReleaseDC(window, dc);
        }
    }
    return result;
}

static VOID KushCalcMove(HWND dialog, INT id, INT x, INT y, INT width, INT height)
{
    SetWindowPos(GetDlgItem(dialog, id), NULL, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

static VOID KushCalcClamp(HWND dialog)
{
    RECT window, work;
    INT x, y;
    if (!KushCalcModern || !SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) return;
    GetWindowRect(dialog, &window);
    x = min(max(window.left, work.left), work.right - (window.right - window.left));
    y = min(max(window.top, work.top), work.bottom - (window.bottom - window.top));
    SetWindowPos(dialog, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static VOID KushCalcCleanup(HWND dialog)
{
    HWND child;
    if (KushCalcOriginalOutputProc)
        SetWindowLongPtrW(GetDlgItem(dialog, IDC_TEXT_OUTPUT), GWLP_WNDPROC,
                          (LONG_PTR)KushCalcOriginalOutputProc);
    if (KushCalcModern)
        for (child = GetWindow(dialog, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
            SendMessageW(child, WM_SETFONT, (WPARAM)KushCalcOriginalFont, FALSE);
    if (KushCalcFont) DeleteObject(KushCalcFont);
    if (KushCalcDisplayFont) DeleteObject(KushCalcDisplayFont);
    if (KushCalcBackground) DeleteObject(KushCalcBackground);
    KushCalcFont = KushCalcDisplayFont = NULL;
    KushCalcBackground = NULL;
    KushCalcOriginalOutputProc = NULL;
    KushCalcModern = FALSE;
}

static VOID KushCalcInit(HWND dialog)
{
    const INT memory[] = {IDC_BUTTON_MC, IDC_BUTTON_MR, IDC_BUTTON_MS, IDC_BUTTON_MP};
    const INT clear[] = {IDC_BUTTON_BACK, IDC_BUTTON_CE, IDC_BUTTON_CANC};
    const INT grid[4][5] = {
        {IDC_BUTTON_7, IDC_BUTTON_8, IDC_BUTTON_9, IDC_BUTTON_DIV, IDC_BUTTON_SQRT},
        {IDC_BUTTON_4, IDC_BUTTON_5, IDC_BUTTON_6, IDC_BUTTON_MULT, IDC_BUTTON_PERCENT},
        {IDC_BUTTON_1, IDC_BUTTON_2, IDC_BUTTON_3, IDC_BUTTON_SUB, IDC_BUTTON_RX},
        {IDC_BUTTON_0, IDC_BUTTON_SIGN, IDC_BUTTON_DOT, IDC_BUTTON_ADD, IDC_BUTTON_EQU}};
    const WCHAR *switches[] = {L"ClassicCalculator", L"ClassicWindows"};
    HIGHCONTRASTW contrast = {sizeof(contrast)};
    LOGFONTW font;
    RECT window, client, work;
    HWND child, output;
    HKEY key;
    HDC dc;
    INT dpi, width, height;
    UINT i, j;
    if (calc.layout != CALC_LAYOUT_STANDARD ||
        (GetWindowLongPtrW(dialog, GWL_EXSTYLE) & WS_EX_LAYOUTRTL)) return;
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
        (contrast.dwFlags & HCF_HIGHCONTRASTON)) return;
    dc = GetDC(dialog);
    if (!dc) return;
    dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(dialog, dc);
    /* Only the inspected 96DPI geometry is enabled in this first batch. */
    if (dpi != 96) return;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Kush\\Shell", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
    {
        for (i = 0; i < ARRAYSIZE(switches); ++i)
        {
            DWORD type = 0, value = 0, size = sizeof(value);
            if (RegQueryValueExW(key, switches[i], NULL, &type, (BYTE*)&value, &size) == ERROR_SUCCESS &&
                type == REG_DWORD && size == sizeof(value) && value)
            {
                RegCloseKey(key);
                return;
            }
        }
        RegCloseKey(key);
    }
    GetWindowRect(dialog, &window);
    GetClientRect(dialog, &client);
    width = window.right - window.left - client.right + 420;
    height = window.bottom - window.top - client.bottom + 430;
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0) ||
        width > work.right - work.left || height > work.bottom - work.top) return;
    KushCalcOriginalFont = (HFONT)SendMessageW(dialog, WM_GETFONT, 0, 0);
    if (!KushCalcOriginalFont) KushCalcOriginalFont = GetStockObject(DEFAULT_GUI_FONT);
    if (!GetObjectW(KushCalcOriginalFont, sizeof(font), &font)) return;
    font.lfHeight = -15;
    font.lfWeight = FW_NORMAL;
    KushCalcFont = CreateFontIndirectW(&font);
    font.lfHeight = -22;
    KushCalcDisplayFont = CreateFontIndirectW(&font);
    KushCalcBackground = CreateSolidBrush(RGB(246, 248, 250));
    if (!KushCalcFont || !KushCalcDisplayFont || !KushCalcBackground)
    {
        KushCalcCleanup(dialog);
        return;
    }
    KushCalcModern = TRUE;
    SetWindowPos(dialog, NULL, 0, 0, width, height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    for (child = GetWindow(dialog, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        SendMessageW(child, WM_SETFONT, (WPARAM)KushCalcFont, FALSE);
    KushCalcMove(dialog, IDC_TEXT_OUTPUT, 16, 12, 388, 64);
    KushCalcMove(dialog, IDC_TEXT_MEMORY, 16, 76, 28, 18);
    for (i = 0; i < ARRAYSIZE(memory); ++i)
        KushCalcMove(dialog, memory[i], 16 + i * 99, 96, 91, 32);
    for (i = 0; i < ARRAYSIZE(clear); ++i)
        KushCalcMove(dialog, clear[i], 16 + i * 132, 140, 124, 40);
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 5; ++j)
            KushCalcMove(dialog, grid[i][j], 16 + j * 79, 196 + i * 54, 72, 48);
    output = GetDlgItem(dialog, IDC_TEXT_OUTPUT);
    for (i = 0; i < 2; ++i)
    {
        child = GetDlgItem(dialog, i ? IDC_TEXT_MEMORY : IDC_TEXT_OUTPUT);
        SetWindowLongPtrW(child, GWL_EXSTYLE,
            GetWindowLongPtrW(child, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
        SetWindowPos(child, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    SendMessageW(output, WM_SETFONT, (WPARAM)KushCalcDisplayFont, FALSE);
    KushCalcOriginalOutputProc = (WNDPROC)SetWindowLongPtrW(output, GWLP_WNDPROC, (LONG_PTR)KushCalcOutputProc);
}

static BOOL KushCalcDrawButton(LPDRAWITEMSTRUCT item)
{
    RECT rect = item->rcItem, text = rect;
    WCHAR caption[64];
    HBRUSH brush, oldBrush;
    HPEN pen, oldPen;
    HFONT oldFont;
    LPBTNINFO info = (LPBTNINFO)GetWindowLongPtrW(item->hwndItem, GWLP_USERDATA);
    BOOL disabled = !!(item->itemState & ODS_DISABLED);
    BOOL pressed = !!(item->itemState & ODS_SELECTED);
    BOOL primary = item->CtlID == IDC_BUTTON_EQU;
    BOOL digit = item->CtlID == IDC_BUTTON_0 || item->CtlID == IDC_BUTTON_1 ||
        item->CtlID == IDC_BUTTON_2 || item->CtlID == IDC_BUTTON_3 ||
        item->CtlID == IDC_BUTTON_4 || item->CtlID == IDC_BUTTON_5 ||
        item->CtlID == IDC_BUTTON_6 || item->CtlID == IDC_BUTTON_7 ||
        item->CtlID == IDC_BUTTON_8 || item->CtlID == IDC_BUTTON_9;
    BOOL hover = info && info->bHover;
    COLORREF fill = digit ? RGB(255,255,255) : RGB(239,243,246);
    if (pressed) fill = RGB(220,230,235);
    else if (hover) fill = RGB(231,239,243);
    if (primary) fill = pressed ? RGB(9,91,95) : hover ? RGB(17,131,136) : RGB(13,115,119);
    if (disabled) fill = RGB(239,241,243);
    FillRect(item->hDC, &rect, KushCalcBackground);
    brush = CreateSolidBrush(fill);
    pen = CreatePen(PS_SOLID, 1, primary && !disabled ? fill : RGB(216,224,230));
    oldBrush = SelectObject(item->hDC, brush);
    oldPen = SelectObject(item->hDC, pen);
    RoundRect(item->hDC, rect.left, rect.top, rect.right, rect.bottom, 8, 8);
    SelectObject(item->hDC, oldPen);
    SelectObject(item->hDC, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
    oldFont = SelectObject(item->hDC, KushCalcFont);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, disabled ? RGB(147,155,162) : primary ? RGB(255,255,255) : RGB(31,44,54));
    GetWindowTextW(item->hwndItem, caption, ARRAYSIZE(caption));
    if (pressed) OffsetRect(&text, 0, 1);
    DrawTextW(item->hDC, caption, -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if ((item->itemState & ODS_FOCUS) && !(item->itemState & ODS_NOFOCUSRECT))
    {
        InflateRect(&rect, -4, -4);
        DrawFocusRect(item->hDC, &rect);
    }
    SelectObject(item->hDC, oldFont);
    return TRUE;
}

#endif
