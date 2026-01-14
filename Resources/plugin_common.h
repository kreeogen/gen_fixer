// plugin_common.h
#pragma once

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410 // Win98
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>

// Win9x shim для *_LongPtr (удалены дубликаты)
#ifndef GetWindowLongPtrA
#  define GetWindowLongPtrA GetWindowLongA
#endif
#ifndef SetWindowLongPtrA
#  define SetWindowLongPtrA SetWindowLongA
#endif
#ifndef GetClassLongPtrA
#  define GetClassLongPtrA GetClassLongA
#endif
#ifndef SetClassLongPtrA
#  define SetClassLongPtrA SetClassLongA
#endif
#ifndef GWLP_WNDPROC
#  define GWLP_WNDPROC  GWL_WNDPROC
#endif
#ifndef GWLP_USERDATA
#  define GWLP_USERDATA GWL_USERDATA
#endif

// Winamp SDK
#include "..\SDK\wa_ipc.h"
#include "..\SDK\wa_msgids.h"

#ifndef WM_WA_IPC
#define WM_WA_IPC (WM_USER)
#endif
#ifndef IPC_GET_HMENU
#define IPC_GET_HMENU 281
#endif
#ifndef IPC_RESTARTWINAMP
#define IPC_RESTARTWINAMP 135
#endif
#ifndef IPC_SETVOLUME
#define IPC_SETVOLUME 122
#endif
#ifndef IPC_GETSKIN
#define IPC_GETSKIN  201
#endif
#ifndef IPC_SETSKIN
#define IPC_SETSKIN  200
#endif

// Утилиты
static __inline HWND FindWinamp(void) { return FindWindowA("Winamp v1.x", NULL); }
static __inline BOOL IsVisibleWindow(HWND h) { return (h && IsWindow(h) && IsWindowVisible(h)); }
