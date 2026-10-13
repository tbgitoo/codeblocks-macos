/*
 * cb_debug_console_hold — keep a Terminal tty alive for the GDB inferior.
 *
 * Usage: cb_debug_console_hold <unique-token>
 *
 * The unique token appears in `ps` so Code::Blocks can discover this process
 * and its tty (same role as the old "sleep NNNNN" holder).
 *
 * Signals (set only as flags; I/O happens in main):
 *   SIGUSR1 — normal inferior completion → print prompt, wait for a key
 *   SIGTERM / SIGINT / SIGHUP — forced cleanup → exit immediately
 *
 * Code::Blocks must never wait synchronously for the keypress path.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/select.h>

static volatile sig_atomic_t g_finish = 0; /* soft: wait for key */
static volatile sig_atomic_t g_force  = 0; /* hard: exit now */

static void on_finish(int signo)
{
    (void)signo;
    g_finish = 1;
}

static void on_force(int signo)
{
    (void)signo;
    g_force = 1;
}

static void install_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_finish;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = on_force;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

/* Wait for one key, but abort immediately if g_force becomes set. */
static void wait_any_key_or_force(void)
{
    struct termios oldt, newt;
    int have_term = (tcgetattr(STDIN_FILENO, &oldt) == 0);

    if (have_term)
    {
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    }

    while (!g_force)
    {
        fd_set rfds;
        struct timeval tv;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        int r = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
        if (r > 0 && FD_ISSET(STDIN_FILENO, &rfds))
        {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) >= 0)
                break;
            if (errno != EINTR)
                break;
        }
        else if (r < 0 && errno != EINTR)
            break;
    }

    if (have_term)
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: cb_debug_console_hold <unique-token>\n");
        return 1;
    }

    /* Keep argv[1] visible in `ps` for Code::Blocks tty discovery. */
    (void)argv[1];

    install_handlers();

    while (!g_finish && !g_force)
        pause();

    if (g_force)
        return 0;

    printf("\nPress any key to close...\n");
    fflush(stdout);
    wait_any_key_or_force();
    return 0;
}
