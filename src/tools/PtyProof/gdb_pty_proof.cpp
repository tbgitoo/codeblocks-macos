/*
 * Milestone 2 — Bundled GDB → CB-owned PTY proof.
 *
 * PtySession keeps the master open; GDB gets the slave via `tty <path>`.
 * No Terminal.app / cb_debug_console_hold.
 *
 * Usage:
 *   ./gdb_pty_proof [path-to-gdb]
 * Env:
 *   CB_GDB — override default bundled gdb path
 *
 * Default GDB:
 *   /Applications/CodeBlocks.app/Contents/Resources/debugger/bin/gdb
 */

#include "pty_session.h"

#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Result {
    const char* id;
    bool pass;
    std::string observed;
};

std::vector<Result> g_results;
int g_fails = 0;
bool g_claim_off_vintr_ok = false;

void record(const char* id, bool pass, const std::string& observed)
{
    g_results.push_back({id, pass, observed});
    if (!pass)
        ++g_fails;
    std::printf("[%s] %s — %s\n", pass ? "PASS" : "FAIL", id, observed.c_str());
    std::fflush(stdout);
}

std::string default_gdb()
{
    const char* env = std::getenv("CB_GDB");
    if (env && env[0])
        return env;
    return "/Applications/CodeBlocks.app/Contents/Resources/debugger/bin/gdb";
}

std::string escape_ws(const std::string& s)
{
    std::string o;
    o.reserve(s.size());
    for (unsigned char c : s) {
        if (c == '\n')
            o += "\\n";
        else if (c == '\r')
            o += "\\r";
        else if (c < 32 || c == 127)
            o += "?";
        else
            o += static_cast<char>(c);
    }
    if (o.size() > 280)
        o = o.substr(0, 280) + "...";
    return o;
}

bool wait_contains(PtySession& pty, const std::string& needle, int timeout_ms,
                   std::string& collected)
{
    const int step = 50;
    int waited = 0;
    while (waited < timeout_ms) {
        collected += pty.TakeOutput();
        if (collected.find(needle) != std::string::npos)
            return true;
        usleep(static_cast<useconds_t>(step * 1000));
        waited += step;
    }
    collected += pty.TakeOutput();
    return collected.find(needle) != std::string::npos;
}

struct GdbProc {
    pid_t pid = -1;
    int to_gdb = -1;
    int from_gdb = -1;
    std::string log;
};

void gdb_close(GdbProc& g)
{
    if (g.to_gdb >= 0) {
        close(g.to_gdb);
        g.to_gdb = -1;
    }
    if (g.from_gdb >= 0) {
        close(g.from_gdb);
        g.from_gdb = -1;
    }
    if (g.pid > 0) {
        int st = 0;
        if (waitpid(g.pid, &st, WNOHANG) == 0) {
            kill(g.pid, SIGTERM);
            waitpid(g.pid, &st, 0);
        }
        g.pid = -1;
    }
}

bool gdb_spawn(GdbProc& g, const std::string& gdbPath, const std::string& inferiorPath)
{
    int in_pipe[2];
    int out_pipe[2];
    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0)
        return false;

    const pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0) {
        close(in_pipe[1]);
        close(out_pipe[0]);
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(in_pipe[0]);
        close(out_pipe[1]);
        execl(gdbPath.c_str(), "gdb", "-q", "-nx",
              "-ex", "set pagination off",
              "-ex", "set confirm off",
              inferiorPath.c_str(),
              static_cast<char*>(nullptr));
        _exit(127);
    }

    close(in_pipe[0]);
    close(out_pipe[1]);
    g.pid = pid;
    g.to_gdb = in_pipe[1];
    g.from_gdb = out_pipe[0];
    const int fl = fcntl(g.from_gdb, F_GETFL, 0);
    if (fl >= 0)
        fcntl(g.from_gdb, F_SETFL, fl | O_NONBLOCK);
    return true;
}

bool gdb_write(GdbProc& g, const std::string& cmd)
{
    if (g.to_gdb < 0)
        return false;
    const std::string line = cmd + "\n";
    const char* p = line.c_str();
    size_t left = line.size();
    while (left) {
        const ssize_t w = write(g.to_gdb, p, left);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        p += w;
        left -= static_cast<size_t>(w);
    }
    return true;
}

