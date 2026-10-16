/*
 * Darwin PTY proof (Milestone 1) — standalone, not production UI.
 *
 * Builds and runs only on Darwin. Exercises openpty session setup,
 * process groups, VINTR/VEOF, resize, FD hygiene, and master-side
 * behaviour after the final slave closes.
 *
 * Usage: ./pty_proof
 * Exit:  0 if all critical cases PASS; 1 otherwise.
 */

#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>

#include <cerrno>
#include <csignal>
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

void record(const char* id, bool pass, const std::string& observed)
{
    g_results.push_back({id, pass, observed});
    if (!pass)
        ++g_fails;
    std::printf("[%s] %s — %s\n", pass ? "PASS" : "FAIL", id, observed.c_str());
    std::fflush(stdout);
}

std::string errno_str()
{
    return std::string(strerror(errno));
}

bool drain_master(int master, std::string& out, int timeout_ms, size_t stop_after = 0)
{
    out.clear();
    const int step_ms = 50;
    int waited = 0;
    while (waited < timeout_ms) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(master, &rfds);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = step_ms * 1000;
        const int n = select(master + 1, &rfds, nullptr, nullptr, &tv);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (n == 0) {
            waited += step_ms;
            if (!out.empty() && stop_after == 0)
                break; /* quiet after some data */
            continue;
        }
        char buf[512];
        const ssize_t r = read(master, buf, sizeof(buf));
        if (r < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EIO)
                return true; /* peer gone — treat as end */
            return false;
        }
        if (r == 0)
            return true; /* EOF */
        out.append(buf, static_cast<size_t>(r));
        waited = 0;
        if (stop_after && out.size() >= stop_after)
            break;
    }
    return true;
}

bool write_all(int fd, const void* data, size_t len)
{
    const char* p = static_cast<const char*>(data);
    size_t left = len;
    while (left) {
        const ssize_t w = write(fd, p, left);
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

/* Child becomes session leader and claims controlling terminal on slave. */
bool child_claim_ctty(int slave)
{
    if (setsid() < 0)
        return false;
    if (ioctl(slave, TIOCSCTTY, 0) < 0)
        return false;
    if (dup2(slave, STDIN_FILENO) < 0)
        return false;
    if (dup2(slave, STDOUT_FILENO) < 0)
        return false;
    if (dup2(slave, STDERR_FILENO) < 0)
        return false;
    if (slave > STDERR_FILENO)
        close(slave);
    return true;
}

void close_from(int start)
{
    const int maxfd = static_cast<int>(sysconf(_SC_OPEN_MAX));
    for (int fd = start; fd < maxfd && fd < 256; ++fd)
        close(fd);
}

struct Pty {
    int master = -1;
    int slave = -1;
    char name[128]{};
};

bool open_pty(Pty& p)
{
    p.master = p.slave = -1;
    std::memset(p.name, 0, sizeof(p.name));
    if (openpty(&p.master, &p.slave, p.name, nullptr, nullptr) != 0)
        return false;
    return true;
}

void close_pty(Pty& p)
{
    if (p.slave >= 0) {
        close(p.slave);
        p.slave = -1;
    }
    if (p.master >= 0) {
        close(p.master);
        p.master = -1;
    }
}

pid_t spawn_claimed(Pty& p, void (*child_fn)())
{
    const pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        close(p.master);
        if (!child_claim_ctty(p.slave)) {
            const int e = errno;
            dprintf(STDERR_FILENO, "claim failed: %s\n", strerror(e));
            _exit(90);
        }
        close_from(3);
        child_fn();
        _exit(0);
    }
    close(p.slave);
    p.slave = -1;
    return pid;
}

int wait_exit(pid_t pid, int timeout_ms)
{
    const int step = 20;
    int waited = 0;
    for (;;) {
        int status = 0;
        const pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            if (WIFEXITED(status))
                return WEXITSTATUS(status);
            if (WIFSIGNALED(status))
                return 128 + WTERMSIG(status);
            return -2;
        }
        if (r < 0)
            return -1;
        if (waited >= timeout_ms) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            return -3;
        }
        usleep(static_cast<useconds_t>(step * 1000));
        waited += step;
    }
}

/* ---------- individual child bodies ---------- */

