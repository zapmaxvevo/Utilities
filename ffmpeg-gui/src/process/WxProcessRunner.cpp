#include "WxProcessRunner.h"

#include <vector>
#include <wx/process.h>
#include <wx/utils.h>

// ─── WxImpl ──────────────────────────────────────────────────────────────────
// Private wxProcess subclass.  Defined here so wx types never leak into the
// header and IProcessRunner users stay wx-independent.
class WxProcessRunner::WxImpl : public wxProcess
{
public:
    explicit WxImpl(WxProcessRunner* owner)
        : wxProcess(wxPROCESS_REDIRECT), m_owner(owner) {}

    // Called before owner destruction to prevent dangling-pointer callback.
    void Detach() { m_owner = nullptr; }

    void OnTerminate(int /*pid*/, int status) override
    {
        if (m_owner) m_owner->NotifyTerminated(status);
    }

private:
    WxProcessRunner* m_owner;
};

// ─── helpers ─────────────────────────────────────────────────────────────────
static std::string DrainStream(wxInputStream* stream)
{
    if (!stream) return {};
    std::string buf;
    char tmp[4096];
    while (stream->CanRead()) {
        stream->Read(tmp, sizeof(tmp));
        std::size_t n = stream->LastRead();
        if (n == 0) break;
        buf.append(tmp, n);
    }
    return buf;
}

// ─── WxProcessRunner ─────────────────────────────────────────────────────────
WxProcessRunner::WxProcessRunner() = default;

WxProcessRunner::~WxProcessRunner()
{
    if (m_impl) {
        m_impl->Detach();
        if (m_running && m_pid > 0)
            wxProcess::Kill(static_cast<int>(m_pid), wxSIGKILL);
        delete m_impl;
        m_impl = nullptr;
    }
}

bool WxProcessRunner::Launch(const std::string& executable,
                              const std::vector<std::string>& args)
{
    m_impl = new WxImpl(this);

    std::vector<const char*> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(executable.c_str());
    for (const auto& a : args)
        argv.push_back(a.c_str());
    argv.push_back(nullptr);

    m_pid = wxExecute(argv.data(),
                      wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER, m_impl);
    if (m_pid <= 0) {
        delete m_impl;
        m_impl = nullptr;
        return false;
    }

    m_running = true;
    return true;
}

void WxProcessRunner::Kill(bool force)
{
    if (!m_running || m_pid <= 0) return;
    wxProcess::Kill(static_cast<int>(m_pid),
                    force ? wxSIGKILL : wxSIGTERM);
}

bool WxProcessRunner::IsRunning() const  { return m_running; }
int  WxProcessRunner::GetExitCode() const { return m_exitCode; }

std::string WxProcessRunner::PollStdout()
{
    if (!m_impl) return {};
    return DrainStream(m_impl->GetInputStream());
}

std::string WxProcessRunner::PollStderr()
{
    if (!m_impl) return {};
    return DrainStream(m_impl->GetErrorStream());
}

void WxProcessRunner::NotifyTerminated(int exitCode)
{
    m_exitCode = exitCode;
    m_running  = false;
    if (onTerminate) onTerminate(exitCode);
}
