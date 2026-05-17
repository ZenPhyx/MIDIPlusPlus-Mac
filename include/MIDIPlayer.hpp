#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

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
    void pause();
    void resume();
    void seek(double seconds);
    void setSpeed(double speed);      // 0.25 – 2.0
    void setSustainKey(char key);     // key sent when CC64 fires (default: ' ')

    bool   isRunning() const { return m_running.load(); }
    bool   isPaused()  const { return m_paused.load();  }
    double getPosition() const { return m_position.load(); }
    double getDuration() const { return m_duration.load(); }

    // Parse a file and return its duration in seconds without playing it.
    static double fileDuration(const std::string& path);

private:
    std::atomic<bool>   m_running{false};
    std::atomic<bool>   m_paused{false};
    std::atomic<double> m_speed{1.0};
    std::atomic<double> m_position{0.0};
    std::atomic<double> m_duration{0.0};
    std::atomic<double> m_seekTarget{-1.0};
    std::atomic<char>   m_sustainKey{' '};
    std::thread         m_thread;
    std::mutex          m_pauseMu;
    std::condition_variable m_pauseCv;
};
