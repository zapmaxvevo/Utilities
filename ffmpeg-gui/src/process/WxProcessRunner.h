#pragma once
#include "IProcessRunner.h"

// ─── WxProcessRunner ─────────────────────────────────────────────────────────
// Concrete IProcessRunner built on wxProcess / wxExecute.
// All wxWidgets dependencies are confined to WxProcessRunner.cpp; this
// header is wx-free so that IProcessRunner users need not see wx types.
class WxProcessRunner : public IProcessRunner {
public:
    WxProcessRunner();
    ~WxProcessRunner() override;

    bool Launch(const std::string& executable,
                const std::vector<std::string>& args) override;
    void Kill(bool force = false) override;
    bool IsRunning()   const override;
    int  GetExitCode() const override;
    std::string PollStdout() override;
    std::string PollStderr() override;

    // Called by the internal wxProcess subclass when the child exits.
    void NotifyTerminated(int exitCode);

private:
    class WxImpl;   // defined in .cpp — keeps wx types out of this header
    WxImpl* m_impl     = nullptr;
    long    m_pid      = 0;
    bool    m_running  = false;
    int     m_exitCode = -1;
};