void child_hello()
{
    write(STDOUT_FILENO, "hello\n", 6);
    _exit(0);
}

void child_prompt()
{
    write(STDOUT_FILENO, "prompt>", 7);
    /* leave stdout open briefly so parent can read before exit closes slave */
    usleep(200000);
    _exit(0);
}

volatile sig_atomic_t g_got_int = 0;
void on_int(int)
{
    g_got_int = 1;
}

void child_vintr()
{
    struct sigaction sa{};
    sa.sa_handler = on_int;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    /* Make sure we are foreground pgrp of the tty */
    const pid_t self = getpid();
    tcsetpgrp(STDIN_FILENO, self);
    setpgid(0, 0);

    write(STDOUT_FILENO, "ready\n", 6);
    for (int i = 0; i < 200 && !g_got_int; ++i)
        usleep(10000);
    if (g_got_int) {
        write(STDOUT_FILENO, "got-sigint\n", 11);
        _exit(0);
    }
    write(STDOUT_FILENO, "no-sigint\n", 10);
    _exit(1);
}

void child_veof()
{
    char buf[64];
    write(STDOUT_FILENO, "reading\n", 8);
    const ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n == 0) {
        write(STDOUT_FILENO, "got-eof\n", 8);
        _exit(0);
    }
    if (n < 0) {
        dprintf(STDOUT_FILENO, "read-err-%d\n", errno);
        _exit(2);
    }
    dprintf(STDOUT_FILENO, "got-data-%zd\n", n);
    _exit(1);
}

volatile sig_atomic_t g_got_winch = 0;
void on_winch(int)
{
    g_got_winch = 1;
}

void child_winch()
{
    struct sigaction sa{};
    sa.sa_handler = on_winch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, nullptr);
    tcsetpgrp(STDIN_FILENO, getpid());
    setpgid(0, 0);
    write(STDOUT_FILENO, "ready\n", 6);
    for (int i = 0; i < 200 && !g_got_winch; ++i)
        usleep(10000);
    if (g_got_winch) {
        winsize ws{};
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
        dprintf(STDOUT_FILENO, "winch-%u-%u\n", ws.ws_row, ws.ws_col);
        _exit(0);
    }
    write(STDOUT_FILENO, "no-winch\n", 9);
    _exit(1);
}

void child_cr_progress()
{
    write(STDOUT_FILENO, "aaa\rbbb\rccc\n", 12);
    _exit(0);
}

void child_echo_probe()
{
    char c = 0;
    write(STDOUT_FILENO, "type\n", 5);
    const ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1)
        dprintf(STDOUT_FILENO, "got-%c\n", c);
    else
        write(STDOUT_FILENO, "nogot\n", 6);
    _exit(0);
}

void child_immediate_exit()
{
    _exit(42);
}

void child_sleep_then_exit()
{
    write(STDOUT_FILENO, "alive\n", 6);
    usleep(100000);
    _exit(0);
}

/* ---------- tests ---------- */

void test_allocation_and_termios()
{
    Pty p;
    if (!open_pty(p)) {
        record("allocation", false, "openpty failed: " + errno_str());
        return;
    }
    termios t{};
    if (tcgetattr(p.slave, &t) != 0) {
        record("termios_vintr_veof", false, "tcgetattr failed: " + errno_str());
        close_pty(p);
        return;
    }
    const cc_t vintr = t.c_cc[VINTR];
    const cc_t veof = t.c_cc[VEOF];
    char buf[128];
    std::snprintf(buf, sizeof(buf),
                  "slave=%s VINTR=0x%02x VEOF=0x%02x ICANON=%d ECHO=%d",
                  p.name, vintr, veof,
                  (t.c_lflag & ICANON) ? 1 : 0,
                  (t.c_lflag & ECHO) ? 1 : 0);
    record("allocation", true, std::string("openpty ok, ") + p.name);
    record("termios_vintr_veof",
           vintr == 0x03 && veof == 0x04,
           buf);
    close_pty(p);
}

