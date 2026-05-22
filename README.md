# WonShellExecute

```c
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

typedef HINSTANCE (CALLBACK *FN_WOWSHELLEXECCB)(LPCSTR pszCommand, INT nCmdShow, LPCSTR pszWorkDir);

EXTERN_C HINSTANCE WINAPI
WonWOWShellExecute(
    HWND hWnd,
    LPCSTR lpVerb,
    LPCSTR lpFile,
    LPCSTR lpParameters,
    LPCSTR lpDirectory,
    INT iShowCmd,
    FN_WOWSHELLEXECCB callback);
```

License: MIT
