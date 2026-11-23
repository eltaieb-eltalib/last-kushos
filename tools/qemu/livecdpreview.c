/* Display the real native LiveCD pages without launching an installer,
 * shell or reboot when the wizard returns. For QEMU UI checks only. */
#include "../../base/system/userinit/userinit.h"
HINSTANCE hInstance;
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR command, int show)
{
    STATE state = {0};
    hInstance = instance;
    state.NextPage = LOCALEPAGE;
    state.Run = SHELL;
    RunLiveCD(&state);
    return 0;
}
