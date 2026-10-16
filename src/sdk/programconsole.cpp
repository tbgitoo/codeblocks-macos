/*
 * Program Console transport + Run launch (macOS). Stubs elsewhere.
 */
#include "sdk_precomp.h"

#ifndef CB_PRECOMP
    #include "manager.h"
    #include "configmanager.h"
    #include "sdk_events.h"
    #include "globals.h"
#endif

#include "programconsole.h"
#include "programconsole_internal.h"
#include "pipedprocess.h"

#include <wx/process.h>
#include <wx/timer.h>

#if defined(__WXMAC__)
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>

#include <atomic>
#include <memory>
#include <thread>
#endif

namespace
{
const wxString cfgKey = "/console_output_mode";
const wxString cfgProgramConsole = "program_console";
const wxString cfgTerminalApp = "terminal_app";

#if defined(__WXMAC__)

class PtySession
{
public:
    ~PtySession() { Close(); }

    bool Create(std::uint64_t generation)
    {
        Close();
        int master = -1;
        int slave = -1;
        char name[128]{};
        if (openpty(&master, &slave, name, nullptr, nullptr) != 0)
            return false;

        int flags = fcntl(master, F_GETFL, 0);
        if (flags >= 0)
            fcntl(master, F_SETFL, flags | O_NONBLOCK);

        m_master = master;
        m_slaveFd = slave;
        m_slaveName = wxString::FromUTF8(name);
        m_generation = generation;
        m_stop = false;
        m_reader = std::thread([this] { ReaderLoop(); });
        return true;
    }

    void Close()
    {
        m_stop = true;
        if (m_slaveFd >= 0) {
            ::close(m_slaveFd);
            m_slaveFd = -1;
        }
        if (m_master >= 0) {
            ::close(m_master);
            m_master = -1;
        }
        if (m_reader.joinable())
            m_reader.join();
        m_slaveName.clear();
    }

    const wxString& SlaveName() const { return m_slaveName; }
    int MasterFd() const { return m_master; }
    int SlaveFd() const { return m_slaveFd; }
    std::uint64_t Generation() const { return m_generation; }

    bool SendInput(const void* data, size_t len)
    {
        if (m_master < 0 || !data || !len)
            return false;
        const char* p = static_cast<const char*>(data);
        size_t left = len;
        while (left) {
            const ssize_t w = write(m_master, p, left);
            if (w < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN) {
                    usleep(1000);
                    continue;
                }
                return false;
            }
            p += w;
            left -= static_cast<size_t>(w);
        }
        return true;
    }

    bool Resize(unsigned rows, unsigned cols)
    {
        if (m_master < 0)
            return false;
        winsize ws{};
        ws.ws_row = static_cast<unsigned short>(rows ? rows : 1);
        ws.ws_col = static_cast<unsigned short>(cols ? cols : 1);
        return ioctl(m_master, TIOCSWINSZ, &ws) == 0;
    }

    using OutputFn = void (*)(std::uint64_t generation, const wxString& text);
    void SetOutputHandler(OutputFn fn) { m_outputFn = fn; }

private:
    void ReaderLoop()
    {
        while (!m_stop.load()) {
            const int fd = m_master;
            if (fd < 0)
                break;
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            timeval tv{0, 100000};
            const int n = select(fd + 1, &rfds, nullptr, nullptr, &tv);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                break;
            }
            if (n == 0)
                continue;
            char buf[512];
            const ssize_t r = read(fd, buf, sizeof(buf));
            if (r > 0) {
                if (m_outputFn) {
                    const wxString text = wxString::FromUTF8(buf, static_cast<size_t>(r));
                    if (text.empty())
                        m_outputFn(m_generation, wxString(buf, wxConvISO8859_1, static_cast<size_t>(r)));
                    else
                        m_outputFn(m_generation, text);
                }
                continue;
            }
            if (r == 0 || (r < 0 && errno == EIO)) {
                /* Held slave may keep the PTY open; completion is lifecycle-driven. */
                usleep(50000);
                continue;
            }
            if (errno == EINTR || errno == EAGAIN)
                continue;
            break;
        }
    }

    int m_master = -1;
    int m_slaveFd = -1;
    wxString m_slaveName;
    std::uint64_t m_generation = 0;
    std::atomic<bool> m_stop{false};
    std::thread m_reader;
    OutputFn m_outputFn = nullptr;
};

