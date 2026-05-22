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
```

License: MIT