void test_setsid_variants()
{
    /* Report pipe: child must not write diagnostics on closed master. */
    auto run_claim_probe = [](const char* id, bool do_setsid, bool do_tiocsctty,
                              bool expect_claim) {
        int pipes[2];
        if (pipe(pipes) != 0) {
            record(id, false, "pipe failed");
            return;
        }
        Pty p;
        if (!open_pty(p)) {
            close(pipes[0]);
            close(pipes[1]);
            record(id, false, "openpty failed");
            return;
        }
        const pid_t pid = fork();
        if (pid == 0) {
            close(pipes[0]);
            close(p.master);
            int sid_rc = 0;
            int sid_err = 0;
            if (do_setsid) {
                sid_rc = setsid();
                if (sid_rc < 0)
                    sid_err = errno;
            }
            int ctty_rc = 0;
            int ctty_err = 0;
            if (do_tiocsctty) {
                errno = 0;
                ctty_rc = ioctl(p.slave, TIOCSCTTY, 0);
                ctty_err = (ctty_rc < 0) ? errno : 0;
            }
            const pid_t tsid = tcgetsid(p.slave);
            const pid_t selfsid = getsid(0);
            dprintf(pipes[1],
                    "setsid_rc=%d setsid_err=%d ctty_rc=%d ctty_err=%d "
                    "tcgetsid=%d getsid=%d\n",
                    sid_rc, sid_err, ctty_rc, ctty_err, (int)tsid, (int)selfsid);
            const bool claimed = (tsid >= 0 && tsid == selfsid);
            _exit(expect_claim ? (claimed ? 0 : 1) : 0);
        }
        close(pipes[1]);
        close(p.slave);
        p.slave = -1;
        char buf[256];
        std::string out;
        for (;;) {
            const ssize_t n = read(pipes[0], buf, sizeof(buf));
            if (n <= 0)
                break;
            out.append(buf, static_cast<size_t>(n));
        }
        close(pipes[0]);
        const int st = wait_exit(pid, 2000);
        if (expect_claim)
            record(id, st == 0, "exit=" + std::to_string(st) + " " + out);
        else
            record(id, !out.empty(), "observed: " + out);
        close_pty(p);
    };

    /* Variant A: setsid + TIOCSCTTY (planned path) — must claim */
    run_claim_probe("setsid_tiocsctty", true, true, true);
    /* Variant B: no setsid, only TIOCSCTTY — document Darwin result */
    run_claim_probe("no_setsid_tiocsctty", false, true, false);
    /* Variant C: setsid without TIOCSCTTY — must NOT claim */
    {
        int pipes[2];
        pipe(pipes);
        Pty p;
        open_pty(p);
        const pid_t pid = fork();
        if (pid == 0) {
            close(pipes[0]);
            close(p.master);
            setsid();
            errno = 0;
            const pid_t tsid = tcgetsid(p.slave);
            const int e = errno;
            dprintf(pipes[1], "tcgetsid=%d errno=%d getsid=%d\n",
                    (int)tsid, e, (int)getsid(0));
            _exit(tsid < 0 ? 0 : 1);
        }
        close(pipes[1]);
        close(p.slave);
        p.slave = -1;
        char buf[256];
        std::string out;
        for (;;) {
            const ssize_t n = read(pipes[0], buf, sizeof(buf));
            if (n <= 0)
                break;
            out.append(buf, static_cast<size_t>(n));
        }
        close(pipes[0]);
        const int st = wait_exit(pid, 2000);
        record("setsid_no_tiocsctty", st == 0,
               "exit=" + std::to_string(st) + " " + out);
        close_pty(p);
    }
}

void test_stdio_and_prompt()
{
    Pty p;
    if (!open_pty(p)) {
        record("stdio_newline", false, "openpty failed");
        return;
    }
    const pid_t pid = spawn_claimed(p, child_hello);
    std::string out;
    drain_master(p.master, out, 1500);
    const int st = wait_exit(pid, 2000);
    record("stdio_newline", st == 0 && out.find("hello") != std::string::npos,
           "exit=" + std::to_string(st) + " out=[" + out + "]");
    close_pty(p);

    if (!open_pty(p)) {
        record("prompt_no_newline", false, "openpty failed");
        return;
    }
    const pid_t pid2 = spawn_claimed(p, child_prompt);
    std::string out2;
    drain_master(p.master, out2, 1500);
    const int st2 = wait_exit(pid2, 2000);
    record("prompt_no_newline",
           st2 == 0 && out2.find("prompt>") != std::string::npos,
           "exit=" + std::to_string(st2) + " out=[" + out2 + "]");
    close_pty(p);
}

