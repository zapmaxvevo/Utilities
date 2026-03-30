#include "FFmpegProcess.h"
#include "process/WxProcessRunner.h"

#include <wx/utils.h>   // wxExecute (sync form for CheckAvailable)

// ─── FFmpegProcess ───────────────────────────────────────────────────────────
bool FFmpegProcess::Start(const std::vector<std::string>& args,
                           const std::string& binary)
{
    if (m_runner && m_runner->IsRunning()) return false;

    m_cancelRequested = false;
    m_state           = ProcessState::Idle;

    auto runner = std::make_unique<WxProcessRunner>();
    runner->onTerminate = [this](int code){ OnChildTerminated(code); };

    if (!runner->Launch(binary, args))
        return false;

    m_runner = std::move(runner);
    m_state  = ProcessState::Running;
    return true;
}

std::string FFmpegProcess::PollOutput()
{
    if (!m_runner) return {};
    return m_runner->PollStdout() + m_runner->PollStderr();
}

bool FFmpegProcess::IsRunning() const
{
    return m_runner && m_runner->IsRunning();
}

void FFmpegProcess::Kill()
{
    if (!IsRunning()) return;
    m_cancelRequested = true;
    m_killTime        = Clock::now();
    m_runner->Kill(false);  // SIGTERM — give FFmpeg a chance to finalize output
}

bool FFmpegProcess::NeedsForceKill() const
{
    if (!m_cancelRequested || !IsRunning()) return false;
    using namespace std::chrono;
    return duration_cast<seconds>(Clock::now() - m_killTime).count() >= 3;
}

void FFmpegProcess::ForceKill()
{
    if (!IsRunning()) return;
    m_runner->Kill(true);  // SIGKILL
}

void FFmpegProcess::OnChildTerminated(int exitCode)
{
    m_exitCode = exitCode;
    if (m_cancelRequested)
        m_state = ProcessState::Canceled;
    else
        m_state = (exitCode == 0) ? ProcessState::Succeeded : ProcessState::Failed;
}

// ─── CheckAvailable ──────────────────────────────────────────────────────────
// wx 3.x only exposes the sync+capture overload for wxString commands.
// Quote the binary path to handle spaces in custom paths.
bool FFmpegProcess::CheckAvailable(const std::string& binary)
{
    wxString bin = wxString::FromUTF8(binary);
    if (bin.find(' ') != wxString::npos)
        bin = "\"" + bin + "\"";

    wxArrayString out, err;
    return wxExecute(bin + " -version", out, err, wxEXEC_SYNC) == 0;
}
