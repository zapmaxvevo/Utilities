#pragma once
#include <string>

// ─── ContainerRules ──────────────────────────────────────────────────────────
// Centralizes container-level rules: which containers are audio-only, which
// codecs are compatible, and when -f <container> should be emitted.
//
// This replaces ad-hoc container checks scattered across JobValidator and
// CommandBuilder, making the policy one easy-to-extend place.
class ContainerRules {
public:
    // Returns true if this container cannot carry video streams (mp3, wav, …).
    static bool IsAudioOnly(const std::string& container);

    // Returns true if the container can carry video.
    static bool AcceptsVideo(const std::string& container);

    // Whether to emit an explicit "-f <container>" flag in the command.
    // True for containers whose extension is not reliably auto-detected
    // by all FFmpeg builds (e.g. .opus).
    static bool ShouldEmitFormatFlag(const std::string& container);

    // Returns a non-empty warning string if the video codec is a poor fit
    // for this container; empty string if the combination is acceptable.
    static std::string VideoCodecWarning(const std::string& container,
                                          const std::string& codec);

    // Returns a non-empty warning string if the audio codec is a poor fit
    // for this container; empty string if the combination is acceptable.
    static std::string AudioCodecWarning(const std::string& container,
                                          const std::string& codec);
};
