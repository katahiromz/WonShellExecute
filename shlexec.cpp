// shlexec.h --- Shell execution emulation
// Author: katahiromz
// License: MIT

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <appmgmt.h>
#include <userenv.h>
#include "shlexec.h"
#include "utils.h"
#include "debug.h"
#include "resource.h"

#ifndef STARTF_USEMONITOR
    #define STARTF_USEMONITOR 0x400
#endif
#ifndef ASSOCF_NONE
    #define ASSOCF_NONE 0
#endif
#define SEE_MASK_CLASSALL (SEE_MASK_CLASSNAME | SEE_MASK_CLASSKEY)

enum IRET
{
    IRET_FAILED = -1,
    IRET_0 = 0,
    IRET_1 = 1,
    IRET_2 = 2,
};

struct CEnvironmentBlock
{
    HANDLE m_hUserToken;
    LPWSTR m_pszzBlock;
    INT m_cchAlloc;
    INT m_cchBlock;
    UINT m_nRunAs;

    HRESULT UpdateVar(
        LPCWSTR pszName,
        WCHAR chSep,
        LPCWSTR pszValue,
        BOOL bAppend);

    HRESULT SetVar(LPCWSTR pszName, LPCWSTR pszValue);

private:
    HRESULT _InitBlock(INT cchBlock);
    BOOL _FindVar(PCNZWCH pszName, INT cchName, LPWSTR *ppch);
    INT _BlockLenCached();
    INT _BlockLen(LPCWSTR pchStart);
};

struct CShellExecute
{
    LONG m_cRefs;
    WCHAR m_szPath[MAX_PATH_EX];
    WCHAR m_szWorkDir[MAX_PATH];
    WCHAR m_szCommand[MAX_PATH_EX];
    WCHAR m_szRunAsCommand[MAX_PATH_EX];
    WCHAR m_szDdeCommand[MAX_PATH];
    WCHAR m_szWorkGroupHelper[MAX_PATH];
    WCHAR m_szExecutable2[MAX_PATH];
    WCHAR m_szFriendlyAppName[MAX_PATH];
    WCHAR m_szURL[MAX_PATH_EX];
    DWORD m_dwCreationFlags;
    STARTUPINFOW m_si;
    INT m_wShowWindow;
    UINT m_uValidateUNC;
    PROCESS_INFORMATION m_pi;
    WCHAR m_szAppPathKey[MAX_PATH_EX];
    WCHAR m_szEnvEntry[MAX_PATH];
    WCHAR m_szVerb[MAX_PATH];
    LPWSTR m_pszVerb;
    LPWSTR m_pszParameters;
    HANDLE m_hParameters;
    LPWSTR m_pszClass;
    LPWSTR m_pszTitle;
    HANDLE m_hTitle;
    LPITEMIDLIST m_lpIDList;
    DWORD m_attrs;
    LPITEMIDLIST m_pidlParsed;
    ATOM m_wDdeAppAtom;
    ATOM m_wDdeTopicAtom;
    HANDLE m_hDdeData;
    IQueryAssociations *m_pQueryAssoc;
    HWND m_hWnd;
    LPSECURITY_ATTRIBUTES m_lpProcessAttributes;
    LPSECURITY_ATTRIBUTES m_lpThreadAttributes;
    HANDLE m_hUserToken;
    HANDLE m_hHandle1;
    CEnvironmentBlock m_EnvBlock;
    HINSTANCE m_hInstApp;
    DWORD m_dwError;
    BOOL m_bNoUI;
    BOOL m_bLogUsage;
    BOOL m_bSubst;
    BOOL m_bUseClass;
    BOOL m_bNoQueryClassStore;
    BOOL m_bClassStoreOnly;
    BOOL m_bIsURL;
    BOOL m_bActivateHandler;
    BOOL m_bDdeInfoSet;
    BOOL m_bNoAsync;
    BOOL m_bNoExecPidl;
    BOOL m_bNoResolve;
    BOOL m_bAlreadyQueriedClassStore;
    BOOL m_bInheritHandles;
    BOOL m_bIsNamespaceObject;
    BOOL m_bWaitForInputIdle;
    BOOL m_bUseNullCWD;
    BOOL m_bInvokeIDList;
    BOOL m_bCloseProcess;

    CShellExecute()
    {
        ZeroMemory(this, sizeof(*this));
        m_cRefs = 1;
    }
    ~CShellExecute();

    void ExecuteNormal(LPSHELLEXECUTEINFOW sei);
    DWORD Finalize(LPSHELLEXECUTEINFOW sei);

public:
    STDMETHODIMP_(ULONG) Release()
    {
        LONG refs = InterlockedDecrement(&m_cRefs);
        if (!refs)
            delete this;
        return refs;
    }

    void* operator new(size_t count)
    {
        return CoTaskMemAlloc(count);
    }

    void operator delete(void* ptr)
    {
        CoTaskMemFree(ptr);
    }

protected:
    void _SetMask(DWORD fMask);
    void _SetStartup(const SHELLEXECUTEINFOW* pInfo);
    void _Init(LPSHELLEXECUTEINFOW sei);
    void _SetWorkingDir(LPCWSTR lpDirectory);
    void _SetFile(LPCWSTR lpFile, BOOL bURLIsTrailing);

    IRET _TryValidateUNC(LPCWSTR pszFile, LPSHELLEXECUTEINFOW sei, LPITEMIDLIST pidl);
    IRET _TryHooks(LPSHELLEXECUTEINFOW sei);
    IRET _DoExecPidl(LPSHELLEXECUTEINFOW sei, LPCITEMIDLIST pidl);
    IRET _TryInvokeApplication(BOOL bInvoke);
    IRET _PerfPidl(LPITEMIDLIST *ppidl);
    IRET _TryExecPidl(LPSHELLEXECUTEINFOW sei, LPITEMIDLIST pidl);
    BOOL _ShouldExecPidl(LPCITEMIDLIST pidl) const;
    IRET _ZoneCheckFile(LPCWSTR pszPath);
    IRET _VerifyZoneTrust(LPCWSTR pszPath);
    IRET _InitAssociations(LPSHELLEXECUTEINFOW sei, LPCITEMIDLIST pidl);
    IRET _ShouldRetryWithNewClassKey(BOOL bParse);
    IRET _SetCmdTemplate(BOOL bParse);
    IRET _MaybeInstallApp(BOOL bParse);
    IRET _SetDarwinCmdTemplate(BOOL bParse);
    IRET _VerifyExecTrust(LPSHELLEXECUTEINFOW sei);
    IRET _RetryAsync();
    IRET _VerifySaferTrust(LPCWSTR szFullPathname);

    DWORD _TryWowShellExec();
    BOOL _SetCommand();
    BOOL _ShellExecPidl(LPSHELLEXECUTEINFOW sei, LPCITEMIDLIST pidl);
    BOOL _Resolve();
    BOOL _TryResolve(LPCITEMIDLIST pidl);
    BOOL _ShouldTryResolve(LPCITEMIDLIST pidl) const;
    BOOL _ReportWin32(DWORD dwError);
    BOOL _ReportHinst(HINSTANCE hInstApp);
    BOOL _CheckForRegisteredProgram();
    BOOL _ExecMayCreateProcess();
    HRESULT _InitClassAssociations(LPCWSTR lpClass, HKEY hkeyClass, DWORD fMask);
    HRESULT _InitShellAssociations(LPCWSTR pszPath, LPCITEMIDLIST pidl);
    IBindCtx* _PerfBindCtx();

    DWORD _GetCreateFlags(DWORD fMask);
    DWORD _MapHINSTToWin32Err(HINSTANCE hInstApp);
    DWORD _FinalMapError(HINSTANCE* phInstApp);
    HINSTANCE _MapWin32ErrToHINST(DWORD dwError);
    void _SetFileAndUrl();
    void _DoExecCommand();
    void _FixActivationStealingApps(HWND hWnd, INT nCmdShow);
    void _NotifyShortcutInvoke();
    HRESULT _BuildEnvironmentForNewProcess(PCWSTR pszEnvEntry);
    BOOL _FileIsApp();
    HWND _CreateHiddenDDEWindow(HWND hWnd);
    HWND _GetConversationWindow(HWND hwnd);
    BOOL _TryDDEShortCircuit(HWND hWnd, HGLOBAL hDdeCommand, INT nCmdShow);
    IRET _DDEExecute(BOOL bFlag, HWND hWnd, INT nCmdShow, BOOL bNoAsync);
    HGLOBAL _CreateDDECommand(INT nCmdShow, BOOL bLongNameOK, BOOL bUnicode);
    BOOL _PostDDEExecute(HWND hHiddenWnd, HWND hConvWnd, UINT_PTR uiHi, HANDLE hDoneEvent);
    void _DestroyHiddenDDEWindow(HWND hWnd);
    HRESULT _EvaluateTemplate(LPBOOL pbLongNameOK);
    BOOL _IsAppLFNAware();
    BOOL _SetAppRunAsCmdTemplate();
    BOOL _SetDDEInfo();
    IRET _TryExecDDE();

    HRESULT _QueryString(
        ASSOCF    assocFlags,
        ASSOCSTR  assocStr,
        LPWSTR    pszDest,
        INT       cchDest);

    DWORD _InvokeAppThreadProc();
    static DWORD CALLBACK s_InvokeAppThreadProc(PVOID pThis);
};

struct ERROR_INST_PAIR
{
    DWORD dwError;
    DWORD dwInst;
};
static const ERROR_INST_PAIR g_ErrorInstPairs[] =
{
    { ERROR_SHARING_VIOLATION, SE_ERR_SHARE },
    { ERROR_INVALID_DRIVE, SE_ERR_OOM },
    { ERROR_BAD_PATHNAME, SE_ERR_PNF },
    { ERROR_BAD_NETPATH, SE_ERR_PNF },
    { ERROR_PATH_BUSY, SE_ERR_PNF },
    { ERROR_NO_NET_OR_BAD_PATH, SE_ERR_PNF },
    { ERROR_OLD_WIN_VERSION, 10 },
    { ERROR_APP_WRONG_OS, 12 },
    { ERROR_RMODE_APP, 15 },
    { ERROR_SINGLE_INSTANCE_APP, 16 },
    { ERROR_INVALID_DLL, 20 },
    { ERROR_NO_ASSOCIATION, SE_ERR_NOASSOC },
    { ERROR_DDE_FAIL, SE_ERR_DDEFAIL },
    { ERROR_DDE_FAIL, SE_ERR_DDEBUSY },
    { ERROR_DDE_FAIL, SE_ERR_DDETIMEOUT },
    { ERROR_DLL_NOT_FOUND, 32 },
};

EXTERN_C HINSTANCE g_hinst = GetModuleHandleW(NULL);

EXTERN_C GUID POLID_PreXPSP2ShellProtocolBehavior;

WOWSHELLEXECHOOKPROC g_fnWowShellExecCB = NULL;

static LPWSTR GetEnvBlock(HANDLE hToken)
{
    if (!hToken)
        return GetEnvironmentStringsW();

    LPVOID Environment = NULL;
    CreateEnvironmentBlock(&Environment, hToken, TRUE);
    return (LPWSTR)Environment;
}

static VOID FreeEnvBlock(HANDLE hUserToken, LPWSTR pEnv)
{
    if (!pEnv)
        return;

    if (hUserToken)
        DestroyEnvironmentBlock(pEnv);
    else
        FreeEnvironmentStringsW(pEnv);
}

