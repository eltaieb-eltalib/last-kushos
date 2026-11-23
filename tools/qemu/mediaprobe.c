#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BOOL State(HWND window, const char *expected)
{
    char text[512];
    GetWindowTextA(window, text, sizeof(text));
    printf("%s: %s\n", expected, text);
    return strstr(text, expected) != NULL;
}

int main(int argc, char **argv)
{
    HWND window = FindWindowA("ROSMPLAY32", NULL);
    RECT r;
    char caption[512];
    if (!window) { puts("FAIL: player window not found"); return 2; }
    if (argc > 1 && !strcmp(argv[1], "exercise"))
    {
        BOOL pass;
        SendMessage(window, WM_COMMAND, 1501, 0); Sleep(250);
        pass = State(window, "Stopped");
        SendMessage(window, WM_COMMAND, 1500, 0); Sleep(750);
        pass = State(window, "Playing") && pass;
        SendMessage(window, WM_COMMAND, 1500, 0); Sleep(250);
        pass = State(window, "Paused") && pass;
        SendMessage(window, WM_COMMAND, 1007, 0); Sleep(250);
        puts(pass ? "PASS playback command states" : "FAIL playback command states");
        if (!pass) return 1;
    }
    else if (argc > 1 && (!strcmp(argv[1], "small") || !strcmp(argv[1], "large")))
    {
        GetWindowRect(window, &r);
        r.right = r.left + (!strcmp(argv[1], "small") ? 300 : 680);
        r.bottom = r.top + 90;
        SendMessage(window, WM_SIZING, WMSZ_BOTTOMRIGHT, (LPARAM)&r);
        SetWindowPos(window, HWND_TOP, 40, 80, r.right-r.left, r.bottom-r.top, SWP_SHOWWINDOW);
    }
    else if (argc > 1 && !strcmp(argv[1], "command"))
    {
        if (argc < 3) return 2;
        SendMessage(window, WM_COMMAND, atoi(argv[2]), 0);
    }
    Sleep(250);
    GetWindowRect(window, &r);
    GetWindowTextA(window, caption, sizeof(caption));
    printf("Player %ld x %ld: %s\n", r.right-r.left, r.bottom-r.top, caption);
    printf("Toolbar button size: %lu\n", (unsigned long)SendMessage(FindWindowExA(window,NULL,TOOLBARCLASSNAMEA,NULL),TB_GETBUTTONSIZE,0,0));
    return 0;
}
