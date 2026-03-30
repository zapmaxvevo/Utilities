#pragma once
#include <string>

// ─── ProgressInfo ────────────────────────────────────────────────────────────
struct ProgressInfo {
    int    frame   = 0;
    float  fps     = 0.f;
    double timeSec = -1.0;    // current encoded position in seconds
    std::string timeStr;      // raw "HH:MM:SS.mm" from ffmpeg
    float  speed   = 0.f;     // encoding speed relative to realtime
    double percent = -1.0;    // 0-100, or -1 if total duration unknown
};

// ─── ParseResult ─────────────────────────────────────────────────────────────
struct ParseResult {
    std::string  logText;           // lines to append to the log area
    ProgressInfo progress;          // latest parsed progress values
    bool         progressUpdated = false;
};

// ─── ProgressParser ──────────────────────────────────────────────────────────
// Consumes raw bytes from ffmpeg stderr.
//
// FFmpeg convention:
//   - Progress lines end with bare \r (meant to overwrite in a terminal).
//   - Informational/log lines end with \n (or \r\n on Windows).
//
// Feed() separates the two, parses progress, and routes log text back to
// the caller so it can be displayed without the noisy progress spam.
class ProgressParser
{
public:
    // Process a raw chunk. May be called with partial lines; internal state
    // accumulates until a terminator (\r or \n) is seen.
    ParseResult Feed(const std::string& raw);

    bool         HasDuration() const { return m_totalSec > 0; }
    double       GetTotalSec() const { return m_totalSec; }
    ProgressInfo GetProgress() const { return m_last; }
    bool         HasProgress() const { return m_hasProgress; }

    void Reset();

private:
    void OnLogLine     (const std::string& line, ParseResult& out);
    void OnProgressLine(const std::string& line, ParseResult& out);

    static double      ParseTimeSec  (const std::string& s);
    static std::string ExtractField  (const std::string& line, const std::string& key);

    std::string  m_pending;           // incomplete line buffer
    double       m_totalSec   = 0.0;
    ProgressInfo m_last;
    bool         m_hasProgress= false;
};