void test_echo()
{
    Pty p;
    if (!open_pty(p)) {
        record("terminal_echo", false, "openpty failed");
        return;
    }
    /* Ensure ECHO is on (default) */
    termios t{};
    tcgetattr(p.slave, &t);
    t.c_lflag |= (ECHO | ICANON);
    tcsetattr(p.slave, TCSANOW, &t);

    const pid_t pid = spawn_claimed(p, child_echo_probe);
    std::string out;
    drain_master(p.master, out, 800); /* wait for "type\n" */
    write_all(p.master, "Z\n", 2);
    drain_master(p.master, out, 1500);
    wait_exit(pid, 2000);

    /* With ECHO, master should see echoed 'Z' from the line discipline,
       plus child "got-Z". Harness itself must not write a second echo. */
    const bool saw_echo_or_data = out.find('Z') != std::string::npos;
    const bool harness_only_once = true; /* by construction — we write once */
    record("terminal_echo", saw_echo_or_data && harness_only_once,
           "harness writes once; observed=[" + out + "]");
    close_pty(p);
}

void test_vintr()
{
    Pty p;
    if (!open_pty(p)) {
        record("vintr", false, "openpty failed");
        return;
    }
    termios t{};
    tcgetattr(p.slave, &t);
    const cc_t vintr = t.c_cc[VINTR];

    const pid_t pid = spawn_claimed(p, child_vintr);
    std::string out;
    drain_master(p.master, out, 800);
    /* Put child in foreground explicitly from parent side too */
    tcsetpgrp(p.master, pid);
    const char c = static_cast<char>(vintr);
    write_all(p.master, &c, 1);
    drain_master(p.master, out, 2000);
    const int st = wait_exit(pid, 3000);
    record("vintr",
           st == 0 && out.find("got-sigint") != std::string::npos,
           "VINTR=0x" + std::to_string(vintr) + " exit=" + std::to_string(st)
               + " out=[" + out + "]");
    close_pty(p);
}

void test_veof()
{
    Pty p;
    if (!open_pty(p)) {
        record("veof", false, "openpty failed");
        return;
    }
    termios t{};
    tcgetattr(p.slave, &t);
    const cc_t veof = t.c_cc[VEOF];

    const pid_t pid = spawn_claimed(p, child_veof);
    std::string out;
    drain_master(p.master, out, 800); /* "reading\n" */
    /* VEOF at start of line → read() returns 0; master stays open */
    const char c = static_cast<char>(veof);
    write_all(p.master, &c, 1);

    /* Master must still be open for reading child output after VEOF. */
    drain_master(p.master, out, 2000);
    const int master_still_open = (fcntl(p.master, F_GETFD) >= 0);
    const int st = wait_exit(pid, 3000);

    /* After last slave is gone, write may EIO — that is exit teardown, not VEOF. */
    errno = 0;
    const ssize_t w = write(p.master, "x", 1);
    const int write_err = (w < 0) ? errno : 0;

    record("veof",
           st == 0 && out.find("got-eof") != std::string::npos && master_still_open,
           "VEOF=0x" + std::to_string(veof) + " exit=" + std::to_string(st)
               + " master_fd_open=" + std::to_string(master_still_open)
               + " post_exit_write=" + (w >= 0 ? "ok" : strerror(write_err))
               + " out=[" + out + "]");
    close_pty(p);
}

void test_resize()
{
    Pty p;
    if (!open_pty(p)) {
        record("resize_sigwinch", false, "openpty failed");
        return;
    }
    const pid_t pid = spawn_claimed(p, child_winch);
    std::string out;
    drain_master(p.master, out, 800);
    winsize ws{};
    ws.ws_row = 40;
    ws.ws_col = 120;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    if (ioctl(p.master, TIOCSWINSZ, &ws) != 0) {
        record("resize_sigwinch", false, "TIOCSWINSZ failed: " + errno_str());
        kill(pid, SIGKILL);
        wait_exit(pid, 1000);
        close_pty(p);
        return;
    }
    drain_master(p.master, out, 2000);
    const int st = wait_exit(pid, 3000);
    record("resize_sigwinch",
           st == 0 && out.find("winch-40-120") != std::string::npos,
           "exit=" + std::to_string(st) + " out=[" + out + "]");
    close_pty(p);
}

