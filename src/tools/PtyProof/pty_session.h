/*
 * Minimal PtySession transport for Milestone 2 harness.
 * Not production UI; no generation coordinator; Darwin-only.
 */
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class PtySession
{
public:
    PtySession() = default;
    ~PtySession();

    PtySession(const PtySession&) = delete;
    PtySession& operator=(const PtySession&) = delete;

    bool Create();
    void Close();

    const std::string& SlaveName() const { return m_slaveName; }
    int MasterFd() const { return m_master; }

    bool SendInput(const void* data, size_t len);
    bool SendByte(unsigned char c) { return SendInput(&c, 1); }
    bool Resize(unsigned rows, unsigned cols);

    /* Drain accumulated master output (also continues under reader thread). */
    std::string TakeOutput();
    std::string PeekOutput() const;

    bool IsOpen() const { return m_master >= 0; }
    bool ReaderSawEof() const { return m_sawEof.load(); }

private:
    void ReaderLoop();

    int m_master = -1;
    int m_slaveFd = -1; /* held open until Close(); see Create() comment */
    std::string m_slaveName;

    std::thread m_reader;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_sawEof{false};
    mutable std::mutex m_mu;
    std::string m_output;
};