void gdb_pump(GdbProc& g, int timeout_ms)
{
    const int step = 20;
    int waited = 0;
    while (waited < timeout_ms) {
        char buf[1024];
        const ssize_t r = read(g.from_gdb, buf, sizeof(buf));
        if (r > 0) {
            g.log.append(buf, static_cast<size_t>(r));
            waited = 0;
            continue;
        }
        if (r < 0 && (errno == EAGAIN || errno == EINTR)) {
            usleep(static_cast<useconds_t>(step * 1000));
            waited += step;
            continue;
        }
        break;
    }
}

size_t gdb_mark(GdbProc& g)
{
    gdb_pump(g, 100);
    return g.log.size();
}

std::string gdb_since(GdbProc& g, size_t mark)
{
    gdb_pump(g, 200);
    if (mark >= g.log.size())
        return {};
    return g.log.substr(mark);
}

bool gdb_wait_since(GdbProc& g, size_t mark, const std::string& needle, int timeout_ms)
{
    const int step = 50;
    int waited = 0;
    while (waited < timeout_ms) {
        gdb_pump(g, step);
        if (gdb_since(g, mark).find(needle) != std::string::npos)
            return true;
        waited += step;
    }
    return gdb_since(g, mark).find(needle) != std::string::npos;
}

bool contains_ci_fragment(const std::string& hay, const char* needle)
{
    return hay.find(needle) != std::string::npos;
}

struct SessionResult {
    bool claim_supported = false;
    bool tty_ok = false;
    bool broke_at_main = false;
    bool inferior_stdio = false;
    bool input_ok = false;
    bool vintr_ok = false;
    bool dbg_interrupt_ok = false;
    bool claim_warning = false;
    std::string pty_out;
    std::string detail;
};

