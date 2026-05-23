// utils.cpp --- Utility functions
// Author: katahiromz
// License: MIT
#include <initguid.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shlguid.h>
#include <strsafe.h>
#include <winsafer.h>
#include "shlexec.h"
#include "utils.h"
#include "debug.h"
#include "resource.h"

extern "C" {

extern HINSTANCE g_hinst;
BOOL g_bGotDisableMSI = FALSE;
HRESULT g_hrDisableIMI = S_FALSE;
CRITICAL_SECTION g_csDll;
BOOL g_bCurrentProcessIsConsole = FALSE;
INT g_bInDllProcess = -1;
#ifndef NO_ASSIST
IUserAssist *g_uempUa = NULL;
#endif

DEFINE_GUID(SCID_DESCRIPTIONID_GUID,       0x28636AA6, 0x953D, 0x11D2, 0x00B5, 0xD6, 0x00, 0xC0, 0x4F, 0xD9, 0x18, 0xD0);
DEFINE_GUID(GUID_NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
DEFINE_GUID(POLID_PreXPSP2ShellProtocolBehavior,    0x4FC60822, 0x47AF, 0x4BEE, 0xA7, 0xDA, 0xC9, 0x9A, 0x6E, 0x8E, 0x5D, 0x8D);
DEFINE_GUID(POLID_UsePathEnvVarForCommandTemplates, 0x1FF8F603, 0x056A, 0x4F52, 0x89, 0xCC, 0x82, 0xAD, 0x1C, 0xA1, 0xC1, 0x9A);

static SHCOLUMNID SCID_DESCRIPTIONID = { SCID_DESCRIPTIONID_GUID, 2 };

static HRESULT _CopyExe(
    PWSTR pszDest,
    UINT cchDest,
    const WCHAR* pszSrc,
    UINT cchSrc)
{
    *pszDest = UNICODE_NULL;

    HRESULT hr = StringCchCatNW(pszDest, cchDest, pszSrc, cchSrc);
    if (SUCCEEDED(hr))
    {
        StrTrimW(pszDest, L" \t");
    }
    return hr;
}

static BOOL PathIsAbsolute(LPCWSTR pszPath)
{
    if (!pszPath) return FALSE;

    return PathIsUNCW(pszPath) || 
           (PathGetDriveNumberW(pszPath) != -1 && lstrlenW(pszPath) > 2 && pszPath[2] == L'\\');
}

static BOOL _PathAppend(LPCWSTR pszBase, LPCWSTR pszSrc, PWSTR pszDest, size_t cchDest)
{
    if (SUCCEEDED(StringCchCopyW(pszDest, cchDest, pszBase)) && 
        SUCCEEDED(StringCchCatW(pszDest, cchDest, L"\\")))
    {
        return SUCCEEDED(StringCchCatW(pszDest, cchDest, pszSrc));
    }
    return FALSE;
}

void _MakeAppPathKey(LPCWSTR pszPath, PWSTR pszDest, unsigned int cchDest)
{
    static const WCHAR szAppPathsKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths";

    if (_PathAppend(szAppPathsKey, pszPath, pszDest, cchDest))
    {
        LPCWSTR pszExt = PathFindExtensionW(pszPath);
        if (pszExt && !*pszExt)
        {
            StrCatBuffW(pszDest, L".exe", cchDest);
        }
    }
}

BOOL _GetAppPath(LPCWSTR pszPath, PWSTR pvData, unsigned int cchData)
{
    WCHAR pszSubKey[MAX_PATH] = {};
    _MakeAppPathKey(pszPath, pszSubKey, _countof(pszSubKey));
    DWORD cbData = cchData * sizeof(WCHAR);
    return SHGetValueW(HKEY_LOCAL_MACHINE, pszSubKey, NULL, NULL, pvData, &cbData) == ERROR_SUCCESS;
}

#define WHICH_PIF       (1 << 0)
#define WHICH_COM       (1 << 1)
#define WHICH_EXE       (1 << 2)
#define WHICH_BAT       (1 << 3)
#define WHICH_LNK       (1 << 4)
#define WHICH_CMD       (1 << 5)
#define WHICH_OPTIONAL  (1 << 6)

#define WHICH_DEFAULT   (WHICH_PIF | WHICH_COM | WHICH_EXE | WHICH_BAT | WHICH_LNK | WHICH_CMD)

BOOL WINAPI PathFileExistsDefExtW(LPWSTR lpszPath,DWORD dwWhich)
{
  static const WCHAR pszExts[][5]  = { { '.', 'p', 'i', 'f', 0},
                                       { '.', 'c', 'o', 'm', 0},
                                       { '.', 'e', 'x', 'e', 0},
                                       { '.', 'b', 'a', 't', 0},
                                       { '.', 'l', 'n', 'k', 0},
                                       { '.', 'c', 'm', 'd', 0},
                                       { 0, 0, 0, 0, 0} };

  if (!lpszPath || PathIsUNCServerW(lpszPath) || PathIsUNCServerShareW(lpszPath))
    return FALSE;

  if (dwWhich)
  {
    LPCWSTR szExt = PathFindExtensionW(lpszPath);
    if (!*szExt || dwWhich & WHICH_OPTIONAL)
    {
      size_t iChoose = 0;
      int iLen = lstrlenW(lpszPath);
      if (iLen > (MAX_PATH - 5))
        return FALSE;
      while (pszExts[iChoose][0])
      {
        if (dwWhich & 0x1)
        {
        if (GetFileAttributes(lpszPath) != FILE_ATTRIBUTE_DIRECTORY)
        lstrcpyW(lpszPath + iLen, pszExts[iChoose]);
        if (PathFileExistsW(lpszPath))
          return TRUE;
        }
        iChoose++;
        dwWhich >>= 1;
      }
      *(lpszPath + iLen) = (WCHAR)'\0';
      return FALSE;
    }
  }
  return PathFileExistsW(lpszPath);
}

BOOL WINAPI PathFileExistsAndAttributesW(LPCWSTR lpszPath, DWORD *dwAttr)
{
  UINT iPrevErrMode;
  DWORD dwVal;

  if (!lpszPath)
    return FALSE;

  iPrevErrMode = SetErrorMode(SEM_FAILCRITICALERRORS);
  dwVal = GetFileAttributesW(lpszPath);
  SetErrorMode(iPrevErrMode);
  if (dwAttr)
    *dwAttr = dwVal;
  return (dwVal != INVALID_FILE_ATTRIBUTES);
}

BOOL WINAPI
PathFileExistsDefExtAndAttributesW(
    _Inout_ LPWSTR pszPath,
    _In_ DWORD dwWhich,
    _Out_opt_ LPDWORD pdwFileAttributes)
{
    if (pdwFileAttributes)
        *pdwFileAttributes = INVALID_FILE_ATTRIBUTES;

    if (!pszPath)
        return FALSE;

    if (!dwWhich || (*PathFindExtensionW(pszPath) && (dwWhich & WHICH_OPTIONAL)))
        return PathFileExistsAndAttributesW(pszPath, pdwFileAttributes);

    if (!PathFileExistsDefExtW(pszPath, dwWhich))
    {
        if (pdwFileAttributes)
            *pdwFileAttributes = INVALID_FILE_ATTRIBUTES;
        return FALSE;
    }

    if (pdwFileAttributes)
        *pdwFileAttributes = GetFileAttributesW(pszPath);

    return TRUE;
}

HRESULT _PathExeExists(LPWSTR pszPath)
{
    DWORD dwWhich = WHICH_PIF | WHICH_COM | WHICH_EXE | WHICH_BAT | WHICH_CMD | WHICH_OPTIONAL;
    if (!PathFileExistsDefExtAndAttributesW(pszPath, WHICH_OPTIONAL, &dwWhich))
        return CO_E_APPNOTFOUND;

    return S_OK;
}

HRESULT __stdcall _PathFindInFolder(int csidl, STRSAFE_LPCWSTR pszSrc, LPWSTR pszPath, size_t cchDest)
{
    HRESULT result; // eax

    result = SHGetFolderPathW(0, csidl, 0, 0, pszPath);
    if (result >= 0)
    {
        StringCchCatW(pszPath, cchDest, L"\\");
        result = StringCchCatW(pszPath, cchDest, pszSrc);
        if (result >= 0)
            return _PathExeExists(pszPath);
    }
    return result;
}

HRESULT _PathFindInSystem(PWSTR pszSrc, unsigned int cchSrc)
{
    WCHAR pszPath[MAX_PATH];
    HRESULT hr;

    hr = _PathFindInFolder(CSIDL_SYSTEM, pszSrc, pszPath, _countof(pszPath));
    if (SUCCEEDED(hr))
        return StringCchCopyW(pszSrc, cchSrc, pszPath);

    hr = _PathFindInFolder(CSIDL_SYSTEMX86, pszSrc, pszPath, _countof(pszPath));
    if (SUCCEEDED(hr))
        return StringCchCopyW(pszSrc, cchSrc, pszPath);

    return hr;
}

BOOL _PathMatchesSuspicious(LPCWSTR lpString)
{
    WCHAR pszPath[MAX_PATH] = {};
    INT cchString = lstrlenW(lpString);

    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES_COMMON, NULL, 0, pszPath)))
        return StrCmpNIW(lpString, pszPath, cchString) == 0;

    return FALSE;
}

HRESULT SHCoAlloc(SIZE_T cb, PVOID* pp)
{
    if (!pp) return E_POINTER;
    *pp = CoTaskMemAlloc(cb);
    return (*pp != NULL) ? S_OK : E_OUTOFMEMORY;
}

BOOL InRunDllProcess(VOID)
{
    WCHAR szModule[MAX_PATH];

    if (g_bInDllProcess == -1)
    {
        g_bInDllProcess = FALSE;
        if (GetModuleFileNameW(NULL, szModule, _countof(szModule)))
        {
            if (StrStrIW(szModule, L"rundll"))
                g_bInDllProcess = TRUE;
        }
    }

    return g_bInDllProcess;
}
    
// @implemented
BOOL _IsNamespaceObject(LPCWSTR pszpath)
{
    return memcmp(pszpath, L"::{", 3 * sizeof(WCHAR)) == 0;
}

// @implemented
BOOL _FailForceReturn(HRESULT hr)
{
    if (HRESULT_FACILITY(hr) != FACILITY_WIN32)
        return FALSE;
    switch (HRESULT_CODE(hr))
    {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_BAD_NETPATH:
        case ERROR_BAD_NET_NAME:
        case ERROR_CANCELLED:
            return TRUE;
        default:
            return FALSE;
    }
}

