// osinfo.cpp --- OS information
// Author: katahiromz
// License: MIT

#include <windows.h>
#include <winternl.h>
#include <lmcons.h>
#include <lmjoin.h>
#include <lmapibuf.h>
#include "osinfo.h"

typedef NTSTATUS (NTAPI* FN_NtQueryInformationProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

static BOOL g_bGotVerInfo = FALSE;
static OSVERSIONINFOEXA  g_verInfo = {};
static BOOL g_bOnWow64 = -1;
static BOOL g_bApplianceServer = -1;
static BOOL g_bGotDomainInfo = FALSE;
static BOOL g_bMachineIsDomainMember = FALSE;

static inline BOOL IsWin9x()
{
    return g_verInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS;
}

static inline BOOL IsWinNT()
{
    return g_verInfo.dwPlatformId == VER_PLATFORM_WIN32_NT;
}

static inline BOOL IsVersion(DWORD major, DWORD minor = MAXDWORD)
{
    if (g_verInfo.dwMajorVersion != major)
        return FALSE;
    return minor == MAXDWORD || g_verInfo.dwMinorVersion == minor;
}

static inline BOOL IsVersionOrLater(DWORD major, DWORD minor = 0)
{
    if (g_verInfo.dwMajorVersion != major)
        return g_verInfo.dwMajorVersion > major;
    return g_verInfo.dwMinorVersion >= minor;
}

static inline BOOL IsServer()
{
    return g_verInfo.wProductType == VER_NT_SERVER ||
           g_verInfo.wProductType == VER_NT_DOMAIN_CONTROLLER;
}

static inline BOOL IsWorkstation()
{
    return g_verInfo.wProductType == VER_NT_WORKSTATION;
}

static inline BOOL HasSuite(WORD mask)
{
    return (g_verInfo.wSuiteMask & mask) != 0;
}

static inline BOOL IsTSSuiteBasic()
{
    return (g_verInfo.wSuiteMask & 0x8000) == 0;
}

static inline void EnsureVerInfo()
{
    if (g_bGotVerInfo)
        return;

    g_bGotVerInfo = TRUE;
    g_verInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);

    if (!GetVersionExA((LPOSVERSIONINFOA)&g_verInfo))
    {
        g_verInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
        GetVersionExA((LPOSVERSIONINFOA)&g_verInfo);
    }
}

// @implemented
BOOL IsWinlogonRegValueSet(HKEY hKey, LPCSTR lpSubKey1, LPCSTR lpSubKey2, LPCSTR lpValueName)
{
    DWORD dwType = 0, dwData = 0, cbData = sizeof(dwData);
    HKEY hSubKey = NULL;

    if (RegOpenKeyExA(hKey, lpSubKey1, 0, KEY_QUERY_VALUE, &hSubKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueExA(hSubKey, lpValueName, NULL, &dwType, (PBYTE)&dwData, &cbData) == ERROR_SUCCESS)
        {
            if (dwType != REG_DWORD)
                dwData = 0;
        }
        RegCloseKey(hSubKey);
    }

    if (RegOpenKeyExA(hKey, lpSubKey2, 0, KEY_QUERY_VALUE, &hSubKey) == ERROR_SUCCESS)
    {
        cbData = sizeof(dwData);
        if (RegQueryValueExA(hSubKey, lpValueName, NULL, &dwType, (PBYTE)&dwData, &cbData) == ERROR_SUCCESS)
        {
            if (dwType != REG_DWORD)
                dwData = 0;
        }
        RegCloseKey(hSubKey);
    }

    return (dwData != 0);
}

BOOL IsMachineDomainMember(void)
{
    if (!g_bGotDomainInfo)
    {
        g_bGotDomainInfo = TRUE;
        LPWSTR psz;
        NETSETUP_JOIN_STATUS status;
        if (!NetGetJoinInformation(0, &psz, &status))
        {
            if (psz)
                NetApiBufferFree(psz);
            if ( status == NetSetupDomainName )
                g_bMachineIsDomainMember = TRUE;
        }
    }
    return g_bMachineIsDomainMember;
}

// @implemented
BOOL IsWinlogonRegValuePresent(HKEY hKey, LPCSTR lpSubKey, LPCSTR lpValueName)
{
    BOOL ret = FALSE;
    DWORD dwType, cbData;
    BYTE Data[MAX_PATH];

    HKEY hKey2;
    if (RegOpenKeyExA(hKey, lpSubKey, 0, KEY_QUERY_VALUE, &hKey2) == ERROR_SUCCESS)
    {
        cbData = sizeof(Data);
        ret = RegQueryValueExA(hKey2, lpValueName, NULL, &dwType, Data, &cbData) == ERROR_SUCCESS;
        RegCloseKey(hKey2);
    }

    return ret;
}

