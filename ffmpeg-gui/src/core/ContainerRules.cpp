#include "ContainerRules.h"
#include <algorithm>
#include <cctype>

static std::string ToLower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// ─── Audio-only containers ────────────────────────────────────────────────────
// These containers cannot carry video streams.
static const char* kAudioOnly[] = {
    "mp3", "wav", "flac", "ogg", "opus", "m4a", "aiff", "aac"
};

bool ContainerRules::IsAudioOnly(const std::string& container)
{
    const std::string lc = ToLower(container);
    for (const auto* c : kAudioOnly)
        if (lc == c) return true;
    return false;
}

bool ContainerRules::AcceptsVideo(const std::string& container)
{
    return !IsAudioOnly(container);
}

// ─── Format flag ─────────────────────────────────────────────────────────────
// Only emit -f for containers whose extension is ambiguous or not reliably
// auto-detected. We keep this list deliberately small to avoid noise.
bool ContainerRules::ShouldEmitFormatFlag(const std::string& container)
{
    const std::string lc = ToLower(container);
    // .opus extension is not recognized by all FFmpeg builds; -f opus clarifies intent.
    return lc == "opus";
}

// ─── Codec compatibility warnings ────────────────────────────────────────────
std::string ContainerRules::VideoCodecWarning(const std::string& container,
                                               const std::string& codec)
{
    if (codec.empty() || codec == "copy") return {};

    const std::string lc  = ToLower(container);
    const std::string lcc = ToLower(codec);

    if (lc == "webm" && lcc != "libvpx" && lcc != "libvpx-vp9")
        return "WebM works best with VP8 (libvpx) or VP9 (libvpx-vp9).";

    if ((lc == "mp4" || lc == "mov") &&
        (lcc == "libvpx" || lcc == "libvpx-vp9"))
        return "VP8/VP9 is not standard in " + container + " containers.";

    if (lc == "avi" && lcc == "libx265")
        return "H.265 in AVI is non-standard and may not play reliably.";

    return {};
}

std::string ContainerRules::AudioCodecWarning(const std::string& container,
                                               const std::string& codec)
{
    if (codec.empty() || codec == "copy") return {};

    const std::string lc  = ToLower(container);
    const std::string lcc = ToLower(codec);

    if (lc == "mp3" && lcc != "libmp3lame")
        return "MP3 container requires the libmp3lame codec.";

    if (lc == "flac" && lcc != "flac")
        return "FLAC container requires the flac codec.";

    if (lc == "m4a" && lcc != "aac")
        return "M4A is designed for AAC audio; other codecs may not be compatible.";

    if (lc == "webm" && lcc != "libopus" && lcc != "libvorbis")
        return "WebM works best with Opus (libopus) or Vorbis (libvorbis).";

    if ((lc == "mp4" || lc == "mov") && lcc == "libmp3lame")
        return "MP3 audio in MP4/MOV is non-standard; prefer AAC.";

    return {};
}
