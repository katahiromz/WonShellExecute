// WonZoneCheck.h
// Author: katahiromz
// License: MIT

#pragma once

#ifndef WONAPI
    #define WONAPI WINAPI
#endif

typedef HRESULT (WINAPI *FN_WonZoneCheckUrlExW)(
    PCWSTR  pszUrl,
    PBYTE   pbPolicy,
    DWORD   cbPolicy,
    const BYTE *pbContext,
    DWORD   cbContext,
    DWORD   dwAction,
    DWORD   dwFlags,
    IInternetSecurityMgrSite *pSecuritySite);

/*************************************************************************
 * ZoneCheckUrlExW [SHLWAPI.231]
 */
static inline HRESULT WINAPI
WonZoneCheckUrlExW(
    PCWSTR  pszUrl,
    PBYTE   pbPolicy,
    DWORD   cbPolicy,
    const BYTE *pbContext,
    DWORD   cbContext,
    DWORD   dwAction,
    DWORD   dwFlags,
    IInternetSecurityMgrSite *pSecuritySite)
{
    HINSTANCE hSHLWAPI = LoadLibraryW(L"shlwapi");
    if (!hSHLWAPI)
        return E_NOTIMPL;

    FARPROC fn = GetProcAddress(hSHLWAPI, MAKEINTRESOURCEA(231));
    if (!fn)
    {
        FreeLibrary(hSHLWAPI);
        return E_NOTIMPL;
    }

    FN_WonZoneCheckUrlExW fnZoneCheckUrlExW;
    CopyMemory(&fnZoneCheckUrlExW, &fn, sizeof(fn));

    HRESULT hr = fnZoneCheckUrlExW(pszUrl, pbPolicy, cbPolicy, pbContext, cbContext,
                                   dwAction, dwFlags, pSecuritySite);
    FreeLibrary(hSHLWAPI);
    return hr;
}
