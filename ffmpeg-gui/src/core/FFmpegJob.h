#pragma once
#include <string>
#include <vector>

// ─── VideoConfig ─────────────────────────────────────────────────────────────
struct VideoConfig {
    bool        copy       = false;    // stream copy; ignores codec/quality/scale
    std::string codec      = "libx264";
    bool        useCrf     = true;
    int         crf        = 23;
    int         bitrate    = 2000;     // kbps, when useCrf == false
    std::string resolution = "";       // empty → keep source; "1920x1080" etc.
};

// ─── AudioConfig ─────────────────────────────────────────────────────────────
struct AudioConfig {
    bool        copy        = false;   // stream copy audio
    bool        extractOnly = false;   // discard video stream entirely (-vn)
    std::string codec       = "aac";
    int         bitrate     = 128;     // kbps
};

// ─── TrimConfig ──────────────────────────────────────────────────────────────
struct TrimConfig {
    bool        enabled   = false;
    std::string startTime = "";    // -ss (before -i)
    std::string endTime   = "";    // -to (output-side, absolute); mutually exclusive with duration
    std::string duration  = "";    // -t  (output-side, length);  mutually exclusive with endTime
};

// ─── FFmpegJob ───────────────────────────────────────────────────────────────
// Central domain model. The GUI fills this struct; command/ and process/ read it.
// Validation is handled by JobValidator – do not add validation logic here.
struct FFmpegJob {
    std::string              ffmpegPath      = "ffmpeg";
    std::string              inputFile;
    std::string              outputFile;
    std::string              outputContainer;  // e.g. "mp4", "mkv"
    VideoConfig              video;
    AudioConfig              audio;
    TrimConfig               trim;
    std::vector<std::string> extraArgs;        // reserved, not yet exposed in UI
};