BOOL RunningOnWow64(VOID)
{
    BOOL result = g_bOnWow64;
    if (g_bOnWow64 == -1)
    {
        HMODULE hNTDLL = GetModuleHandleW(L"ntdll.dll");
        FN_NtQueryInformationProcess fn = (FN_NtQueryInformationProcess)GetProcAddress(hNTDLL, "NtQueryInformationProcess");
        result = FALSE;
        if (fn)
        {
            HANDLE hProcess = GetCurrentProcess();
            BOOL bValue;
            if (NT_SUCCESS(fn(hProcess, ProcessWow64Information, &bValue, sizeof(bValue), NULL)))
            {
                if (bValue)
                    result = TRUE;
            }
        }
        g_bOnWow64 = result;
    }
    return result;
}

BOOL ShouldShowServerAdminUI(VOID)
{
    DWORD dwValue = 0, cbValue;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                      0,
                      KEY_QUERY_VALUE,
                      &hKey))
    {
        cbValue = sizeof(dwValue);
        RegQueryValueExW(hKey, L"ServerAdminUI", NULL, NULL, (PBYTE)&dwValue, &cbValue);
        RegCloseKey(hKey);
    }
    return dwValue;
}

BOOL IsApplianceServer(VOID)
{
    BOOL result = g_bApplianceServer;

    if (g_bApplianceServer == -1)
    {
        LSTATUS error;
        HKEY hKey;
        error = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "System\\WPA\\ApplianceServer", 0, KEY_QUERY_VALUE, &hKey);
        if (error == ERROR_SUCCESS)
        {
            DWORD dwType, dwValue = 0, cbValue = sizeof(dwValue);
            error = RegQueryValueExA(hKey, "Installed", NULL, &dwType, (PBYTE)&dwValue, &cbValue);
            if (error == ERROR_SUCCESS && dwType == REG_DWORD && dwValue)
                g_bApplianceServer = TRUE;
            RegCloseKey(hKey);
        }
        result = g_bApplianceServer;

        if (g_bApplianceServer == -1)
        {
            result = FALSE;
            g_bApplianceServer = FALSE;
        }
    }

    return result;
}

