// utils.h --- Utility functions
// Author: katahiromz
// License: MIT

#pragma once

BOOL _IsNamespaceObject(LPCWSTR pszpath);
BOOL _FailForceReturn(HRESULT hr);
BOOL _IsLink(LPCWSTR pszPath, DWORD dwFileAttributes);
BOOL PathIsShortcut(LPCWSTR pszPath, DWORD dwFileAttributes);
INT TryShellExecuteHooks(LPSHELLEXECUTEINFOW sei);
INT _ProcessAllowsInvalidUrls(void);
HRESULT IsDarwinEnabled(void);
DWORD ParseDarwinID(LPWSTR Descriptor, LPWSTR CommandLine, DWORD CommandLineLength);
BOOL DoesAppWantUrl(PCWSTR pszPath);

#ifdef NO_ASSIST
struct IUserAssist : public IUnknown
{
    STDMETHOD(FireEvent)(
        GUID const *guid,
        INT param1,
        ULONG param2,
        WPARAM wparam,
        LPARAM lparam);
    STDMETHOD(QueryEvent)(
        GUID const *guid,
        INT param,
        WPARAM wparam,
        LPARAM lparam,
        PVOID ptr);
    STDMETHOD(SetEvent)(
        GUID const *guid,
        INT param,
        WPARAM wparam,
        LPARAM lparam,
        PVOID ptr);
    STDMETHOD(Enable)(
        BOOL bEnable);
};

IUserAssist *GetUserAssist(void);
#endif

INT GetUEMAssoc(LPCWSTR pszFile, LPCWSTR pszPath, LPCITEMIDLIST pidl);

HRESULT UEMFireEvent(
    REFGUID guid,
    INT     eCmd,
    ULONG   uFlags,
    WPARAM  wparam,
    LPARAM  lparam);

HRESULT
SHGetNameAndFlagsW(
    _In_ LPCITEMIDLIST pidl,
    _In_ DWORD dwFlags,
    _Out_opt_ LPWSTR pszText,
    _In_ UINT cchBuf,
    _Inout_opt_ DWORD *pdwAttributes);

BOOL PathToAppPath(LPCWSTR pszPath, PVOID pvData);
HRESULT PathToAppPathKey(LPCWSTR pszPath, LPWSTR pszDest, size_t cchDest);

HRESULT PathToAppPathKeyBase(LPCWSTR pszBase, LPCWSTR pszPath,
                             LPWSTR pszDest, size_t cchDest);

/* PathResolve flags */
#define PRF_CHECKEXISTANCE  0x01
#define PRF_EXECUTABLE      0x02
#define PRF_QUALIFYONPATH   0x04
#define PRF_WINDOWS31       0x08

BOOL WINAPI PathResolveW(LPWSTR path, LPCWSTR *dirs, DWORD flags);

BOOL UEMIsLoaded(void);

void SetAppStartingCursor(HWND hwnd, BOOL bFlag);

HRESULT SHCoInitialize(void); // --> Ros:SHCoInitializeAnyApartment;
BOOL InRunDllProcess(); // --> Ros:SHELL_InRunDllProcess
INT SHWindowsPolicy(REFGUID rguid, INT nDefault);
HRESULT SHGetUIObjectOf(LPCITEMIDLIST pidl, HWND hWnd, REFIID riid, PVOID *ppv);
HRESULT SHBindToIDListParent(LPCITEMIDLIST pidl, REFIID riid, PVOID *ppv, LPCITEMIDLIST *ppidlLast);
HRESULT SHGetAssociations(LPCITEMIDLIST pidl, IQueryAssociations **ppQueryAssoc);

HRESULT SHGetUIObjectFromFullPIDL(
    LPCITEMIDLIST   pidl,
    HWND            hWnd,
    REFIID          riid,
    PVOID          *ppv);

HRESULT _InvokeInProcExec(IContextMenu *pContextMenu, LPSHELLEXECUTEINFOW sei);

HRESULT SEI2ICIX(
    const SHELLEXECUTEINFOW *pSEI,
    CMINVOKECOMMANDINFOEX   *pICIX,
    HLOCAL                  *phLocal);

PSTR ThunkStrToAnsi(PCWSTR pwszSrc, PSTR pszDst, INT cchBuf);
BOOL _PathIsFile(LPCWSTR lpFileName);

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
    BOOL                  bLogUsage);