// Initializes the environment variable block with the given character count capacity.
HRESULT CEnvironmentBlock::_InitBlock(INT cchBlock)
{
    const INT cchRequired = cchBlock + _BlockLenCached();
    if (cchRequired <= m_cchAlloc)
        return S_OK;

    const DWORD cchNewAlloc = ((m_cchAlloc + cchBlock + 265) >> 8) << 8; // Align to 256

    if (m_pszzBlock)
    {
        SIZE_T cbNewAlloc = cchNewAlloc * sizeof(WCHAR);
        LPWSTR pszNewText = (LPWSTR)LocalReAlloc(m_pszzBlock, cbNewAlloc, LMEM_MOVEABLE);
        if (pszNewText)
        {
            m_cchAlloc  = cchNewAlloc - 10;
            m_pszzBlock = pszNewText;
        }
    }
    else
    {
        LPWSTR pEnvBlock = GetEnvBlock(m_hUserToken);
        if (pEnvBlock)
        {
            const INT cchOldBlock = _BlockLen(pEnvBlock);
            const DWORD cchInitAlloc = ((cchOldBlock + cchBlock + 265) >> 8) << 8;

            LPWSTR pszNewBlock = (LPWSTR)LocalAlloc(LPTR, cchInitAlloc * sizeof(WCHAR));
            m_pszzBlock = pszNewBlock;

            if (pszNewBlock)
            {
                CopyMemory(pszNewBlock, pEnvBlock, cchOldBlock * sizeof(WCHAR));
                m_cchAlloc = cchInitAlloc - 10;
                m_cchBlock = cchOldBlock;
            }

            FreeEnvBlock(m_hUserToken, pEnvBlock);
        }
    }

    if (m_cchAlloc < cchRequired)
        return E_OUTOFMEMORY;

    return S_OK;
}

// Searches the environment block for a variable by name and returns a pointer to it.
BOOL CEnvironmentBlock::_FindVar(PCNZWCH pszName, INT cchName, LPWSTR *ppch)
{
    LPWSTR pch = this->m_pszzBlock;
    INT iComp = CSTR_LESS_THAN; // 初期値 = 1 (元コードの 1)

    if (*pch)
    {
        while (iComp == CSTR_LESS_THAN)
        {
            iComp = CompareStringW(LOCALE_SYSTEM_DEFAULT, NORM_IGNORECASE, pch, cchName, pszName, cchName);

            *ppch = pch;

            pch += lstrlenW(pch) + 1;

            if (!*pch)
            {
                if (iComp == CSTR_LESS_THAN)
                    *ppch = pch;

                break;
            }
        }
    }
    else
    {
        *ppch = pch;
    }

    return (iComp == CSTR_EQUAL);
}

// Returns the cached total length of the environment block in characters.
INT CEnvironmentBlock::_BlockLenCached()
{
    if (!m_cchBlock && m_pszzBlock)
        m_cchBlock = _BlockLen(m_pszzBlock);
    return m_cchBlock;
}

// Calculates the total length of the environment block starting from the given pointer.
INT CEnvironmentBlock::_BlockLen(LPCWSTR pchStart)
{
    LPCWSTR pch;
    for (pch = pchStart; *pch; pch += lstrlenW(pch) + 1)
        ;
    return pch - pchStart + 1;
}

// Updates an environment variable in the block, optionally appending or prepending the value with a separator.
HRESULT CEnvironmentBlock::UpdateVar(
    LPCWSTR pszName,
    WCHAR chSep,
    LPCWSTR pszValue,
    BOOL bAppend)
{
    const INT cchName  = lstrlenW(pszName);
    const INT cchValue = lstrlenW(pszValue);
    HRESULT hr = _InitBlock(cchName + cchValue + 3);
    if (FAILED(hr))
    {
        return hr;
    }

    LPWSTR pchVarStart = NULL;

    if (!_FindVar(pszName, cchName, &pchVarStart))
        return SetVar(pszName, pszValue);

    const INT cchTotalBlock = _BlockLenCached();

    if (bAppend)
    {
        const INT cchCurrentVar = lstrlenW(pchVarStart);
        LPWSTR pchVarEnd = pchVarStart + cchCurrentVar;

        LPWSTR pchDest = pchVarEnd + cchValue + 1;
        const INT cchRemaining = cchTotalBlock - (pchVarEnd - m_pszzBlock);
        MoveMemory(pchDest, pchVarEnd, cchRemaining * sizeof(WCHAR));

        *pchVarEnd = chSep;
        StrCpyW(pchVarEnd + 1, pszValue);
    }
    else
    {
        LPWSTR pchOldValueStart = pchVarStart + cchName + 1; 

        LPWSTR pchDest = pchOldValueStart + cchValue + 1;
        const INT cchRemaining = cchTotalBlock - (pchOldValueStart - m_pszzBlock);
        MoveMemory(pchDest, pchOldValueStart, cchRemaining * sizeof(WCHAR));

        StrCpyW(pchOldValueStart, pszValue);
        pchOldValueStart[cchValue] = chSep;
    }

    m_cchBlock += (cchValue + 1);
    return S_OK;
}

// Sets an environment variable to the specified value in the block.
HRESULT CEnvironmentBlock::SetVar(LPCWSTR pszName, LPCWSTR pszValue)
{
    const INT cchName     = lstrlenW(pszName);
    const INT cchNewValue = lstrlenW(pszValue);
    const INT cchTotalRequired = cchName + cchNewValue + 2;

    HRESULT hr = _InitBlock(cchTotalRequired);
    if (FAILED(hr))
        return hr;

    LPWSTR pchStart = NULL;

    if (_FindVar(pszName, cchName, &pchStart))
    {
        LPWSTR pchOldValue = StrChrW(pchStart, L'=') + 1;
        const INT cchOldValue = lstrlenW(pchOldValue);

        LPWSTR pchNextVarDest = &pchOldValue[cchNewValue + 1];
        LPWSTR pchNextVarSrc  = &pchOldValue[cchOldValue + 1];

        const INT cchTotalBlock = _BlockLenCached();
        const INT cchRemaining  = cchTotalBlock - (pchNextVarSrc - m_pszzBlock);
        MoveMemory(pchNextVarDest, pchNextVarSrc, cchRemaining * sizeof(WCHAR));

        StrCpyW(pchOldValue, pszValue);

        m_cchBlock += (cchNewValue - cchOldValue);
    }
    else
    {
        const INT cchTotalBlock = _BlockLenCached();
        const INT cchRemaining  = cchTotalBlock - (pchStart - this->m_pszzBlock);
        MoveMemory(&pchStart[cchTotalRequired], pchStart, cchRemaining * sizeof(WCHAR));

        StrCpyW(pchStart, pszName);

        LPWSTR pchEqual = &pchStart[cchName];
        *pchEqual = L'=';

        StrCpyW(pchEqual + 1, pszValue);

        m_cchBlock += cchTotalRequired;
    }

    return S_OK;
}

CShellExecute::~CShellExecute()
{
    if (m_hHandle1)
        CloseHandle(m_hHandle1);
    if (m_dwError && m_hDdeData)
    {
        DWORD dwPID = GetCurrentProcessId();
        SHFreeShared(m_hDdeData, dwPID);
    }
    if (m_wDdeTopicAtom)
        GlobalDeleteAtom(m_wDdeTopicAtom);
    if (m_wDdeAppAtom)
        GlobalDeleteAtom(m_wDdeAppAtom);
    if (m_pQueryAssoc)
        m_pQueryAssoc->Release();
    if (m_pi.hProcess)
        CloseHandle(m_pi.hProcess);
    if (m_pi.hThread)
        CloseHandle(m_pi.hThread);
    if (m_hParameters)
        LocalFree(m_hParameters);
    if (m_hTitle)
        LocalFree(m_hTitle);
    ILFree(m_pidlParsed);
    if (m_EnvBlock.m_pszzBlock)
        LocalFree(m_EnvBlock.m_pszzBlock);
}

// Builds the process creation flags from the given shell execute mask flags.
DWORD CShellExecute::_GetCreateFlags(DWORD fMask)
{
    DWORD ret = CREATE_DEFAULT_ERROR_MODE | CREATE_UNICODE_ENVIRONMENT;
    if (fMask & SEE_MASK_FLAG_SEPVDM)
        ret |= CREATE_SEPARATE_WOW_VDM;
    if (!(fMask & SEE_MASK_NO_CONSOLE))
        ret |= CREATE_NEW_CONSOLE;
    return ret;
}

// Parses the SEE_MASK flags and sets the corresponding member variables for the execution context.
void CShellExecute::_SetMask(DWORD fMask)
{
    m_bSubst              = (fMask & SEE_MASK_DOENVSUBST);
    m_bNoUI               = (fMask & SEE_MASK_FLAG_NO_UI);
    m_bLogUsage           = (fMask & SEE_MASK_FLAG_LOG_USAGE);
    m_bNoQueryClassStore  = (fMask & SEE_MASK_NOQUERYCLASSSTORE) || !IsOS(OS_DOMAINMEMBER);
    m_bNoAsync            = (fMask & SEE_MASK_NOASYNC);
    m_bWaitForInputIdle   = (fMask & SEE_MASK_WAITFORINPUTIDLE);

    DWORD dwClassKeyFlags = (fMask & SEE_MASK_CLASSKEY);
    m_bUseClass           = (dwClassKeyFlags == SEE_MASK_CLASSNAME) ||
                            (dwClassKeyFlags == SEE_MASK_CLASSKEY);
    m_bInvokeIDList       = (fMask & SEE_MASK_INVOKEIDLIST) == SEE_MASK_INVOKEIDLIST;
    m_dwCreationFlags     = _GetCreateFlags(fMask);
    m_uValidateUNC = !!(fMask & SEE_MASK_CONNECTNETDRV);
    if (m_bNoUI)
        m_uValidateUNC |= 2;

    m_bNoExecPidl = !!(fMask & (SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC |
                                SEE_MASK_UNKNOWN_0x1000 | 0x400000));
}

// Populates the STARTUPINFOW structure from the shell execute info.
void CShellExecute::_SetStartup(const SHELLEXECUTEINFOW* pInfo)
{
    m_si.cb = sizeof(STARTUPINFOW);
    m_si.dwFlags |= STARTF_USESHOWWINDOW;
    m_si.wShowWindow = (WORD)pInfo->nShow;
    m_si.lpTitle = m_pszTitle;

    if (pInfo->fMask & SEE_MASK_HASIDLIST)
        m_si.lpReserved = (LPWSTR)pInfo->hInstApp;

    if ((pInfo->fMask & SEE_MASK_HASLINKNAME) && m_pszTitle)
        m_si.dwFlags |= STARTF_TITLEISLINKNAME;

    if (pInfo->fMask & SEE_MASK_HOTKEY)
    {
        m_si.dwFlags |= STARTF_USEHOTKEY;
        m_si.hStdInput = UlongToHandle(pInfo->dwHotKey);
    }

    HANDLE hMonitorOrIcon = NULL;
    if (pInfo->fMask & (SEE_MASK_ICON | SEE_MASK_HMONITOR))
        hMonitorOrIcon = (HANDLE)pInfo->hIcon;
    else if (pInfo->hwnd)
        hMonitorOrIcon = (HANDLE)MonitorFromWindow(pInfo->hwnd, MONITOR_DEFAULTTONEAREST);

    if (hMonitorOrIcon)
    {
        m_si.dwFlags |= STARTF_USEMONITOR;
        m_si.hStdOutput = hMonitorOrIcon;
    }
}

