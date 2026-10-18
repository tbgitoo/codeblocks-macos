/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision: 7107 $
 * $Id: main.cpp 7107 2011-04-15 11:19:11Z mortenmacfly $
 * $HeadURL: https://svn.code.sf.net/p/codeblocks/code/trunk/src/tools/ConsoleRunner/main.cpp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#ifdef __WXMSW__
    #include <windows.h>
    #include <conio.h>
    #define wait_key getch
#else
    #include <termios.h>
    #include <unistd.h>
#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
    #include <sys/wait.h>
#endif

/* Match Windows: any key, not only Enter. */
static int wait_key(void)
{
    struct termios oldt, newt;
    int ch;

    if (tcgetattr(STDIN_FILENO, &oldt) != 0)
        return getchar();

    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif

#ifdef __MINGW32__
int _CRT_glob = 0;
#endif

bool hasSpaces(const char* str)
{
    char last = 0;
    while (str && *str)
    {
        if ((*str == ' ' || *str == '\t') && last != '\\')
            return true;
        last = *str++;
    }
    return false;
}

#ifdef __WXMSW__
static void raise_console()
{
    // A GUI parent (the IDE) keeps the foreground, so the new console opens
    // behind it. Lift it without leaving it topmost.
    HWND hwnd = GetConsoleWindow();
    if (!hwnd)
        return;
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd);
}

static bool console_is_visible()
{
    HWND hwnd = GetConsoleWindow();
    if (!hwnd || !IsWindowVisible(hwnd))
        return false;
    RECT r;
    if (!GetWindowRect(hwnd, &r))
        return false;
    // Windows Terminal's pseudoconsole is "visible" but has no area. The tab
    // lives in a window the IDE does not reveal.
    return (r.right > r.left) && (r.bottom > r.top);
}

// Windows 11 gives a console program started by the GUI IDE to the default
// terminal, usually a tab in a Windows Terminal window that is already open.
// That looks like "Run never opens a console". Host the same command in
// conhost so a real window appears. Returns true when this process only
// waited for that hosted copy.
static bool relaunch_with_visible_console(int argc, char** argv, int* exitCode)
{
    if (GetEnvironmentVariableA("CB_CONSOLE_RUNNER_HOST", NULL, 0) > 0)
        return false;
    if (console_is_visible())
        return false;

    char self[MAX_PATH];
    char systemDir[MAX_PATH];
    if (!GetModuleFileNameA(NULL, self, MAX_PATH))
        return false;
    if (!GetSystemDirectoryA(systemDir, MAX_PATH))
        return false;

    size_t bytes = strlen(systemDir) + strlen(self) + 64;
    for (int i = 1; i < argc; ++i)
        bytes += strlen(argv[i]) + 3;

    char* cmd = new char[bytes];
    strcpy(cmd, "\"");
    strcat(cmd, systemDir);
    strcat(cmd, "\\conhost.exe\" -- \"");
    strcat(cmd, self);
    strcat(cmd, "\"");
    for (int i = 1; i < argc; ++i)
    {
        strcat(cmd, " ");
        bool sp = hasSpaces(argv[i]);
        if (sp)
            strcat(cmd, "\"");
        strcat(cmd, argv[i]);
        if (sp)
            strcat(cmd, "\"");
    }

    SetEnvironmentVariableA("CB_CONSOLE_RUNNER_HOST", "1");
    FreeConsole();

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    delete[] cmd;
    if (!ok)
    {
        SetEnvironmentVariableA("CB_CONSOLE_RUNNER_HOST", NULL);
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    *exitCode = (int)code;
    return true;
}

static void ensure_console()
{
    // A GUI parent can attach a console that has no window: GetConsoleWindow()
    // is NULL and AllocConsole() fails with ERROR_ALREADY_EXISTS. Detach that
    // hidden console and allocate a visible one. Without this, getch() waits
    // on a window the user never sees.
    if (!GetConsoleWindow())
    {
        FreeConsole();
        if (!AllocConsole())
            return;
        FILE* ignored = nullptr;
        ignored = freopen("CONOUT$", "w", stdout);
        ignored = freopen("CONOUT$", "w", stderr);
        ignored = freopen("CONIN$", "r", stdin);
        (void)ignored;
    }
    raise_console();
}
#endif

int execute_command(char *cmdline)
{
#ifdef __WXMSW__
    //Windows's system() seems to not be able to handle parentheses in
    //the path, so we have to launch the program a different way.

    ensure_console();
    SetConsoleTitle(cmdline);

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

    // Start the child process. Inherit the console we just made visible.
    BOOL created = CreateProcess( NULL, TEXT(cmdline), NULL, NULL, FALSE, 0,
                   NULL, NULL, &si, &pi );
    if (!created)
    {
        DWORD err = GetLastError();
        printf("Failed to start process (error %lu)\n", err);
        return 1;
    }

    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

    // Get the return value of the child process
    DWORD ret;
    GetExitCodeProcess( pi.hProcess, &ret );

    // Close process and thread handles.
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

    return ret;
#else
    int ret = system(cmdline);
    if(WIFEXITED(ret))
    {
        return WEXITSTATUS(ret);
    }
    else
    {
        return -1;
    }
#endif
}

int main(int argc, char** argv)
{
#ifdef __WXMSW__
    int hosted = 0;
    if (relaunch_with_visible_console(argc, argv, &hosted))
        return hosted;
    ensure_console();
#endif
    if (argc < 2)
    {
        printf("Usage: cb_console_runner <filename> <args ...>\n");
        return 1;
    }

    // count size of arguments
    size_t fullsize = 0;
    for (int i = 1; i < argc; ++i)
    {
        fullsize += strlen(argv[i]);
    }
    // add some slack for spaces between args plus quotes around executable
    fullsize += argc + 32;

    char* cmdline = new char[fullsize];
    memset(cmdline, 0, fullsize);

    // 1st arg (executable) enclosed in quotes to support filenames with spaces
    bool sp = hasSpaces(argv[1]);
    if (sp)
        strcat(cmdline, "\"");
    strcat(cmdline, argv[1]);
    if (sp)
        strcat(cmdline, "\"");
    strcat(cmdline, " ");

    for (int i = 2; i < argc; ++i)
    {
        sp = hasSpaces(argv[i]);
        if (sp)
            strcat(cmdline, "\"");
        strcat(cmdline, argv[i]);
        if (sp)
            strcat(cmdline, "\"");
        strcat(cmdline, " ");
    }

    timeval tv;
    gettimeofday(&tv, NULL);
    double cl = tv.tv_sec + (double)tv.tv_usec / 1000000;

    int ret = execute_command(cmdline);

#ifdef __WXMSW__
    ensure_console();
#endif

    gettimeofday(&tv, NULL);
    cl = (tv.tv_sec + (double)tv.tv_usec / 1000000) - cl;

    printf("\nProcess returned %d (0x%X)   execution time : %0.3f s", ret, ret, cl);
    printf("\nPress any key to continue.\n");

    wait_key();

    delete[] cmdline;
    return ret;
}
