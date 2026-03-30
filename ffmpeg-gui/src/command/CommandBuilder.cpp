#include "CommandBuilder.h"
#include "core/ContainerRules.h"
#include <sstream>
#include <cctype>

// ─── ShellQuote ───────────────────────────────────────────────────────────────
// Produces a shell-safe representation of an argument for the VISIBLE command
// string. The actual execution always uses structured argv (no shell involved).
//
// Linux/Mac: single-quote wrapping; internal ' escaped as '\''
// Windows:   double-quote wrapping; internal " escaped as \"
static bool NeedsQuoting(const std::string& arg)
{
    for (unsigned char c : arg)
        if (std::isspace(c) || c == '"' || c == '\'' || c == '\\' ||
            c == '(' || c == ')' || c == '&'  || c == '|'  ||
            c == ';' || c == '<' || c == '>'  || c == '`'  ||
            c == '$' || c == '!' || c == '#'  || c == '*'  || c == '?')
            return true;
    return false;
}

static std::string ShellQuote(const std::string& arg)
{
    if (!NeedsQuoting(arg)) return arg;

#ifdef _WIN32
    std::string out = "\"";
    for (char c : arg) {
        if (c == '"') out += "\\\"";
        else          out += c;
    }
    out += '"';
    return out;
#else
    // POSIX sh: single-quote everything; escape embedded single quotes.
    std::string out = "'";
    for (char c : arg) {
        if (c == '\'') out += "'\\''";
        else           out += c;
    }
    out += '\'';
    return out;
#endif
}

// ─── BuildArgs (shared) ───────────────────────────────────────────────────────
static std::vector<std::string> BuildArgs(const FFmpegJob& job)
{
    std::vector<std::string> args;

    args.push_back("-y");

    if (job.trim.enabled && !job.trim.startTime.empty()) {
        args.push_back("-ss");
        args.push_back(job.trim.startTime);
    }

    args.push_back("-i");
    args.push_back(job.inputFile);

    if (job.trim.enabled) {
        if (!job.trim.endTime.empty()) {
            args.push_back("-to");
            args.push_back(job.trim.endTime);
        } else if (!job.trim.duration.empty()) {
            args.push_back("-t");
            args.push_back(job.trim.duration);
        }
    }

    // ── Video ──
    if (job.audio.extractOnly) {
        args.push_back("-vn");
    } else if (job.video.copy) {
        args.push_back("-c:v");
        args.push_back("copy");
    } else if (!job.video.codec.empty()) {
        args.push_back("-c:v");
        args.push_back(job.video.codec);

        if (job.video.useCrf) {
            args.push_back("-crf");
            args.push_back(std::to_string(job.video.crf));
        } else {
            args.push_back("-b:v");
            args.push_back(std::to_string(job.video.bitrate) + "k");
        }

        if (!job.video.resolution.empty()) {
            args.push_back("-vf");
            args.push_back("scale=" + job.video.resolution);
        }
    }

    // ── Audio ──
    if (job.audio.copy) {
        args.push_back("-c:a");
        args.push_back("copy");
    } else if (!job.audio.codec.empty()) {
        args.push_back("-c:a");
        args.push_back(job.audio.codec);
        args.push_back("-b:a");
        args.push_back(std::to_string(job.audio.bitrate) + "k");
    }

    // ── Extra args (already tokenised by ExtraArgsParser in the GUI layer) ──
    for (const auto& a : job.extraArgs)
        args.push_back(a);

    // ── Format flag ──
    // Emit -f <container> for containers whose extension is not reliably
    // auto-detected by all FFmpeg builds (policy lives in ContainerRules).
    if (!job.outputContainer.empty() &&
        ContainerRules::ShouldEmitFormatFlag(job.outputContainer))
    {
        args.push_back("-f");
        args.push_back(job.outputContainer);
    }

    args.push_back(job.outputFile);
    return args;
}

// ─── Public API ──────────────────────────────────────────────────────────────
std::vector<std::string> CommandBuilder::BuildArguments(const FFmpegJob& job)
{
    return BuildArgs(job);
}

std::string CommandBuilder::BuildCommandString(const FFmpegJob& job)
{
    auto args = BuildArgs(job);
    std::ostringstream oss;
    oss << ShellQuote(job.ffmpegPath.empty() ? "ffmpeg" : job.ffmpegPath);
    for (const auto& arg : args)
        oss << ' ' << ShellQuote(arg);
    return oss.str();
}
