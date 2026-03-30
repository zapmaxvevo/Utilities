#include <doctest/doctest.h>
#include "core/JobValidator.h"

static constexpr const char* kExistingFile = "/dev/null";

static FFmpegJob ValidJob()
{
    FFmpegJob j;
    j.inputFile       = kExistingFile;
    j.outputFile      = "/tmp/ffgui_test_out.mp4";
    j.outputContainer = "mp4";
    j.video.codec     = "libx264";
    j.video.useCrf    = true;
    j.video.crf       = 23;
    j.audio.codec     = "aac";
    j.audio.bitrate   = 128;
    return j;
}

static bool ErrorContains(const ValidationResult& r, const std::string& sub)
{
    for (const auto& e : r.errors)
        if (e.find(sub) != std::string::npos) return true;
    return false;
}

static bool WarnContains(const ValidationResult& r, const std::string& sub)
{
    for (const auto& w : r.warnings)
        if (w.find(sub) != std::string::npos) return true;
    return false;
}

TEST_SUITE("JobValidator") {

// ── Required fields ───────────────────────────────────────────────────────────
TEST_CASE("empty input file is an error") {
    auto j = ValidJob();
    j.inputFile = "";
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
    bool found = ErrorContains(r, "input") || ErrorContains(r, "Input");
    CHECK(found);
}

TEST_CASE("empty output file is an error") {
    auto j = ValidJob();
    j.outputFile = "";
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
}

// ── Input == output ───────────────────────────────────────────────────────────
TEST_CASE("input same as output is an error") {
    auto j = ValidJob();
    j.outputFile = j.inputFile;
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
    CHECK(ErrorContains(r, "same"));
}

// ── File existence ────────────────────────────────────────────────────────────
TEST_CASE("non-existent input file is an error") {
    auto j = ValidJob();
    j.inputFile = "/definitely/does/not/exist_abc123.mp4";
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
    bool found = ErrorContains(r, "not exist") || ErrorContains(r, "does not");
    CHECK(found);
}

// ── Extension / container mismatch ───────────────────────────────────────────
TEST_CASE("output extension different from container is an error") {
    auto j = ValidJob();
    j.outputFile      = "/tmp/out.mkv";
    j.outputContainer = "mp4";
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
    bool found = ErrorContains(r, "match") || ErrorContains(r, "extension");
    CHECK(found);
}

TEST_CASE("matching extension and container passes") {
    auto r = JobValidator::Validate(ValidJob());
    CHECK(r.IsValid());
}

// ── Stream copy + scale ───────────────────────────────────────────────────────
TEST_CASE("copy video with non-empty resolution is an error") {
    auto j = ValidJob();
    j.video.copy       = true;
    j.video.codec      = "";
    j.video.resolution = "1280x720";
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
    bool found = ErrorContains(r, "scale") || ErrorContains(r, "copy");
    CHECK(found);
}

// ── Trim ─────────────────────────────────────────────────────────────────────
TEST_CASE("trim with bad start time format is an error") {
    auto j = ValidJob();
    j.trim.enabled   = true;
    j.trim.startTime = "not-a-time";
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
}

TEST_CASE("trim end time before start time is an error") {
    auto j = ValidJob();
    j.trim.enabled   = true;
    j.trim.startTime = "00:00:30";
    j.trim.endTime   = "00:00:10";
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
    bool found = ErrorContains(r, "after") || ErrorContains(r, "End time");
    CHECK(found);
}

TEST_CASE("trim duration of zero is an error") {
    auto j = ValidJob();
    j.trim.enabled  = true;
    j.trim.duration = "0";
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
    bool found = ErrorContains(r, "Duration") || ErrorContains(r, "duration");
    CHECK(found);
}

TEST_CASE("trim with both end time and duration is an error") {
    auto j = ValidJob();
    j.trim.enabled  = true;
    j.trim.endTime  = "00:01:00";
    j.trim.duration = "30";
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
}

TEST_CASE("valid trim with end time passes") {
    auto j = ValidJob();
    j.trim.enabled   = true;
    j.trim.startTime = "00:00:10";
    j.trim.endTime   = "00:01:00";
    auto r = JobValidator::Validate(j);
    CHECK(r.IsValid());
}

TEST_CASE("valid trim with duration passes") {
    auto j = ValidJob();
    j.trim.enabled  = true;
    j.trim.duration = "30";
    auto r = JobValidator::Validate(j);
    CHECK(r.IsValid());
}

// ── Audio-only to video container ─────────────────────────────────────────────
TEST_CASE("audio-only output to video container yields a warning") {
    auto j = ValidJob();
    j.audio.extractOnly = true;
    j.outputFile        = "/tmp/out.mp4";
    j.outputContainer   = "mp4";
    auto r = JobValidator::Validate(j);
    CHECK(r.IsValid());   // warning, not error
    bool found = WarnContains(r, "mp4") || WarnContains(r, "container");
    CHECK(found);
}

// ── MP3 codec enforcement ─────────────────────────────────────────────────────
TEST_CASE("mp3 container with non-libmp3lame codec is an error") {
    auto j = ValidJob();
    j.outputFile        = "/tmp/out.mp3";
    j.outputContainer   = "mp3";
    j.audio.codec       = "aac";
    j.audio.extractOnly = true;
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
    CHECK(ErrorContains(r, "libmp3lame"));
}

// ── CRF range ────────────────────────────────────────────────────────────────
TEST_CASE("CRF above 51 for libx264 is an error") {
    auto j = ValidJob();
    j.video.crf = 60;
    auto r = JobValidator::Validate(j);
    CHECK_FALSE(r.IsValid());
}

TEST_CASE("CRF 0 for libx264 is valid (lossless)") {
    auto j = ValidJob();
    j.video.crf = 0;
    auto r = JobValidator::Validate(j);
    CHECK(r.IsValid());
}

// ── Codec/container warnings via ContainerRules ───────────────────────────────
TEST_CASE("vp9 video codec in mp4 container produces a warning") {
    auto j = ValidJob();
    j.video.codec = "libvpx-vp9";
    auto r = JobValidator::Validate(j);
    CHECK(r.IsValid());   // warning only
    CHECK_FALSE(r.warnings.empty());
}

TEST_CASE("libmp3lame audio in mp4 container produces a warning") {
    auto j = ValidJob();
    j.audio.codec = "libmp3lame";
    auto r = JobValidator::Validate(j);
    CHECK(r.IsValid());
    CHECK_FALSE(r.warnings.empty());
}

} // TEST_SUITE