BOOL IsFolderGUID(IShellFolder *psfRoot, LPCITEMIDLIST pidl);
BOOL RestrictedApp(LPCWSTR pszPath);
BOOL DisallowedApp(LPCWSTR pszPath);
BOOL IsNameListedUnderKey(PCWSTR pszFileName, PCWSTR pszSubKey);

#include "WonZoneCheck.h"
#define ZoneCheckUrlExW WonZoneCheckUrlExW

#include "WonSafeOpen.h"
#define SafeOpenPromptForShellExec WonSafeOpenPromptForShellExec

HRESULT TBCRegisterObjectParam(LPCWSTR pszKey, IUnknown *pUnk, LPBC *ppbc);
HRESULT TBCGetEnvironmentVariable(LPCWSTR pszName, LPWSTR pszDest, INT cchDest);
HRESULT TBCGetObjectParam(OLECHAR *pszKey, REFIID riid, PVOID *ppv);

INT SHSysErrorMessageBox(
    HWND   hWnd,
    LPCWSTR pszTitle,
    UINT   nTextID,
    DWORD  dwMessageId,
    PCWSTR pszSearch,
    UINT   uType);

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
    PHANDLE       phDdeData);

class ShStrA
{
public:
    ShStrA()
    {
        m_szBuff[0] = ANSI_NULL;
        m_pch = m_szBuff;
        m_cchBuff = _countof(m_szBuff) - 1;
    }

    ~ShStrA()
    {
        Reset();
    }

    operator LPSTR() { return m_pch; }
    INT GetBuffLength() const { return m_cchBuff; }

    void Reset()
    {
        if (m_pch && m_cchBuff != _countof(m_szBuff) - 1)
            LocalFree(m_pch);
        m_szBuff[0] = ANSI_NULL;
        m_pch = m_szBuff;
        m_cchBuff = _countof(m_szBuff) - 1;
    }

    HRESULT SetSize(UINT cchMax)
    {
        UINT ich;
        for (ich = m_cchBuff; cchMax > ich; ich *= 4)
            ;
        if (ich == m_cchBuff)
            return S_OK;

        if (ich <= _countof(m_szBuff) - 1)
        {
            if (m_pch && m_cchBuff)
                lstrcpynA(m_szBuff, m_pch, _countof(m_szBuff) - 1);
            Reset();
            m_pch = m_szBuff;
        }
        else
        {
            PSTR pszText = (PSTR)LocalAlloc(LPTR, ich);
            if (!pszText)
                return E_OUTOFMEMORY;

            lstrcpynA(pszText, m_pch, cchMax);
            Reset();
            m_cchBuff = ich;
            m_pch = pszText;
        }
        return S_OK;
    }

    HRESULT SetStr(LPCWSTR lpWideCharStr, UINT cchWideChar)
    {
        Reset();
        return _SetStr(lpWideCharStr, cchWideChar);
    }

protected:
    CHAR m_szBuff[66];
    PCHAR m_pch;
    INT m_cchBuff;

    HRESULT _SetStr(LPCWSTR lpWideCharStr, INT cchWideChar)
    {
        INT cch = cchWideChar;
        if (!lpWideCharStr || !cchWideChar)
            return S_FALSE;

        if (cchWideChar == -1)
            cch = WideCharToMultiByte(CP_ACP, 0, lpWideCharStr, -1, NULL, 0, NULL, NULL);

        if (!cch)
            return S_FALSE;

        HRESULT hr = SetSize(cch + 1);
        if (FAILED(hr))
            return hr;

        INT cchStr = WideCharToMultiByte(CP_ACP, 0, lpWideCharStr, cchWideChar, m_pch, m_cchBuff, NULL, NULL);
        if (cchStr >= m_cchBuff)
            cchStr = m_cchBuff;

        m_pch[cchStr] = ANSI_NULL;
        return hr;
    }
};

class ShStrW
{
public:
    ShStrW()
    {
        m_szBuff[0] = UNICODE_NULL;
        m_pch = m_szBuff;
        m_cchBuff = _countof(m_szBuff) - 1;
    }

    ~ShStrW()
    {
        Reset();
    }

    operator LPWSTR() { return m_pch; }
    INT GetBuffLength() const { return m_cchBuff; }

    void Reset()
    {
        if (m_pch && m_cchBuff != _countof(m_szBuff) - 1)
            LocalFree(m_pch);

        m_szBuff[0] = UNICODE_NULL;
        m_pch = m_szBuff;
        m_cchBuff = _countof(m_szBuff) - 1;
    }