// Initializes the CShellExecute object from the provided SHELLEXECUTEINFOW.
void CShellExecute::_Init(LPSHELLEXECUTEINFOW sei)
{
    TRACE("\n");
    _SetMask(sei->fMask);

    m_pszParameters = (LPWSTR)sei->lpParameters;
    m_lpIDList = (sei->fMask & SEE_MASK_INVOKEIDLIST) ? (LPITEMIDLIST)sei->lpIDList : NULL;

    if ((sei->fMask & (SEE_MASK_HASTITLE | SEE_MASK_HASLINKNAME)))
        m_pszTitle = (LPWSTR)sei->lpClass;
    else
        m_pszTitle = NULL;

    m_bActivateHandler = TRUE;

    m_pszVerb = NULL;
    if (sei->lpVerb && *sei->lpVerb)
    {
        SHUnicodeToUnicode(sei->lpVerb, m_szVerb, _countof(m_szVerb));
        m_pszVerb = m_szVerb;

        if (lstrcmpiW(sei->lpVerb, L"runas") == 0)
            m_EnvBlock.m_nRunAs = 4;
    }

    m_hWnd = sei->hwnd;
    m_wShowWindow = sei->nShow;
    sei->hProcess = NULL;

    _SetStartup(sei);
}

// Resolves and validates the working directory.
void CShellExecute::_SetWorkingDir(LPCWSTR lpDirectory)
{
    TRACE("\n");
    INT iDrive;

    if (!lpDirectory || !*lpDirectory)
    {
        if (m_EnvBlock.m_nRunAs)
        {
            m_bUseNullCWD = TRUE;
            return;
        }

        GetCurrentDirectoryW(_countof(m_szWorkDir), m_szWorkDir);
        goto VALIDATE;
    }

    StrCpyNW(m_szWorkDir, lpDirectory, _countof(m_szWorkDir));
    if (m_bSubst)
        DoEnvironmentSubstW(m_szWorkDir, _countof(m_szWorkDir));

    if (PathIsDirectoryW(m_szWorkDir) && lstrcmpW(m_szWorkDir, L".\\") != 0)
        return;

    iDrive = PathGetDriveNumberW(m_szWorkDir);
    if (iDrive >= 0)
        PathStripToRootW(m_szWorkDir);
    else
        GetCurrentDirectoryW(_countof(m_szWorkDir), m_szWorkDir);

VALIDATE:
    if (!PathIsDirectoryW(m_szWorkDir))
        GetWindowsDirectoryW(m_szWorkDir, _countof(m_szWorkDir));
}

// Sets the target file path, detects whether it is a URL, and optionally captures a trailing URL from the buffer.
void CShellExecute::_SetFile(LPCWSTR lpFile, BOOL bURLIsTrailing)
{
    TRACE("\n");
    if (lpFile && *lpFile)
    {
        m_bIsURL = UrlIsW(lpFile, URLIS_URL);

        StrCpyNW(m_szPath, lpFile, _countof(m_szPath));

        m_bIsNamespaceObject = (!m_bInvokeIDList && !m_bUseClass &&
                                _IsNamespaceObject(m_szPath));

        if (m_bSubst)
            DoEnvironmentSubstW(m_szPath, _countof(m_szPath));

        if (bURLIsTrailing)
        {
            LPCWSTR pszTrailingUrl = &lpFile[lstrlenW(lpFile) + 1];

            if (!IsBadStringPtrW(pszTrailingUrl, _countof(m_szURL)) &&
                PathIsURLW(pszTrailingUrl))
            {
                StrCpyNW(m_szURL, pszTrailingUrl, _countof(m_szURL));
            }
        }
    }
    else if (!m_lpIDList)
    {
        StrCpyNW(m_szPath, m_szWorkDir, _countof(m_szPath));
    }

    PathUnquoteSpacesW(m_szPath);
}

// Stores a Win32 error code and returns TRUE if the error is non-zero.
BOOL CShellExecute::_ReportWin32(DWORD dwError)
{
    m_dwError = dwError;
    return dwError != ERROR_SUCCESS;
}

// Creates and returns a bind context for use during PIDL parsing (stub; returns NULL).
IBindCtx* CShellExecute::_PerfBindCtx()
{
    return NULL; // FIXME
}

// Resolves or uses the existing PIDL for the target path, populating shell attributes and the output PIDL pointer.
IRET CShellExecute::_PerfPidl(LPITEMIDLIST *ppidl)
{
    TRACE("\n");
    *ppidl = m_lpIDList;

    if (m_lpIDList)
    {
        m_attrs = SFGAO_STORAGECAPMASK;

        HRESULT hr;
        if (!m_szPath[0])
        {
            hr = SHGetNameAndFlagsW(m_lpIDList, SHGDN_FORPARSING,
                                    m_szPath, _countof(m_szPath), &m_attrs);
            if (SUCCEEDED(hr))
            {
                if (!(m_attrs & SFGAO_STREAM))
                    m_szPath[0] = UNICODE_NULL;
            }
        }
        else
        {
            hr = SHGetNameAndFlagsW(m_lpIDList, 0, NULL, 0, &m_attrs);
        }

        if (FAILED(hr))
            m_attrs = 0;

        return IRET_1;
    }

    IBindCtx *pBC = _PerfBindCtx();
    HRESULT hr = SHParseDisplayName(m_szPath, pBC, &m_pidlParsed,
                                    SFGAO_STORAGECAPMASK, &m_attrs);
    if (pBC)
        pBC->Release();

    const BOOL bIsURL      = UrlIsW(m_szPath, URLIS_URL);
    const BOOL bIsFileURL  = UrlIsW(m_szPath, URLIS_FILEURL);
    const BOOL bAllowError = !_FailForceReturn(hr) ||
                             SHWindowsPolicy(POLID_PreXPSP2ShellProtocolBehavior, NULL);
    const BOOL bAllowHR    = (SUCCEEDED(hr) || pBC || !bIsFileURL);
    const BOOL bAllowInvalid = (hr != E_INVALIDARG || _ProcessAllowsInvalidUrls());

    if (!bIsURL || (bAllowError && bAllowHR && bAllowInvalid))
    {
        m_lpIDList = m_pidlParsed;
        *ppidl     = m_pidlParsed;
        return IRET_1;
    }

    DWORD dwError = ERROR_FILE_NOT_FOUND;
    if (HRESULT_FACILITY(hr) == FACILITY_WIN32)
        dwError = HRESULT_CODE(hr);

    _ReportWin32(dwError);
    return IRET_0;
}

// Validates a UNC path by checking filesystem attributes or prompting for a network connection.
IRET CShellExecute::_TryValidateUNC(LPCWSTR pszFile, LPSHELLEXECUTEINFOW sei, LPITEMIDLIST pidl)
{
    TRACE("\n");
    if (!PathIsUNCW(pszFile))
        return IRET_2;

    if ((m_attrs & SFGAO_FILESYSTEM) || SHValidateUNC(m_hWnd, (LPWSTR)pszFile, m_uValidateUNC))
        return IRET_2;

    const DWORD dwError = GetLastError();
    if (dwError == ERROR_NOT_SUPPORTED && sei && PathIsUNCW(pszFile))
    {
        if (SHValidateUNC(m_hWnd, (LPWSTR)pszFile, 6))
            return _DoExecPidl(sei, pidl);

        SetLastError(ERROR_NOT_SUPPORTED);
    }

    if (sei)
        _ReportWin32(GetLastError());

    return IRET_0;
}

// Records the application instance handle, mapping legacy HINSTANCE error codes to Win32 errors when necessary.
BOOL CShellExecute::_ReportHinst(HINSTANCE hInstApp)
{
    if (HandleToUlong(hInstApp) <= SE_ERR_DLLNOTFOUND && hInstApp)
    {
        DWORD dwError = _MapHINSTToWin32Err(hInstApp);
        return _ReportWin32(dwError);
    }

    m_hInstApp = hInstApp;
    return FALSE;
}

// Invokes registered shell execute hooks and returns IRET_0 if a hook handled the execution, IRET_2 otherwise.
IRET CShellExecute::_TryHooks(LPSHELLEXECUTEINFOW sei)
{
    TRACE("\n");
    DWORD error = ERROR_FILE_NOT_FOUND;
    if (!(sei->fMask & SEE_MASK_NO_HOOKS) && TryShellExecuteHooks(sei) != S_FALSE)
    {
        _ReportHinst(sei->hInstApp);
        return IRET_0;
    }
    return IRET_2;
}

// Looks up a registered application path for a bare filename and updates the path if a valid executable is found.
BOOL CShellExecute::_CheckForRegisteredProgram()
{
    WCHAR szPath[MAX_PATH];

    if (PathFindFileNameW(m_szPath) != m_szPath ||
        !PathToAppPath(m_szPath, szPath) ||
        !PathResolve(szPath, NULL, PRF_CHECKEXISTANCE | PRF_EXECUTABLE) ||
        PathIsDirectoryW(szPath) )
    {
        return FALSE;
    }

    StrCpyNW(m_szPath, szPath, _countof(m_szPath));
    return TRUE;
}

// Resolves the target path to an absolute executable path, guessing a URL scheme as a fallback.
BOOL CShellExecute::_Resolve()
{
    PCWSTR dirs[2] = { m_szWorkDir, NULL };

    if (m_bNoResolve || m_bIsURL || m_bIsNamespaceObject || _CheckForRegisteredProgram() ||
        PathResolve(m_szPath, dirs, PRF_REQUIREABSOLUTE | PRF_FIRSTDIRDEF |
                                    PRF_TRYPROGRAMEXTENSIONS))
    {
        return TRUE;
    }

    DWORD cchOut = _countof(m_szPath);
    if (UrlApplySchemeW(m_szPath, m_szPath, &cchOut, URL_APPLY_GUESSSCHEME) == S_OK)
    {
        m_bIsURL = TRUE;
        return TRUE;
    }

    m_dwError = ERROR_FILE_NOT_FOUND;
    return FALSE;
}

// Obtains an IContextMenu from the given PIDL and invokes the shell execute verb in-process.
BOOL CShellExecute::_ShellExecPidl(LPSHELLEXECUTEINFOW sei, LPCITEMIDLIST pidl)
{
    IContextMenu *pContextMenu = NULL;
    HRESULT hr = SHGetUIObjectFromFullPIDL(pidl, sei->hwnd,
                                           IID_IContextMenu, (PVOID*)&pContextMenu);
    if (SUCCEEDED(hr))
    {
        hr = _InvokeInProcExec(pContextMenu, sei);
        pContextMenu->Release();
    }

    if (FAILED(hr))
    {
        DWORD dwError = (HRESULT_FACILITY(hr) == FACILITY_WIN32)
                        ? HRESULT_CODE(hr)
                        : GetLastError();

        if (!dwError)
            dwError = ERROR_ACCESS_DENIED;

        if (dwError != ERROR_CANCELLED)
            _ReportWin32(dwError);
    }

    return SUCCEEDED(hr);
}

// Executes the shell verb on a PIDL, creating a PIDL from the path if one is not supplied.
IRET CShellExecute::_DoExecPidl(LPSHELLEXECUTEINFOW sei, LPCITEMIDLIST pidl)
{
    LPITEMIDLIST pidlTemp = NULL;
    if (!pidl)
    {
        pidlTemp = ILCreateFromPathW(m_szPath);
        pidl = pidlTemp;
        if (!pidlTemp)
            return IRET_1;
    }
    _ShellExecPidl(sei, pidl);
    if (pidlTemp)
        ILFree(pidlTemp);
    return IRET_0;
}

BOOL CShellExecute::_TryResolve(LPCITEMIDLIST pidl)
{
    return pidl || m_bNoResolve || _Resolve();
}

BOOL CShellExecute::_ShouldTryResolve(LPCITEMIDLIST pidl) const
{
    return (m_szPath[0] || pidl) && (!m_bUseClass || m_bInvokeIDList || m_bIsNamespaceObject);
}

BOOL CShellExecute::_ShouldExecPidl(LPCITEMIDLIST pidl) const
{
    if ((!m_pszVerb || !lstrcmpiW(m_pszVerb, L"open")) && !m_bNoExecPidl)
        return TRUE;
    if (m_bIsURL || m_bInvokeIDList || m_bIsNamespaceObject || (m_attrs & SFGAO_LINK))
        return TRUE;
    return !pidl && PathIsShortcut(m_szPath, INVALID_FILE_ATTRIBUTES);
}