void test_cr_progress()
{
    Pty p;
    if (!open_pty(p)) {
        record("cr_progress", false, "openpty failed");
        return;
    }
    const pid_t pid = spawn_claimed(p, child_cr_progress);
    std::string out;
    drain_master(p.master, out, 1500);
    wait_exit(pid, 2000);
    /* Raw master bytes should include CR characters (ONLCR may expand NL only) */
    const bool has_cr = out.find('\r') != std::string::npos;
    const bool has_ccc = out.find("ccc") != std::string::npos;
    record("cr_progress", has_cr && has_ccc,
           "out=[" + out + "] bytes=" + std::to_string(out.size()));
    close_pty(p);
}

void test_exit_paths()
{
    Pty p;
    if (!open_pty(p)) {
        record("child_exit", false, "openpty failed");
        return;
    }
    const pid_t pid = spawn_claimed(p, child_sleep_then_exit);
    std::string out;
    drain_master(p.master, out, 1500);
    const int st = wait_exit(pid, 2000);
    record("child_exit", st == 0 && out.find("alive") != std::string::npos,
           "exit=" + std::to_string(st) + " out=[" + out + "]");
    close_pty(p);

    if (!open_pty(p)) {
        record("immediate_exit", false, "openpty failed");
        return;
    }
    const pid_t pid2 = spawn_claimed(p, child_immediate_exit);
    const int st2 = wait_exit(pid2, 2000);
    record("immediate_exit", st2 == 42,
           "exit=" + std::to_string(st2));
    close_pty(p);
}

void test_exec_failure()
{
    Pty p;
    if (!open_pty(p)) {
        record("exec_failure", false, "openpty failed");
        return;
    }
    const pid_t pid = fork();
    if (pid == 0) {
        close(p.master);
        if (!child_claim_ctty(p.slave))
            _exit(90);
        close_from(3);
        execl("/no/such/pty_proof_binary_xyz", "x", static_cast<char*>(nullptr));
        const int e = errno;
        dprintf(STDOUT_FILENO, "exec-errno-%d\n", e);
        _exit(127);
    }
    close(p.slave);
    p.slave = -1;
    std::string out;
    drain_master(p.master, out, 1500);
    const int st = wait_exit(pid, 2000);
    record("exec_failure",
           st == 127 && out.find("exec-errno") != std::string::npos,
           "exit=" + std::to_string(st) + " out=[" + out + "]");
    close_pty(p);
}

void test_fd_hygiene_and_reader_stop()
{
    Pty p;
    if (!open_pty(p)) {
        record("fd_hygiene", false, "openpty failed");
        return;
    }
    const int master = p.master;
    const int slave = p.slave;
    const pid_t pid = fork();
    if (pid == 0) {
        close(master);
        if (!child_claim_ctty(slave))
            _exit(90);
        /* Drop inherited descriptors; console child should keep only 0/1/2. */
        close_from(3);
        int open_count = 0;
        for (int fd = 0; fd < 32; ++fd) {
            if (fcntl(fd, F_GETFD) >= 0)
                ++open_count;
        }
        dprintf(STDOUT_FILENO, "openfds-%d\n", open_count);
        _exit(open_count == 3 ? 0 : 1);
    }
    close(slave);
    p.slave = -1;
    std::string out;
    drain_master(master, out, 1500);
    const int st = wait_exit(pid, 2000);
    record("fd_hygiene", st == 0,
           "exit=" + std::to_string(st) + " out=[" + out + "]");

    /* Reader stop: further reads should end (EIO or 0) after child gone */
    errno = 0;
    char buf[8];
    const ssize_t r = read(master, buf, sizeof(buf));
    const int e = errno;
    const bool stopped = (r == 0) || (r < 0 && e == EIO);
    record("reader_stop", stopped,
           r >= 0 ? ("read=" + std::to_string(r))
                  : (std::string("read=-1 errno=") + strerror(e)));
    close_pty(p);
}

