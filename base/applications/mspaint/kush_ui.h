/* Presentation helpers only. Native paint models and command handlers stay intact. */
#pragma once

extern BOOL g_KushPaint;
extern SIZE g_KushPaintMinimum;

inline VOID KushPaintFill(HDC dc, const RECT *rect, COLORREF color)
{
    COLORREF old = SetDCBrushColor(dc, color);
    FillRect(dc, rect, (HBRUSH)GetStockObject(DC_BRUSH));
    SetDCBrushColor(dc, old);
}

inline LRESULT KushPaintErase(HWND window, HDC dc, BOOL& handled)
{
    if (!g_KushPaint)
    {
        handled = FALSE;
        return 0;
    }
    RECT client;
    GetClientRect(window, &client);
    KushPaintFill(dc, &client, RGB(255, 255, 255));
    return 1;
}
