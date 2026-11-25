/* gltest: create an OpenGL context on a hidden window and print what the
 * ICD reports (vendor, renderer, version). Builds for x64 and x86. */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>

/* Show OutputDebugString text (Mesa's messages) on stdout */
static LONG CALLBACK DebugStringHandler(EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == 0x40010006 /* DBG_PRINTEXCEPTION_C */ &&
        ep->ExceptionRecord->NumberParameters >= 2)
    {
        printf("[debug] %.*s", (int)ep->ExceptionRecord->ExceptionInformation[0],
               (const char *)ep->ExceptionRecord->ExceptionInformation[1]);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(void)
{
    WNDCLASSA wc = {0};
    HWND hwnd;
    HDC hdc;
    HGLRC rc;
    int pf;
    PIXELFORMATDESCRIPTOR pfd = {0};

    AddVectoredExceptionHandler(1, DebugStringHandler);
    printf("%d-bit GetConsoleWindow()=%p\n", (int)sizeof(void*) * 8, (void*)GetConsoleWindow());
    {
        /* What opengl32 needs for the default ICD (Kush 1.1) */
        char name[64] = "";
        DWORD size = sizeof(name);
        HMODULE icd = LoadLibraryA("libgallium_wgl.dll");
        LONG r = RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Kush\\OpenGL", "DefaultICD",
                              RRF_RT_REG_SZ, NULL, name, &size);
        printf("%d-bit DefaultICD=%s (reg %ld), LoadLibrary(libgallium_wgl.dll)=%p err %lu\n",
               (int)sizeof(void*) * 8, name, r, (void*)icd, icd ? 0 : GetLastError());
        if (icd)
            printf("%d-bit DrvValidateVersion=%p\n", (int)sizeof(void*) * 8,
                   (void*)GetProcAddress(icd, "DrvValidateVersion"));
    }
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "kushgltest";
    RegisterClassA(&wc);
    hwnd = CreateWindowA("kushgltest", "gl", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64,
                         NULL, NULL, wc.hInstance, NULL);
    hdc = GetDC(hwnd);
    {
        DWORD q = 0x1101; /* OPENGL_GETINFO */
        HMODULE icd = GetModuleHandleA("libgallium_wgl.dll");
        printf("%d-bit ExtEscape(QUERYESCSUPPORT, OPENGL_GETINFO)=%d\n", (int)sizeof(void*) * 8,
               ExtEscape(hdc, QUERYESCSUPPORT, sizeof(q), (LPCSTR)&q, 0, NULL));
        if (icd)
        {
            BOOL (WINAPI *pValidate)(DWORD) = (void*)GetProcAddress(icd, "DrvValidateVersion");
            LONG (WINAPI *pDescribe)(HDC, INT, ULONG, PIXELFORMATDESCRIPTOR*) =
                (void*)GetProcAddress(icd, "DrvDescribePixelFormat");
            printf("%d-bit DrvValidateVersion(1)=%d DrvDescribePixelFormat count=%ld\n",
                   (int)sizeof(void*) * 8, pValidate ? pValidate(1) : -1,
                   pDescribe ? pDescribe(hdc, 1, 0, NULL) : -1);
        }
    }
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pf = ChoosePixelFormat(hdc, &pfd);
    if (!pf || !SetPixelFormat(hdc, pf, &pfd))
    {
        printf("pixel format failed: %d err %lu\n", pf, GetLastError());
        return 1;
    }
    rc = wglCreateContext(hdc);
    if (!rc || !wglMakeCurrent(hdc, rc))
    {
        printf("wglCreateContext/MakeCurrent failed: err %lu\n", GetLastError());
        return 2;
    }
    printf("%d-bit GL_VENDOR:   %s\n", (int)sizeof(void*) * 8, (const char*)glGetString(GL_VENDOR));
    printf("%d-bit GL_RENDERER: %s\n", (int)sizeof(void*) * 8, (const char*)glGetString(GL_RENDERER));
    printf("%d-bit GL_VERSION:  %s\n", (int)sizeof(void*) * 8, (const char*)glGetString(GL_VERSION));
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    return 0;
}
