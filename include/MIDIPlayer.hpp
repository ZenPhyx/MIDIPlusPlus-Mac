#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <thread>

class MIDIPlayer {
public:
    using KeyCallback    = std::function<void(char, bool press)>;
    using StatusCallback = std::function<void(const std::string&)>;
    using DoneCallback   = std::function<void()>;

    ~MIDIPlayer() { stop(); }

    void playFile(const std::string& path,
                  KeyCallback    onKey,
                  StatusCallback onStatus,
                  DoneCallback   onDone);

    void stop();
    bool isRunning() const { return m_running.load(); }

private:
    std::atomic<bool> m_running{false};
    std::thread       m_thread;
};