    HRESULT SetSize(INT cchMax)
    {
        INT ich;
        for (ich = m_cchBuff; cchMax > ich; ich *= 4)
            ;
        if (ich == m_cchBuff)
            return S_OK;

        if (ich <= _countof(m_szBuff) - 1)
        {
            if (m_pch && m_cchBuff)
                StrCpyNW(m_szBuff, m_pch, _countof(m_szBuff) - 1);
            Reset();
        }
        else
        {
            PWSTR pszText = (PWSTR)LocalAlloc(LPTR, ich * sizeof(WCHAR));
            if (!pszText)
                return E_OUTOFMEMORY;

            StrCpyNW(pszText, m_pch, cchMax);
            Reset();
            m_pch = pszText;
            m_cchBuff = ich;
        }

        return S_OK;
    }

    HRESULT SetStr(LPCWSTR lpString, INT cchString)
    {
        Reset();
        return _SetStr(lpString, cchString);
    }

    HRESULT SetStr(ShStrW *pOther)
    {
        return SetStr(pOther->m_pch, -1);
    }

    HRESULT SetStr(LPCSTR lpString)
    {
        Reset();
        return _SetStr(lpString);
    }

    HRESULT Append(LPCWSTR lpString, INT cchString)
    {
        if (!lpString)
            return E_INVALIDARG;
        INT cchStr = lstrlenW(m_pch);
        if (cchString == -1)
            cchString = lstrlenW(lpString);
        INT cch2 = cchString + 1;
        if (FAILED(SetSize(cch2 + cchStr)))
            return E_OUTOFMEMORY;
        StrCpyNW(&m_pch[cchStr], lpString, cch2);
        return S_OK;
    }

protected:
    WCHAR m_szBuff[66];
    PWCHAR m_pch;
    INT m_cchBuff;

    HRESULT _SetStr(LPCSTR lpString)
    {
        if (!lpString)
            return S_FALSE;

        INT cchString = lstrlenA(lpString);
        if (!cchString)
            return S_FALSE;

        HRESULT hr = SetSize(cchString + 1);
        if (SUCCEEDED(hr))
            MultiByteToWideChar(CP_ACP, 0, lpString, -1, m_pch, m_cchBuff);
        return hr;
    }

    HRESULT _SetStr(LPCWSTR lpString, INT cchString)
    {
        if (!lpString || !cchString)
            return S_FALSE;

        INT cchStr = cchString;
        if (cchString == -1)
            cchStr = lstrlenW(lpString);

        if (!cchStr)
            return S_FALSE;

        INT cchMax = cchStr + 1;
        HRESULT hr = SetSize(cchStr + 1);
        if (SUCCEEDED(hr))
        {
            INT cchBuff = m_cchBuff;
            if (cchMax < cchBuff)
                cchBuff = cchMax;
            StrCpyNW(m_pch, lpString, cchBuff);
        }

        return hr;
    }
};

inline BOOL _ParamIsApp(PCWSTR psz1)
{
    return !StrCmpNW(psz1, L"%1", 2) || !StrCmpNW(psz1, L"\"%1\"", 4);
}

BOOL IsCurrentProcessConsole(VOID);
BOOL IsConsoleApp(LPCWSTR lpFileName);
LRESULT CALLBACK DDESubClassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
HWND GetTopParentWindow(HWND hWnd);

HWND WINAPI SHCreateWorkerWindowW(WNDPROC wndProc, HWND hWndParent, DWORD dwExStyle,
                                  DWORD dwStyle, HMENU hMenu, LONG_PTR wnd_extra);
HWND ActivateHandler(HWND hWnd, WPARAM wParam);
BOOL Window_IsLFNAware(HWND hWnd);
HINSTANCE Window_GetInstance(HWND hWnd);

DWORD SHProcessMessagesUntilEventEx(
    HWND  hWnd,
    HANDLE hEvent,
    DWORD dwMilliseconds,
    DWORD dwWakeMask);
BOOL App_IsLFNAware(LPCWSTR lpFileName);
DWORD CheckForAppPathsBoolValue(LPCWSTR pszPath, LPCWSTR pszValue);
DWORD SHGetAttributes(IShellFolder* psf, LPCITEMIDLIST pidl, DWORD dwAttributes);
DWORD SHGetObjectCompatFlags(IUnknown *pUnk, const CLSID *clsid);
BYTE staticIsOS(DWORD dwInfoType);
BOOL IsMachineDomainMember(void);
DWORD WINAPI SHGetAppCompatFlags(_In_ DWORD dwMask);

static inline int SHGetAppCompatFlags(int)
{
    return 0;
}