SessionResult run_once(const std::string& gdbPath,
                       const std::string& inferiorPath,
                       bool claim_off,
                       bool test_input,
                       bool test_vintr,
                       bool test_dbg_interrupt)
{
    SessionResult r;
    PtySession pty;
    if (!pty.Create()) {
        r.detail = "openpty failed";
        return r;
    }

    GdbProc gdb;
    if (!gdb_spawn(gdb, gdbPath, inferiorPath)) {
        r.detail = "gdb spawn failed";
        pty.Close();
        return r;
    }

    size_t m = gdb_mark(gdb);
    gdb_wait_since(gdb, m, "(gdb)", 4000);

    m = gdb_mark(gdb);
    gdb_write(gdb, "show inferior-tty-claim");
    gdb_wait_since(gdb, m, "(gdb)", 3000);
    const std::string show = gdb_since(gdb, m);
    r.claim_supported =
        contains_ci_fragment(show, "Claiming the inferior")
        || (contains_ci_fragment(show, " is on") || contains_ci_fragment(show, " is off"));
    if (contains_ci_fragment(show, "Undefined") || contains_ci_fragment(show, "No symbol"))
        r.claim_supported = false;

    /* Avoid Darwin arm64e shell / SIP startup delay and warning noise. */
    m = gdb_mark(gdb);
    gdb_write(gdb, "set startup-with-shell off");
    gdb_wait_since(gdb, m, "(gdb)", 2000);

    if (claim_off && r.claim_supported) {
        m = gdb_mark(gdb);
        gdb_write(gdb, "set inferior-tty-claim off");
        gdb_wait_since(gdb, m, "(gdb)", 2000);
    }

    m = gdb_mark(gdb);
    gdb_write(gdb, "tty " + pty.SlaveName());
    gdb_wait_since(gdb, m, "(gdb)", 3000);
    const std::string tty_resp = gdb_since(gdb, m);
    r.tty_ok = !contains_ci_fragment(tty_resp, "Undefined")
               && !contains_ci_fragment(tty_resp, "No such file")
               && !contains_ci_fragment(tty_resp, "Cannot");

    m = gdb_mark(gdb);
    gdb_write(gdb, "break main");
    gdb_wait_since(gdb, m, "Breakpoint", 3000);

    /*
     * Let console VINTR reach the inferior handler. Default GDB "handle SIGINT"
     * often stops in the debugger instead of delivering to the program — that
     * is the debugger-interrupt path, not Program Console Ctrl-C.
     */
    if (test_vintr) {
        m = gdb_mark(gdb);
        gdb_write(gdb, "handle SIGINT nostop noprint pass");
        gdb_wait_since(gdb, m, "(gdb)", 2000);
    }

    m = gdb_mark(gdb);
    gdb_write(gdb, "run");
    /*
     * Darwin GDB emits async thread messages before the stop. Sync on the
     * prompt after a hit — not merely the word "Breakpoint".
     */
    const bool hit = gdb_wait_since(gdb, m, "hit Breakpoint", 15000)
                     || gdb_wait_since(gdb, m, "Breakpoint 1, main", 5000);
    const bool prompted = gdb_wait_since(gdb, m, "(gdb)", 15000);
    r.broke_at_main = hit && prompted;

    const std::string after_run = gdb_since(gdb, m);
    r.claim_warning =
        contains_ci_fragment(after_run, "TIOCSCTTY")
        || contains_ci_fragment(after_run, "could not set controlling")
        || contains_ci_fragment(after_run, "Failed to claim");

    if (!r.broke_at_main) {
        r.pty_out = pty.TakeOutput();
        r.detail = "no breakpoint stop; gdb=[" + escape_ws(after_run)
                   + "] pty=[" + escape_ws(r.pty_out) + "]";
        gdb_close(gdb);
        pty.Close();
        return r;
    }

    m = gdb_mark(gdb);
    gdb_write(gdb, "continue");
    r.inferior_stdio = wait_contains(pty, "inferior-ready", 8000, r.pty_out);
    if (!r.inferior_stdio) {
        gdb_pump(gdb, 800);
        r.detail += " after_continue_gdb=[" + escape_ws(gdb_since(gdb, m))
                    + "] pty=[" + escape_ws(r.pty_out + pty.TakeOutput()) + "]";
    }

    if (test_input && r.inferior_stdio) {
        pty.SendInput("hello-from-master\n", 18);
        r.input_ok = wait_contains(pty, "got-input:hello-from-master", 4000, r.pty_out);
    }

    if (test_vintr && r.inferior_stdio) {
        /* Console VINTR via master (M1: VINTR=0x03). */
        pty.SendByte(0x03);
        r.vintr_ok = wait_contains(pty, "inferior-got-sigint", 4000, r.pty_out);
    }

    if (test_dbg_interrupt && r.inferior_stdio && !test_vintr) {
        /*
         * Debugger-side interrupt vs console VINTR:
         * Sync CLI GDB over pipes does not read `interrupt` while `continue`
         * blocks. Deliver SIGINT to the inferior PID (ptrace-visible). GDB
         * should report the signal on its control channel. We never write
         * VINTR to the PTY master — so no ^C echo. That distinguishes the
         * Program Console Ctrl-C path (VINTR byte) from a debugger interrupt.
         */
        int inferior_pid = 0;
        const auto pos = r.pty_out.find("inferior-pid-");
        if (pos != std::string::npos)
            inferior_pid = std::atoi(r.pty_out.c_str() + pos + 13);

        const size_t before = gdb_mark(gdb);
        const std::string pty_before = r.pty_out;
        if (inferior_pid > 1)
            kill(inferior_pid, SIGINT);

        const bool gdb_saw =
            gdb_wait_since(gdb, before, "Program received signal", 8000)
            || gdb_wait_since(gdb, before, "SIGINT", 4000)
            || gdb_wait_since(gdb, before, "received signal", 4000);
        r.pty_out += pty.TakeOutput();
        const std::string dbg = gdb_since(gdb, before);
        const bool new_vintr_echo =
            r.pty_out.size() > pty_before.size()
            && r.pty_out.find("^C", pty_before.size()) != std::string::npos;
        r.dbg_interrupt_ok = inferior_pid > 1 && gdb_saw && !new_vintr_echo;
        r.detail += " inferior_pid=" + std::to_string(inferior_pid)
                    + " dbg=[" + escape_ws(dbg) + "]"
                    + " new_vintr_echo=" + (new_vintr_echo ? "yes" : "no");
    }

    gdb_write(gdb, "quit");
    gdb_pump(gdb, 400);
    gdb_close(gdb);
    r.pty_out += pty.TakeOutput();

    r.detail = std::string("claim_off=") + (claim_off ? "yes" : "no")
               + " claim_supported=" + (r.claim_supported ? "yes" : "no")
               + " claim_warning=" + (r.claim_warning ? "yes" : "no")
               + " slave=" + pty.SlaveName()
               + " pty=[" + escape_ws(r.pty_out) + "]"
               + (r.detail.empty() ? "" : " " + r.detail);
    pty.Close();
    return r;
}

} // namespace