// Determines whether the target should be executed via its PIDL (namespace object, URL, link, etc.) and does so if appropriate.
IRET CShellExecute::_TryExecPidl(LPSHELLEXECUTEINFOW sei, LPITEMIDLIST pidl)
{
    TRACE("\n");

    if (!_ShouldTryResolve(pidl))
        return IRET_2;

    if (!_TryResolve(pidl))
        return IRET_0;

    if (_ShouldExecPidl(pidl))
        return _DoExecPidl(sei, pidl);

    return IRET_2;
}

// Verifies the execution trust level of the given path using Safer/Software Restriction Policies.
IRET CShellExecute::_VerifySaferTrust(LPCWSTR szFullPathname)
{
    return IRET_0; // FIXME
}

// Performs a security zone check on the file path and returns the appropriate IRET based on the zone policy.
IRET CShellExecute::_ZoneCheckFile(LPCWSTR pszPath)
{
    DWORD policy = 0, context = 0;
    UINT uFlags = 0x403;
    ZoneCheckUrlExW(pszPath, (PBYTE)&policy, sizeof(policy),
                    (PBYTE)&context, sizeof(context), 0x1806, uFlags, NULL);

    switch (policy & URLPOLICY_MASK_PERMISSIONS)
    {
        case URLPOLICY_ALLOW:
            return IRET_2;
        case URLPOLICY_QUERY:
            if (SafeOpenPromptForShellExec(m_hWnd, pszPath))
                return IRET_1;
            m_dwError = ERROR_CANCELLED;
            break;
        case URLPOLICY_DISALLOW:
            m_dwError = ERROR_ACCESS_DENIED;
            break;
        default:
            break;
    }

    return IRET_0;
}

// Checks whether the file's extension is dangerous and, if so, performs zone trust checks on the path.
IRET CShellExecute::_VerifyZoneTrust(LPCWSTR pszPath)
{
    LPCWSTR pszExt = PathFindExtensionW(m_szPath);
    if (!AssocIsDangerous(pszExt))
        return IRET_2;

    IRET iret = _ZoneCheckFile(m_szPath);
    if (iret == IRET_2 && pszPath != m_szPath)
        iret = _ZoneCheckFile(pszPath);

    return iret;
}

// Verifies execution trust for the target by dispatching to zone or safer checks depending on the item type.
IRET CShellExecute::_VerifyExecTrust(LPSHELLEXECUTEINFOW sei)
{
    TRACE("\n");
    IRET iret = IRET_2;

    const BOOL bIsFile    = !m_lpIDList && _PathIsFile(m_szPath);
    const BOOL bIsContent = (m_attrs & (SFGAO_FILESYSTEM | SFGAO_FOLDER | SFGAO_STREAM)) ==
                            (SFGAO_FILESYSTEM | SFGAO_STREAM);

    if (bIsFile || bIsContent)
    {
        LPCWSTR pszVerifyPath = (sei->fMask & SEE_MASK_HASLINKNAME) && m_pszTitle
                                ? m_pszTitle
                                : m_szPath;

        BOOL bZoneCheck = !(sei->fMask & SEE_MASK_NOZONECHECKS);
        if (bZoneCheck)
        {
            if (GetEnvironmentVariableW(L"SEE_MASK_NOZONECHECKS", m_szEnvEntry, _countof(m_szEnvEntry)))
                bZoneCheck = lstrcmpiW(m_szEnvEntry, L"1") != 0;
        }

        if (!bZoneCheck)
            return _VerifySaferTrust(pszVerifyPath);

        iret = _VerifyZoneTrust(pszVerifyPath);
        if (iret == IRET_2)
            return _VerifySaferTrust(pszVerifyPath);
    }
    else if ((m_attrs & SFGAO_FOLDER) || !m_lpIDList)
    {
        if (!(sei->fMask & SEE_MASK_HASLINKNAME))
            return iret;

        LPITEMIDLIST pidlFolder = m_lpIDList;
        SFGAOF psfgaoOut = SFGAO_FOLDER;
        BOOL bFreePidl = FALSE;

        if (!pidlFolder)
        {
            if (!m_szPath[0])
                return iret;

            if (FAILED(SHParseDisplayName(m_szPath, NULL, &pidlFolder, SFGAO_FOLDER, &psfgaoOut)))
                return IRET_0;

            bFreePidl = TRUE;
        }

        if (IsFolderGUID(NULL, pidlFolder))
        {
            DWORD policy = 0, context = 0;
            UINT uFlags = PUAF_FORCEUI_FOREGROUND | PUAF_ISFILE;
#undef URLACTION_SHELL_SHELLEXECUTE
#define URLACTION_SHELL_SHELLEXECUTE 0x00001806
            HRESULT hr = ZoneCheckUrlExW(m_szPath, (PBYTE)&policy, sizeof(policy),
                                         (PBYTE)&context, sizeof(context),
                                         URLACTION_SHELL_SHELLEXECUTE, uFlags, NULL);
            iret = (hr == S_OK) ? IRET_1 : IRET_0;
        }
        else
        {
            iret = IRET_1;
        }

        if (bFreePidl)
            ILFree(pidlFolder);
    }

    return iret;
}

// Creates and initializes an IQueryAssociations object from an explicit class name or registry key.
HRESULT CShellExecute::_InitClassAssociations(LPCWSTR lpClass, HKEY hkeyClass, DWORD fMask)
{
    TRACE("\n");
    HRESULT hr = AssocCreate(CLSID_QueryAssociations,
                             IID_IQueryAssociations, (PVOID*)&m_pQueryAssoc);
    if (FAILED(hr))
        return hr;

    if ((fMask & SEE_MASK_CLASSKEY) == SEE_MASK_CLASSKEY)
        return m_pQueryAssoc->Init(ASSOCF_NONE, NULL, hkeyClass, NULL);

    if ((fMask & SEE_MASK_CLASSKEY) == SEE_MASK_CLASSNAME)
        return m_pQueryAssoc->Init(ASSOCF_NONE, lpClass, NULL, NULL);

    return m_pQueryAssoc->Init(ASSOCF_NONE, L"Folder", NULL, NULL);
}

// Initializes shell associations for the target path or PIDL, falling back to extension-based associations if needed.
HRESULT CShellExecute::_InitShellAssociations(LPCWSTR pszPath, LPCITEMIDLIST pidl)
{
    TRACE("\n");
    HRESULT hr = E_FAIL;
    LPITEMIDLIST pidlTemp = NULL;

    if (!*pszPath)
    {
        if (!pidl)
            goto no_pidl;

        SHGetNameAndFlagsW(pidl, SHGDN_FORPARSING, m_szPath, _countof(m_szPath), NULL);
        m_bNoResolve = TRUE;
        goto try_assoc;
    }

    if (!pidl)
    {
        hr = SHILCreateFromPath(pszPath, &pidlTemp, NULL);
        if (FAILED(hr))
            goto no_pidl;

        pidl = pidlTemp;
        if (!pidl)
            goto no_pidl;
    }

try_assoc:
    {
        IBindCtx *pBC = NULL;
        TBCRegisterObjectParam(L"ShellExec SHGetAssociations", NULL, &pBC);
        hr = SHGetAssociations(pidl, &m_pQueryAssoc);
        if (pBC)
            pBC->Release();

        DWORD cbData = 0;
        const BOOL bHasCommand = SUCCEEDED(hr) && (
            SUCCEEDED(m_pQueryAssoc->GetString(ASSOCF_NONE, ASSOCSTR_COMMAND,
                                               m_pszVerb, NULL, &cbData)) ||
            SUCCEEDED(m_pQueryAssoc->GetData(ASSOCF_NONE, ASSOCDATA_MSIDESCRIPTOR,
                                             m_pszVerb, NULL, &cbData))
        );

        if (!bHasCommand)
        {
            if (!m_pQueryAssoc)
                hr = AssocCreate(CLSID_QueryAssociations, IID_PPV_ARGS(&m_pQueryAssoc));

            if (m_pQueryAssoc)
            {
                hr = m_pQueryAssoc->Init(ASSOCF_NONE, L"Unknown", NULL, NULL);
                if (SUCCEEDED(hr) && m_bNoUI)
                    m_bClassStoreOnly = TRUE;
            }
        }
    }
    goto cleanup;

no_pidl:
    {
        const WCHAR *pszExt = PathFindExtensionW(m_szPath);
        if (*pszExt)
            hr = _InitClassAssociations(pszExt, NULL, SEE_MASK_CLASSNAME);
    }

cleanup:
    if (pidlTemp)
        ILFree(pidlTemp);
    return hr;
}

// Selects and invokes the appropriate association initialization method based on the execution context flags.
IRET CShellExecute::_InitAssociations(LPSHELLEXECUTEINFOW sei, LPCITEMIDLIST pidl)
{
    TRACE("\n");
    HRESULT hr;

    if (sei && (m_bUseClass || (!m_szPath[0] && !m_lpIDList)))
        hr = _InitClassAssociations(sei->lpClass, sei->hkeyClass, sei->fMask);
    else
        hr = _InitShellAssociations(m_szPath, pidl ? pidl : m_lpIDList);

    if (FAILED(hr))
    {
        if (PathIsExe(m_szPath))
            hr = S_FALSE;
        else
            m_dwError = ERROR_NO_ASSOCIATION;
    }

    return SUCCEEDED(hr) ? IRET_1 : IRET_0;
}

// Builds a DDE command string from the current execution parameters and allocates it as a global memory handle.
HGLOBAL CShellExecute::_CreateDDECommand(INT nCmdShow, BOOL bLongNameOK, BOOL bUnicode)
{
    HGLOBAL hResult = NULL;

    ShStrW strW;
    if (FAILED(strW.SetSize(0x1088)))
        return NULL;

    const INT expandResult = ReplaceParameters(
        strW,
        strW ? strW.GetBuffLength() : 0,
        m_szPath,
        m_szDdeCommand,
        m_pszParameters,
        nCmdShow,
        &m_si.hStdInput,
        bLongNameOK,
        m_lpIDList,
        &m_hDdeData);
    if (expandResult)
        return NULL;

    if (bUnicode)
    {
        const INT cch = lstrlenW(strW);
        const SIZE_T cb = (cch + 1) * sizeof(WCHAR);

        HGLOBAL hGlobal = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, cb);
        if (hGlobal)
        {
            LPWSTR psz = (LPWSTR)GlobalLock(hGlobal);
            if (psz)
            {
                StrCpyNW(psz, strW, (INT)(cb / sizeof(WCHAR)));
                GlobalUnlock(hGlobal);
                hResult = hGlobal;
            }
            else
            {
                GlobalFree(hGlobal);
            }
        }
    }
    else
    {
        ShStrA strA;
        if (SUCCEEDED(strA.SetStr(strW, -1)))
        {
            const INT cch = lstrlenA(strA);
            const SIZE_T cb = 2 * (cch + 1) * sizeof(CHAR);
            HGLOBAL hGlobal = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, cb);
            if (hGlobal)
            {
                PCHAR psz = (PCHAR)GlobalLock(hGlobal);
                if (psz)
                {
                    lstrcpynA(psz, strA, (INT)(cb / sizeof(CHAR)));
                    GlobalUnlock(hGlobal);
                    hResult = hGlobal;
                }
                else
                {
                    GlobalFree(hGlobal);
                }
            }
        }
    }

    return hResult;
}

// Attempts a fast-path DDE execution by reusing an existing conversation window.
BOOL CShellExecute::_TryDDEShortCircuit(HWND hWnd, HGLOBAL hDdeCommand, INT nCmdShow)
{
    return FALSE; // FIXME
}

