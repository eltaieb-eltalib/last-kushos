/*
 * Kush 1.1: create a desktop shortcut from a 32-bit program the way installers
 * do it (IShellLinkW + IPersistFile::Save, NSIS CreateShortcut).
 *   i686-w64-mingw32-gcc -O1 -municode -o lnk32.exe lnk32.c -lole32 -luuid -lshell32
 */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

int wmain(void)
{
    IShellLinkW *sl = NULL; IPersistFile *pf = NULL;
    WCHAR desk[MAX_PATH], path[MAX_PATH];
    HRESULT hr = CoInitialize(NULL);
    printf("CoInitialize = %lx\n", hr);
    hr = SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk);
    printf("SHGetFolderPath(Desktop) = %lx [%ls]\n", hr, desk);
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl);
    printf("CoCreateInstance(ShellLink) = %lx\n", hr);
    if (FAILED(hr)) return 1;
    IShellLinkW_SetPath(sl, L"C:\\Firefox\\firefox.exe");
    IShellLinkW_SetWorkingDirectory(sl, L"C:\\Firefox");
    IShellLinkW_SetDescription(sl, L"Firefox (shortcut made by a 32-bit program)");
    hr = IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf);
    printf("QueryInterface(IPersistFile) = %lx\n", hr);
    if (FAILED(hr)) return 2;
    swprintf(path, MAX_PATH, L"%ls\\Firefox32.lnk", desk);
    hr = IPersistFile_Save(pf, path, TRUE);
    printf("IPersistFile::Save(%ls) = %lx\n", path, hr);
    printf("file exists = %d\n", GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
    SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW, path, NULL);
    IPersistFile_Release(pf); IShellLinkW_Release(sl);
    CoUninitialize();
    return 0;
}
