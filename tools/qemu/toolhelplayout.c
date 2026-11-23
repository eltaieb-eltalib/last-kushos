#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../sdk/include/psdk/tlhelp32.h"
#include <stddef.h>
#ifdef _WIN64
_Static_assert(sizeof(PROCESSENTRY32W)==568,"Win64 process size");
_Static_assert(offsetof(PROCESSENTRY32W,szExeFile)==44,"Win64 name offset");
_Static_assert(sizeof(HEAPLIST32)==32,"Win64 heap list size");
_Static_assert(sizeof(HEAPENTRY32)==56,"Win64 heap entry size");
#else
_Static_assert(sizeof(PROCESSENTRY32W)==556,"Win32 process size unchanged");
_Static_assert(offsetof(PROCESSENTRY32W,szExeFile)==36,"Win32 name offset unchanged");
_Static_assert(sizeof(HEAPLIST32)==16,"Win32 heap list size unchanged");
_Static_assert(sizeof(HEAPENTRY32)==36,"Win32 heap entry size unchanged");
#endif
