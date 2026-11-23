/* Native common-dialog layout check. Cancel does not create or change a file. */
#include <windows.h>
#include <commdlg.h>
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    OPENFILENAMEW ofn = {0};
    WCHAR file[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = instance;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = L"X:\\reactos";
    ofn.lpstrTitle = L"Kush native no-places compatibility check";
    ofn.lpstrFilter = L"Text files\0*.txt\0All files\0*.*\0\0";
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.FlagsEx = OFN_EX_NOPLACESBAR;
    GetOpenFileNameW(&ofn);
    return 0;
}