// Creates a hidden worker window to serve as the client end of a DDE conversation.
HWND CShellExecute::_CreateHiddenDDEWindow(HWND hWnd)
{
    HWND hwndTopParent = GetTopParentWindow(hWnd);
    return SHCreateWorkerWindowW(DDESubClassWndProc, hwndTopParent, 0, 0, NULL, 0);
}

// Broadcasts WM_DDE_INITIATE to find an existing DDE server window and returns the conversation window handle.
HWND CShellExecute::_GetConversationWindow(HWND hwnd)
{
#ifndef ILMM_IE4
    #define ILMM_IE4 0
#endif
    BOOL bLowMemory = SHIsLowMemoryMachine(ILMM_IE4);
    ULONG_PTR dwResult;
    SendMessageTimeoutW(HWND_BROADCAST,
                        WM_DDE_INITIATE,
                        (WPARAM)hwnd,
                        MAKELPARAM(m_wDdeAppAtom, m_wDdeTopicAtom),
                        SMTO_ABORTIFHUNG,
                        bLowMemory ? 80000 : 30000,
                        &dwResult);
    return (HWND)GetPropW(hwnd, L"ddeconv");
}

// Destroys the hidden DDE worker window if it still exists.
void CShellExecute::_DestroyHiddenDDEWindow(HWND hWnd)
{
    if (IsWindow(hWnd))
        DestroyWindow(hWnd);
}

// Posts a WM_DDE_EXECUTE message to the server window and optionally waits for the command to complete.
BOOL
CShellExecute::_PostDDEExecute(HWND hHiddenWnd, HWND hConvWnd, UINT_PTR uiHi, HANDLE hDoneEvent)
{
    DWORD dwProcessId = 0;
    GetWindowThreadProcessId(hConvWnd, &dwProcessId);
    if (dwProcessId)
        AllowSetForegroundWindow(dwProcessId);

    const LPARAM lParam = PackDDElParam(WM_DDE_EXECUTE, 0, uiHi);
    if (!PostMessageW(hConvWnd, WM_DDE_EXECUTE, (WPARAM)hHiddenWnd, lParam))
        return FALSE;

    const HINSTANCE hInst = Window_GetInstance(hConvWnd);
    _ReportHinst(hInst);

    if (hDoneEvent)
    {
        SetPropW(hHiddenWnd, L"ddeevent", hDoneEvent);
        SHProcessMessagesUntilEventEx(NULL, hDoneEvent, INFINITE, QS_ALLINPUT | 0xFF);
    }
    else if (IsWindow(hHiddenWnd))
    {
        SetTimer(hHiddenWnd, 0x5555, 180000, NULL);
    }

    return TRUE;
}

// Performs a full DDE execution sequence: creates the command, finds the server, and posts the execute message.
IRET CShellExecute::_DDEExecute(BOOL bFlag, HWND hWnd, INT nCmdShow, BOOL bNoAsync)
{
    DWORD  dwError       = ERROR_OUTOFMEMORY;
    int    bSuccess      = 0;
    HANDLE hEvent        = NULL;

    HGLOBAL hDdeCommand = _CreateDDECommand(nCmdShow, TRUE, TRUE);
    if (!hDdeCommand)
    {
        _ReportWin32(dwError);
        return IRET_1;
    }

    if (_TryDDEShortCircuit(hWnd, hDdeCommand, nCmdShow))
    {
        GlobalFree(hDdeCommand);
        return IRET_1;
    }

    if (bNoAsync)
    {
        hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!hEvent)
            goto Cleanup;
    }

    {
        HWND hHiddenWnd = _CreateHiddenDDEWindow(hWnd);
        if (!hHiddenWnd)
            goto CleanupEvent;

        HWND hConvWnd = _GetConversationWindow(hHiddenWnd);
        if (!hConvWnd)
        {
            dwError = ERROR_FILE_NOT_FOUND;
        }
        else
        {
            if (m_bActivateHandler)
                ActivateHandler(hConvWnd, (WPARAM)m_si.hStdInput);

            const BOOL bLongNameAware = Window_IsLFNAware(hConvWnd);
            const BOOL bUnicode       = IsWindowUnicode(hConvWnd);

            if (!bLongNameAware || !bUnicode)
            {
                GlobalFree(hDdeCommand);
                hDdeCommand = NULL;

                if (m_hDdeData)
                {
                    SHFreeShared(m_hDdeData, GetCurrentProcessId());
                    m_hDdeData = NULL;
                }

                hDdeCommand = _CreateDDECommand(nCmdShow, bLongNameAware, bUnicode);
            }

            dwError = ERROR_DDE_FAIL;
            if (_PostDDEExecute(hHiddenWnd, hConvWnd, (UINT_PTR)hDdeCommand, hEvent))
            {
                hDdeCommand = NULL;
                bSuccess    = 1;
                if (!hEvent)
                    hHiddenWnd = NULL;
            }
        }

        _DestroyHiddenDDEWindow(hHiddenWnd);
    }

CleanupEvent:
    if (hEvent)
        CloseHandle(hEvent);

Cleanup:
    if (hDdeCommand)
        GlobalFree(hDdeCommand);

    if (bSuccess)
        return IRET_1;

    if (bFlag && dwError == ERROR_FILE_NOT_FOUND)
    {
        _QueryString(ASSOCF_NONE, ASSOCSTR_DDEIFEXEC, m_szDdeCommand, _countof(m_szDdeCommand));
        return IRET_0;
    }

    _ReportWin32(dwError);
    return IRET_1;
}

// Attempts to execute the command via the registered WOW shell execute callback for 16-bit compatibility.
DWORD CShellExecute::_TryWowShellExec()
{
    ShStrA str1, str2;

    if (!g_fnWowShellExecCB)
        return ERROR_FILE_NOT_FOUND;

    HINSTANCE hinstApp = (HINSTANCE)SE_ERR_OOM;
    if (SUCCEEDED(str1.SetStr(m_szCommand, -1)) && SUCCEEDED(str2.SetStr(m_szWorkDir, -1)))
        hinstApp = g_fnWowShellExecCB(str1, m_si.wShowWindow, str2);

    if (!_ReportHinst(hinstApp))
    {
        if (m_bDdeInfoSet)
            _DDEExecute(FALSE, m_hWnd, m_wShowWindow, m_bNoAsync);
    }

    return ERROR_SUCCESS;
}

// Checks restrictions and UNC validity, then attempts WOW execution before allowing normal process creation.
BOOL CShellExecute::_ExecMayCreateProcess()
{
    TRACE("\n");
    if (SHRestricted(REST_RESTRICTRUN) && RestrictedApp(m_szExecutable2) ||
        SHRestricted(REST_DISALLOWRUN) && DisallowedApp(m_szExecutable2))
    {
        return !_ReportWin32(ERROR_UNHANDLED_ERROR);
    }

    if (_TryValidateUNC(m_szExecutable2, NULL, NULL) <= 0)
        return !_ReportWin32(GetLastError());

    if (_TryWowShellExec() > 0)
        return !_ReportWin32(ERROR_SUCCESS);

    return 0;
}

// Prevents minimized foreground windows from being incorrectly replaced when launching with SW_SHOWMINNOACTIVE.
void CShellExecute::_FixActivationStealingApps(HWND hWnd, INT nCmdShow)
{
    if (nCmdShow == SW_SHOWMINNOACTIVE)
    {
        HWND hwndFore = GetForegroundWindow();
        if (hwndFore != hWnd)
        {
            if (IsIconic(hwndFore))
                SetForegroundWindow(hWnd);
        }
    }
}

// Fires a shell change notification for shortcut invocation (currently a stub pending SHChangeNotify implementation).
void CShellExecute::_NotifyShortcutInvoke()
{
    // FIXME: SHChangeNotify(SHCNE_EXTENDED_EVENT)
}

// Constructs the environment block for the new process, merging the app's PATH, a custom entry, and compat layer.
HRESULT CShellExecute::_BuildEnvironmentForNewProcess(PCWSTR pszEnvEntry)
{
    m_EnvBlock.m_hUserToken = m_hUserToken;

    HRESULT hr = PathToAppPathKey(m_szWorkGroupHelper, m_szAppPathKey, ARRAYSIZE(m_szAppPathKey));
    if (FAILED(hr))
        return hr;

    BOOL bAppend = FALSE;
    DWORD cbData = sizeof(bAppend);
    if (FAILED(SHGetValueW(HKEY_LOCAL_MACHINE, m_szAppPathKey, L"AppendPath", NULL, &bAppend, &cbData)))
        bAppend = FALSE;

    cbData = sizeof(m_szAppPathKey);
    if (SUCCEEDED(SHGetValueW(HKEY_LOCAL_MACHINE, m_szAppPathKey, L"PATH", NULL, m_szAppPathKey, &cbData)))
    {
        hr = m_EnvBlock.UpdateVar(L"PATH", L';', m_szAppPathKey, bAppend);
        if (FAILED(hr))
            return hr;
    }

    if (pszEnvEntry)
    {
        StrCpyNW(m_szEnvEntry, pszEnvEntry, _countof(m_szEnvEntry));
        WCHAR *pch = StrChrW(m_szEnvEntry, L'=');
        if (pch)
        {
            *pch = UNICODE_NULL;
            hr = m_EnvBlock.SetVar(m_szEnvEntry, pch + 1);
            if (FAILED(hr))
                return hr;
        }
    }

    if (SUCCEEDED(TBCGetEnvironmentVariable(L"__COMPAT_LAYER", m_szEnvEntry, ARRAYSIZE(m_szEnvEntry))))
        hr = m_EnvBlock.SetVar(L"__COMPAT_LAYER", m_szEnvEntry);

    return hr;
}

// Returns TRUE if the target path is an executable file being opened with the default or no verb.
BOOL CShellExecute::_FileIsApp()
{
    if (m_szRunAsCommand[0] || !PathIsExe(m_szPath))
        return FALSE;

    return !m_pszVerb || StrCmpIW(m_pszVerb, L"open");
}

// Returns TRUE if the application can handle long file names or if quoting makes LFN handling unnecessary.
BOOL CShellExecute::_IsAppLFNAware()
{
    BOOL ret = App_IsLFNAware(m_szWorkGroupHelper);
    return (!ret ||
            StrChrW(m_szPath, L' ') == NULL ||
            StrChrW(m_szWorkGroupHelper, L' ') == NULL ||
            m_szRunAsCommand[0] == L'"' ||
            StrStrW(m_szRunAsCommand, L"\"%1\"") != NULL);
}

// Expands the run-as command template into application and command-line components, resolving the executable path.
HRESULT CShellExecute::_EvaluateTemplate(LPBOOL pbLongNameOK)
{
    WCHAR szExpanded[MAX_PATH_EX];
    if (ExpandEnvironmentStringsW(L"\"%PROGRAMFILES%\\Common Files\\Adobe\\WorkFlow\\"
                                  L"AdobeWorkGroupHelper.exe \"%1\"\"",
                                  szExpanded, _countof(szExpanded)) &&
        StrCmpIW(m_szRunAsCommand, szExpanded) == 0)
    {
        ExpandEnvironmentStringsW(
            L"%PROGRAMFILES%\\Common Files\\Adobe\\WorkFlow\\AdobeWorkGroupHelper.exe",
            m_szWorkGroupHelper, _countof(m_szWorkGroupHelper));

        DWORD cchFriendlyName = _countof(m_szFriendlyAppName);

        AssocQueryStringW(ASSOCF_VERIFY | ASSOCF_INIT_BYEXENAME,
                          ASSOCSTR_FRIENDLYAPPNAME,
                          m_szWorkGroupHelper,
                          NULL,
                          m_szFriendlyAppName,
                          &cchFriendlyName);

        StringCchCopyW(m_szExecutable2, _countof(m_szExecutable2), m_szWorkGroupHelper);
        return S_OK;
    }

    PWSTR pszApplication = NULL, pszCommandLine = NULL;
    HRESULT hr = WonSHEvaluateSystemCommandTemplate(m_szRunAsCommand, &pszApplication, &pszCommandLine, NULL);
    if (FAILED(hr))
        return hr;

    hr = StringCchCopyW(m_szWorkGroupHelper, _countof(m_szWorkGroupHelper), pszApplication);

    *pbLongNameOK = _IsAppLFNAware();

    StringCchCopyW(m_szRunAsCommand, _countof(m_szRunAsCommand), pszCommandLine);

    _QueryString(ASSOCF_VERIFY, ASSOCSTR_FRIENDLYAPPNAME, m_szFriendlyAppName, _countof(m_szFriendlyAppName));
    _QueryString(ASSOCF_VERIFY, ASSOCSTR_EXECUTABLE, m_szExecutable2, _countof(m_szExecutable2));

    CoTaskMemFree(pszApplication);
    CoTaskMemFree(pszCommandLine);

    return hr;
}

