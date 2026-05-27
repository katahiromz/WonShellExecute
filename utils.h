// utils.h --- Utility functions
// Author: katahiromz
// License: MIT

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    BOOL _IsNamespaceObject(LPCWSTR pszpath);
    BOOL _FailForceReturn(HRESULT hr);
    BOOL _IsLink(LPCWSTR pszPath, DWORD dwFileAttributes);
    BOOL PathIsShortcut(LPCWSTR pszPath, DWORD dwFileAttributes);
    HRESULT TryShellExecuteHooks(LPSHELLEXECUTEINFOW sei);
    INT _ProcessAllowsInvalidUrls(VOID);
    HRESULT IsDarwinEnabled(VOID);
    DWORD ParseDarwinID(LPWSTR Descriptor, LPWSTR CommandLine, DWORD CommandLineLength);
    BOOL DoesAppWantUrl(PCWSTR pszPath);
    VOID SetAppStartingCursor(HWND hWnd, BOOL bStart);

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

    HRESULT SHCoInitialize(VOID); // --> Ros:SHCoInitializeAnyApartment;
    BOOL InRunDllProcess(VOID); // --> Ros:SHELL_InRunDllProcess
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
    BOOL IsMachineDomainMember(void);
    DWORD WINAPI SHGetAppCompatFlags(_In_ DWORD dwMask);

    HRESULT WonSHBindToFolderIDListParent(
        IShellFolder *psfRoot,
        LPCITEMIDLIST pidl,
        REFIID riid,
        PVOID* ppv,
        LPCITEMIDLIST *ppidlLast);

    HRESULT WonSHEvaluateSystemCommandTemplate(LPCWSTR pszCommand, LPWSTR *ppszApplication, LPWSTR *ppszCommandLine, LPWSTR *ppszUnknown);

    INT WINAPIV WonShellMessageBoxWrapW(HINSTANCE hInstance, HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, ...);

#ifdef __cplusplus
} // extern "C"
#endif
