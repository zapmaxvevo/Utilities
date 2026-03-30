#pragma once
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "process/IProcessRunner.h"

// ─── ProcessState ────────────────────────────────────────────────────────────
enum class ProcessState { Idle, Running, Succeeded, Failed, Canceled };

// ─── FFmpegProcess ───────────────────────────────────────────────────────────
// Manages the FFmpeg process lifecycle: state machine, graceful cancellation
// (SIGTERM → SIGKILL after 3 s), and output polling.
//
// Uses IProcessRunner for the actual spawn/kill/poll operations so that the
// underlying mechanism (WxProcessRunner today, a Win32 runner later) can be
// swapped without touching this class or the GUI.
class FFmpegProcess {
public:
    FFmpegProcess()  = default;
    ~FFmpegProcess() = default;

    // Launch ffmpeg. binary defaults to "ffmpeg" (resolved via PATH).
    bool Start(const std::vector<std::string>& args,
               const std::string& binary = "ffmpeg");

    // Drain available output (stdout + stderr combined). Non-blocking.
    std::string PollOutput();

    bool         IsRunning()   const;
    ProcessState GetState()    const { return m_state;    }
    int          GetExitCode() const { return m_exitCode; }

    // Send SIGTERM. The UI timer should call NeedsForceKill() every tick
    // and escalate to ForceKill() after the 3-second grace period.
    void Kill();
    bool NeedsForceKill() const;
    void ForceKill();

    // Synchronous availability check: runs "<binary> -version".
    static bool CheckAvailable(const std::string& binary = "ffmpeg");

    // Called by the runner via its onTerminate callback when the child exits.
    void OnChildTerminated(int exitCode);

private:
    std::unique_ptr<IProcessRunner> m_runner;
    bool         m_cancelRequested = false;
    int          m_exitCode        = -1;
    ProcessState m_state           = ProcessState::Idle;
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_killTime;
};