// Builds the final command-line string by expanding the run-as template and substituting file and parameter tokens.
BOOL CShellExecute::_SetCommand()
{
    TRACE("\n");
    LPWSTR szRunAsCommand = m_szRunAsCommand;
    HINSTANCE hInstError = NULL;
    DWORD dwAssocBufLen;
    BOOL bLongNameOK = FALSE;
    BOOL ret = FALSE;
    HRESULT hr = S_OK;

    if (_ParamIsApp(m_szRunAsCommand) || _FileIsApp())
    {
        hr = StringCchCopyW(m_szWorkGroupHelper, _countof(m_szWorkGroupHelper), m_szPath);

        // Get friendly name
        dwAssocBufLen = _countof(m_szFriendlyAppName);
        AssocQueryStringW(ASSOCF_OPEN_BYEXENAME | ASSOCF_VERIFY,
                          ASSOCSTR_FRIENDLYAPPNAME,
                          m_szWorkGroupHelper,
                          NULL,
                          m_szFriendlyAppName,
                          &dwAssocBufLen);

        StringCchCopyW(m_szExecutable2, _countof(m_szExecutable2), m_szPath);
    }
    else
    {
        if (!*szRunAsCommand)
        {
            m_dwError = ERROR_NO_ASSOCIATION;
            return FALSE;
        }

        hr = _EvaluateTemplate(&bLongNameOK);
        if (hr == CO_E_APPNOTFOUND)
        {
#ifndef NO_RUNAS
            if (m_bNoUI)
            {
                _ReportWin32(hr);
                return FALSE;
            }

            // Show "Open As" dialog
            OPENASINFO oai = {};
            oai.pcszFile = m_szPath;
            oai.oaifInFlags = OAIF_ALLOW_REGISTRATION | OAIF_REGISTER_EXT | OAIF_EXEC;
            const DWORD dwDialogError = (OpenAsDialog(m_hWnd, &oai) < 0)
                                      ? ERROR_CANCELLED
                                      : ERROR_SUCCESS;

            _ReportWin32(dwDialogError);
            hr = S_FALSE;
#endif
        }
    }

    if (hr != S_OK)
    {
        if (SUCCEEDED(hr))
            return FALSE;

        _ReportWin32(hr);
        return FALSE;
    }

    hInstError = (HINSTANCE)UlongToHandle(ReplaceParameters(m_szCommand,
        _countof(m_szCommand),
        m_szPath,
        m_szRunAsCommand,
        m_pszParameters,
        m_wShowWindow,
        0,
        !bLongNameOK,
        m_lpIDList,
        &m_hDdeData));
    if (hInstError != NULL)
        _ReportHinst(hInstError);
    else
        ret = TRUE;

    if (!m_bInheritHandles && SHRestricted(REST_INHERITCONSOLEHANDLES))
        m_bInheritHandles = IsCurrentProcessConsole() && IsConsoleApp(m_szWorkGroupHelper);

    return ret;
}

// Builds the command line and environment, then spawns the target process via _SHCreateProcess.
void CShellExecute::_DoExecCommand()
{
    TRACE("\n");
    HWND hwndFore = ::GetForegroundWindow();

    if (!_SetCommand() || !_ExecMayCreateProcess())
        return;

    _BuildEnvironmentForNewProcess(FALSE);

    PCWSTR pszWorkDir = m_bUseNullCWD ? NULL : m_szWorkDir;

    if (_SHCreateProcess(
            m_hWnd,
            m_hUserToken,
            m_szFriendlyAppName,
            m_szWorkGroupHelper,
            m_szCommand,
            m_dwCreationFlags,
            m_lpProcessAttributes,
            m_lpThreadAttributes,
            m_bInheritHandles,
            m_EnvBlock.m_pszzBlock,
            pszWorkDir,
            &m_si,
            &m_pi,
            m_EnvBlock.m_nRunAs,
            m_bLogUsage))
    {
        if (m_bDdeInfoSet || m_bWaitForInputIdle)
            WaitForInputIdle(m_pi.hProcess, 60000);

        _FixActivationStealingApps(hwndFore, m_wShowWindow);

        if (m_bDdeInfoSet)
            _DDEExecute(FALSE, m_hWnd, m_wShowWindow, m_bNoAsync);
        else
            _ReportHinst(NULL);

        if (m_bLogUsage && (m_si.dwFlags & STARTF_TITLEISLINKNAME))
            _NotifyShortcutInvoke();
    }
    else
    {
        _ReportWin32(GetLastError());
    }
}

// Queries the class store for a new association key and updates the query object if a better handler is found.
IRET CShellExecute::_ShouldRetryWithNewClassKey(BOOL bParse)
{
    if (m_bAlreadyQueriedClassStore ||
        m_bNoQueryClassStore ||
        FAILED(m_pQueryAssoc->GetData(NULL, (ASSOCDATA)3, NULL, NULL, NULL)))
    {
        return IRET_2;
    }

    if (!bParse)
        return IRET_FAILED;

    if (!m_szPath[0])
        return IRET_2;

    WCHAR szDotExt[MAX_PATH];
    StrCpyNW(szDotExt, PathFindExtensionW(m_szPath), _countof(szDotExt));

    INSTALLDATA pInstallInfo = { FILEEXT, szDotExt };
    if (InstallApplication(&pInstallInfo))
        return IRET_2;

    LPITEMIDLIST pidl = ILCreateFromPathW(m_szPath);
    if (!pidl)
        return IRET_2;

    IQueryAssociations *pNewAssoc = NULL;
    if (SUCCEEDED(SHGetAssociations(pidl, &pNewAssoc)))
    {
        m_pQueryAssoc->Release();
        m_pQueryAssoc = pNewAssoc;

        if (m_pszVerb && lstrcmpiW(m_pszVerb, L"openas") == 0)
            m_pszVerb = NULL;

        m_bClassStoreOnly = FALSE;
        m_bAlreadyQueriedClassStore = TRUE;
    }

    ILFree(pidl);
    return IRET_2;
}

// Retrieves and parses a Darwin (MSI) descriptor from the association data to build the run-as command template.
IRET CShellExecute::_SetDarwinCmdTemplate(BOOL bParse)
{
    INT iret = IRET_2;
    DWORD cbAppPathKey = sizeof(m_szAppPathKey);
    HRESULT hr = m_pQueryAssoc->GetData(ASSOCF_NONE, ASSOCDATA_MSIDESCRIPTOR, m_pszVerb,
                                        m_szAppPathKey, &cbAppPathKey);
    if (FAILED(hr))
        return IRET_2;

    if (!bParse)
        return IRET_FAILED;

    DWORD dwError = ParseDarwinID(m_szAppPathKey, m_szRunAsCommand, _countof(m_szRunAsCommand));
    if (dwError == ERROR_SUCCESS)
        return IRET_1;

    _ReportWin32(dwError);
    return IRET_0;
}

// Tries Darwin installation first, then falls back to retrying with a new class store key if Darwin is unavailable.
IRET CShellExecute::_MaybeInstallApp(BOOL bParse)
{
    if (!IsDarwinEnabled())
        return _ShouldRetryWithNewClassKey(bParse);
    IRET iret = _SetDarwinCmdTemplate(bParse);
    if (iret == IRET_2)
        return _ShouldRetryWithNewClassKey(bParse);
    return iret;
}

// Reads the RunAsCommand value from the application's App Paths registry key into the run-as command buffer.
BOOL CShellExecute::_SetAppRunAsCmdTemplate()
{
    HRESULT hr = PathToAppPathKey(m_szPath, m_szEnvEntry, _countof(m_szEnvEntry));
    if (FAILED(hr))
        return FALSE;

    DWORD cbData = sizeof(m_szRunAsCommand);
    if (SHGetValueW(HKEY_LOCAL_MACHINE, m_szEnvEntry, L"RunAsCommand", 0,
                    m_szRunAsCommand, &cbData) != ERROR_SUCCESS)
    {
        return FALSE;
    }

    return m_szRunAsCommand[0] != UNICODE_NULL;
}

// Resolves the command template for execution by checking Darwin, class store, App Paths, and association command.
IRET CShellExecute::_SetCmdTemplate(BOOL bParse)
{
    IRET iret = _MaybeInstallApp(bParse);
    if (iret != IRET_2)
        return iret;

    if (m_bClassStoreOnly)
    {
        m_dwError = ERROR_NO_ASSOCIATION;
        return IRET_0;
    }

    if (!m_EnvBlock.m_nRunAs && PathIsExe(m_szPath) && _SetAppRunAsCmdTemplate())
        return IRET_1;

    if (FAILED(_QueryString(ASSOCF_NONE, ASSOCSTR_COMMAND,
                            m_szRunAsCommand, _countof(m_szRunAsCommand))))
    {
        m_dwError = ERROR_NO_ASSOCIATION;
        return IRET_0;
    }

    return IRET_1;
}

// Queries a string value from the current IQueryAssociations object and copies it to the destination buffer.
HRESULT CShellExecute::_QueryString(
    ASSOCF    assocFlags,
    ASSOCSTR  assocStr,
    LPWSTR    pszDest,
    INT       cchDest)
{
    if (!m_pQueryAssoc)
        return E_FAIL;

    DWORD cchResult = _countof(m_szAppPathKey);
    HRESULT hr = m_pQueryAssoc->GetString(assocFlags, assocStr,
                                          m_pszVerb, m_szAppPathKey, &cchResult);
    if (SUCCEEDED(hr))
        SHUnicodeToUnicode(m_szAppPathKey, pszDest, cchDest);

    return hr;
}

// Replaces the target path with the stored URL if the associated executable is URL-aware.
void CShellExecute::_SetFileAndUrl()
{
    if (!m_szURL[0])
        return;
    HRESULT hr = _QueryString(ASSOCF_NONE, ASSOCSTR_EXECUTABLE,
                              m_szEnvEntry, _countof(m_szEnvEntry));
    if (SUCCEEDED(hr) && DoesAppWantUrl(m_szEnvEntry) )
        StrCpyNW(m_szPath, m_szURL, _countof(m_szPath));
}

// Thread entry point that forces synchronous invocation and then releases the CShellExecute object.
DWORD CShellExecute::_InvokeAppThreadProc()
{
    m_bNoAsync = TRUE;
    _TryInvokeApplication(TRUE);
    Release();
    return 0;
}

// Static thread callback that casts the opaque pointer to CShellExecute and calls _InvokeAppThreadProc.
DWORD CALLBACK CShellExecute::s_InvokeAppThreadProc(PVOID pThis)
{
    return ((CShellExecute *)pThis)->_InvokeAppThreadProc();
}

