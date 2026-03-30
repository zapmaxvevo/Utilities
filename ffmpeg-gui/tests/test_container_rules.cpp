#include <doctest/doctest.h>
#include "core/ContainerRules.h"

TEST_SUITE("ContainerRules") {

// ── IsAudioOnly ───────────────────────────────────────────────────────────────
TEST_CASE("mp3 is audio-only") {
    CHECK(ContainerRules::IsAudioOnly("mp3"));
    CHECK(ContainerRules::IsAudioOnly("MP3"));
}
TEST_CASE("wav is audio-only") {
    CHECK(ContainerRules::IsAudioOnly("wav"));
}
TEST_CASE("flac is audio-only") {
    CHECK(ContainerRules::IsAudioOnly("flac"));
}
TEST_CASE("ogg is audio-only") {
    CHECK(ContainerRules::IsAudioOnly("ogg"));
}
TEST_CASE("opus is audio-only") {
    CHECK(ContainerRules::IsAudioOnly("opus"));
}
TEST_CASE("m4a is audio-only") {
    CHECK(ContainerRules::IsAudioOnly("m4a"));
}

// ── AcceptsVideo ──────────────────────────────────────────────────────────────
TEST_CASE("mp4 accepts video") {
    CHECK(ContainerRules::AcceptsVideo("mp4"));
}
TEST_CASE("mkv accepts video") {
    CHECK(ContainerRules::AcceptsVideo("mkv"));
}
TEST_CASE("webm accepts video") {
    CHECK(ContainerRules::AcceptsVideo("webm"));
}
TEST_CASE("avi accepts video") {
    CHECK(ContainerRules::AcceptsVideo("avi"));
}
TEST_CASE("mp3 does not accept video") {
    CHECK_FALSE(ContainerRules::AcceptsVideo("mp3"));
}
TEST_CASE("wav does not accept video") {
    CHECK_FALSE(ContainerRules::AcceptsVideo("wav"));
}

// ── ShouldEmitFormatFlag ──────────────────────────────────────────────────────
TEST_CASE("opus needs -f flag") {
    CHECK(ContainerRules::ShouldEmitFormatFlag("opus"));
}
TEST_CASE("mp4 does not need -f flag") {
    CHECK_FALSE(ContainerRules::ShouldEmitFormatFlag("mp4"));
}
TEST_CASE("mkv does not need -f flag") {
    CHECK_FALSE(ContainerRules::ShouldEmitFormatFlag("mkv"));
}
TEST_CASE("mp3 does not need -f flag") {
    CHECK_FALSE(ContainerRules::ShouldEmitFormatFlag("mp3"));
}

// ── VideoCodecWarning ─────────────────────────────────────────────────────────
TEST_CASE("vp9 in mp4 yields warning") {
    CHECK_FALSE(ContainerRules::VideoCodecWarning("mp4", "libvpx-vp9").empty());
}
TEST_CASE("vp8 in mp4 yields warning") {
    CHECK_FALSE(ContainerRules::VideoCodecWarning("mp4", "libvpx").empty());
}
TEST_CASE("h264 in webm yields warning") {
    CHECK_FALSE(ContainerRules::VideoCodecWarning("webm", "libx264").empty());
}
TEST_CASE("vp9 in webm is fine") {
    CHECK(ContainerRules::VideoCodecWarning("webm", "libvpx-vp9").empty());
}
TEST_CASE("h264 in mp4 is fine") {
    CHECK(ContainerRules::VideoCodecWarning("mp4", "libx264").empty());
}
TEST_CASE("copy codec never warns") {
    CHECK(ContainerRules::VideoCodecWarning("webm", "copy").empty());
    CHECK(ContainerRules::VideoCodecWarning("mp4",  "copy").empty());
}

// ── AudioCodecWarning ─────────────────────────────────────────────────────────
TEST_CASE("aac in mp3 container yields warning") {
    CHECK_FALSE(ContainerRules::AudioCodecWarning("mp3", "aac").empty());
}
TEST_CASE("libmp3lame in mp3 container is fine") {
    CHECK(ContainerRules::AudioCodecWarning("mp3", "libmp3lame").empty());
}
TEST_CASE("libopus in webm is fine") {
    CHECK(ContainerRules::AudioCodecWarning("webm", "libopus").empty());
}
TEST_CASE("aac in webm yields warning") {
    CHECK_FALSE(ContainerRules::AudioCodecWarning("webm", "aac").empty());
}
TEST_CASE("libmp3lame in mp4 yields warning") {
    CHECK_FALSE(ContainerRules::AudioCodecWarning("mp4", "libmp3lame").empty());
}
TEST_CASE("aac in mp4 is fine") {
    CHECK(ContainerRules::AudioCodecWarning("mp4", "aac").empty());
}
TEST_CASE("non-aac in m4a yields warning") {
    CHECK_FALSE(ContainerRules::AudioCodecWarning("m4a", "libmp3lame").empty());
}
TEST_CASE("audio copy codec never warns") {
    CHECK(ContainerRules::AudioCodecWarning("mp3",  "copy").empty());
    CHECK(ContainerRules::AudioCodecWarning("webm", "copy").empty());
}

} // TEST_SUITE