// @implemented
BOOL _IsLink(LPCWSTR pszPath, DWORD dwFileAttributes)
{
    SHFILEINFOW sfi;
    ZeroMemory(&sfi, sizeof(sfi));
    sfi.dwAttributes = SFGAO_LINK;

    UINT uFlags = SHGFI_ATTR_SPECIFIED | SHGFI_ATTRIBUTES;
    if (dwFileAttributes != INVALID_FILE_ATTRIBUTES)
        uFlags |= SHGFI_USEFILEATTRIBUTES;
    return SHGetFileInfoW(pszPath, dwFileAttributes, &sfi, sizeof(SHFILEINFOW), uFlags) &&
           !!(sfi.dwAttributes & SFGAO_LINK);
}

// @implemented
BOOL PathIsLnk(LPCWSTR pszPath)
{
    if (!pszPath)
        return FALSE;
    LPCWSTR ExtensionW = PathFindExtensionW(pszPath);
    return lstrcmpiW(L".lnk", ExtensionW) == 0;
}

// @implemented
BOOL PathIsShortcut(LPCWSTR pszPath, DWORD dwFileAttributes)
{
    if (dwFileAttributes != INVALID_FILE_ATTRIBUTES && (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        return _IsLink(pszPath, dwFileAttributes);
    if (PathIsLnk(pszPath))
        return TRUE;
    if (PathIsExe(pszPath))
        return FALSE;
    return _IsLink(pszPath, dwFileAttributes);
}

HRESULT InvokeShellExecuteHook(REFCLSID rclsid, SHELLEXECUTEINFOW* sei, HRESULT *phrResult)
{
    *phrResult = S_FALSE;
    return E_NOTIMPL; // FIXME
}

HRESULT TryShellExecuteHooks(LPSHELLEXECUTEINFOW sei)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellExecuteHooks",
                      0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        return S_FALSE;
    }

    HRESULT hr = S_FALSE;

    for (DWORD iValue = 0; ; ++iValue)
    {
        WCHAR ValueName[40];
        DWORD cchValueName = _countof(ValueName) - 1;

        if (RegEnumValueW(hKey, iValue, ValueName, &cchValueName,
                          NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
        {
            break;
        }

        CLSID clsid;
        if (FAILED(SHCLSIDFromString(ValueName, &clsid)))
            continue;

        HRESULT hrHook;
        if (!InvokeShellExecuteHook(clsid, sei, &hrHook) && hrHook != S_FALSE)
        {
            hr = hrHook;
            break;
        }
    }

    RegCloseKey(hKey);
    return hr;
}

typedef struct tagSHPOLICY_CONSTRAINT
{
    WORD wFlags;
    WORD wMinSize;
    DWORD dwMin;
    DWORD dwMax;
} SHPOLICY_CONSTRAINT, *PSHPOLICY_CONSTRAINT;

HRESULT SHPolicyGetValue(
    LPCWSTR pszKey1,
    LPCWSTR pszKey2,
    LPCWSTR pszValue,
    const SHPOLICY_CONSTRAINT *pConstraint,
    LPDWORD pdwType,
    LPVOID pvData,
    LPDWORD pcbData)
{
    WCHAR szSubKey[MAX_PATH];

    HRESULT hr = StringCchPrintfW(szSubKey, _countof(szSubKey), L"%s\\%s", pszKey1, pszKey2);
    if (FAILED(hr))
        return hr;

    DWORD srrf = pConstraint->wFlags;
    DWORD cbDataSaved = pcbData ? *pcbData : 0;

    LSTATUS error = SHRegGetValueW(HKEY_LOCAL_MACHINE, szSubKey, pszValue, srrf, pdwType, pvData, pcbData);
    if (error == ERROR_FILE_NOT_FOUND)
    {
        if (pcbData)
            *pcbData = cbDataSaved;

        error = SHRegGetValueW(HKEY_CURRENT_USER, szSubKey, pszValue, srrf, pdwType, pvData, pcbData);
    }

    if (error > 0)
        hr = HRESULT_FROM_WIN32(error);

    if (SUCCEEDED(hr) && pvData && 
        pConstraint->wFlags == SRRF_RT_DWORD && pConstraint->wMinSize == sizeof(DWORD))
    {
        DWORD dwValue = *(LPDWORD)pvData;
        if (dwValue < pConstraint->dwMin || dwValue > pConstraint->dwMax)
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }

    return hr;
}

BOOL _ProcessAllowsInvalidUrls(VOID)
{
    static const SHPOLICY_CONSTRAINT c_Bool = { SRRF_RT_DWORD, sizeof(DWORD), FALSE, TRUE };

    WCHAR szModule[MAX_PATH];
    if (!GetModuleFileNameW(NULL, szModule, _countof(szModule)))
        return FALSE;

    LPCWSTR pszFileName = PathFindFileNameW(szModule);
    if (!pszFileName || !*pszFileName)
        return FALSE;

    DWORD dwValue  = 0;
    DWORD cbData = sizeof(dwValue);
    HRESULT hr = SHPolicyGetValue(
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
        L"AllowShellExecHandleCIFFailure",
        pszFileName,
        &c_Bool,
        NULL,
        &dwValue,
        &cbData);

    if (FAILED(hr))
        return CheckForAppPathsBoolValue(pszFileName, L"AllowShellExecHandleCIFFailure");

    return dwValue == TRUE;
}

DWORD SHGetObjectCompatFlags(IUnknown *pUnk, const CLSID *clsid)
{
    return 0; // FIXME
}

// @implemented
DWORD
SHGetAttributes(_In_ IShellFolder* psf, _In_ LPCITEMIDLIST pidl, _In_ DWORD dwAttributes)
{
    LPCITEMIDLIST pidlLast = pidl;
    IShellFolder *release = NULL;

    if (!psf)
    {
        SHBindToParent(pidl, IID_IShellFolder, (PVOID*)&psf, &pidlLast);
        if (!psf)
            return 0;
        release = psf;
    }

    DWORD oldAttrs = dwAttributes;
    if (FAILED(psf->GetAttributesOf(1, &pidlLast, &dwAttributes)))
        dwAttributes = 0;
    else
        dwAttributes &= oldAttrs;

    if ((dwAttributes & SFGAO_FOLDER) &&
        (dwAttributes & SFGAO_STREAM) &&
        !(dwAttributes & SFGAO_STORAGEANCESTOR) &&
        (oldAttrs & SFGAO_STORAGEANCESTOR) &&
        (SHGetObjectCompatFlags(psf, NULL) & 0x200))
    {
        dwAttributes &= ~(SFGAO_STREAM | SFGAO_STORAGEANCESTOR);
        dwAttributes |= SFGAO_STORAGEANCESTOR;
    }

    if (release)
        release->Release();
    return dwAttributes;
}

INT GetUEMAssoc(LPCWSTR pszFile, LPCWSTR pszPath, LPCITEMIDLIST pidl)
{
    TRACE("\n");
    LPCWSTR pszFileExt = PathFindExtensionW(pszFile);
    if (!StrCmpICW(pszFileExt, L".exe"))
        return 1;

    LPCWSTR pszPathExt = PathFindExtensionW(pszPath);
    if (StrCmpCW(pszFileExt, pszPathExt) != 0)
        return 2;

    if (!pidl)
        return 0;

    IShellFolder *psf = NULL;
    LPCITEMIDLIST pidlChild = NULL;
    if (FAILED(WonSHBindToFolderIDListParent(NULL, pidl, IID_PPV_ARGS(&psf), &pidlChild)))
        return 0;

    INT ret = 0;
#ifndef SFGAO_CANMONIKER
    #define SFGAO_CANMONIKER 0x400000 // Obsolted
#endif
    if (SHGetAttributes(psf, pidlChild, SFGAO_FOLDER | SFGAO_CANMONIKER) == SFGAO_FOLDER)
        ret = 4;

    psf->Release();
    return ret;
}

HRESULT IsDarwinEnabled(VOID)
{
    static const LPCWSTR RestrictRunKey =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\RestrictRun";

    if (!g_bGotDisableMSI)
    {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RestrictRunKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            if (SHQueryValueExW(hKey, L"DisableMSI", NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
                g_hrDisableIMI = S_OK;
            RegCloseKey(hKey);
        }
        g_bGotDisableMSI = TRUE;
    }

    return g_hrDisableIMI;
}

DWORD CommandLineFromMsiDescriptor(LPWSTR Descriptor, LPWSTR CommandLine, DWORD *CommandLineLength)
{
    // FIXME
    return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD ParseDarwinID(LPWSTR Descriptor, LPWSTR CommandLine, DWORD CommandLineLength)
{
    DWORD error = CommandLineFromMsiDescriptor(Descriptor, CommandLine, &CommandLineLength);
    if (error)
        return HRESULT_FROM_WIN32(error);
    return S_OK;
}

BOOL DoesAppWantUrl(PCWSTR pszPath)
{
    WCHAR szSubKey[MAX_PATH];
    PathToAppPathKey(pszPath, szSubKey, _countof(szSubKey));
    DWORD error = SHGetValueW(HKEY_LOCAL_MACHINE, szSubKey, L"UseURL", NULL, NULL, NULL);
    if (error != ERROR_SUCCESS || !PathToAppPath(pszPath, szSubKey))
        return FALSE;
    return StrCmpIW(pszPath, szSubKey) == 0;
}

DEFINE_GUID(CLSID_UserAssist, 0xdd313e04, 0xfeff, 0x11d1, 0x8e, 0xcd, 0x00, 0x00, 0xf8, 0x7a, 0x47, 0x0c);

HMODULE SHPinDllOfCLSID(REFIID refiid)
{
    return NULL; // FIXME
}

#ifndef NO_ASSIST
IUserAssist* GetUserAssist(VOID)
{
    if (g_uempUa)
        return g_uempUa;

    const CLSCTX clsctx = staticIsOS(OS_NT5ORGREATER)
        ? (CLSCTX)(CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER)
        : (CLSCTX)(CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER);

    IUserAssist* pUA = NULL;
    CoCreateInstance(CLSID_UserAssist, NULL, clsctx, IID_PPV_ARGS(&pUA));

    if (pUA)
        SHPinDllOfCLSID(CLSID_UserAssist);

    EnterCriticalSection(&g_csDll);
    if (!g_uempUa)
    {
        g_uempUa = pUA;
        pUA = NULL;
    }
    LeaveCriticalSection(&g_csDll);

    if (pUA)
        pUA->Release();

    return g_uempUa;
}
#endif

HRESULT UEMFireEvent(
    REFGUID guid,
    INT     eCmd,
    ULONG   uFlags,
    WPARAM  wparam,
    LPARAM  lparam)
{
    TRACE("\n");
#ifdef NO_ASSIST
    return E_NOTIMPL;
#else
    IUserAssist *pua = GetUserAssist();
    if (!pua)
        return E_FAIL;

    return pua->FireEvent(&guid, eCmd, uFlags, wparam, lparam);
#endif
}

HRESULT DisplayNameOfW(IShellFolder *psf, LPCITEMIDLIST pidl, DWORD dwFlags, LPWSTR pszBuf, UINT cchBuf)
{
    STRRET strret;
    *pszBuf = UNICODE_NULL;
    HRESULT hr = psf->GetDisplayNameOf(pidl, dwFlags, &strret);
    if (SUCCEEDED(hr))
        return StrRetToBufW(&strret, pidl, pszBuf, cchBuf);
    return hr;
}

HRESULT
SHGetNameAndFlagsW(
    _In_ LPCITEMIDLIST pidl,
    _In_ DWORD dwFlags,
    _Out_opt_ LPWSTR pszText,
    _In_ UINT cchBuf,
    _Inout_opt_ DWORD *pdwAttributes)
{
    if (pszText)
        *pszText = UNICODE_NULL;

    HRESULT hrCoInit = SHCoInitialize();

    IShellFolder *psfFolder;
    LPCITEMIDLIST ppidlLast;
    HRESULT hr = SHBindToParent(pidl, IID_PPV_ARGS(&psfFolder), &ppidlLast);
    if (SUCCEEDED(hr))
    {
        if (pszText)
            hr = DisplayNameOfW(psfFolder, ppidlLast, dwFlags, pszText, cchBuf);

        if (SUCCEEDED(hr))
        {
            if (pdwAttributes)
                *pdwAttributes = SHGetAttributes(psfFolder, ppidlLast, *pdwAttributes);
        }

        psfFolder->Release();
    }

    if (SUCCEEDED(hrCoInit))
        CoUninitialize();

    return hr;
}

HRESULT PathToAppPathKeyBase(LPCWSTR pszBase, LPCWSTR pszPath,
                             LPWSTR pszDest, size_t cchDest)
{
    LPCWSTR pszFileName = PathFindFileNameW(pszPath);
    HRESULT hr = StringCchPrintfW(pszDest, cchDest, L"%s\\%s", pszBase, pszFileName);
    if (SUCCEEDED(hr) && !*PathFindExtensionW(pszDest))
        hr = StringCchCatW(pszDest, cchDest, L".exe");
    return hr;
}

HRESULT PathToAppPathKey(LPCWSTR pszPath, STRSAFE_LPWSTR pszDest, size_t cchDest)
{
    return PathToAppPathKeyBase(
        L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths",
        pszPath, pszDest, cchDest);
}

BOOL PathToAppPath(LPCWSTR pszPath, PVOID pvData)
{
    WCHAR szSubKey[MAX_PATH];
    DWORD cbData = sizeof(szSubKey);

    return SUCCEEDED(PathToAppPathKey(pszPath, szSubKey, _countof(szSubKey)))
        && SHRegGetValueW(HKEY_LOCAL_MACHINE, szSubKey, NULL,
                          SRRF_RT_REG_EXPAND_SZ, NULL, pvData, &cbData) == ERROR_SUCCESS;
}

DWORD CheckForAppPathsBoolValue(LPCWSTR pszPath, LPCWSTR pszValue)
{
    WCHAR szSubKey[MAX_PATH];
    DWORD dwValue = 0, cbData = sizeof(dwValue);
    if (SUCCEEDED(PathToAppPathKey(pszPath, szSubKey, _countof(szSubKey))))
        SHGetValueW(HKEY_LOCAL_MACHINE, szSubKey, pszValue, 0, &dwValue, &cbData);
    return dwValue;
}

BOOL UEMIsLoaded(void)
{
    return GetModuleHandleW(L"ole32.dll") && GetModuleHandleW(L"browseui.dll");
}

typedef HRESULT(WINAPI *FN_SHWindowsPolicyGetValue)(REFGUID, PVOID, PDWORD);

HRESULT
SHWindowsPolicyGetValue(
    _In_ REFGUID rpolid,
    _Out_opt_ PVOID pvData,
    _Out_opt_ PDWORD pcbData)
{
    HINSTANCE hSHLWAPI = GetModuleHandleA("shlwapi");
    FARPROC fn = GetProcAddress(hSHLWAPI, MAKEINTRESOURCEA(560));
    FN_SHWindowsPolicyGetValue pSHWindowsPolicyGetValue;
    CopyMemory(&pSHWindowsPolicyGetValue, &fn, sizeof(fn));
    return pSHWindowsPolicyGetValue(rpolid, pvData, pcbData);
}

INT SHWindowsPolicy(REFGUID rguid, INT nDefault)
{
    INT value   = 0;
    DWORD cbData = sizeof(value);

    if (FAILED(SHWindowsPolicyGetValue(rguid, &value, &cbData)))
        return nDefault;

    return value;
}

HRESULT SHBindToIDListParent(LPCITEMIDLIST pidl, REFIID riid, PVOID *ppv, LPCITEMIDLIST *ppidlLast)
{
    return WonSHBindToFolderIDListParent(0, pidl, riid, ppv, ppidlLast);
}

HRESULT SHGetUIObjectOf(LPCITEMIDLIST pidl, HWND hWnd, REFIID riid, PVOID *ppv)
{
    IShellFolder *pFolder;
    LPCITEMIDLIST pidlChild;
    HRESULT hr = SHBindToIDListParent(pidl, IID_PPV_ARGS(&pFolder), &pidlChild);
    if (SUCCEEDED(hr))
    {
        hr = pFolder->GetUIObjectOf(hWnd, 1, &pidlChild, riid, NULL, ppv);
        pFolder->Release();
    }
    return hr;
}

HRESULT SHGetAssociations(LPCITEMIDLIST pidl, IQueryAssociations **ppQueryAssoc)
{
    return SHGetUIObjectOf(pidl, 0, IID_IQueryAssociations, (PVOID*)ppQueryAssoc);
}

HRESULT SHGetUIObjectFromFullPIDL(
    LPCITEMIDLIST   pidl,
    HWND            hWnd,
    REFIID          riid,
    PVOID          *ppv)
{
    IShellFolder  *pFolder   = NULL;
    LPCITEMIDLIST  pidlChild = NULL;

    HRESULT hr = SHBindToIDListParent(pidl, IID_PPV_ARGS(&pFolder), &pidlChild);
    if (SUCCEEDED(hr))
    {
        hr = pFolder->GetUIObjectOf(hWnd, 1, &pidlChild, riid, NULL, ppv);
        pFolder->Release();
    }

    return hr;
}

HRESULT _InvokeInProcExec(IContextMenu *pContextMenu, LPSHELLEXECUTEINFOW sei)
{
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu)
        return E_OUTOFMEMORY;

    CMINVOKECOMMANDINFOEX icix = {};
    HLOCAL hLocal;
    if (FAILED(SEI2ICIX(sei, &icix, &hLocal)))
    {
        DestroyMenu(hMenu);
        return E_OUTOFMEMORY;
    }

    const BOOL bNoVerb = !icix.lpVerb || !*icix.lpVerb;
    icix.fMask |= CMIC_MASK_FLAG_NO_UI;

    HRESULT hr = pContextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF,
                                                (bNoVerb ? CMF_DEFAULTONLY : CMF_NORMAL));
    if (SUCCEEDED(hr))
    {
        if (bNoVerb)
        {
            UINT iDefItem = GetMenuDefaultItem(hMenu, FALSE, 0);
            icix.lpVerb = (iDefItem == (UINT)-1) ? NULL : (LPCSTR)UlongToPtr(iDefItem - 1);
        }
        hr = pContextMenu->InvokeCommand((CMINVOKECOMMANDINFO*)&icix);
    }

    if (hLocal)
        LocalFree(hLocal);

    DestroyMenu(hMenu);
    return hr;
}

PSTR ThunkStrToAnsi(PCWSTR pwszSrc, PSTR pszDst, INT cchBuf)
{
    if (!pwszSrc)
        return NULL;
    SHUnicodeToAnsi(pwszSrc, pszDst, cchBuf);
    return pszDst;
}

BOOL GetMonitorRects(HMONITOR hMonitor, LPRECT prc, BOOL bWorkArea)
{
    MONITORINFO mi = { sizeof(mi) };
    if (hMonitor && GetMonitorInfoW(hMonitor, &mi))
    {
        if (prc)
            *prc = bWorkArea ? mi.rcWork : mi.rcMonitor;
        return TRUE;
    }

    if (prc)
        SetRect(prc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    return FALSE;
}

// Convert SHELLEXECUTEINFOW to CMINVOKECOMMANDINFOEX
HRESULT SEI2ICIX(
    const SHELLEXECUTEINFOW *pSEI,
    CMINVOKECOMMANDINFOEX   *pICIX,
    HLOCAL                  *phLocal)
{
    *phLocal = NULL;
    *pICIX = {};

    const DWORD c_fMask = SEE_MASK_ICON | SEE_MASK_HOTKEY | SEE_MASK_NOCLOSEPROCESS |
                          SEE_MASK_CONNECTNETDRV | SEE_MASK_NOASYNC | SEE_MASK_DOENVSUBST |
                          SEE_MASK_FLAG_NO_UI | 0x800 | SEE_MASK_NO_CONSOLE | SEE_MASK_NO_HOOKS |
                          SEE_MASK_HASLINKNAME | SEE_MASK_FLAG_SEPVDM |
                          SEE_MASK_USE_RESERVED | SEE_MASK_HASTITLE |
                          SEE_MASK_NOZONECHECKS | SEE_MASK_FLAG_LOG_USAGE |
                          CMIC_MASK_SHIFT_DOWN | CMIC_MASK_PTINVOKE;
    ASSERT(c_fMask == 0x348FAFF0);

    pICIX->cbSize    = sizeof(CMINVOKECOMMANDINFOEX);
    pICIX->fMask     = (pSEI->fMask & c_fMask);
    pICIX->hwnd      = pSEI->hwnd;
    pICIX->nShow     = pSEI->nShow;
    pICIX->dwHotKey  = pSEI->dwHotKey;
    pICIX->lpVerbW       = pSEI->lpVerb;
    pICIX->lpParametersW = pSEI->lpParameters;
    pICIX->lpDirectoryW  = pSEI->lpDirectory;

    if (pSEI->fMask & SEE_MASK_HMONITOR)
    {
        HMONITOR hMonitor = (HMONITOR)pSEI->hIcon;
        RECT rcMonitor;
        if (hMonitor && GetMonitorRects(hMonitor, &rcMonitor, FALSE))
        {
            pICIX->fMask |= CMIC_MASK_PTINVOKE;
            pICIX->ptInvoke = { rcMonitor.left, rcMonitor.top };
        }
    }
    else
    {
        pICIX->hIcon = pSEI->hIcon;
    }

    if (pSEI->fMask & (SEE_MASK_CLASSNAME | SEE_MASK_CLASSKEY))
        pICIX->lpTitleW = pSEI->lpClass;

    const INT cbVerb   = WideCharToMultiByte(CP_ACP, 0, pSEI->lpVerb,       -1, NULL, 0, NULL, NULL);
    const INT cbParams = WideCharToMultiByte(CP_ACP, 0, pSEI->lpParameters, -1, NULL, 0, NULL, NULL);
    const INT cbDir    = WideCharToMultiByte(CP_ACP, 0, pSEI->lpDirectory,  -1, NULL, 0, NULL, NULL);

    HRESULT hr = S_OK;
    const SIZE_T cbTotal = cbVerb + cbParams + cbDir;
    if (cbTotal)
    {
        phLocal = (HLOCAL*)LocalAlloc(LPTR, cbTotal);
        if (phLocal)
        {
            PCHAR pBuf = (PCHAR)phLocal;
            pICIX->lpVerb      = ThunkStrToAnsi(pSEI->lpVerb,       pBuf,              cbVerb);
            pICIX->lpParameters= ThunkStrToAnsi(pSEI->lpParameters, pBuf + cbVerb,     cbParams);
            pICIX->lpDirectory = ThunkStrToAnsi(pSEI->lpDirectory,  pBuf + cbVerb + cbParams, cbDir);
        }
    }

    pICIX->fMask |= CMIC_MASK_UNICODE;
    return hr;
}

BOOL _PathIsFile(LPCWSTR lpFileName)
{
    DWORD attrs = GetFileAttributesW(lpFileName);
    return INVALID_FILE_ATTRIBUTES != -1 && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

INT CheckForInstallApplication(LPCWSTR pszPath, LPCWSTR unused)
{
    return 0; // FIXME
}

HANDLE _GetSandboxToken(VOID)
{
    SAFER_LEVEL_HANDLE pLevelHandle;
    HANDLE hToken = NULL;
    if (SaferCreateLevel(SAFER_SCOPEID_MACHINE, SAFER_LEVELID_CONSTRAINED, SAFER_LEVEL_OPEN, &pLevelHandle, NULL))
    {
        if (!SaferComputeTokenFromLevel(pLevelHandle, NULL, &hToken, 0, NULL))
            hToken = NULL;
        SaferCloseLevel(pLevelHandle);
    }
    return hToken;
}

BOOL _SHCreateProcess(
    HWND                  hWnd,
    HANDLE                hUserToken,
    LPCWSTR               pszFriendlyAppName,
    LPCWSTR               pszWorkGroupHelper,
    LPWSTR                pszCommand,
    DWORD                 dwCreationFlags,
    LPSECURITY_ATTRIBUTES lpProcessAttribute,
    LPSECURITY_ATTRIBUTES lpThreadAttribute,
    BOOL                  bInheritHandles,
    LPWSTR                pszzEnvBlock,
    LPCWSTR               pszWorkDir,
    LPSTARTUPINFOW        psi,
    PROCESS_INFORMATION  *ppi,
    UINT                  nRunAs,
    BOOL                  bLogUsage)
{
    WCHAR szDest    [MAX_PATH] = {};
    WCHAR szUserName[126] = {};
    WCHAR szDomain  [126] = {};
    WCHAR szPassword[MAX_PATH] = {};
    WCHAR szBuffer  [MAX_PATH] = {};

    LPCWSTR pszPath = pszWorkGroupHelper;

    if (!nRunAs)
        nRunAs = CheckForInstallApplication(pszWorkGroupHelper, szDest);

#ifndef NO_RUNAS
    if ((nRunAs == 4 || nRunAs == 5) && pszFriendlyAppName)
    {
        StringCchCopyW(szDest, _countof(szDest), pszFriendlyAppName);
        goto retry_logon;
    }
#endif

    for (;;)
    {
        BOOL  bSuccess          = FALSE;
        DWORD dwLastError       = 0;

        if (PathMatchSpecW(PathFindFileNameW(pszPath), L"*.CMD;*.BAT"))
            pszPath = NULL;

        switch (nRunAs)
        {
            case 0:
#ifdef NO_RUNAS
            default:
#endif
                bSuccess = CreateProcessW(pszPath, pszCommand, lpProcessAttribute,
                                          lpThreadAttribute, bInheritHandles, dwCreationFlags,
                                          pszzEnvBlock, pszWorkDir, psi, ppi);
                break;

#ifndef NO_RUNAS
            case 1:
                if (hUserToken)
                {
                    bSuccess = CreateProcessAsUserW(hUserToken, pszPath, pszCommand,
                                                    lpProcessAttribute, lpThreadAttribute,
                                                    bInheritHandles,
                                                    (dwCreationFlags |
                                                     CREATE_PRESERVE_CODE_AUTHZ_LEVEL),
                                                    pszzEnvBlock, pszWorkDir, psi, ppi);
                }
                else
                {
                    bSuccess = CreateProcessW(pszPath, pszCommand, lpProcessAttribute,
                                              lpThreadAttribute, bInheritHandles,
                                              (dwCreationFlags |
                                               CREATE_PRESERVE_CODE_AUTHZ_LEVEL),
                                              pszzEnvBlock, pszWorkDir, psi, ppi);
                }
                break;

            case 2:
                if (HANDLE hSandboxToken = _GetSandboxToken())
                {
                    bSuccess = CreateProcessAsUserW(hSandboxToken, pszPath, pszCommand,
                                                    lpProcessAttribute, lpThreadAttribute,
                                                    bInheritHandles, dwCreationFlags,
                                                    pszzEnvBlock, pszWorkDir, psi, ppi);
                    CloseHandle(hSandboxToken);
                }
                break;

            case 3:
            {
                HRESULT hrTS = InstallOnTerminalServerWithUIDD(
                    hWnd,
                    pszPath, pszCommand,
                    lpProcessAttribute, lpThreadAttribute,
                    bInheritHandles, dwCreationFlags,
                    pszzEnvBlock, pszWorkDir, psi, ppi);

                bSuccess    = SUCCEEDED(hrTS);
                dwLastError = (HRESULT_FACILITY(hrTS) == FACILITY_WIN32)
                            ? HRESULT_CODE(hrTS) : ERROR_ACCESS_DENIED;
                break;
            }

            case 4:
            case 5:
            {
                WCHAR *pszSavedDesktop = psi->lpDesktop;
                szUserName[_countof(szUserName) - 1] = L'\0';
                szDomain  [_countof(szDomain)   - 1] = L'\0';

                bSuccess = CreateProcessWithLogonW(
                    szUserName, szDomain, szPassword,
                    LOGON_WITH_PROFILE,
                    pszPath, pszCommand,
                    dwCreationFlags, NULL,
                    pszWorkDir, psi, ppi);

                if (bSuccess)
                    goto done;

                psi->lpDesktop = pszSavedDesktop;
                dwLastError    = GetLastError();

                if (_IsLogonError(dwLastError))
                {
                    LoadStringW(g_hinst, 6442, szBuffer, _countof(szBuffer));
                    SHSysErrorMessageBox(hWnd, szDest, IDS_TWOARGS, dwLastError, szBuffer, MB_ICONERROR);
retry_logon:
                    nRunAs = _LogonUser(hWnd, nRunAs, szDest);
                    continue;
                }
                break;
            }

            case 6:
                SetLastError(ERROR_CANCELLED);
                goto cleanup;

            default:
                goto cleanup;
#endif
        }

        if (bSuccess)
            goto done;

        if (dwLastError)
            SetLastError(dwLastError);

        goto cleanup;

done:
        if (bLogUsage && UEMIsLoaded())
            UEMFireEvent(CLSID_ActiveDesktop, 15, 3, -1, (LPARAM)pszPath);

cleanup:
        SecureZeroMemory(szDest,     sizeof(szDest));
        SecureZeroMemory(szUserName, sizeof(szUserName));
        SecureZeroMemory(szDomain,   sizeof(szDomain));
        SecureZeroMemory(szPassword, sizeof(szPassword));

        return bSuccess;
    }
}

BOOL VariantToBuffer(const VARIANT* const pVarIn, void* pvDataOut, UINT cbToCopy)
{
    if (!pVarIn || !pvDataOut)
        return FALSE;

    if (pVarIn->vt != (VT_ARRAY | VT_UI1) || !pVarIn->parray)
        return FALSE;

    if (SafeArrayGetDim(pVarIn->parray) != 1)
        return FALSE;

    LONG lLbound = 0, lUbound = 0;
    if (FAILED(SafeArrayGetLBound(pVarIn->parray, 1, &lLbound)) ||
        FAILED(SafeArrayGetUBound(pVarIn->parray, 1, &lUbound)))
    {
        return FALSE;
    }

    ULONG cElements = (lUbound >= lLbound) ? (ULONG)(lUbound - lLbound + 1) : 0;
    if (cElements < cbToCopy)
        return FALSE;

    PVOID ppvData = NULL;
    if (FAILED(SafeArrayAccessData(pVarIn->parray, &ppvData)))
        return FALSE;

    CopyMemory(pvDataOut, ppvData, cbToCopy);

    SafeArrayUnaccessData(pVarIn->parray);
    return TRUE;
}

BOOL IsFolderGUID(IShellFolder *psfRoot, LPCITEMIDLIST pidl)
{
    constexpr DWORD SFGAO_FOLDER_FLAGS = SFGAO_FOLDER | SFGAO_HASSUBFOLDER;

    BOOL          bResult  = FALSE;
    IShellFolder *pFolder  = NULL;
    LPCITEMIDLIST ppidlLast = NULL;

    if (FAILED(WonSHBindToFolderIDListParent(psfRoot, pidl, IID_PPV_ARGS(&pFolder), &ppidlLast)))
        return FALSE;

    if (SHGetAttributes(pFolder, ppidlLast, SFGAO_FOLDER_FLAGS) == SFGAO_FOLDER_FLAGS)
    {
        bResult = TRUE;

        IShellFolder2 *pFolder2 = NULL;
        if (SUCCEEDED(pFolder->QueryInterface(IID_PPV_ARGS(&pFolder2))))
        {
            VARIANT varIn = {};
            if (SUCCEEDED(pFolder2->GetDetailsEx(ppidlLast, &SCID_DESCRIPTIONID, &varIn)))
            {
                SHDESCRIPTIONID descId = {};
                if (VariantToBuffer(&varIn, &descId, sizeof(descId)))
                    bResult = !IsEqualGUID(descId.clsid, GUID_NULL);

                VariantClear(&varIn);
            }
            pFolder2->Release();
        }
    }

    pFolder->Release();
    return bResult;
}

BOOL IsNameListedUnderKey(PCWSTR pszFileName, PCWSTR pszSubKey)
{
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, pszSubKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return FALSE;

    WCHAR szValueName[MAX_PATH], szData[MAX_PATH];

    for (DWORD dwIndex = 0; ; ++dwIndex)
    {
        DWORD cchValueName = _countof(szValueName);
        DWORD cbData       = sizeof(szData);
        DWORD dwType       = 0;

        LSTATUS status = RegEnumValueW(hKey, dwIndex,
                                       szValueName, &cchValueName, NULL, &dwType,
                                       (PBYTE)szData, &cbData);

        if (status != ERROR_SUCCESS)
            break;

        if (lstrcmpiW(szData, pszFileName) == 0)
        {
            RegCloseKey(hKey);
            return TRUE;
        }
    }

    RegCloseKey(hKey);
    return FALSE;
}

BOOL RestrictedApp(LPCWSTR pszPath)
{
    static const LPCWSTR RestrictRunKey =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\RestrictRun";
    LPCWSTR FileName = PathFindFileNameW(pszPath);
    return lstrcmpiW(FileName, L"rundll32.exe") &&
           lstrcmpiW(FileName, L"systray.exe") &&
           IsNameListedUnderKey(FileName, RestrictRunKey) == 0;
}

BOOL DisallowedApp(LPCWSTR pszPath)
{
    static const LPCWSTR DisallowRun =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\DisallowRun";
    LPWSTR FileName = PathFindFileNameW(pszPath);
    return IsNameListedUnderKey(FileName, DisallowRun);
}

HRESULT TBCRegisterObjectParam(LPCWSTR pszKey, IUnknown *pUnk, LPBC *ppbc)
{
#if 1
    return E_FAIL;
#else
    *ppbc = NULL;

    IBindCtx *pbc = NULL;
    HRESULT hr = TBCGetBindCtx(1, &pbc);
    if (FAILED(hr))
        return hr;

    hr = BindCtx_RegisterObjectParam(pbc, pszKey, pUnk, ppbc);
    pbc->Release();

    return hr;
#endif
}

HRESULT TBCGetBindCtx(BOOL bCreate, IBindCtx **ppBC)
{
    return E_NOTIMPL; // FIXME
}

HRESULT TBCGetObjectParam(OLECHAR *pszKey, REFIID riid, PVOID* ppv)
{
    IBindCtx *pbc;
    HRESULT hr = TBCGetBindCtx(0, &pbc);
    if (FAILED(hr))
        return hr;

    IUnknown *pUnk;
    hr = pbc->GetObjectParam(pszKey, &pUnk);
    if (SUCCEEDED(hr))
    {
        if (ppv)
            hr = pUnk->QueryInterface(riid, ppv);

        pUnk->Release();
    }

    pbc->Release();
    return hr;
}

HRESULT WINAPI SHPropertyBag_ReadStr(IPropertyBag *ppb, LPCWSTR pszPropName, LPWSTR pszDst, int cchMax)
{
    return E_FAIL; // FIXME
}

HRESULT TBCGetEnvironmentVariable(LPCWSTR pszName, LPWSTR pszDest, INT cchDest)
{
    IPropertyBag *pPB;
    HRESULT hr = TBCGetObjectParam(L"ThreadEnvironmentVariables", IID_IPropertyBag, (PVOID*)&pPB);
    if (SUCCEEDED(hr))
    {
        hr = SHPropertyBag_ReadStr(pPB, pszName, pszDest, cchDest);
        pPB->Release();
    }
    return hr;
}

INT _VarArgsFormatMessage(LPWSTR lpBuffer, DWORD nSize, DWORD dwMessageId, ...)
{
    va_list args;
    va_start(args, dwMessageId);
    INT result = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        dwMessageId,
        0,
        lpBuffer,
        nSize,
        &args);
    va_end(args);
    return result;
}

BOOL _LoadErrMsg(UINT uID, STRSAFE_LPWSTR pszDest, size_t cchDest, DWORD dwError)
{
    WCHAR Buffer[MAX_PATH];
    if (!LoadStringW(g_hinst, uID, Buffer, _countof(Buffer)))
        return FALSE;
    StringCchPrintfW(pszDest, cchDest, Buffer, dwError);
    return TRUE;
}

INT SHSysErrorMessageBox(
    HWND   hWnd,
    LPCWSTR pszTitle,
    UINT   nTextID,
    DWORD  dwMessageId,
    PCWSTR pszSearch,
    UINT   uType)
{
    WCHAR szBuffer[2 * MAX_PATH] = {};

    BOOL bFormatted = (dwMessageId == ERROR_MR_MID_NOT_FOUND)
        ? _VarArgsFormatMessage(szBuffer, _countof(szBuffer), ERROR_MR_MID_NOT_FOUND, L"", pszSearch)
        : _VarArgsFormatMessage(szBuffer, _countof(szBuffer), dwMessageId, pszSearch, L"", L"", L"", L"", L"");

    if (!bFormatted && !_LoadErrMsg(IDS_FILESYSTEMERROR, szBuffer, _countof(szBuffer), dwMessageId))
        return IDCANCEL;

    if (nTextID == IDS_TWOARGS && (!pszSearch || StrStrW(szBuffer, pszSearch)))
        nTextID = IDS_ONEARG;

    return WonShellMessageBoxWrapW(g_hinst, hWnd, MAKEINTRESOURCEW(nTextID), pszTitle, uType, szBuffer);
}

PCWSTR _GetNextParm(PCWSTR pszStart, PWSTR pszDst, unsigned int cchDstMax)
{
    if (!pszStart || !*pszStart)
        return NULL;

    PWSTR pDstEnd = pszDst ? &pszDst[cchDstMax - 1] : NULL;

    while (*pszStart == L' ')
        pszStart++;

    if (!*pszStart)
        return NULL;

    BOOL isQuoted = (*pszStart == L'"');
    if (isQuoted)
        pszStart++;

    for (;;)
    {
        PCWSTR pNextQuote = StrChrW(pszStart, L'"');

        if (isQuoted)
        {
            if (!pNextQuote)
                return NULL;
        }
        else
        {
            PCWSTR pNextSpace = StrChrW(pszStart, L' ');

            if (pNextSpace)
            {
                if (!pNextQuote || pNextSpace < pNextQuote)
                {
                    if (pszDst)
                    {
                        size_t cchCopy = pNextSpace - pszStart;
                        if ((size_t)(pDstEnd - pszDst) < cchCopy)
                        {
                            return NULL;
                        }
                        StrCpyNW(pszDst, pszStart, (INT)(cchCopy + 1));
                    }
                    return pNextSpace;
                }
            }
            else if (!pNextQuote)
            {
                if (pszDst)
                {
                    size_t cchCopy = lstrlenW(pszStart);
                    if ((size_t)(pDstEnd - pszDst) >= cchCopy)
                    {
                        StrCpyNW(pszDst, pszStart, (INT)(cchCopy + 1));
                    }
                }
                return NULL;
            }
        }

        BOOL isEscapedQuote = (pNextQuote[1] == L'"');

        if (pszDst)
        {
            size_t cchCopy = pNextQuote - pszStart;
            if (isEscapedQuote)
                cchCopy++;

            if ((size_t)(pDstEnd - pszDst) < cchCopy)
                return NULL;

            StrCpyNW(pszDst, pszStart, (INT)(cchCopy + 1));
            pszDst += cchCopy;
        }

        if (isEscapedQuote)
        {
            pszStart = pNextQuote + 2;
        }
        else
        {
            return pNextQuote + 1;
        }
    }
}

// @implemented
INT ReplaceParameters(
    LPWSTR        pszDest,
    INT           cchDest,
    LPCWSTR       pszPath,
    LPCWSTR       pszDdeCommand,
    LPCWSTR       pszParameters,
    INT           nCmdShow,
    HANDLE       *phToken,
    BOOL          bLongNameOK,
    LPCITEMIDLIST pidl,
    PHANDLE       phDdeData)
{
    WCHAR szPath[MAX_PATH] = {};
    PWCHAR pDest = pszDest;
    LPCWSTR pEnd = pszDest + cchDest - 1;
    LPCWSTR pCmd = pszDdeCommand;

    if (!*pCmd)
    {
        *pDest = UNICODE_NULL;
        return ERROR_SUCCESS;
    }

#define CAN_WRITE(cch)  ((pEnd - pDest) >= (cch))
#define APPEND_PATH(psz)                                    \
    do {                                                    \
        INT _cch = lstrlenW(psz);                           \
        if (!CAN_WRITE(_cch + 1)) return ERROR_INSUFFICIENT_BUFFER; \
        StrCpyNW(pDest, (psz), _cch + 1);                  \
        pDest += _cch;                                      \
    } while (0)

    while (*pCmd)
    {
        if (*pCmd != L'%')
        {
            if (!CAN_WRITE(2))
                return ERROR_INSUFFICIENT_BUFFER;
            *pDest++ = *pCmd++;
            continue;
        }

        const WCHAR ch = *++pCmd;

        switch (ch)
        {
            case L'*':
                // %* : All parameters
                if (!pszParameters)
                    break;
                APPEND_PATH(pszParameters);
                break;

            case L'0':
            case L'1':
            case L'L':
            case L'l':
                // %1 / %l : Long path
                APPEND_PATH(pszPath);
                break;

            case L'D':
            case L'd':
                // %d : Getting path from PIDL
                if (!pidl || FAILED(SHGetNameAndFlagsW(pidl, SHGDN_FORPARSING, szPath, MAX_PATH, NULL)))
                    return ERROR_FILE_NOT_FOUND;
                APPEND_PATH(szPath);
                break;

            case L'H':
            case L'h':
            {
                // %h : Value in hexadecimal
                HANDLE hVal = phToken ? *phToken : NULL;
                wnsprintfW(szPath, MAX_PATH, L"%X", (ULONG_PTR)hVal);
                INT cch = lstrlenW(szPath);
                if (!CAN_WRITE(cch + 1))
                    return ERROR_INSUFFICIENT_BUFFER;
                StrCpyNW(pDest, szPath, cch + 1);
                pDest += cch;
                if (phToken)
                    *phToken = NULL;
                break;
            }

            case L'I':
            case L'i':
            {
                // %i : Store the PIDL in shared memory and output it in ":handle:pid" format
                if (!phDdeData)
                {
                    StrCpyNW(szPath, L":0", MAX_PATH);
                    APPEND_PATH(szPath);
                    break;
                }
                if (pidl && !*phDdeData)
                {
                    const DWORD cbPidl = ILGetSize(pidl);
                    *phDdeData = SHAllocShared(pidl, cbPidl, GetCurrentProcessId());
                    if (!*phDdeData)
                        return ERROR_OUTOFMEMORY;
                }
                wnsprintfW(szPath, MAX_PATH, L":%ld:%ld", HandleToUlong(*phDdeData), GetCurrentProcessId());
                APPEND_PATH(szPath);
                break;
            }

            case L'S':
            case L's':
                // %s : Output nCmdShow in decimal
                wnsprintfW(szPath, MAX_PATH, L"%ld", nCmdShow);
                APPEND_PATH(szPath);
                break;

            case L'2': case L'3': case L'4': case L'5':
            case L'6': case L'7': case L'8': case L'9':
            {
                // %2 ～ %9 : Nth parameter
                INT nParam       = ch - L'0';
                LPCWSTR pszParam = pszParameters;
                for (INT n = 2; n < nParam && pszParam; ++n)
                    pszParam = _GetNextParm(pszParam, NULL, 0);

                if (pszParam)
                {
                    _GetNextParm(pszParam, szPath, MAX_PATH);
                    APPEND_PATH(szPath);
                }
                break;
            }

            case L'~':
            {
                // %~2 ～ %~9 : All parameters from the Nth onward
                const WCHAR chNext = *++pCmd;
                if (chNext < L'2' || chNext > L'9')
                {
                    pCmd -= 2;
                    if (!CAN_WRITE(2))
                        return ERROR_INSUFFICIENT_BUFFER;
                    *pDest++ = *pCmd++;
                    continue;
                }
                INT nParam       = chNext - L'0';
                LPCWSTR pszParam = pszParameters;
                for (INT n = 2; n < nParam && pszParam; ++n)
                    pszParam = _GetNextParm(pszParam, NULL, 0);

                if (pszParam)
                {
                    INT cch = lstrlenW(pszParam);
                    if (!CAN_WRITE(cch + 1))
                        return ERROR_INSUFFICIENT_BUFFER;
                    StrCpyNW(pDest, pszParam, cch + 1);
                    pDest += cch;
                }
                break;
            }

            default:
                // Unknown escape
                --pCmd;
                if (!CAN_WRITE(2))
                    return ERROR_INSUFFICIENT_BUFFER;
                *pDest++ = *pCmd++;
                continue;
        }

        ++pCmd;
    }

#undef APPEND_PATH
#undef CAN_WRITE

    *pDest = UNICODE_NULL;
    return ERROR_SUCCESS;
}

enum ExtensionTag : DWORD
{
    EXT_NONE = 0,
    EXT_DOT  = 0x0000002E,  // '.' only
    EXT_COM  = 0x6D6F632E,  // ".com"
    EXT_BAT  = 0x7461622E,  // ".bat"
    EXT_CMD  = 0x646D632E,  // ".cmd"
    EXT_PIF  = 0x6669702E,  // ".pif"
    EXT_LNK  = 0x6B6E6C2E,  // ".lnk"
    EXT_ICO  = 0x6F63692E,  // ".ico"
    EXT_EXE  = 0x6578652E,  // ".exe"
};

ExtensionTag HasExtension(LPCWSTR pszPath)
{
    LPCWSTR pszExt = PathFindExtensionW(pszPath);

    if (*pszExt != L'.')
        return EXT_NONE;

    WCHAR szExt[5];
    lstrcpynW(szExt, pszExt, _countof(szExt));

    if (!lstrcmpiW(szExt, L".com")) return EXT_COM;
    if (!lstrcmpiW(szExt, L".bat")) return EXT_BAT;
    if (!lstrcmpiW(szExt, L".cmd")) return EXT_CMD;
    if (!lstrcmpiW(szExt, L".pif")) return EXT_PIF;
    if (!lstrcmpiW(szExt, L".lnk")) return EXT_LNK;
    if (!lstrcmpiW(szExt, L".ico")) return EXT_ICO;
    if (!lstrcmpiW(szExt, L".exe")) return EXT_EXE;

    return EXT_NONE;
}

INT GetExeType(LPCWSTR lpFileName)
{
    const ExtensionTag extTag = HasExtension(lpFileName);

    switch (extTag)
    {
        case EXT_CMD:
        case EXT_BAT:
            return IMAGE_NT_SIGNATURE;

        case EXT_COM:
            if (PathIsUNCServerW(lpFileName) || PathIsUNCServerShareW(lpFileName))
                return 0;
            return IMAGE_DOS_SIGNATURE;

        case EXT_EXE:
            break;

        default:
            return 0;
    }

    HANDLE hFile = CreateFileW(lpFileName, GENERIC_EXECUTE | GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        hFile = CreateFileW(lpFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            return 0;
    }

    FILETIME ftLastAccess;
    if (GetFileTime(hFile, NULL, &ftLastAccess, NULL))
        SetFileTime(hFile, NULL, &ftLastAccess, NULL);

    IMAGE_DOS_HEADER dosHeader = {};
    DWORD cbRead = 0;
    if (!ReadFile(hFile, &dosHeader, sizeof(dosHeader), &cbRead, NULL)
        || cbRead != sizeof(dosHeader)
        || dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
    {
        CloseHandle(hFile);
        return 0;
    }

    WORD  headerSig   = IMAGE_DOS_SIGNATURE;
    WORD  subsystem   = 0;

    SetFilePointer(hFile, dosHeader.e_lfanew, NULL, FILE_BEGIN);

    WORD rawHeader[31] = {};
    ReadFile(hFile, rawHeader, sizeof(rawHeader), &cbRead, NULL);

    if ((rawHeader[0] & 0xFFFF) == IMAGE_NT_SIGNATURE)
    {
        SetFilePointer(hFile, dosHeader.e_lfanew + 72, NULL, FILE_BEGIN);
        DWORD dwSubsystem = 0;
        ReadFile(hFile, &dwSubsystem, sizeof(dwSubsystem), &cbRead, NULL);

        subsystem = static_cast<WORD>((dwSubsystem & 0xFF0000) >> 8 | (dwSubsystem & 0xFF));

        SetFilePointer(hFile, dosHeader.e_lfanew + 92, NULL, FILE_BEGIN);
        DWORD dwImageBase = 0;
        ReadFile(hFile, &dwImageBase, sizeof(dwImageBase), &cbRead, NULL);

        headerSig = IMAGE_NT_SIGNATURE & 0xFFFF;
        if (LOWORD(dwImageBase) != 2)
            subsystem = 0;
    }
    else if (rawHeader[0] == ('N' | 'E' << 8))
    {
        const BYTE neSubsystem = (BYTE)rawHeader[27];
        if (neSubsystem != 2 && neSubsystem != 0)
        {
            headerSig = IMAGE_DOS_SIGNATURE;
            subsystem = 0;
        }
        else if (subsystem)
        {
            headerSig = rawHeader[0];
        }
        else
        {
            headerSig = IMAGE_DOS_SIGNATURE;
        }
    }
    else
    {
        headerSig = IMAGE_DOS_SIGNATURE;
        subsystem = 0;
    }

    CloseHandle(hFile);

    return MAKELONG(headerSig, subsystem);
}

BOOL IsCurrentProcessConsole(VOID)
{
    if (!g_bCurrentProcessIsConsole)
    {
        WCHAR szFilename[MAX_PATH];
        if (GetModuleFileNameW(NULL, szFilename, MAX_PATH) && IsConsoleApp(szFilename))
            g_bCurrentProcessIsConsole = 1;
        else
            g_bCurrentProcessIsConsole = 2;
    }
    return g_bCurrentProcessIsConsole == 1;
}

BOOL IsConsoleApp(LPCWSTR lpFileName)
{
    return GetExeType(lpFileName) == IMAGE_NT_SIGNATURE;
}

BOOL App_IsLFNAware(LPCWSTR lpFileName)
{
    const DWORD exeType = GetExeType(lpFileName);
    const WORD wSig     = LOWORD(exeType);
    const WORD wVersion = HIWORD(exeType);

    const BOOL bLFNCapable =
        (wSig == IMAGE_NT_SIGNATURE)  ||                      // PE (Win32)
        (wSig == IMAGE_OS2_SIGNATURE && wVersion >= 0x0400);  // NE (Win16) v4.00+

    if (!bLFNCapable)
        return FALSE;

    WCHAR szKey[MAX_PATH];
    PathToAppPathKey(lpFileName, szKey, _countof(szKey));
    return (SHGetValueW(HKEY_LOCAL_MACHINE, szKey, L"UseShortName", NULL, NULL,
                        NULL) != ERROR_SUCCESS);
}

HANDLE _WaitForDDEMsg(HWND hWnd, DWORD dwMilliseconds, UINT uTerminateMsg)
{
    HANDLE hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!hEvent)
        return NULL;

    SetPropW(hWnd, L"TermEvent", hEvent);

    DWORD dwStartTick = 0;
    if (dwMilliseconds != INFINITE)
        dwStartTick = GetTickCount();

    DWORD dwRemainingTime = dwMilliseconds;

    for (;;)
    {
        DWORD dwWaitResult = MsgWaitForMultipleObjects(1, &hEvent, FALSE, dwRemainingTime, QS_ALLPOSTMESSAGE);

        if (dwWaitResult == WAIT_OBJECT_0 || dwWaitResult == WAIT_TIMEOUT)
            break; 

        if (dwWaitResult == WAIT_OBJECT_0 + 1)
        {
            MSG msg;
            while (PeekMessageW(&msg, NULL, WM_DDE_FIRST, WM_DDE_LAST, PM_REMOVE))
            {
                DispatchMessageW(&msg);
                if (msg.hwnd == hWnd && msg.message == uTerminateMsg)
                    break;
            }
        }
        else
        {
            break;
        }

        if (dwMilliseconds != INFINITE)
        {
            DWORD dwElapsed = GetTickCount() - dwStartTick;
            if (dwElapsed >= dwMilliseconds)
                break;
            dwRemainingTime = dwMilliseconds - dwElapsed;
        }
    }

    return RemovePropW(hWnd, L"TermEvent");
}

LRESULT CALLBACK DDESubClassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    HWND hConvWnd = (HWND)GetPropW(hWnd, L"ddeconv");

    switch (uMsg)
    {
        case WM_DESTROY:
        {
            KillTimer(hWnd, 0x5555);

            if (hConvWnd)
            {
                SetPropW(hWnd, L"ddeconv", (HANDLE)HANDLE_FLAG_INHERIT);
                PostMessageW(hConvWnd, WM_DDE_TERMINATE, (WPARAM)hWnd, 0);
                _WaitForDDEMsg(hWnd, 10000, WM_DDE_TERMINATE);
                RemovePropW(hWnd, L"ddeconv");
            }

            HANDLE hDoneEvent = RemovePropW(hWnd, L"ddeevent");
            if (hDoneEvent)
                SetEvent(hDoneEvent);

            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
        }

        case WM_TIMER:
            if (wParam == 0x5555)
                DestroyWindow(hWnd);
            else
                return DefWindowProcW(hWnd, uMsg, wParam, lParam);
            break;

        case WM_DDE_TERMINATE:
        {
            if ((HWND)wParam == hConvWnd)
            {
                PostMessageW((HWND)wParam, WM_DDE_TERMINATE, (WPARAM)hWnd, 0);
                RemovePropW(hWnd, L"ddeconv");
                DestroyWindow(hWnd);
            }

            HANDLE hTermEvent = GetPropW(hWnd, L"TermEvent");
            if (hTermEvent)
                SetEvent(hTermEvent);

            break;
        }

        case WM_DDE_ACK:
        {
            const HWND hSender = (HWND)wParam;
            if (!hConvWnd)
            {
                SetPropW(hWnd, L"ddeconv", hSender);
                return 0;
            }

            const BOOL bExpectedSender = (hConvWnd == (HWND)UlongToHandle(1))
                                      || (hSender == hConvWnd);
            if (!bExpectedSender)
                return 0;

            UINT_PTR uiLo = 0, uiHi = 0;
            if (UnpackDDElParam(WM_DDE_ACK, lParam, &uiLo, &uiHi))
            {
                GlobalFree((HGLOBAL)uiHi);
                FreeDDElParam(WM_DDE_ACK, lParam);
            }

            if (hConvWnd == (HWND)UlongToHandle(1))
                return 0;

            DestroyWindow(hWnd);
            break;
        }

        default:
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

HWND GetTopParentWindow(HWND hWnd)
{
    HWND ret, hwndNode;

    if (!IsWindow(hWnd))
        return NULL;
    ret = hWnd;
    for (hwndNode = GetWindow(hWnd, GW_OWNER); hwndNode; hwndNode = GetWindow(hwndNode, GW_OWNER))
        ret = hwndNode;
    return ret;
}

HWND ThreadID_GetVisibleWindow(DWORD dwThreadId)
{
    HWND hDesktop = GetDesktopWindow();

    for (HWND hWnd = GetWindow(hDesktop, GW_CHILD);
         hWnd != NULL;
         hWnd = GetWindow(hWnd, GW_HWNDNEXT))
    {
        const DWORD dwOwnerThread = GetWindowThreadProcessId(hWnd, NULL);
        if (IsWindowVisible(hWnd) && dwOwnerThread == dwThreadId)
            return hWnd;
    }

    return NULL;
}

HWND ActivateHandler(HWND hWnd, WPARAM wParam)
{
    HWND hTopParent  = GetTopParentWindow(hWnd);
    HWND hLastActive = GetLastActivePopup(hTopParent);

    if (IsWindowVisible(hLastActive))
        return hLastActive;

    const DWORD dwThreadId = GetWindowThreadProcessId(hTopParent, NULL);
    HWND hVisible = ThreadID_GetVisibleWindow(dwThreadId);
    if (!hVisible)
        return NULL;

    HWND hPopup = GetLastActivePopup(hVisible);

    if (IsIconic(hVisible))
    {
        ShowWindow(hVisible, SW_RESTORE);
    }
    else
    {
        BringWindowToTop(hVisible);
        if (hPopup && hPopup != hVisible)
            BringWindowToTop(hPopup);
    }

    if (wParam)
        return (HWND)SendMessageW(hVisible, WM_SETHOTKEY, wParam, 0);

    return hVisible;
}

BOOL Window_IsLFNAware(HWND hWnd)
{
    return GetWindowLongPtrW(hWnd, GWLP_HINSTANCE) == 0;
}

HINSTANCE Window_GetInstance(HWND hWnd)
{
    DWORD dwProcessId = 0;
    GetWindowThreadProcessId(hWnd, &dwProcessId);
    return (HINSTANCE)UlongToHandle(dwProcessId ? 33 : 0);
}

DWORD SHProcessMessagesUntilEventEx(
    HWND  hWnd,
    HANDLE hEvent,
    DWORD dwMilliseconds,
    DWORD dwWakeMask)
{
    if (!hEvent && dwMilliseconds == INFINITE)
        return WAIT_FAILED;

    const DWORD dwStartTick = GetTickCount();
    const DWORD dwDeadline  = dwStartTick + dwMilliseconds;
    DWORD dwRemaining       = dwMilliseconds;

    const DWORD nCount = hEvent ? 1 : 0;
    DWORD result;

    for (;;)
    {
        result = MsgWaitForMultipleObjects(nCount, &hEvent, FALSE, dwRemaining, dwWakeMask);
        if (result != WAIT_OBJECT_0 + nCount)
            break;

        MSG msg;
        while (PeekMessageW(&msg, hWnd, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);

            if (msg.message == WM_SETCURSOR)
            {
                SetCursor(LoadCursor(NULL, IDC_WAIT));
            }
            else
            {
                DispatchMessageW(&msg);
            }
        }

        if (dwMilliseconds != INFINITE)
            dwRemaining = dwDeadline - GetTickCount();
    }

    return result;
}

DWORD WINAPI SHGetAppCompatFlags(_In_ DWORD dwMask)
{
    // FIXME
    return 0;
}

VOID SetAppStartingCursor(HWND hWnd, BOOL bStart)
{
    // FIXME
}

HRESULT SHCoInitialize(VOID)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr))
        return CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE);
    return hr;
}

DWORD WINAPI SHRegisterClassW(WNDCLASSW * lpWndClass)
{
    WNDCLASSW WndClass;
    if (GetClassInfoW(lpWndClass->hInstance, lpWndClass->lpszClassName, &WndClass))
        return TRUE;
    return RegisterClassW(lpWndClass);
}

HWND WINAPI SHCreateWorkerWindowW(WNDPROC wndProc, HWND hWndParent, DWORD dwExStyle,
                                  DWORD dwStyle, HMENU hMenu, LONG_PTR wnd_extra)
{
  static const WCHAR szClass[] = { 'W', 'o', 'r', 'k', 'e', 'r', 'W', 0 };
  WNDCLASSW wc;
  HWND hWnd;

  /* Create Window class */
  wc.style         = 0;
  wc.lpfnWndProc   = DefWindowProcW;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = sizeof(LONG_PTR);
  wc.hInstance     = GetModuleHandleW(NULL);
  wc.hIcon         = NULL;
  wc.hCursor       = LoadCursorW(NULL, (LPWSTR)IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = szClass;

  SHRegisterClassW(&wc);

  hWnd = CreateWindowExW(dwExStyle, szClass, 0, dwStyle, 0, 0, 0, 0,
                         hWndParent, hMenu, GetModuleHandleA(NULL), 0);
  if (hWnd)
  {
    SetWindowLongPtrW(hWnd, 0, wnd_extra);
    if (wndProc) SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)wndProc);
  }

  return hWnd;
}