struct Coordinator
{
    std::uint64_t nextGeneration = 1;
    std::uint64_t currentGeneration = 0;
    bool active = false;
    bool isDebug = false;
    bool debugInferiorRunning = false;
};

Coordinator g_coord;
std::unique_ptr<PtySession> g_session;
double g_runStartSec = 0.0;

/* Forward decl — panel lives in the app binary; set from RegisterPanel path via weak hook. */
void (*g_clearScrollback)() = nullptr;
void (*g_appendOutput)(std::uint64_t, const wxString&) = nullptr;
void (*g_showPanel)() = nullptr;
void (*g_setActiveGen)(std::uint64_t) = nullptr;
void (*g_armInputFocus)() = nullptr;
void (*g_cancelInputFocus)() = nullptr;

void OutputThunk(std::uint64_t generation, const wxString& text)
{
    if (g_appendOutput)
        g_appendOutput(generation, text);
}

bool ChildClaimAndExec(int slaveFd, const wxString& command, const wxString& workingDir)
{
    if (setsid() < 0)
        return false;
    if (ioctl(slaveFd, TIOCSCTTY, 0) < 0)
        return false;
    if (dup2(slaveFd, STDIN_FILENO) < 0 ||
        dup2(slaveFd, STDOUT_FILENO) < 0 ||
        dup2(slaveFd, STDERR_FILENO) < 0)
        return false;
    if (slaveFd > STDERR_FILENO)
        ::close(slaveFd);

    /* Drop inherited descriptors (keep 0/1/2). */
    const int maxfd = static_cast<int>(sysconf(_SC_OPEN_MAX));
    for (int fd = 3; fd < maxfd && fd < 256; ++fd)
        ::close(fd);

    if (!workingDir.empty())
        chdir(workingDir.mb_str(wxConvUTF8));

    setpgid(0, 0);
    /* Avoid SIGTTOU stopping us before we become the foreground pgrp. */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    tcsetpgrp(STDIN_FILENO, getpid());

    const wxCharBuffer cmd = command.mb_str(wxConvUTF8);
    execl("/bin/sh", "sh", "-c", cmd.data(), static_cast<char*>(nullptr));
    return false;
}

/*
 * Must be a PipedProcess: CompilerGCC::OnIdle casts pProcess to PipedProcess*
 * and calls HasInput(). A bare wxProcess crashes there.
 */
class PtyRunProcessTracker : public PipedProcess
{
public:
    PtyRunProcessTracker(PipedProcess** pvThis, wxEvtHandler* parent, int id, int index, long pid)
        : PipedProcess(pvThis, parent, id, /*pipe=*/false, wxEmptyString, index),
          m_waitPid(pid)
    {
        m_Pid = static_cast<int>(pid);
        m_waitTimer.SetOwner(this);
        Bind(wxEVT_TIMER, &PtyRunProcessTracker::OnWaitTimer, this);
        m_waitTimer.Start(100);
    }

    ~PtyRunProcessTracker() override
    {
        m_waitTimer.Stop();
    }

    bool HasInput() override
    {
        /* Stdio is on the Program Console PTY, not wxProcess pipes. */
        return false;
    }

private:
    void OnWaitTimer(wxTimerEvent&)
    {
        if (m_waitPid <= 0)
            return;
        int status = 0;
        const pid_t r = waitpid(static_cast<pid_t>(m_waitPid), &status, WNOHANG);
        if (r == 0)
            return;
        m_waitTimer.Stop();
        int exitCode = 0;
        if (r == m_waitPid) {
            if (WIFEXITED(status))
                exitCode = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                exitCode = 128 + WTERMSIG(status);
        } else {
            exitCode = -1;
        }

        ProgramConsole::AnnounceRunFinished(exitCode);
        /* Reuse PipedProcess termination path (posts event + self-deletes). */
        OnTerminate(static_cast<int>(m_waitPid), exitCode);
    }