// Clones any borrowed pointers into owned storage and spawns a background thread to retry the invocation asynchronously.
IRET CShellExecute::_RetryAsync()
{
    if (m_lpIDList && !m_pidlParsed)
    {
        m_pidlParsed = ILClone(m_lpIDList);
        m_lpIDList   = m_pidlParsed;
    }

    if (m_pszParameters)
    {
        m_hParameters  = StrDupW(m_pszParameters);
        m_pszParameters = (LPWSTR)m_hParameters;
    }

    if (m_pszTitle)
    {
        m_hTitle        = StrDupW(m_pszTitle);
        m_si.lpTitle    = (LPWSTR)m_hTitle;
        m_pszTitle      = (LPWSTR)m_hTitle;
    }

    m_bCloseProcess = TRUE;
    InterlockedIncrement(&m_cRefs);

    if (SHCreateThread(s_InvokeAppThreadProc, this, CTF_COINIT | CTF_PROCESS_REF | CTF_THREAD_REF, NULL))
        return IRET_1;

    _ReportWin32(GetLastError());
    Release();
    return IRET_0;
}

// Queries DDE command, application, and topic strings from the association and registers them as global atoms.
BOOL CShellExecute::_SetDDEInfo()
{
    if (FAILED(_QueryString(0, ASSOCSTR_DDECOMMAND, m_szDdeCommand, _countof(m_szDdeCommand))))
        return FALSE;

    m_bActivateHandler = FAILED(m_pQueryAssoc->GetData(
        NULL,
        ASSOCDATA_NOACTIVATEHANDLER,
        m_pszVerb,
        NULL,
        NULL));

    if (FAILED(_QueryString(0, ASSOCSTR_DDEAPPLICATION, m_szEnvEntry, _countof(m_szEnvEntry))))
        return FALSE;

    if (m_wDdeAppAtom)
        GlobalDeleteAtom(m_wDdeAppAtom);
    m_wDdeAppAtom = GlobalAddAtomW(m_szEnvEntry);

    if (FAILED(_QueryString(0, ASSOCSTR_DDETOPIC, m_szEnvEntry, _countof(m_szEnvEntry))))
        return FALSE;

    if (m_wDdeTopicAtom)
        GlobalDeleteAtom(m_wDdeTopicAtom);
    m_wDdeTopicAtom = GlobalAddAtomW(m_szEnvEntry);

    m_bDdeInfoSet = TRUE;
    return TRUE;
}

// Attempts DDE execution by initializing DDE info and dispatching the DDE execute sequence.
IRET CShellExecute::_TryExecDDE()
{
    if (_SetDDEInfo() && _DDEExecute(TRUE, m_hWnd, m_wShowWindow, m_bNoAsync))
        return IRET_0;
    return IRET_2;
}

// Orchestrates the full application invocation sequence: resolving the command template, DDE, and direct execution.
IRET CShellExecute::_TryInvokeApplication(BOOL bInvoke)
{
    TRACE("\n");
    IRET iret = IRET_1;

    if (bInvoke)
    {
        iret = _SetCmdTemplate(bInvoke);
        if (iret < 1)
            goto done;
    }

    _SetFileAndUrl();

    iret = _TryExecDDE();
    if (iret < 1)
        goto done;

    if (!bInvoke)
    {
        iret = _SetCmdTemplate(0);
        if (iret < 1)
            goto done;
    }

    _DoExecCommand();
    iret = IRET_0;

done:
    if (iret == IRET_FAILED)
        return _RetryAsync();

    return iret;
}

// Drives the main shell execution pipeline: initializing, resolving, trust-checking, associating, and invoking.
void CShellExecute::ExecuteNormal(LPSHELLEXECUTEINFOW sei)
{
    TRACE("\n");
    SetAppStartingCursor(sei->hwnd, TRUE);

    _Init(sei);
    _SetWorkingDir(sei->lpDirectory);
    _SetFile(sei->lpFile, sei->fMask & 0x400000);

    LPITEMIDLIST pidl;
    if (_PerfPidl(&pidl) > 0 &&
        _TryValidateUNC(m_szPath, sei, pidl) > 0 &&
        _TryHooks(sei) > 0 &&
        _TryExecPidl(sei, pidl) > 0 &&
        _VerifyExecTrust(sei) > 0 &&
        _InitAssociations(sei, pidl) > 0)
    {
        BOOL bTryInvoke = m_bNoAsync || (sei->fMask & SEE_MASK_NOCLOSEPROCESS);
        _TryInvokeApplication(bTryInvoke);
    }

    if (!m_dwError && UEMIsLoaded())
    {
        TRACE("\n");
        INT assoc = GetUEMAssoc(m_szPath, m_szWorkGroupHelper, m_lpIDList);
        UEMFireEvent(CLSID_ActiveDesktop, 50, 2, 4, assoc);
    }

    SetAppStartingCursor(sei->hwnd, FALSE);
}

// Transfers the process handle to the caller if requested and maps the final error/instance result.
DWORD CShellExecute::Finalize(LPSHELLEXECUTEINFOW sei)
{
    TRACE("\n");
    if (!m_bCloseProcess && m_pi.hProcess && !m_dwError && (sei->fMask & SEE_MASK_NOCLOSEPROCESS))
    {
        sei->hProcess = m_pi.hProcess;
        m_pi.hProcess = NULL;
    }
    return _FinalMapError(&sei->hInstApp);
}

// Converts the stored Win32 error to an HINSTANCE value and applies drive-type refinement for file-not-found errors.
DWORD CShellExecute::_FinalMapError(HINSTANCE *phInstApp)
{
    if (m_dwError)
    {
        if (m_dwError == ERROR_FILE_NOT_FOUND)
        {
            if (PathIsRootW(m_szPath) && !PathIsUNCW(m_szPath))
            {
                WCHAR ch = m_szPath[0];
                INT nDriveNumber = (ch - 1) & 0x1F;
                INT driveType = DriveType(nDriveNumber);
                m_dwError = (driveType == DRIVE_CDROM || driveType == DRIVE_REMOVABLE)
                    ? ERROR_NOT_READY : ERROR_BAD_UNIT;
            }
        }

        SetLastError(m_dwError);

        if (phInstApp)
            *phInstApp = (HINSTANCE)_MapWin32ErrToHINST(m_dwError);
    }
    else if (phInstApp)
    {
        *phInstApp = m_hInstApp ? m_hInstApp : (HINSTANCE)42;
    }

    return m_dwError;
}

// Maps a legacy HINSTANCE error code to the corresponding Win32 error code using the known error-pair table.
DWORD CShellExecute::_MapHINSTToWin32Err(HINSTANCE hInstApp)
{
    DWORD dwInst = HandleToUlong(hInstApp);
    switch (dwInst)
    {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_ACCESS_DENIED:
        case ERROR_NOT_ENOUGH_MEMORY:
            return dwInst;
    }

    for (size_t i = 0; i < _countof(g_ErrorInstPairs); ++i)
    {
        if (dwInst == g_ErrorInstPairs[i].dwInst)
            return g_ErrorInstPairs[i].dwError;
    }

    return ERROR_SUCCESS;
}

// Maps a Win32 error code to the corresponding legacy HINSTANCE value using the known error-pair table.
HINSTANCE CShellExecute::_MapWin32ErrToHINST(DWORD dwError)
{
    switch (dwError)
    {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_ACCESS_DENIED:
        case ERROR_NOT_ENOUGH_MEMORY:
            return (HINSTANCE)UlongToHandle(dwError);
    }

    for (size_t i = 0; i < _countof(g_ErrorInstPairs); ++i)
    {
        if (dwError == g_ErrorInstPairs[i].dwError)
            return (HINSTANCE)UlongToHandle(g_ErrorInstPairs[i].dwInst);
    }

    return (HINSTANCE)UlongToHandle(ERROR_ACCESS_DENIED);
}

// Allocates a CShellExecute object, runs the normal execution pipeline, finalizes the result, and releases it.
static DWORD ShellExecuteNormal(LPSHELLEXECUTEINFOW sei)
{
    TRACE("\n");
    CShellExecute *pShellExecute = new CShellExecute();
    if (!pShellExecute)
    {
        sei->hInstApp = (HINSTANCE)ERROR_NOT_ENOUGH_MEMORY;
        return ERROR_OUTOFMEMORY;
    }

    pShellExecute->ExecuteNormal(sei);
    DWORD dwError = pShellExecute->Finalize(sei);
    pShellExecute->Release();

    return dwError;
}

// Displays an appropriate error message box for a shell execute failure, selected by Win32 error code.
static void _DisplayShellExecError(
    DWORD fMask,
    HWND hWnd,
    PCWSTR pszText,
    PCWSTR pszCaption,
    DWORD dwError)
{
    TRACE("\n");
    BOOL bShowMsgBox = FALSE;
    PCWSTR pszTitle = pszCaption ? pszCaption : pszText;
    INT nTextID = 0;
    INT nTitleID = 0;

    if ((fMask & SEE_MASK_FLAG_NO_UI) || dwError == ERROR_CANCELLED)
    {
        SetLastError(dwError);
        return;
    }

    if (hWnd)
        SetForegroundWindow(hWnd);

    switch (dwError)
    {
        case ERROR_FILE_NOT_FOUND:       nTextID = IDS_CANTFIND2; bShowMsgBox = TRUE; break;
        case ERROR_PATH_NOT_FOUND:       nTextID = IDS_BADPATH2; bShowMsgBox = TRUE; break;
        case ERROR_TOO_MANY_OPEN_FILES:  nTextID = IDS_TOOMANYFILES; bShowMsgBox = TRUE; break;
        case ERROR_ACCESS_DENIED:        nTextID = IDS_BADPATH1; bShowMsgBox = TRUE; break;
        case ERROR_NOT_ENOUGH_MEMORY:    // fall-through
        case ERROR_OUTOFMEMORY:          nTextID = IDS_NOMEMORY; bShowMsgBox = TRUE; break;
        case ERROR_BAD_FORMAT:           nTextID = IDS_BADFORMAT; bShowMsgBox = TRUE; break;
        case ERROR_SHARING_VIOLATION:    nTextID = IDS_FILELOCKED; bShowMsgBox = TRUE; break;
        case ERROR_BAD_NET_NAME:         // fall-through
        case ERROR_SEM_TIMEOUT:          nTextID = IDS_CANTFIND1; bShowMsgBox = TRUE; break;
        case ERROR_OLD_WIN_VERSION:      nTextID = IDS_OLDOS; bShowMsgBox = TRUE; break;
        case ERROR_APP_WRONG_OS:         nTextID = IDS_BADPROGRAM; bShowMsgBox = TRUE; break;
        case ERROR_SINGLE_INSTANCE_APP:  nTextID = IDS_ONEPROGRAMONLY; bShowMsgBox = TRUE; break;
        case ERROR_RMODE_APP:            nTextID = IDS_CANTRUN; bShowMsgBox = TRUE; break;
        case ERROR_BAD_PATHNAME:         nTextID = IDS_BADPATH2; bShowMsgBox = TRUE; break;
        case ERROR_INVALID_DLL:          nTextID = IDS_BADLIBRARY; bShowMsgBox = TRUE; break;
        case ERROR_NO_ASSOCIATION:       nTextID = IDS_BADASSOC; bShowMsgBox = TRUE; break;
        case ERROR_DDE_FAIL:             nTextID = IDS_BADSENDING; bShowMsgBox = TRUE; break;
        case 0xFFFFFFFF:
            nTextID = IDS_CANCELBYREST;
            if (!pszCaption)
            {
                nTitleID = IDS_RESTRICTION;
                pszTitle = NULL;
            }
            bShowMsgBox = TRUE;
            break;

        default:
        {
            if (dwError)
                SHSysErrorMessageBox(hWnd, pszTitle, IDS_TWOARGS, dwError, pszText, MB_ICONERROR);
            break;
        }
    }

    if (bShowMsgBox)
    {
        UINT uType = MB_ICONERROR;
        if (nTextID == 8448)
            uType |= MB_SYSTEMMODAL;

        LPCWSTR lpFinalTitle = (nTitleID != 0) ? MAKEINTRESOURCEW(nTitleID) : pszTitle;

        WonShellMessageBoxWrapW(g_hinst, hWnd, MAKEINTRESOURCEW(nTextID), lpFinalTitle, uType);
    }

    SetLastError(dwError);
}

