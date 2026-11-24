/*
 * Kush 1.1: WoW64 GUI smoke test (32-bit). Opens a window with text and a
 * button; closes itself after 60 s. Build:
 *   i686-w64-mingw32-gcc -O2 -static -mwindows -o guitest32.exe guitest.c
 */
#include <windows.h>

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_CREATE:
        CreateWindowW(L"BUTTON", L"32-bit button (WoW64)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      20, 70, 260, 32, h, (HMENU)1, NULL, NULL);
        CreateWindowW(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                      20, 115, 260, 120, h, (HMENU)2, NULL, NULL);
        SendDlgItemMessageW(h, 2, CB_ADDSTRING, 0, (LPARAM)L"Kush 1.1 - WoW64");
        SendDlgItemMessageW(h, 2, CB_ADDSTRING, 0, (LPARAM)L"\x0627\x0644\x0633\x0648\x062F\x0627\x0646");
        SendDlgItemMessageW(h, 2, CB_SETCURSEL, 0, 0);
        SetTimer(h, 1, 60000, NULL);
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        TextOutW(dc, 20, 20, L"Hello from a 32-bit program on Kush 1.1 (WoW64)", 47);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w) == 1)
            MessageBoxW(h, L"MessageBox from a 32-bit program", L"WoW64", MB_OK | MB_ICONINFORMATION);
        return 0;
    case WM_TIMER:
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE prev, LPWSTR cmd, int show)
{
    WNDCLASSW wc = {0};
    MSG msg;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"KushWow64Test";
    RegisterClassW(&wc);
    CreateWindowW(L"KushWow64Test", L"Kush WoW64 GUI test (32-bit)", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                  100, 100, 480, 260, NULL, NULL, hi, NULL);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