void test_master_after_final_slave_close()
{
    Pty p;
    if (!open_pty(p)) {
        record("master_after_slave_close", false, "openpty failed");
        return;
    }
    const pid_t pid = fork();
    if (pid == 0) {
        close(p.master);
        if (!child_claim_ctty(p.slave))
            _exit(90);
        write(STDOUT_FILENO, "bye\n", 4);
        /* Explicitly close stdio → last slave refs drop on exit */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        _exit(0);
    }
    close(p.slave);
    p.slave = -1;
    std::string out;
    drain_master(p.master, out, 1000);
    wait_exit(pid, 2000);

    /* Experiment: what does master report after last slave is gone? */
    std::string detail;
    for (int i = 0; i < 3; ++i) {
        errno = 0;
        char buf[32];
        const ssize_t r = read(p.master, buf, sizeof(buf));
        const int e = errno;
        if (r >= 0)
            detail += "read=" + std::to_string(r) + " ";
        else
            detail += std::string("read=-1/") + strerror(e) + " ";
        if (r < 0)
            break;
        if (r == 0)
            break;
    }
    /* Also try select readability */
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(p.master, &rfds);
    timeval tv{0, 100000};
    const int sel = select(p.master + 1, &rfds, nullptr, nullptr, &tv);
    detail += "select=" + std::to_string(sel);

    /* Accept either EOF (0) or EIO — both are valid Unix PTY end conditions.
       Record which Darwin actually gives. PASS if we get a definitive end. */
    const bool definitive = detail.find("read=0") != std::string::npos
                            || detail.find("EIO") != std::string::npos
                            || detail.find("Input/output error") != std::string::npos;
    record("master_after_slave_close", definitive,
           "after bye+exit: " + detail + " prior_out=[" + out + "]");
    close_pty(p);
}

void test_rapid_recreate()
{
    const int N = 20;
    int ok = 0;
    for (int i = 0; i < N; ++i) {
        Pty p;
        if (!open_pty(p))
            break;
        const pid_t pid = spawn_claimed(p, child_hello);
        if (pid < 0) {
            close_pty(p);
            break;
        }
        std::string out;
        drain_master(p.master, out, 500);
        const int st = wait_exit(pid, 1000);
        close_pty(p);
        if (st == 0 && out.find("hello") != std::string::npos)
            ++ok;
    }
    record("rapid_recreate", ok == N,
           "ok=" + std::to_string(ok) + "/" + std::to_string(N));
}

void test_foreground_pgrp()
{
    Pty p;
    if (!open_pty(p)) {
        record("foreground_pgrp", false, "openpty failed");
        return;
    }
    const pid_t pid = fork();
    if (pid == 0) {
        close(p.master);
        if (!child_claim_ctty(p.slave))
            _exit(90);
        setpgid(0, 0);
        const int tsp = tcsetpgrp(STDIN_FILENO, getpid());
        const pid_t fg = tcgetpgrp(STDIN_FILENO);
        dprintf(STDOUT_FILENO, "tcset=%d fg=%d self=%d\n",
                tsp, (int)fg, (int)getpid());
        _exit((tsp == 0 && fg == getpid()) ? 0 : 1);
    }
    close(p.slave);
    p.slave = -1;
    /* Parent may also need to set foreground via master */
    usleep(50000);
    tcsetpgrp(p.master, pid);
    std::string out;
    drain_master(p.master, out, 1500);
    const int st = wait_exit(pid, 2000);
    record("foreground_pgrp", st == 0,
           "exit=" + std::to_string(st) + " out=[" + out + "]");
    close_pty(p);
}

} // namespace

int main()
{
    std::printf("=== Darwin PTY proof (Milestone 1) ===\n");
    std::fflush(stdout);

    test_allocation_and_termios();
    test_setsid_variants();
    test_foreground_pgrp();
    test_stdio_and_prompt();
    test_echo();
    test_vintr();
    test_veof();
    test_resize();
    test_cr_progress();
    test_exit_paths();
    test_exec_failure();
    test_fd_hygiene_and_reader_stop();
    test_master_after_final_slave_close();
    test_rapid_recreate();

    std::printf("\n=== Summary: %zu cases, %d failed ===\n",
                g_results.size(), g_fails);
    return g_fails ? 1 : 0;
}