LPITEMIDLIST SafeILClone(LPCITEMIDLIST pidl)
{
    return pidl ? ILClone(pidl) : NULL;
}

LPITEMIDLIST ILCloneParent(LPCITEMIDLIST pidl)
{
    LPITEMIDLIST pidlNew = SafeILClone(pidl);
    if (pidlNew)
        ILRemoveLastID(pidlNew);
    return pidlNew;
}

HRESULT
SHBindToObjectEx(
    IShellFolder* pShellFolder,
    LPCITEMIDLIST pidl,
    IBindCtx* pBindCtx,
    REFIID riid,
    PVOID* ppvObj)
{
    if (!ppvObj)
        return E_POINTER;

    *ppvObj = NULL;

    IShellFolder* pTargetFolder = pShellFolder;
    IShellFolder* pDesktopFolderToRelease = NULL;

    if (!pTargetFolder)
    {
        HRESULT hrDesktop = SHGetDesktopFolder(&pDesktopFolderToRelease);
        if (FAILED(hrDesktop))
            return E_FAIL;
        pTargetFolder = pDesktopFolderToRelease;
    }

    HRESULT hr = E_FAIL;

    if (pidl != NULL && pidl->mkid.cb > 0)
    {
        hr = pTargetFolder->BindToObject(pidl, pBindCtx, riid, ppvObj);
    }
    else
    {
        hr = pTargetFolder->QueryInterface(riid, ppvObj);
    }

    if (pDesktopFolderToRelease != NULL)
        pDesktopFolderToRelease->Release();

    if (SUCCEEDED(hr) && !*ppvObj)
        return E_FAIL;

    return hr;
}

