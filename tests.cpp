// tests.cpp --- WonShellExecute tests
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc <= 1)
    {
        puts("tests [your.exe [\"parameters\"]]");
        return 0;
    }
    const char *params = ((argc >= 3) ? argv[2] : NULL);
    return ((INT_PTR)ShellExecuteA(NULL, NULL, argv[1], params, NULL, SW_SHOWNORMAL) <= 32) ? 1 : 0;
}
