#include "JobValidator.h"
#include "core/ContainerRules.h"
#include <filesystem>
#include <cctype>
#include <algorithm>
#include <cstdio>

// ─── ParseTimeSec ─────────────────────────────────────────────────────────────
// Returns:
//   -1.0  → empty string (field not set) — treated as valid / absent
//   >= 0  → seconds parsed successfully
//   -2.0  → parse error / invalid format
double JobValidator::ParseTimeSec(const std::string& s)
{
    if (s.empty()) return -1.0;

    int    h = 0, m = 0;
    double sec = 0.0;

    if (std::sscanf(s.c_str(), "%d:%d:%lf", &h, &m, &sec) == 3) {
        if (h < 0 || m < 0 || m >= 60 || sec < 0.0 || sec >= 60.0) return -2.0;
        return h * 3600.0 + m * 60.0 + sec;
    }
    if (std::sscanf(s.c_str(), "%d:%lf", &m, &sec) == 2) {
        if (m < 0 || sec < 0.0 || sec >= 60.0) return -2.0;
        return m * 60.0 + sec;
    }
    if (std::sscanf(s.c_str(), "%lf", &sec) == 1) {
        if (sec < 0.0) return -2.0;
        return sec;
    }
    return -2.0;
}

// ─── helpers ─────────────────────────────────────────────────────────────────
static bool FileExists(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

static std::string ToLower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// ─── Validate ────────────────────────────────────────────────────────────────
ValidationResult JobValidator::Validate(const FFmpegJob& job)
{
    ValidationResult r;

    // ── Required fields ──
    if (job.inputFile.empty())
        r.errors.push_back("No input file selected.");
    if (job.outputFile.empty())
        r.errors.push_back("No output file specified.");

    // ── Input == output ──
    if (!job.inputFile.empty() && job.inputFile == job.outputFile)
        r.errors.push_back("Input and output file are the same.");

    // ── Input exists ──
    if (!job.inputFile.empty() && !FileExists(job.inputFile))
        r.errors.push_back("Input file does not exist: " + job.inputFile);

    // ── Container / extension consistency ──
    if (!job.outputFile.empty() && !job.outputContainer.empty()) {
        auto dot = job.outputFile.rfind('.');
        if (dot != std::string::npos && dot + 1 < job.outputFile.size()) {
            std::string ext = ToLower(job.outputFile.substr(dot + 1));
            if (ext != ToLower(job.outputContainer))
                r.errors.push_back(
                    "Output extension (." + ext + ") does not match selected container ("
                    + job.outputContainer + "). Update the filename or change the container.");
        }
    }

    // ── Container rejects video but job encodes video ──
    if (!job.outputContainer.empty() &&
        ContainerRules::IsAudioOnly(job.outputContainer) &&
        !job.audio.extractOnly && !job.video.copy)
    {
        // Soft check: if no video codec is set it's fine; warn otherwise.
        // (audio.extractOnly is the explicit -vn path; video.copy streams it through)
        if (!job.video.codec.empty())
            r.errors.push_back(
                "Container '" + job.outputContainer + "' is audio-only; "
                "remove the video codec or use extract-audio mode.");
    }

    // ── Trim time validation ──
    if (job.trim.enabled) {
        double startSec = ParseTimeSec(job.trim.startTime);
        double endSec   = ParseTimeSec(job.trim.endTime);
        double durSec   = ParseTimeSec(job.trim.duration);

        if (startSec == -2.0)
            r.errors.push_back("Invalid start time: \"" + job.trim.startTime + "\"."
                                " Use HH:MM:SS or seconds.");
        if (endSec == -2.0)
            r.errors.push_back("Invalid end time: \"" + job.trim.endTime + "\"."
                                " Use HH:MM:SS or seconds.");
        if (durSec == -2.0)
            r.errors.push_back("Invalid duration: \"" + job.trim.duration + "\"."
                                " Use HH:MM:SS or seconds.");

        if (!job.trim.endTime.empty() && !job.trim.duration.empty())
            r.errors.push_back("Cannot set both end time and duration. Use one or the other.");

        if (startSec >= 0.0 && endSec >= 0.0 && endSec <= startSec)
            r.errors.push_back("End time must be after start time.");

        if (!job.trim.duration.empty() && durSec >= 0.0 && durSec < 0.001)
            r.errors.push_back("Duration must be greater than zero.");
    }

    // ── Video: copy + scale is invalid ──
    if (job.video.copy && !job.video.resolution.empty())
        r.errors.push_back("Cannot scale video when using stream copy (-c:v copy).");

    // ── Video quality range ──
    if (!job.video.copy && !job.audio.extractOnly) {
        if ((job.video.codec == "libx264" || job.video.codec == "libx265") &&
             job.video.useCrf && (job.video.crf < 0 || job.video.crf > 51))
            r.errors.push_back("CRF must be 0–51 for " + job.video.codec + ".");

        if (!job.video.useCrf && job.video.bitrate <= 0)
            r.errors.push_back("Video bitrate must be greater than 0.");
    }

    // ── Audio bitrate ──
    if (!job.audio.copy && !job.audio.extractOnly && job.audio.bitrate <= 0)
        r.errors.push_back("Audio bitrate must be greater than 0.");

    // ── MP3 container requires libmp3lame (hard error, not just a warning) ──
    // extractOnly just means -vn (no video); the audio codec still matters.
    if (ToLower(job.outputContainer) == "mp3"
        && !job.audio.copy
        && job.audio.codec != "libmp3lame")
    {
        r.errors.push_back("MP3 container requires the libmp3lame audio codec.");
    }

    // ── Warnings ─────────────────────────────────────────────────────────────

    // Audio-only mode but a video container is selected
    if (job.audio.extractOnly && !job.outputContainer.empty()
        && !ContainerRules::IsAudioOnly(job.outputContainer))
    {
        r.warnings.push_back(
            "Exporting audio-only to a video container (" + job.outputContainer
            + "). Consider using mp3, wav, ogg or flac.");
    }

    // Stream copy with trim → imprecise cuts
    if ((job.video.copy || job.audio.copy) && job.trim.enabled)
        r.warnings.push_back(
            "Stream copy with trim may produce imprecise cuts "
            "due to keyframe alignment.");

    // Codec / container compatibility (via ContainerRules)
    if (!job.outputContainer.empty()) {
        std::string vw = ContainerRules::VideoCodecWarning(
                             job.outputContainer, job.video.codec);
        if (!vw.empty()) r.warnings.push_back(vw);

        std::string aw = ContainerRules::AudioCodecWarning(
                             job.outputContainer, job.audio.codec);
        if (!aw.empty()) r.warnings.push_back(aw);
    }

    return r;
}
