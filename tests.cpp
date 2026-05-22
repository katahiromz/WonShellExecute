// tests.cpp --- WonShellExecute tests
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <stdio.h>
#include <string>

std::string ArgvToCommandLineA(int argc, char **argv)
{
    std::string cmdline;

    for (int iarg = 0; iarg < argc; ++iarg) {
        std::string arg = argv[iarg];

        if (iarg != 0) cmdline += ' ';

        bool needQuote = arg.empty() || arg.find_first_of(" \t\n\v\"") != std::string::npos;

        if (needQuote) cmdline += '"';

        for (size_t j = 0; j < arg.size(); ) {
            size_t bs = 0;
            while (j < arg.size() && arg[j] == '\\') {
                ++j;
                ++bs;
            }

            if (j == arg.size()) {
                cmdline.append(bs * (needQuote ? 2 : 1), '\\');
                break;
            }
            if (arg[j] == '"') {
                cmdline.append(bs * 2 + 1, '\\');
                cmdline += '"';
                ++j;
            } else {
                cmdline.append(bs, '\\');
                cmdline += arg[j++];
            }
        }

        if (needQuote) cmdline += '"';
    }

    return cmdline;
}

int main(int argc, char **argv)
{
    if (argc <= 1)
    {
        puts("tests [your.exe [\"parameters\"]]");
        return 0;
    }

    std::string params = ArgvToCommandLineA(argc - 2, argv + 2);

    BOOL bOK = ((INT_PTR)ShellExecuteA(NULL, NULL, argv[1], params.c_str(), NULL, SW_SHOWNORMAL) > 32);
    return bOK ? 0 : 1;
}