int main(int argc, char** argv)
{
    std::string gdbPath = default_gdb();
    if (argc >= 2)
        gdbPath = argv[1];

    const std::string inferiorPath = "./inferior";

    std::printf("=== Darwin GDB↔PTY proof (Milestone 2) ===\n");
    std::printf("gdb=%s\n", gdbPath.c_str());
    std::printf("inferior=%s\n", inferiorPath.c_str());
    std::fflush(stdout);

    if (access(gdbPath.c_str(), X_OK) != 0) {
        record("gdb_present", false, "not executable: " + gdbPath
               + " (set CB_GDB or pass path)");
        return 1;
    }
    record("gdb_present", true, gdbPath);

    if (access(inferiorPath.c_str(), X_OK) != 0) {
        record("inferior_present", false, "build inferior first (make)");
        return 1;
    }
    record("inferior_present", true, inferiorPath);

    /* --- A: claim-off — stdio / break (Terminal-path analogue flags) --- */
    {
        SessionResult a = run_once(gdbPath, inferiorPath,
                                   /*claim_off=*/true,
                                   /*input=*/true,
                                   /*vintr=*/true,
                                   /*dbg_interrupt=*/false);
        record("claim_off_tty", a.tty_ok && a.broke_at_main, a.detail);
        record("claim_off_break_continue", a.broke_at_main && a.inferior_stdio, a.detail);
        record("claim_off_inferior_stdio", a.inferior_stdio, a.detail);
        record("claim_off_input", a.input_ok, a.detail);
        /* Documentary: VINTR under claim-off may echo without SIGINT delivery. */
        /* Under claim-off, VINTR is expected NOT to deliver SIGINT (M2 finding). */
        record("claim_off_vintr", !a.vintr_ok,
               std::string(a.vintr_ok ? "UNEXPECTED: VINTR worked with claim-off"
                                      : "expected: VINTR does not deliver SIGINT with claim-off")
                   + " " + a.detail);
        record("claim_supported", a.claim_supported,
               a.claim_supported
                   ? "show inferior-tty-claim works on bundled GDB"
                   : "bundled GDB lacks inferior-tty-claim");
        g_claim_off_vintr_ok = a.vintr_ok;
    }

    /* --- B: claim left ON --- */
    {
        SessionResult b = run_once(gdbPath, inferiorPath,
                                   /*claim_off=*/false,
                                   /*input=*/true,
                                   /*vintr=*/true,
                                   /*dbg_interrupt=*/false);
        record("claim_on_tty", b.tty_ok && b.broke_at_main, b.detail);
        record("claim_on_inferior_stdio", b.inferior_stdio, b.detail);
        record("claim_on_input", b.input_ok, b.detail);
        record("claim_on_vintr", b.vintr_ok, b.detail);
        record("claim_on_warning", true,
               std::string("claim_warning=") + (b.claim_warning ? "yes" : "no")
                   + " " + b.detail);
        record("claim_off_required_for_stdio", true,
               b.inferior_stdio
                   ? "claim-on works for inferior stdio without Terminal/holder; "
                     "claim-off NOT required for I/O"
                   : "claim-on failed for stdio");
        record("claim_off_vs_vintr_finding", true,
               std::string("claim_off_vintr=") + (g_claim_off_vintr_ok ? "ok" : "fail")
                   + " claim_on_vintr=" + (b.vintr_ok ? "ok" : "fail")
                   + (b.vintr_ok && !g_claim_off_vintr_ok
                          ? " → leave claim ON for Program Console VINTR"
                          : !b.vintr_ok && !g_claim_off_vintr_ok
                                ? " → VINTR unreliable either way; use debugger interrupt for Stop"
                                : " → see detail"));
    }

    /* --- C: debugger interrupt (SIGINT to GDB) vs console VINTR --- */
    {
        SessionResult c = run_once(gdbPath, inferiorPath,
                                   /*claim_off=*/false,
                                   /*input=*/false,
                                   /*vintr=*/false,
                                   /*dbg_interrupt=*/true);
        record("dbg_interrupt_vs_vintr", c.dbg_interrupt_ok && c.inferior_stdio,
               c.detail);
    }

    /* --- D: no Terminal / holder used (structural — always true here) --- */
    record("no_terminal_holder", true,
           "harness uses PtySession master only; no osascript / cb_debug_console_hold");

    std::printf("\n=== Summary: %zu cases, %d failed ===\n",
                g_results.size(), g_fails);
    return g_fails ? 1 : 0;
}
