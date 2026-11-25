/* gltest: create an OpenGL context on a hidden window and print what the
 * ICD reports (vendor, renderer, version). Builds for x64 and x86. */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>

int main(void)
{
    WNDCLASSA wc = {0};
    HWND hwnd;
    HDC hdc;
    HGLRC rc;
    int pf;
    PIXELFORMATDESCRIPTOR pfd = {0};

    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "kushgltest";
    RegisterClassA(&wc);
    hwnd = CreateWindowA("kushgltest", "gl", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64,
                         NULL, NULL, wc.hInstance, NULL);
    hdc = GetDC(hwnd);
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