BOOL staticIsOS(DWORD dwInfoType)
{
    EnsureVerInfo();

    switch (dwInfoType)
    {
        case OS_WIN32S:         // 0x00
            return g_verInfo.dwPlatformId == VER_PLATFORM_WIN32s;

        case OS_NT:             // 0x01
            return IsWinNT();

        case OS_WIN9XORNT:      // 0x02  Win9x
            if (!IsWin9x())
                return FALSE;
            return IsVersionOrLater(4);

        case OS_NTORGREATER:    // 0x03  NT
            if (!IsWinNT())
                return FALSE;
            return IsVersionOrLater(4);

        case OS_WIN2000ORGREATER:
            if (!IsWinNT())
                return FALSE;
            return IsVersionOrLater(5);

        case OS_WIN98ORGREATER:
            if (!IsWin9x())
                return FALSE;
            if (g_verInfo.dwMajorVersion > 4)
                return TRUE;
            if (g_verInfo.dwMajorVersion == 4)
                return g_verInfo.dwMinorVersion >= 10; // Win98
            return FALSE;

        case OS_WIN98_GOLD:     // 0x06  Win98 RTM (build 1998)
            return IsWin9x() && IsVersion(4, 10) && LOWORD(g_verInfo.dwBuildNumber) == 1998;

        case OS_WIN2000PRO:     // 0x08
            if (!IsWorkstation())
                return FALSE;
            return IsVersion(5);

        case OS_WIN2000SERVER:  // 0x09
            if (!IsServer())
                return FALSE;
            if (HasSuite(SUITE_ENTERPRISE | 0x02))
                return FALSE; // Advanced Server / Datacenter
            return IsVersion(5);

        case OS_WIN2000ADVSERVER: // 0x0A
            if (!IsServer())
                return FALSE;
            return IsVersion(5) && HasSuite(SUITE_ENTERPRISE) && IsTSSuiteBasic();

        case OS_WIN2000DATACENTER: // 0x0B
            if (!IsServer())
                return FALSE;
            return IsVersion(5) && HasSuite(0x40 /*SUITE_DATACENTER*/) && IsTSSuiteBasic(); // 0x0B

        case OS_WIN2000TERMINAL: // 0x0C  Terminal Services
            if (!HasSuite(SUITE_TERMINAL))
                return FALSE;
            return IsVersionOrLater(5);

        case OS_EMBEDDED:       // 0x0D
            return HasSuite(SUITE_EMBEDDEDNT);

        case OS_TERMINALCLIENT: // 0x0E
            return GetSystemMetrics(SM_REMOTESESSION);

        case OS_TERMINALREMOTEADMIN: // 0x0F
            return HasSuite(SUITE_TERMINAL) && HasSuite(SUITE_SINGLEUSERTS);

        case OS_WIN95_GOLD:     // 0x10  Win95 RTM (build 950)
            return IsWin9x() && IsVersion(4, 0) && LOWORD(g_verInfo.dwBuildNumber) == 950;

        case OS_MEORGREATER:    // 0x11  Win98 SE (4.90)
            if (!IsWin9x())
                return FALSE;
            if (g_verInfo.dwMajorVersion > 4)
                return TRUE;
            if (g_verInfo.dwMajorVersion == 4)
                return g_verInfo.dwMinorVersion >= 90;
            return FALSE;

        case OS_XPORGREATER:    // 0x12
            if (!IsWinNT())
                return FALSE;
            if (g_verInfo.dwMajorVersion > 5)
                return TRUE;
            if (g_verInfo.dwMajorVersion < 5)
                return FALSE;
            if (g_verInfo.dwMinorVersion)
                return TRUE;
            return LOWORD(g_verInfo.dwBuildNumber) > 0x893; // build 2195

        case OS_HOME:           // 0x13
            return IsWinNT() && HasSuite(SUITE_PERSONAL);

        case OS_PROFESSIONAL:   // 0x14
            return IsWinNT() && IsWorkstation();

        case OS_DATACENTER:     // 0x15
            if (g_verInfo.wProductType == VER_NT_SERVER)
                goto check_datacenter;
            if (g_verInfo.wProductType != VER_NT_DOMAIN_CONTROLLER)
                return FALSE;
    check_datacenter:
            return HasSuite(0x40) && IsTSSuiteBasic();

        case OS_ADVSERVER:      // 0x16
            if (g_verInfo.wProductType != VER_NT_SERVER &&
                g_verInfo.wProductType != VER_NT_DOMAIN_CONTROLLER)
            {
                return FALSE;
            }
            return HasSuite(SUITE_ENTERPRISE) && IsTSSuiteBasic();

        case OS_SERVER:         // 0x17
            if (!IsServer())
                return FALSE;
            if (HasSuite(0xA2))
                return FALSE; // SUITE_ENTERPRISE | SUITE_BACKOFFICE | 0x02
            return !HasSuite(0x400);

        case OS_TERMINALSERVER: // 0x18  Terminal Server
            return HasSuite(SUITE_TERMINAL) && !HasSuite(SUITE_SINGLEUSERTS);

        case OS_PERSONALTERMINALSERVER: // 0x19  Personal Terminal Server
            return HasSuite(SUITE_SINGLEUSERTS) && !HasSuite(SUITE_TERMINAL);

        case OS_FASTUSERSWITCHING: // 0x1A
            if (!HasSuite(SUITE_TERMINAL | SUITE_SINGLEUSERTS))
                return FALSE;
            return IsWinlogonRegValueSet(
                HKEY_LOCAL_MACHINE,
                "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\policies\\system",
                "AllowMultipleTSSessions") != 0;

        case OS_WELCOMELOGONUI:  // 0x1B
            if (!IsWorkstation())
                return FALSE;
            if (IsMachineDomainMember())
                return FALSE;
            if (IsWinlogonRegValuePresent(
                    HKEY_LOCAL_MACHINE,
                    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                    "GinaDLL"))
            {
                return FALSE;
            }
            return IsWinlogonRegValueSet(HKEY_LOCAL_MACHINE,
                "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\policies\\system",
                "LogonType") != 0;

        case OS_DOMAINMEMBER:   // 0x1C
            return IsMachineDomainMember();

        case OS_ANYSERVER:      // 0x1D
            return IsServer();

        case OS_WOW6432:        // 0x1E
            return RunningOnWow64();

        case OS_WEBSERVER:      // 0x1F
            return FALSE;

        case OS_SMALLBUSINESSSERVER: // 0x20
            return HasSuite(SUITE_SMALLBUSINESS);

        case OS_TABLETPC:       // 0x21
            return GetSystemMetrics(SM_TABLETPC);

        case OS_SERVERADMINUI:  // 0x22
            return ShouldShowServerAdminUI();

        case OS_MEDIACENTER:    // 0x23
            return GetSystemMetrics(SM_MEDIACENTER) != 0;

        case OS_APPLIANCE:      // 0x24
            return IsApplianceServer();

        case OS_SERVERR2:
            return GetSystemMetrics(SM_SERVERR2) != 0;

        case 0x25:
        case 0x28:
        case 0x29:
        case 0x2B:
        default:
            return FALSE;
    }
}