    long m_waitPid = 0;
    wxTimer m_waitTimer;
};

#endif // __WXMAC__
} // namespace

namespace ProgramConsole
{

bool PreferProgramConsole()
{
#if defined(__WXMAC__)
    if (!platform::macosx)
        return false;
    const wxString mode = Manager::Get()->GetConfigManager("app")->Read(cfgKey, cfgProgramConsole);
    return mode != cfgTerminalApp;
#else
    return false;
#endif
}

void ShowPanel()
{
#if defined(__WXMAC__)
    if (g_showPanel)
        g_showPanel();
#endif
}

bool BeginRunSession()
{
#if defined(__WXMAC__)
    if (!PreferProgramConsole())
        return false;
    const std::uint64_t gen = g_coord.nextGeneration++;
    auto session = std::make_unique<PtySession>();
    session->SetOutputHandler(&OutputThunk);
    if (!session->Create(gen))
        return false;
    g_session = std::move(session);
    g_coord.currentGeneration = gen;
    g_coord.active = false;
    g_coord.isDebug = false;
    g_coord.debugInferiorRunning = false;
    return true;
#else
    return false;
#endif
}

void ActivateRunSession()
{
#if defined(__WXMAC__)
    g_coord.active = true;
    {
        timeval tv{};
        gettimeofday(&tv, nullptr);
        g_runStartSec = static_cast<double>(tv.tv_sec) + static_cast<double>(tv.tv_usec) / 1000000.0;
    }
    if (g_clearScrollback)
        g_clearScrollback();
    if (g_setActiveGen)
        g_setActiveGen(g_coord.currentGeneration);
    if (g_showPanel)
        g_showPanel();
#endif
}

void AbortRunSession()
{
#if defined(__WXMAC__)
    if (g_session)
        g_session->Close();
    g_session.reset();
    g_coord.active = false;
    g_coord.isDebug = false;
    g_coord.debugInferiorRunning = false;
#endif
}

void EndRunSession()
{
#if defined(__WXMAC__)
    if (g_session)
        g_session->Close();
    g_session.reset();
    g_coord.active = false;
    g_coord.isDebug = false;
    g_coord.debugInferiorRunning = false;
#endif
}

void AnnounceRunFinished(int exitCode)
{
#if defined(__WXMAC__)
    /* Same banner as cb_console_runner; skip "Press any key" — pane stays open. */
    g_coord.active = false;
    if (!g_appendOutput || g_coord.currentGeneration == 0)
        return;
    double elapsed = 0.0;
    if (g_runStartSec > 0.0) {
        timeval tv{};
        gettimeofday(&tv, nullptr);
        elapsed = (static_cast<double>(tv.tv_sec) + static_cast<double>(tv.tv_usec) / 1000000.0)
                  - g_runStartSec;
        if (elapsed < 0.0)
            elapsed = 0.0;
    }
    const wxString line = wxString::Format(
        "\nProcess returned %d (0x%X)   execution time : %0.3f s\n",
        exitCode, exitCode, elapsed);
    g_appendOutput(g_coord.currentGeneration, line);
#else
    wxUnusedVar(exitCode);
#endif
}

void AnnounceDebugFinished(int exitCode)
{
#if defined(__WXMAC__)
    /* Exit code only — do not report wall time under the debugger. */
    g_coord.active = false;
    if (!g_appendOutput || g_coord.currentGeneration == 0)
        return;
    const wxString line = wxString::Format(
        "\nProcess returned %d (0x%X)\n",
        exitCode, exitCode);
    g_appendOutput(g_coord.currentGeneration, line);
#else
    wxUnusedVar(exitCode);
#endif
}

bool IsRunSessionActive()
{
#if defined(__WXMAC__)
    return g_coord.active && g_session;
#else
    return false;
#endif
}

bool AllowInputFocusSteal()
{
#if defined(__WXMAC__)
    if (!g_coord.active || !g_session)
        return false;
    if (g_coord.isDebug)
        return g_coord.debugInferiorRunning;
    return true;
#else
    return false;
#endif
}

void NotifyDebugInferiorState(bool running)
{
#if defined(__WXMAC__)
    if (!g_coord.isDebug || !g_coord.active)
        return;
    g_coord.debugInferiorRunning = running;
    if (running) {
        if (g_armInputFocus)
            g_armInputFocus();
    } else if (g_cancelInputFocus) {
        g_cancelInputFocus();
    }
#else
    wxUnusedVar(running);
#endif
}

bool BeginDebugSession(wxString& outSlavePath)
{
    outSlavePath.clear();
#if defined(__WXMAC__)
    if (!BeginRunSession() || !g_session)
        return false;
    g_coord.isDebug = true;
    g_coord.debugInferiorRunning = false;
    outSlavePath = g_session->SlaveName();
    return !outSlavePath.empty();
#else
    return false;
#endif
}

void ActivateDebugSession()
{
    ActivateRunSession();
}

void AbortDebugSession()
{
    AbortRunSession();
}

void EndDebugSession()
{
    EndRunSession();
}

long LaunchOnPty(const wxString& command, const wxString& workingDir)
{
#if defined(__WXMAC__)
    if (!g_session || g_session->SlaveFd() < 0)
        return 0;

    const int slaveFd = g_session->SlaveFd();
    const pid_t pid = fork();
    if (pid < 0)
        return 0;
    if (pid == 0) {
        /* Child: claim controlling tty on a dup of the held slave. */
        const int fd = ::dup(slaveFd);
        if (fd < 0)
            _exit(127);
        if (!ChildClaimAndExec(fd, command, workingDir))
            _exit(127);
        _exit(127);
    }
    return static_cast<long>(pid);
#else
    wxUnusedVar(command);
    wxUnusedVar(workingDir);
    return 0;
#endif
}

wxProcess* CreateRunProcessTracker(wxProcess** pvThis, wxEvtHandler* parent,
                                   int id, int index, long pid)
{
#if defined(__WXMAC__)
    return new PtyRunProcessTracker(reinterpret_cast<PipedProcess**>(pvThis),
                                    parent, id, index, pid);
#else
    wxUnusedVar(pvThis);
    wxUnusedVar(parent);
    wxUnusedVar(id);
    wxUnusedVar(index);
    wxUnusedVar(pid);
    return nullptr;
#endif
}

#if defined(__WXMAC__)
void BindPanelHooks(void (*clear)(),
                    void (*append)(std::uint64_t, const wxString&),
                    void (*show)(),
                    void (*setGen)(std::uint64_t),
                    void (*armInputFocus)(),
                    void (*cancelInputFocus)())
{
    g_clearScrollback = clear;
    g_appendOutput = append;
    g_showPanel = show;
    g_setActiveGen = setGen;
    g_armInputFocus = armInputFocus;
    g_cancelInputFocus = cancelInputFocus;
}

bool SendPanelInput(const void* data, size_t len)
{
    return g_session && g_session->SendInput(data, len);
}

bool ResizeSession(unsigned rows, unsigned cols)
{
    return g_session && g_session->Resize(rows, cols);
}

unsigned char GetVIntrByte()
{
    if (!g_session || g_session->SlaveFd() < 0)
        return 0x03;
    termios t{};
    if (tcgetattr(g_session->SlaveFd(), &t) != 0)
        return 0x03;
    return static_cast<unsigned char>(t.c_cc[VINTR]);
}

unsigned char GetVEofByte()
{
    if (!g_session || g_session->SlaveFd() < 0)
        return 0x04;
    termios t{};
    if (tcgetattr(g_session->SlaveFd(), &t) != 0)
        return 0x04;
    return static_cast<unsigned char>(t.c_cc[VEOF]);
}
#endif

} // namespace ProgramConsole
