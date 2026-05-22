// WonSafeOpen.h
// Author: katahiromz
// License: MIT
#pragma once

#ifndef WONAPI
    #define WONAPI WINAPI
#endif

typedef BOOL (WINAPI *FN_SafeOpenPromptForShellExec)(HWND hWnd, LPCWSTR pszPath);

static inline BOOL WONAPI
WonSafeOpenPromptForShellExec(HWND hWnd, LPCWSTR pszPath)
{
    HINSTANCE hSHDOCVW = LoadLibraryW(L"shdocvw.dll");
    if (!hSHDOCVW)
        return FALSE;

    FARPROC fn = GetProcAddress(hSHDOCVW, MAKEINTRESOURCEA(228));
    if (!fn)
    {
        FreeLibrary(hSHDOCVW);
        return FALSE;
    }

    FN_SafeOpenPromptForShellExec fnSafeOpenPromptForShellExec;
    CopyMemory(&fnSafeOpenPromptForShellExec, &fn, sizeof(fnSafeOpenPromptForShellExec));

    BOOL ret = fnSafeOpenPromptForShellExec(hWnd, pszPath);
    FreeLibrary(hSHDOCVW);
    return ret;
}
