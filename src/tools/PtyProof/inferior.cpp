/*
 * Tiny inferior for Milestone 2 GDB↔PTY proof.
 * Prints markers, reads one line, waits for SIGINT or timeout.
 */
#include <cstdio>
#include <csignal>
#include <cstring>
#include <unistd.h>

static volatile sig_atomic_t g_got_int = 0;

static void on_int(int)
{
    g_got_int = 1;
}

int main()
{
    std::signal(SIGINT, on_int);

    std::printf("inferior-ready\n");
    std::printf("inferior-pid-%d\n", static_cast<int>(getpid()));
    std::fflush(stdout);

    char buf[128];
    if (std::fgets(buf, sizeof(buf), stdin)) {
        std::printf("got-input:%s", buf);
        const size_t n = std::strlen(buf);
        if (n == 0 || buf[n - 1] != '\n')
            std::printf("\n");
        std::fflush(stdout);
    } else {
        std::printf("got-input-eof\n");
        std::fflush(stdout);
    }

    for (int i = 0; i < 300 && !g_got_int; ++i)
        usleep(10000);

    if (g_got_int)
        std::printf("inferior-got-sigint\n");
    else
        std::printf("inferior-timeout\n");
    std::fflush(stdout);
    return g_got_int ? 0 : 1;
}
