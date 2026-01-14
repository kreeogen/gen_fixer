
// sdk_compat.h  (minimal Winamp SDK compatibility for VC7.1 / Win9x)
// Keep this file ANSI-only (no <string>, no new CRTs).

#ifndef SDK_COMPAT_H
#define SDK_COMPAT_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Winamp WM_WA_IPC base
#ifndef WM_WA_IPC
#define WM_WA_IPC (WM_USER)
#endif

// Numeric IPCs used below. Values are from classic wa_ipc.h (2.x/5.x era).
#ifndef IPC_GET_EXTENDED_FILE_INFO
#define IPC_GET_EXTENDED_FILE_INFO 290  // extendedFileInfoStruct (ANSI)
#endif

#ifndef IPC_GET_BASIC_FILE_INFO
#define IPC_GET_BASIC_FILE_INFO    291  // basicFileInfoStruct (ANSI)
#endif

#ifndef IPC_SET_EXTENDED_FILE_INFO
#define IPC_SET_EXTENDED_FILE_INFO 294
#endif

#ifndef IPC_WRITE_EXTENDED_FILE_INFO
#define IPC_WRITE_EXTENDED_FILE_INFO 295
#endif

// Wide-char variants appeared in later SDKs; values varied by branch.
// We keep them optional and only process if these defines exist upstream.
// You can uncomment & adjust if needed in your environment.
// #define IPC_GET_EXTENDED_FILE_INFOW 3034
// #define IPC_SET_EXTENDED_FILE_INFOW 3038
// #define IPC_WRITE_EXTENDED_FILE_INFOW 3039

// Structures (ANSI). Keep packing/layout identical to SDK.
typedef struct extendedFileInfoStruct_tag {
  const char *filename; // in
  const char *metadata; // in (e.g. "title","artist","album","comment","genre", etc.)
  char *ret;            // out buffer
  int   retlen;         // out buffer length (chars, including '\0')
} extendedFileInfoStruct;

typedef struct basicFileInfoStruct_tag {
  const char *filename;  // in
  int  quickCheck;       // in: 0=always get; 1=quick; 2=default (set to 0 if quick not used)
  // filled by winamp:
  int  length;           // out (ms)
  char *title;           // out (buffer provided by caller)
  int  titlelen;         // in: size of title buffer
} basicFileInfoStruct;

// (Optional) Wide-char variants, guarded to avoid breaking old SDKs.
#ifdef IPC_GET_EXTENDED_FILE_INFOW
typedef struct extendedFileInfoStructW_tag {
  const wchar_t *filename; // in
  const wchar_t *metadata; // in
  wchar_t *ret;            // out buffer
  int     retlen;          // out buffer length (wchar_t count, incl. L'\0')
} extendedFileInfoStructW;
#endif

// Minimal definition of winampGeneralPurposePlugin (gen.h)
typedef struct {
  int   version;
  char *description;
  int  (*init)   (void);
  void (*config) (void);
  void (*quit)   (void);
  HWND  hwndParent;
  HINSTANCE hDllInstance;
} winampGeneralPurposePlugin;

// Helper macros for Win9x/VC7.1 (no SetWindowLongPtr)
#ifndef GWLP_WNDPROC
  #define GWLP_WNDPROC GWL_WNDPROC
#endif
#ifndef SetWindowLongPtrA
  #define SetWindowLongPtrA(h,i,p) ((WNDPROC)SetWindowLongA((h),(i),(LONG)(p)))
#endif
#ifndef GetWindowLongPtrA
  #define GetWindowLongPtrA(h,i)   ((LONG_PTR)GetWindowLongA((h),(i)))
#endif

#endif // SDK_COMPAT_H
