/*
 * Wine WOW64 ReactOS port 
 *
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         wow64win.dll
 * FILE:            dll/wow64win/callback32.h
 * PROGRAMMER:      Marcin Jabłoński
 */

#pragma once

typedef struct _WINDOWPROC_CALLBACK_ARGUMENTS32
{
    ULONG Proc;
    ULONG IsAnsiProc;
    ULONG Wnd;
    UINT Msg;
    ULONG wParam;
    ULONG lParam;
    INT lParamBufferSize;
    ULONG Result;
} WINDOWPROC_CALLBACK_ARGUMENTS32, *PWINDOWPROC_CALLBACK_ARGUMENTS32;

typedef struct _EVENTPROC_CALLBACK_ARGUMENTS32
{
    ULONG hook;
    DWORD event;
    ULONG hwnd; 
    LONG idObject;
    LONG idChild;
    DWORD dwEventThread;
    DWORD dwmsEventTime;
    ULONG Proc;
    INT Mod;
    ULONG offPfn;
} EVENTPROC_CALLBACK_ARGUMENTS32, *PEVENTPROC_CALLBACK_ARGUMENTS32;

typedef struct _LOADMENU_CALLBACK_ARGUMENTS32
{
    ULONG hModule;
    ULONG InterSource;
    WCHAR MenuName[1];
} LOADMENU_CALLBACK_ARGUMENTS32, *PLOADMENU_CALLBACK_ARGUMENTS32;

typedef struct _COPYIMAGE_CALLBACK_ARGUMENTS32
{
    ULONG hImage;
    UINT uType;
    int cxDesired;
    int cyDesired;
    UINT fuFlags;
} COPYIMAGE_CALLBACK_ARGUMENTS32, *PCOPYIMAGE_CALLBACK_ARGUMENTS32;

typedef struct _CLIENT_LOAD_LIBRARY_ARGUMENTS32
{
    UNICODE_STRING32 strLibraryName;
    UNICODE_STRING32 strInitFuncName;
    BOOL Unload;
    BOOL ApiHook;
} CLIENT_LOAD_LIBRARY_ARGUMENTS32, *PCLIENT_LOAD_LIBRARY_ARGUMENTS32;

typedef struct _LPK_CALLBACK_ARGUMENTS32
{
    ULONG lpString;
    ULONG hdc;
    INT x;
    INT y;
    UINT flags;
    RECT rect;
    UINT count;
    BOOL bRect;
} LPK_CALLBACK_ARGUMENTS32, *PLPK_CALLBACK_ARGUMENTS32;

/* Kush: window hooks for 32-bit programs (User32CallHookProcFromKernel) */
typedef struct _HOOKPROC_CALLBACK_ARGUMENTS32
{
    INT HookId;
    INT Code;
    ULONG wParam;
    ULONG lParam;       /* offset of the extra data from the start, or a value */
    ULONG Proc;
    INT Mod;
    ULONG offPfn;
    BOOLEAN Ansi;
    LONG Result;
    UINT lParamSize;
    WCHAR ModuleName[512];
} HOOKPROC_CALLBACK_ARGUMENTS32, *PHOOKPROC_CALLBACK_ARGUMENTS32;

typedef struct _K32_MSG32 { ULONG hwnd; UINT message; ULONG wParam; ULONG lParam; DWORD time; POINT pt; } K32_MSG32;
typedef struct _K32_MOUSEHOOKSTRUCT32 { POINT pt; ULONG hwnd; UINT wHitTestCode; ULONG dwExtraInfo; } K32_MOUSEHOOKSTRUCT32;
typedef struct _K32_KBDLLHOOKSTRUCT32 { DWORD vkCode; DWORD scanCode; DWORD flags; DWORD time; ULONG dwExtraInfo; } K32_KBDLLHOOKSTRUCT32;
typedef struct _K32_MSLLHOOKSTRUCT32 { POINT pt; DWORD mouseData; DWORD flags; DWORD time; ULONG dwExtraInfo; } K32_MSLLHOOKSTRUCT32;
typedef struct _K32_CBTACTIVATESTRUCT32 { BOOL fMouse; ULONG hWndActive; } K32_CBTACTIVATESTRUCT32;
typedef struct _K32_CREATESTRUCTW32
{
    ULONG lpCreateParams; ULONG hInstance; ULONG hMenu; ULONG hwndParent;
    int cy; int cx; int y; int x; LONG style; ULONG lpszName; ULONG lpszClass; DWORD dwExStyle;
} K32_CREATESTRUCTW32;
typedef struct _K32_HOOKPROC_CBT_CREATEWND_EXTRA_ARGUMENTS32 { K32_CREATESTRUCTW32 Cs; ULONG WndInsertAfter; } K32_HOOKPROC_CBT_CREATEWND_EXTRA_ARGUMENTS32;
typedef struct _K32_CWPSTRUCT32 { ULONG lParam; ULONG wParam; UINT message; ULONG hwnd; } K32_CWPSTRUCT32;
typedef struct _K32_CWPRETSTRUCT32 { LONG lResult; ULONG lParam; ULONG wParam; UINT message; ULONG hwnd; } K32_CWPRETSTRUCT32;
