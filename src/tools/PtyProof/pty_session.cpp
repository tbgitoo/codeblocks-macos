#include "pty_session.h"

#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <util.h>

#include <cerrno>
#include <cstring>

PtySession::~PtySession()
{
    Close();
}

bool PtySession::Create()
{
    Close();

    int master = -1;
    int slave = -1;
    char name[128]{};
    if (openpty(&master, &slave, name, nullptr, nullptr) != 0)
        return false;

    /*
     * Keep our slave FD open until Close().
     *
     * M2 finding: if the last slave FD is closed while a master reader is
     * already draining, Darwin delivers read()==0 (EOF) on the master and a
     * naive reader stops. GDB later opens the slave by path for the inferior,
     * but the harness reader is already dead. Holding our slave FD (no
     * Terminal.app / cb_debug_console_hold) avoids that race. GDB can still
     * open the same slave path concurrently.
     */
    m_slaveFd = slave;

    int flags = fcntl(master, F_GETFL, 0);
    if (flags >= 0)
        fcntl(master, F_SETFL, flags | O_NONBLOCK);

    m_master = master;
    m_slaveName = name;
    m_stop = false;
    m_sawEof = false;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_output.clear();
    }
    m_reader = std::thread([this] { ReaderLoop(); });
    return true;
}

void PtySession::Close()
{
    m_stop = true;
    if (m_slaveFd >= 0) {
        close(m_slaveFd);
        m_slaveFd = -1;
    }
    if (m_master >= 0) {
        close(m_master);
        m_master = -1;
    }
    if (m_reader.joinable())
        m_reader.join();
    m_slaveName.clear();
}

bool PtySession::SendInput(const void* data, size_t len)
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

bool PtySession::Resize(unsigned rows, unsigned cols)
{
    if (m_master < 0)
        return false;
    winsize ws{};
    ws.ws_row = static_cast<unsigned short>(rows ? rows : 1);
    ws.ws_col = static_cast<unsigned short>(cols ? cols : 1);
    return ioctl(m_master, TIOCSWINSZ, &ws) == 0;
}

std::string PtySession::TakeOutput()
{
    std::lock_guard<std::mutex> lock(m_mu);
    std::string out;
    out.swap(m_output);
    return out;
}

std::string PtySession::PeekOutput() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_output;
}

void PtySession::ReaderLoop()
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
            std::lock_guard<std::mutex> lock(m_mu);
            m_output.append(buf, static_cast<size_t>(r));
            continue;
        }
        if (r == 0) {
            /* Soft EOF: slave may reopen (GDB attach). Keep polling until Close. */
            m_sawEof = true;
            usleep(50000);
            continue;
        }
        if (errno == EINTR || errno == EAGAIN)
            continue;
        if (errno == EIO) {
            m_sawEof = true;
            usleep(50000);
            continue;
        }
        break;
    }
}