HRESULT WonSHBindToFolderIDListParent(
    IShellFolder *psfRoot,
    LPCITEMIDLIST pidl,
    REFIID riid,
    PVOID* ppv,
    LPCITEMIDLIST *ppidlLast)
{
    HRESULT hr;
    LPITEMIDLIST pidlNew = ILCloneParent(pidl);
    if (pidlNew)
    {
        hr = SHBindToObjectEx(psfRoot, pidlNew, 0, riid, ppv);
        ILFree(pidlNew);
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }

    if (ppidlLast)
        *ppidlLast = ILFindLastID(pidl);

    return hr;
}

PWSTR _PathGetArgsLikeCreateProcess(LPCWSTR lpString)
{
    PWSTR pch;
    if (*lpString == '"')
        pch = StrChrW(lpString + 1, '"');
    else
        pch = StrChrW(lpString, ' ');
    if (pch)
        return pch + 1;
    else
        return (PWSTR)&lpString[lstrlenW(lpString)];
}

static BOOL SHLWAPI_PathFindInOtherDirs(LPWSTR lpszFile, DWORD dwWhich)
{
  static const WCHAR szSystem[] = { 'S','y','s','t','e','m','\0'};
  static const WCHAR szPath[] = { 'P','A','T','H','\0'};
  DWORD dwLenPATH;
  LPCWSTR lpszCurr;
  WCHAR *lpszPATH;
  WCHAR buff[MAX_PATH];

  /* Try system directories */
  GetSystemDirectoryW(buff, MAX_PATH);
  if (!PathAppendW(buff, lpszFile))
     return FALSE;
  if (PathFileExistsDefExtW(buff, dwWhich))
  {
    lstrcpyW(lpszFile, buff);
    return TRUE;
  }
  GetWindowsDirectoryW(buff, MAX_PATH);
  if (!PathAppendW(buff, szSystem ) || !PathAppendW(buff, lpszFile))
    return FALSE;
  if (PathFileExistsDefExtW(buff, dwWhich))
  {
    lstrcpyW(lpszFile, buff);
    return TRUE;
  }
  GetWindowsDirectoryW(buff, MAX_PATH);
  if (!PathAppendW(buff, lpszFile))
    return FALSE;
  if (PathFileExistsDefExtW(buff, dwWhich))
  {
    lstrcpyW(lpszFile, buff);
    return TRUE;
  }
  /* Try dirs listed in %PATH% */
  dwLenPATH = GetEnvironmentVariableW(szPath, buff, MAX_PATH);

  if (!dwLenPATH || !(lpszPATH = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (dwLenPATH + 1) * sizeof (WCHAR))))
    return FALSE;

  GetEnvironmentVariableW(szPath, lpszPATH, dwLenPATH + 1);
  lpszCurr = lpszPATH;
  while (lpszCurr)
  {
    LPCWSTR lpszEnd = lpszCurr;
    LPWSTR pBuff = buff;

    while (*lpszEnd == ' ')
      lpszEnd++;
    while (*lpszEnd && *lpszEnd != ';')
      *pBuff++ = *lpszEnd++;
    *pBuff = '\0';

    if (*lpszEnd)
      lpszCurr = lpszEnd + 1;
    else
      lpszCurr = NULL; /* Last Path, terminate after this */

    if (!PathAppendW(buff, lpszFile))
    {
      HeapFree(GetProcessHeap(), 0, lpszPATH);
      return FALSE;
    }
    if (PathFileExistsDefExtW(buff, dwWhich))
    {
      lstrcpyW(lpszFile, buff);
      HeapFree(GetProcessHeap(), 0, lpszPATH);
      return TRUE;
    }
  }
  HeapFree(GetProcessHeap(), 0, lpszPATH);
  return FALSE;
}

