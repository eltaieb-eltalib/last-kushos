/*
 * Kush Fluent Start (kushstart.h)
 *
 * Original Fluent-inspired Start experience for Kush 1.1.
 * Original design and implementation for the Kush project (GPL-2.0+,
 * same license as the ReactOS explorer it plugs into).
 *
 * Registry kill-switch:
 *   HKCU\Software\Kush\Shell\ClassicStartMenu = 1  -> always use the
 *   classic ReactOS Start menu.
 */

#pragma once

/* Returns TRUE when the user asked for the classic Start menu. */
BOOL KushStart_IsClassicRequested(VOID);

/* Shows the Kush Start window, or hides it when it is already open.
 * hTray is the Shell_TrayWnd window. Returns TRUE when the Kush Start
 * handled the request (FALSE means: caller should fall back to the
 * classic menu, e.g. window creation failed). */
BOOL KushStart_Toggle(HWND hTray);

/* Hides the Kush Start window when it is open (no-op otherwise). */
VOID KushStart_Hide(VOID);

/* TRUE while the Kush Start window exists. */
BOOL KushStart_IsOpen(VOID);

/* Returns TRUE when the user asked for the classic (left-aligned)
 * taskbar layout: HKCU\Software\Kush\Shell\ClassicTaskbar = 1. */
BOOL KushTaskbar_IsClassicRequested(VOID);
BOOL KushTaskbar_UseCompactTasks(VOID);
