#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "../../base/applications/msconfig/resource.h"

static int consoleCount;
static BOOL CALLBACK CountConsole(HWND window, LPARAM unused)
{
    char name[80];
    (void)unused;
    GetClassNameA(window, name, sizeof(name));
    if (!strcmp(name, "ConsoleWindowClass")) ++consoleCount;
    return TRUE;
}

static HWND PageWith(HWND main, int id)
{
    HWND child;
    for (child = GetWindow(main, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        if (GetDlgItem(child, id)) return child;
    return NULL;
}

int main(int argc, char **argv)
{
    HWND main = FindWindowA(NULL, "System Configuration Program");
    HWND general, services, startup, tools, list;
    RECT r, work;
    UINT before, after;
    BOOL pass = TRUE;
    if (!main) { puts("FAIL: msconfig window not found"); return 2; }
    general = PageWith(main, IDC_CBX_NORMAL_START);
    services = PageWith(main, IDC_SERVICES_LIST);
    startup = PageWith(main, IDC_STARTUP_LIST);
    tools = PageWith(main, IDC_TOOLS_LIST);
    if (!general || !services || !startup || !tools) { puts("FAIL: missing native page"); return 2; }
    if (argc > 1 && !strcmp(argv[1], "selective"))
        SendMessage(GetDlgItem(general, IDC_CBX_SELECTIVE_STARTUP), BM_CLICK, 0, 0);
    else if (argc > 1 && !strcmp(argv[1], "normal"))
        SendMessage(GetDlgItem(general, IDC_CBX_NORMAL_START), BM_CLICK, 0, 0);
    else if (argc > 1 && (!strcmp(argv[1], "veto") || !strcmp(argv[1], "startup-veto")))
    {
        list = !strcmp(argv[1], "veto") ? GetDlgItem(services, IDC_SERVICES_LIST) : GetDlgItem(startup, IDC_STARTUP_LIST);
        before = ListView_GetCheckState(list, 0);
        SendMessage(list, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 28));
        SendMessage(list, WM_LBUTTONUP, 0, MAKELPARAM(10, 28));
        after = ListView_GetCheckState(list, 0);
        printf("Read-only checkbox before=%u after=%u\n", before, after);
        pass = before == after;
    }
    else if (argc > 1 && !strcmp(argv[1], "run-console"))
    {
        int initial;
        consoleCount = 0; EnumWindows(CountConsole, 0); initial = consoleCount;
        list = GetDlgItem(tools, IDC_TOOLS_LIST);
        SendMessage(list, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(40, 28));
        SendMessage(list, WM_LBUTTONUP, 0, MAKELPARAM(40, 28));
        SendMessage(GetDlgItem(tools, IDC_BTN_RUN), BM_CLICK, 0, 0);
        Sleep(1000);
        consoleCount = 0; EnumWindows(CountConsole, 0);
        printf("Tools Console windows before=%d after=%d\n", initial, consoleCount);
        pass = consoleCount > initial;
    }
    if (argc > 1 && !strcmp(argv[1], "selective"))
        pass = IsWindowEnabled(GetDlgItem(general, IDC_CBX_SYSTEM_INI));
    if (argc > 1 && !strcmp(argv[1], "normal"))
        pass = !IsWindowEnabled(GetDlgItem(general, IDC_CBX_SYSTEM_INI));
    pass = pass && !IsWindowEnabled(GetDlgItem(main, IDOK)) &&
           !IsWindowEnabled(GetDlgItem(main, IDC_BTN_APPLY)) &&
           !IsWindowEnabled(GetDlgItem(main, IDC_BTN_HELP)) &&
           IsWindowEnabled(GetDlgItem(main, IDCANCEL)) &&
           !IsWindowEnabled(GetDlgItem(services, IDC_BTN_SERVICES_ACTIVATE)) &&
           !IsWindowEnabled(GetDlgItem(startup, IDC_BTN_STARTUP_ACTIVATE));
    GetWindowRect(main, &r);
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    printf("Window %ld x %ld; within workarea=%d\n", r.right-r.left, r.bottom-r.top,
           r.left >= work.left && r.top >= work.top && r.right <= work.right && r.bottom <= work.bottom);
    printf("Buttons: OK=%d Apply=%d Help=%d Cancel=%d\n",
           IsWindowEnabled(GetDlgItem(main, IDOK)), IsWindowEnabled(GetDlgItem(main, IDC_BTN_APPLY)),
           IsWindowEnabled(GetDlgItem(main, IDC_BTN_HELP)), IsWindowEnabled(GetDlgItem(main, IDCANCEL)));
    printf("General: normal=%ld selective=%ld inputs=%d\n",
           (long)SendDlgItemMessage(general, IDC_CBX_NORMAL_START, BM_GETCHECK, 0, 0),
           (long)SendDlgItemMessage(general, IDC_CBX_SELECTIVE_STARTUP, BM_GETCHECK, 0, 0),
           IsWindowEnabled(GetDlgItem(general, IDC_CBX_SYSTEM_INI)));
    printf("Native items: tabs=%ld services=%ld startup=%ld tools=%ld\n",
           (long)SendDlgItemMessage(main, IDC_TAB, TCM_GETITEMCOUNT, 0, 0),
           (long)SendDlgItemMessage(services, IDC_SERVICES_LIST, LVM_GETITEMCOUNT, 0, 0),
           (long)SendDlgItemMessage(startup, IDC_STARTUP_LIST, LVM_GETITEMCOUNT, 0, 0),
           (long)SendDlgItemMessage(tools, IDC_TOOLS_LIST, LVM_GETITEMCOUNT, 0, 0));
    puts(pass ? "PASS bounded native control check" : "FAIL bounded native control check");
    return pass ? 0 : 1;
}
