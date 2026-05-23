// shlexec.cpp --- Shell execution emulation
// Author: katahiromz
// License: MIT

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PATH_EX 2084
#define NO_RUNAS
#define NO_ASSIST

#ifndef SEE_MASK_HASIDLIST
    #define SEE_MASK_HASIDLIST 0x40000
#endif
#ifndef SEE_MASK_UNKNOWN_0x1000
    #define SEE_MASK_UNKNOWN_0x1000 0x00001000
#endif
#ifndef SEE_MASK_NO_HOOKS
    #define SEE_MASK_NO_HOOKS 0x00002000
#endif
#ifndef SEE_MASK_HASLINKNAME
    #define SEE_MASK_HASLINKNAME 0x00010000
#endif
#ifndef SEE_MASK_FLAG_SEPVDM
    #define SEE_MASK_FLAG_SEPVDM 0x00020000
#endif
#ifndef SEE_MASK_USE_RESERVED
    #define SEE_MASK_USE_RESERVED 0x00040000
#endif
#ifndef SEE_MASK_HASTITLE
    #define SEE_MASK_HASTITLE 0x00080000
#endif
#ifndef SEE_MASK_FILEANDURL
    #define SEE_MASK_FILEANDURL 0x00400000
#endif
#ifndef SEE_MASK_ICON
    #define SEE_MASK_ICON 0x00000010
#endif

BOOL WINAPI WonShellExecuteExA(LPSHELLEXECUTEINFOA sei);
BOOL WINAPI WonShellExecuteExW(LPSHELLEXECUTEINFOW sei);

HINSTANCE WINAPI
WonShellExecuteW(
    HWND hwnd,
    LPCWSTR lpVerb,
    LPCWSTR lpFile,
    LPCWSTR lpParameters,
    LPCWSTR lpDirectory,
    INT nShowCmd);

HINSTANCE WINAPI
WonShellExecuteA(
    HWND hWnd,
    LPCSTR lpVerb,
    LPCSTR lpFile,
    LPCSTR lpParameters,
    LPCSTR lpDirectory,
    INT iShowCmd);

#ifdef UNICODE
    #define WonShellExecute WonShellExecuteW
    #define WonShellExecuteEx WonShellExecuteExW
#else
    #define WonShellExecute WonShellExecuteA
    #define WonShellExecuteEx WonShellExecuteExA
#endif

typedef HINSTANCE (CALLBACK *WOWSHELLEXECHOOKPROC)(LPCSTR pszCommand, INT nCmdShow, LPCSTR pszWorkDir);

HINSTANCE WINAPI
WonWOWShellExecute(
    HWND hWnd,
    LPCSTR lpVerb,
    LPCSTR lpFile,
    LPCSTR lpParameters,
    LPCSTR lpDirectory,
    INT iShowCmd,
    WOWSHELLEXECHOOKPROC callback);

HINSTANCE WINAPI
RealShellExecuteExA(
    _In_opt_ HWND hwnd,
    _In_opt_ LPCSTR lpOperation,
    _In_opt_ LPCSTR lpFile,
    _In_opt_ LPCSTR lpParameters,
    _In_opt_ LPCSTR lpDirectory,
    _In_opt_ LPSTR lpReturn,
    _In_opt_ LPCSTR lpTitle,
    _In_opt_ LPVOID lpReserved,
    _In_ INT nCmdShow,
    _Out_opt_ PHANDLE lphProcess,
    _In_ DWORD dwFlags);

HINSTANCE WINAPI
RealShellExecuteExW(
    _In_opt_ HWND hwnd,
    _In_opt_ LPCWSTR lpOperation,
    _In_opt_ LPCWSTR lpFile,
    _In_opt_ LPCWSTR lpParameters,
    _In_opt_ LPCWSTR lpDirectory,
    _In_opt_ LPWSTR lpReturn,
    _In_opt_ LPCWSTR lpTitle,
    _In_opt_ LPVOID lpReserved,
    _In_ INT nCmdShow,
    _Out_opt_ PHANDLE lphProcess,
    _In_ DWORD dwFlags);

#ifdef __cplusplus
} // extern "C"
#endif