BOOL WINAPI PathFindOnPathExW(LPWSTR lpszFile, LPCWSTR *lppszOtherDirs, DWORD dwWhich)
{
    WCHAR buff[MAX_PATH];

    if (!lpszFile || !PathIsFileSpecW(lpszFile))
        return FALSE;

    /* Search provided directories first */
    if (lppszOtherDirs && *lppszOtherDirs)
    {
        LPCWSTR *lpszOtherPath = lppszOtherDirs;
        while (lpszOtherPath && *lpszOtherPath && (*lpszOtherPath)[0])
        {
            PathCombineW(buff, *lpszOtherPath, lpszFile);
            if (PathFileExistsDefExtW(buff, dwWhich))
            {
                lstrcpyW(lpszFile, buff);
                return TRUE;
            }
            lpszOtherPath++;
        }
    }
    /* Not found, try system and path dirs */
    return SHLWAPI_PathFindInOtherDirs(lpszFile, dwWhich);
}

typedef BOOL (WINAPI *FN_PathIsValidCharW)(WCHAR, DWORD);

static BOOL WINAPI PathIsValidCharW(WCHAR c, DWORD cls)
{
    HINSTANCE hSHLWAPI = GetModuleHandleA("shlwapi");
    FARPROC fn = GetProcAddress(hSHLWAPI,  MAKEINTRESOURCEA(456));
    FN_PathIsValidCharW pPathIsValidCharW;
    CopyMemory(&pPathIsValidCharW, &fn, sizeof(fn));
    return pPathIsValidCharW(c, cls);
}