// Retrieves the last error if none was supplied, then delegates to _DisplayShellExecError for UI reporting.
static void _ShellExecuteError(LPSHELLEXECUTEINFOW sei, LPCWSTR pszCaption, DWORD dwError)
{
    if (!dwError)
        dwError = GetLastError();
    _DisplayShellExecError(sei->fMask, sei->hwnd, sei->lpFile, pszCaption, dwError);
}

// @implemented
EXTERN_C BOOL WINAPI WonShellExecuteExW(LPSHELLEXECUTEINFOW sei)
{
    TRACE("\n");
    HRESULT hrCoInit = SHCoInitialize();

    DWORD dwError;
    if (IsBadReadPtr(sei, sizeof(SHELLEXECUTEINFOW)) || sei->cbSize != sizeof(SHELLEXECUTEINFOW))
    {
        dwError = ERROR_ACCESS_DENIED;
        sei->hInstApp = (HINSTANCE)ERROR_ACCESS_DENIED;
    }
    else
    {
        ULONG fOldMask = sei->fMask;
        sei->fMask = fOldMask | 0x800;

        if (SHRegGetBoolUSValueW(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",
                                 L"MaximizeApps",
                                 FALSE,
                                 FALSE))
        {
            switch (sei->nShow)
            {
                case SW_SHOWNORMAL:
                case SW_SHOW:
                case SW_RESTORE:
                case SW_SHOWDEFAULT:
                    sei->nShow = SW_SHOWMAXIMIZED;
                    break;
            }
        }

        if (!(sei->fMask & SEE_MASK_NOASYNC) && InRunDllProcess())
            sei->fMask |= SEE_MASK_NOASYNC | SEE_MASK_WAITFORINPUTIDLE;

        dwError = ShellExecuteNormal(sei);

        if (dwError && dwError != ERROR_DLL_NOT_FOUND && dwError != ERROR_CANCELLED)
            _ShellExecuteError(sei, NULL, dwError);

        sei->fMask = fOldMask;
    }

    if (SUCCEEDED(hrCoInit))
        CoUninitialize();

    if (dwError)
        SetLastError(dwError);

    return dwError == ERROR_SUCCESS;
}

// @implemented
EXTERN_C HINSTANCE WINAPI
WonShellExecuteW(
    HWND hwnd,
    LPCWSTR lpVerb,
    LPCWSTR lpFile,
    LPCWSTR lpParameters,
    LPCWSTR lpDirectory,
    INT nShowCmd)
{
    TRACE("\n");

    SHELLEXECUTEINFOW sei;
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_UNKNOWN_0x1000;
    sei.hwnd = hwnd;
    sei.lpVerb = lpVerb;
    sei.lpFile = lpFile;
    sei.lpParameters = lpParameters;
    sei.lpDirectory = lpDirectory;
    sei.nShow = nShowCmd;
    sei.lpIDList = 0;
    sei.lpClass = 0;
    sei.hkeyClass = 0;
    sei.dwHotKey = 0;
    sei.hProcess = 0;

#ifndef SHACF_WIN95SHLEXEC
    #define SHACF_WIN95SHLEXEC 0x200
#endif
    if (!(SHGetAppCompatFlags(SHACF_WIN95SHLEXEC) & SHACF_WIN95SHLEXEC))
        sei.fMask |= SEE_MASK_NOASYNC;
    WonShellExecuteExW(&sei);
    return sei.hInstApp;
}

static __inline LPWSTR __SHCloneStrAtoW(WCHAR **target, const char *source)
{
	int len = MultiByteToWideChar(CP_ACP, 0, source, -1, NULL, 0);
	*target = (WCHAR *)SHAlloc(len * sizeof(WCHAR));
	MultiByteToWideChar(CP_ACP, 0, source, -1, *target, len);
	return *target;
}

EXTERN_C BOOL WINAPI
WonShellExecuteExA(LPSHELLEXECUTEINFOA sei)
{
    SHELLEXECUTEINFOW seiW;
    BOOL ret;
    WCHAR *wVerb = NULL, *wFile = NULL, *wParameters = NULL, *wDirectory = NULL, *wClass = NULL;
    TRACE("\n");

    if (sei->cbSize != sizeof(SHELLEXECUTEINFOA))
    {
        sei->hInstApp = (HINSTANCE)ERROR_ACCESS_DENIED;
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }

    memcpy(&seiW, sei, sizeof(SHELLEXECUTEINFOW));

    seiW.cbSize = sizeof(SHELLEXECUTEINFOW);

    if (sei->lpVerb)
        seiW.lpVerb = __SHCloneStrAtoW(&wVerb, sei->lpVerb);

    if (sei->lpFile)
        seiW.lpFile = __SHCloneStrAtoW(&wFile, sei->lpFile);

    if (sei->lpParameters)
        seiW.lpParameters = __SHCloneStrAtoW(&wParameters, sei->lpParameters);

    if (sei->lpDirectory)
        seiW.lpDirectory = __SHCloneStrAtoW(&wDirectory, sei->lpDirectory);

    if ((sei->fMask & SEE_MASK_CLASSALL) == SEE_MASK_CLASSNAME && sei->lpClass)
        seiW.lpClass = __SHCloneStrAtoW(&wClass, sei->lpClass);
    else
        seiW.lpClass = NULL;

    ret = WonShellExecuteExW(&seiW);

    sei->hInstApp = seiW.hInstApp;

    if (sei->fMask & SEE_MASK_NOCLOSEPROCESS)
        sei->hProcess = seiW.hProcess;

    SHFree(wVerb);
    SHFree(wFile);
    SHFree(wParameters);
    SHFree(wDirectory);
    SHFree(wClass);

    return ret;
}

EXTERN_C HINSTANCE WINAPI
WonShellExecuteA(HWND hWnd, LPCSTR lpVerb, LPCSTR lpFile, LPCSTR lpParameters, LPCSTR lpDirectory, INT iShowCmd)
{
    TRACE("\n");
    SHELLEXECUTEINFOA sei;
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_UNKNOWN_0x1000;
    sei.hwnd = hWnd;
    sei.lpVerb = lpVerb;
    sei.lpFile = lpFile;
    sei.lpParameters = lpParameters;
    sei.lpDirectory = lpDirectory;
    sei.nShow = iShowCmd;
    sei.lpIDList = 0;
    sei.lpClass = 0;
    sei.hkeyClass = 0;
    sei.dwHotKey = 0;
    sei.hProcess = 0;

#ifndef SHACF_WIN95SHLEXEC
    #define SHACF_WIN95SHLEXEC 0x00000200
#endif
    if (!(SHGetAppCompatFlags(SHACF_WIN95SHLEXEC) & SHACF_WIN95SHLEXEC))
        sei.fMask |= SEE_MASK_NOASYNC;
    WonShellExecuteExA(&sei);
    return sei.hInstApp;
}

// Executes a shell command with extended options using ANSI strings
EXTERN_C HINSTANCE WINAPI
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
    _In_ DWORD dwFlags)
{
    TRACE("\n");
    SHELLEXECUTEINFOA sei   = {};
    sei.cbSize              = sizeof(sei);
    sei.fMask               = SEE_MASK_FLAG_NO_UI | SEE_MASK_UNKNOWN_0x1000;
    sei.hwnd                = hwnd;
    sei.lpVerb              = lpOperation;
    sei.lpFile              = lpFile;
    sei.lpParameters        = lpParameters;
    sei.lpDirectory         = lpDirectory;
    sei.nShow               = nCmdShow;

    if (lpReserved)
    {
        sei.fMask |= SEE_MASK_FLAG_SEPVDM;
        sei.hInstApp = (HINSTANCE)lpReserved;
    }

    if (lpTitle)
    {
        sei.fMask |= SEE_MASK_HASTITLE;
        sei.lpClass  = lpTitle;
    }

    if (dwFlags & 0x01)
        sei.fMask |= 0x00020000;

    if (dwFlags & 0x02)
        sei.fMask |= SEE_MASK_NO_CONSOLE;

    if (lphProcess)
    {
        sei.fMask |= SEE_MASK_NOCLOSEPROCESS;
        ShellExecuteExA(&sei);
        *lphProcess = sei.hProcess;
    }
    else
    {
        ShellExecuteExA(&sei);
    }

    return sei.hInstApp;
}

// Unicode implementation of RealShellExecuteEx with SAL annotations, forwarding to ShellExecuteExW.
EXTERN_C
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
    _In_ DWORD dwFlags)
{
    SHELLEXECUTEINFOW ExecInfo;
    TRACE("\n");

    ZeroMemory(&ExecInfo, sizeof(ExecInfo));
    ExecInfo.cbSize = sizeof(ExecInfo);
    ExecInfo.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_UNKNOWN_0x1000;
    ExecInfo.hwnd = hwnd;
    ExecInfo.lpVerb = lpOperation;
    ExecInfo.lpFile = lpFile;
    ExecInfo.lpParameters = lpParameters;
    ExecInfo.lpDirectory = lpDirectory;
    ExecInfo.nShow = (WORD)nCmdShow;

    if (lpReserved)
    {
        ExecInfo.fMask |= SEE_MASK_USE_RESERVED;
        ExecInfo.hInstApp = (HINSTANCE)lpReserved;
    }

    if (lpTitle)
    {
        ExecInfo.fMask |= SEE_MASK_HASTITLE;
        ExecInfo.lpClass = lpTitle;
    }

    if (dwFlags & 1)
        ExecInfo.fMask |= SEE_MASK_FLAG_SEPVDM;

    if (dwFlags & 2)
        ExecInfo.fMask |= SEE_MASK_NO_CONSOLE;

    if (lphProcess == NULL)
    {
        ShellExecuteExW(&ExecInfo);
    }
    else
    {
        ExecInfo.fMask |= SEE_MASK_NOCLOSEPROCESS;
        ShellExecuteExW(&ExecInfo);
        *lphProcess = ExecInfo.hProcess;
    }

    return ExecInfo.hInstApp;
}

// Sets the WOW shell execute callback, delegates to RealShellExecuteExA, then clears the callback.
EXTERN_C HINSTANCE WINAPI
WonWOWShellExecute(
    HWND hWnd,
    LPCSTR lpVerb,
    LPCSTR lpFile,
    LPCSTR lpParameters,
    LPCSTR lpDirectory,
    INT iShowCmd,
    WOWSHELLEXECHOOKPROC callback)
{
    HINSTANCE result;
    TRACE("\n");
    g_fnWowShellExecCB = callback;
    if (!lpParameters)
        lpParameters = "";
    result = RealShellExecuteExA(hWnd, lpVerb, lpFile, lpParameters, lpDirectory, NULL, "", NULL, iShowCmd, NULL, 0);
    g_fnWowShellExecCB = NULL;
    return result;
}

// Executes a shell command for WOW (16-bit compatibility) with a hook callback.
HINSTANCE WINAPI WOWShellExecute(
    HWND hwnd,
    LPCSTR lpOperation,
    LPCSTR lpFile,
    LPCSTR lpParameters,
    LPCSTR lpDirectory,
    WORD nShowCmd,
    WOWSHELLEXECHOOKPROC callback)
{
    HINSTANCE result;
    TRACE("\n");
    g_fnWowShellExecCB = callback;
    if (!lpParameters)
        lpParameters = "";
    result = RealShellExecuteExA(hwnd, lpOperation, lpFile, lpParameters, lpDirectory, NULL, "", NULL, nShowCmd, NULL, 0);
    g_fnWowShellExecCB = NULL;
    return result;
}
