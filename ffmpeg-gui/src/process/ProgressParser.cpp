#include "ProgressParser.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

// ─── helpers ─────────────────────────────────────────────────────────────────

// "HH:MM:SS.mm" or "MM:SS.mm" or "SS.mm" → seconds (returns -1 on failure)
double ProgressParser::ParseTimeSec(const std::string& s)
{
    if (s.empty() || s == "N/A") return -1.0;
    int    h = 0, m = 0;
    double sec = 0.0;
    if (std::sscanf(s.c_str(), "%d:%d:%lf", &h, &m, &sec) == 3)
        return h * 3600.0 + m * 60.0 + sec;
    if (std::sscanf(s.c_str(), "%d:%lf",    &m, &sec)      == 2)
        return m * 60.0 + sec;
    if (std::sscanf(s.c_str(), "%lf",       &sec)          == 1)
        return sec;
    return -1.0;
}

// Extract the value of "key=<value>" from a progress line.
// Handles padding spaces between '=' and value (e.g. "frame=  589").
std::string ProgressParser::ExtractField(const std::string& line, const std::string& key)
{
    std::string needle = key + "=";
    auto pos = line.find(needle);
    if (pos == std::string::npos) return {};
    pos += needle.size();
    while (pos < line.size() && line[pos] == ' ') ++pos;
    std::string val;
    while (pos < line.size() && line[pos] != ' ' && line[pos] != '\r' && line[pos] != '\n')
        val += line[pos++];
    return val;
}

// ─── Reset ───────────────────────────────────────────────────────────────────
void ProgressParser::Reset()
{
    m_pending.clear();
    m_totalSec    = 0.0;
    m_last        = ProgressInfo{};
    m_hasProgress = false;
}

// ─── Feed ────────────────────────────────────────────────────────────────────
ParseResult ProgressParser::Feed(const std::string& raw)
{
    ParseResult result;

    // Prepend any leftover incomplete line from last call
    std::string buf = m_pending + raw;
    m_pending.clear();

    std::string line;
    for (std::size_t i = 0; i < buf.size(); ++i) {
        const char c = buf[i];

        if (c == '\r') {
            if (i + 1 < buf.size() && buf[i + 1] == '\n') {
                // \r\n  → treat as a normal log line
                OnLogLine(line, result);
                result.logText += line + '\n';
                line.clear();
                ++i;   // consume the \n
            } else {
                // bare \r → FFmpeg progress line
                if (!line.empty())
                    OnProgressLine(line, result);
                line.clear();
            }
        } else if (c == '\n') {
            OnLogLine(line, result);
            result.logText += line + '\n';
            line.clear();
        } else {
            line += c;
        }
    }

    m_pending = line;   // keep incomplete trailing data for next call
    return result;
}

// ─── OnLogLine ───────────────────────────────────────────────────────────────
// Called for every \n-terminated line. Extracts total duration if not yet known.
void ProgressParser::OnLogLine(const std::string& line, ParseResult& /*out*/)
{
    if (m_totalSec > 0.0) return;

    auto pos = line.find("Duration:");
    if (pos == std::string::npos) return;

    pos += std::strlen("Duration:");
    while (pos < line.size() && line[pos] == ' ') ++pos;

    std::string timeStr;
    while (pos < line.size() && line[pos] != ',' && line[pos] != ' ')
        timeStr += line[pos++];

    double sec = ParseTimeSec(timeStr);
    if (sec > 0.0) m_totalSec = sec;
}

// ─── OnProgressLine ──────────────────────────────────────────────────────────
// Called for every \r-terminated (bare) line – i.e. FFmpeg progress output.
void ProgressParser::OnProgressLine(const std::string& line, ParseResult& out)
{
    // Sanity check: must look like a progress line
    if (line.find("frame=") == std::string::npos &&
        line.find("time=")  == std::string::npos)
        return;

    ProgressInfo info;

    auto fstr = ExtractField(line, "frame");
    if (!fstr.empty()) {
        try { info.frame = std::stoi(fstr); } catch (...) {}
    }

    auto fpstr = ExtractField(line, "fps");
    if (!fpstr.empty()) {
        try { info.fps = std::stof(fpstr); } catch (...) {}
    }

    auto tstr = ExtractField(line, "time");
    if (!tstr.empty() && tstr != "N/A") {
        info.timeStr = tstr;
        info.timeSec = ParseTimeSec(tstr);
    }

    auto spstr = ExtractField(line, "speed");
    if (!spstr.empty() && spstr != "N/A") {
        if (!spstr.empty() && spstr.back() == 'x') spstr.pop_back();
        try { info.speed = std::stof(spstr); } catch (...) {}
    }

    if (m_totalSec > 0.0 && info.timeSec >= 0.0)
        info.percent = std::min(100.0, info.timeSec / m_totalSec * 100.0);

    m_last        = info;
    m_hasProgress = true;
    out.progressUpdated = true;
    out.progress        = info;
}