static LPCWSTR _PathGuessNextBestArgs(LPCWSTR pszPath)
{
    LPCWSTR pFirstSpace = NULL;
    BOOL bValid = TRUE;

    LPCWSTR p = pszPath;
    while (*p && bValid)
    {
        switch (*p)
        {
        case L' ':
            if (!pFirstSpace)
                pFirstSpace = p;
            break;

        case L'"':
        case L'%':
            bValid = false;
            break;

        case L'\\':
            if (PathIsUNCW(p))
                bValid = false;
            else
                pFirstSpace = NULL;
            break;

        default:
            bValid = PathIsValidCharW(*p, 0x1E4);
            break;
        }

        ++p;
    }

    if (pFirstSpace)
    {
        while (*pFirstSpace == L' ')
            ++pFirstSpace;
        return pFirstSpace;
    }

    return bValid ? p : NULL;
}

HRESULT WonSHEvaluateSystemCommandTemplate(
    PCWSTR pszCmdTemplate,
    PWSTR* ppszApplication,
    PWSTR* ppszCommandLine,
    PWSTR* ppszParameters)
{
    HRESULT hr = S_OK;
    WCHAR szPath[MAX_PATH] = {};
    LPCWSTR pszArgs = _PathGetArgsLikeCreateProcess(pszCmdTemplate);

    PWSTR pszExePart = szPath;
    
    hr = _CopyExe(szPath, _countof(szPath), pszCmdTemplate, static_cast<UINT>(pszArgs - pszCmdTemplate));
    if (FAILED(hr)) goto Cleanup;

    const BOOL fWasQuoted = (szPath[0] == L'"');
    if (fWasQuoted)
    {
        PathUnquoteSpacesW(szPath);
    }

    if (!PathIsAbsolute(szPath))
    {
        if (!PathIsFileSpecW(szPath))
        {
            hr = E_FAIL;
            goto Cleanup;
        }

        if (_GetAppPath(szPath, szPath, _countof(szPath)))
        {
            hr = _PathExeExists(szPath);
        }
        else
        {
            if (SHWindowsPolicy(POLID_UsePathEnvVarForCommandTemplates, 0))
            {
                hr = PathFindOnPathExW(szPath, NULL, WHICH_DEFAULT)
                     ? S_OK
                     : HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

                pszExePart = PathFindFileNameW(szPath);
                goto Cleanup;
            }
            else
            {
                hr = _PathFindInSystem(szPath, _countof(szPath));
            }
        }

        pszExePart = PathFindFileNameW(szPath);
        goto Cleanup;
    }

    if (!fWasQuoted && _PathMatchesSuspicious(szPath))
    {
        hr = E_FAIL;
        goto TryNextToken;
    }

    hr = _PathExeExists(szPath);

    if (FAILED(hr) && !fWasQuoted)
    {
        while (true)
        {
        TryNextToken:
            if (!*pszArgs) goto Cleanup;

            LPCWSTR pszNextArgs = _PathGuessNextBestArgs(pszArgs);
            pszArgs = pszNextArgs;
            if (!pszNextArgs)
            {
                hr = CO_E_APPNOTFOUND;
                goto NotFound;
            }

            hr = _CopyExe(szPath, _countof(szPath), pszCmdTemplate, static_cast<UINT>(pszNextArgs - pszCmdTemplate));
            if (SUCCEEDED(hr)) break;

            if (!pszArgs) goto Cleanup;
        }

        hr = _PathExeExists(szPath);

    NotFound:
        if (FAILED(hr))
        {
            if (!pszArgs) goto Cleanup;
            goto TryNextToken;
        }
    }

Cleanup:
    *ppszApplication = NULL;
    if (ppszCommandLine) *ppszCommandLine = NULL;
    if (ppszParameters)  *ppszParameters = NULL;

    if (FAILED(hr)) return hr;

    if (!pszArgs) pszArgs = L"";

    hr = SHStrDupW(szPath, ppszApplication);
    if (FAILED(hr)) goto FreeOnError;

    if (ppszCommandLine)
    {
        UINT cchNeeded = static_cast<UINT>(lstrlenW(pszExePart)) + 
                         static_cast<UINT>(lstrlenW(pszArgs)) + 8;

        hr = SHCoAlloc(sizeof(WCHAR) * cchNeeded, reinterpret_cast<PVOID*>(ppszCommandLine));
        if (FAILED(hr)) goto FreeOnError;

        hr = StringCchPrintfW(*ppszCommandLine, cchNeeded, L"\"%s\" %s", pszExePart, pszArgs);
        if (FAILED(hr)) goto FreeOnError;
    }

    if (ppszParameters)
    {
        hr = SHStrDupW(pszArgs, ppszParameters);
        if (FAILED(hr)) goto FreeOnError;
    }

    return hr;

FreeOnError:
    if (*ppszApplication)
    {
        CoTaskMemFree(*ppszApplication);
        *ppszApplication = NULL;
    }
    if (ppszCommandLine && *ppszCommandLine)
    {
        CoTaskMemFree(*ppszCommandLine);
        *ppszCommandLine = NULL;
    }
    return hr;
}

