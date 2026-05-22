// test2.cpp --- WonShellExecute tests
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string>
#include "shlexec.h"

int main(void)
{
    HWND hwnd = NULL;
    LPITEMIDLIST pidl;
    HRESULT hr = SHGetSpecialFolderLocation(hwnd, CSIDL_STARTMENU, &pidl);
    if (FAILED(hr))
        return 1;

    SHELLEXECUTEINFOW sei = {sizeof(sei)};
    sei.hwnd = hwnd;
    sei.lpVerb = L"open";
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_IDLIST;
    sei.lpIDList = pidl;
    BOOL bOK = WonShellExecuteExW(&sei);

    CoTaskMemFree(pidl);
    return bOK ? 0 : 1;
}
