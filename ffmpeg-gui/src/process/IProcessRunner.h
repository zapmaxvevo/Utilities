#pragma once
#include <functional>
#include <string>
#include <vector>

// ─── IProcessRunner ──────────────────────────────────────────────────────────
// Low-level interface for spawning and monitoring a child process.
// Separates the "how to run a process" concern from FFmpegProcess's
// "what to do with the FFmpeg lifecycle" concern.
//
// The sole concrete implementation today is WxProcessRunner (wxWidgets).
// A future WinProcessRunner (Win32 API) can be plugged in without touching
// FFmpegProcess or the GUI.
class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;

    // Spawn the process.  Returns false if launch fails.
    virtual bool Launch(const std::string& executable,
                        const std::vector<std::string>& args) = 0;

    // Request termination.  force=false → SIGTERM, force=true → SIGKILL.
    virtual void Kill(bool force = false) = 0;

    virtual bool IsRunning()   const = 0;
    virtual int  GetExitCode() const = 0;

    // Drain available bytes from each stream (non-blocking).
    virtual std::string PollStdout() = 0;
    virtual std::string PollStderr() = 0;

    // Optional callback fired (from the event loop) when the process ends.
    // Set before calling Launch().
    std::function<void(int /*exitCode*/)> onTerminate;
};