INT WINAPIV WonShellMessageBoxWrapW(HINSTANCE hInstance, HWND hWnd, LPCWSTR lpText,
                                    LPCWSTR lpCaption, UINT uType, ...)
{
    WCHAR *szText = NULL, szTitle[100];
    LPCWSTR pszText, pszTitle = szTitle;
    LPWSTR pszTemp;
    va_list args;
    int ret;

    va_start(args, uType);

    if (IS_INTRESOURCE(lpCaption))
        LoadStringW(hInstance, LOWORD(lpCaption), szTitle, _countof(szTitle));
    else
        pszTitle = lpCaption;

    if (IS_INTRESOURCE(lpText))
    {
        const WCHAR *ptr;
        UINT len = LoadStringW(hInstance, LOWORD(lpText), (LPWSTR)&ptr, 0);

        if (len)
        {
            szText = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
            if (szText) LoadStringW(hInstance, LOWORD(lpText), szText, len + 1);
        }
        pszText = szText;
        if (!pszText) {
            va_end(args);
            return 0;
        }
    }
    else
        pszText = lpText;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING,
                   pszText, 0, 0, (LPWSTR)&pszTemp, 0, &args);

    va_end(args);

    uType |= MB_SETFOREGROUND;
    ret = MessageBoxW(hWnd, pszTemp, pszTitle, uType);

    HeapFree(GetProcessHeap(), 0, szText);
    LocalFree(pszTemp);
    return ret;
}

} // extern "C"
