#include <doctest/doctest.h>
#include "command/CommandBuilder.h"

#include <algorithm>

static bool Contains(const std::vector<std::string>& v, const std::string& s)
{
    return std::find(v.begin(), v.end(), s) != v.end();
}

static bool StringContains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

static FFmpegJob BasicJob()
{
    FFmpegJob j;
    j.inputFile       = "/src/input.mp4";
    j.outputFile      = "/dst/output.mp4";
    j.outputContainer = "mp4";
    j.video.codec     = "libx264";
    j.video.useCrf    = true;
    j.video.crf       = 23;
    j.audio.codec     = "aac";
    j.audio.bitrate   = 128;
    return j;
}

TEST_SUITE("CommandBuilder") {

// ── Basic command ─────────────────────────────────────────────────────────────
TEST_CASE("basic command contains expected flags") {
    auto args = CommandBuilder::BuildArguments(BasicJob());
    CHECK(Contains(args, "-y"));
    CHECK(Contains(args, "-i"));
    CHECK(Contains(args, "/src/input.mp4"));
    CHECK(Contains(args, "-c:v"));
    CHECK(Contains(args, "libx264"));
    CHECK(Contains(args, "-crf"));
    CHECK(Contains(args, "23"));
    CHECK(Contains(args, "-c:a"));
    CHECK(Contains(args, "aac"));
    CHECK(Contains(args, "/dst/output.mp4"));
}

TEST_CASE("output file is the last argument") {
    auto args = CommandBuilder::BuildArguments(BasicJob());
    REQUIRE_FALSE(args.empty());
    CHECK(args.back() == "/dst/output.mp4");
}

TEST_CASE("command string starts with ffmpeg binary") {
    auto cmd = CommandBuilder::BuildCommandString(BasicJob());
    CHECK(StringContains(cmd, "ffmpeg"));
}

TEST_CASE("command string contains input file") {
    auto cmd = CommandBuilder::BuildCommandString(BasicJob());
    CHECK(StringContains(cmd, "/src/input.mp4"));
}

// ── Audio-only ────────────────────────────────────────────────────────────────
TEST_CASE("audio-only mode adds -vn") {
    auto j = BasicJob();
    j.audio.extractOnly = true;
    auto args = CommandBuilder::BuildArguments(j);
    CHECK(Contains(args, "-vn"));
    CHECK_FALSE(Contains(args, "-c:v"));
}

TEST_CASE("audio-only in command string contains -vn") {
    auto j = BasicJob();
    j.audio.extractOnly = true;
    auto cmd = CommandBuilder::BuildCommandString(j);
    CHECK(StringContains(cmd, "-vn"));
}

// ── Trim ─────────────────────────────────────────────────────────────────────
TEST_CASE("trim with duration produces -t flag") {
    auto j = BasicJob();
    j.trim.enabled  = true;
    j.trim.duration = "30";
    auto args = CommandBuilder::BuildArguments(j);
    CHECK(Contains(args, "-t"));
    CHECK(Contains(args, "30"));
    CHECK_FALSE(Contains(args, "-to"));
}

TEST_CASE("trim with end time produces -to flag") {
    auto j = BasicJob();
    j.trim.enabled   = true;
    j.trim.startTime = "00:00:10";
    j.trim.endTime   = "00:01:00";
    auto args = CommandBuilder::BuildArguments(j);
    CHECK(Contains(args, "-ss"));
    CHECK(Contains(args, "-to"));
    CHECK_FALSE(Contains(args, "-t"));
}

TEST_CASE("trim disabled produces no -ss flag") {
    auto j = BasicJob();
    j.trim.enabled   = false;
    j.trim.startTime = "00:00:10";
    j.trim.endTime   = "00:01:00";
    auto args = CommandBuilder::BuildArguments(j);
    CHECK_FALSE(Contains(args, "-ss"));
    CHECK_FALSE(Contains(args, "-to"));
}

// ── Container format flag ────────────────────────────────────────────────────
TEST_CASE("opus container emits -f opus") {
    auto j = BasicJob();
    j.outputFile      = "/dst/output.opus";
    j.outputContainer = "opus";
    j.audio.extractOnly = true;
    j.audio.codec     = "libopus";
    auto args = CommandBuilder::BuildArguments(j);
    CHECK(Contains(args, "-f"));
    CHECK(Contains(args, "opus"));
    // -f must appear before output file
    auto fi = std::find(args.begin(), args.end(), "-f");
    auto oi = std::find(args.begin(), args.end(), "/dst/output.opus");
    REQUIRE(fi != args.end());
    REQUIRE(oi != args.end());
    CHECK(fi < oi);
}

TEST_CASE("mp4 container does not emit -f flag") {
    auto args = CommandBuilder::BuildArguments(BasicJob());
    CHECK_FALSE(Contains(args, "-f"));
}

// ── Extra args ────────────────────────────────────────────────────────────────
TEST_CASE("extra args appear before output file") {
    auto j = BasicJob();
    j.extraArgs = { "-movflags", "+faststart" };
    auto args = CommandBuilder::BuildArguments(j);
    CHECK(Contains(args, "-movflags"));
    CHECK(Contains(args, "+faststart"));
    auto mi = std::find(args.begin(), args.end(), "-movflags");
    auto oi = std::find(args.begin(), args.end(), "/dst/output.mp4");
    REQUIRE(mi != args.end());
    REQUIRE(oi != args.end());
    CHECK(mi < oi);
}

// ── BuildCommandString vs BuildArguments consistency ─────────────────────────
TEST_CASE("BuildCommandString reflects all tokens from BuildArguments") {
    auto j    = BasicJob();
    auto args = CommandBuilder::BuildArguments(j);
    auto cmd  = CommandBuilder::BuildCommandString(j);
    // Every arg should appear somewhere in the command string (may be quoted).
    for (const auto& a : args) {
        if (a.empty()) continue;
        INFO("arg missing from command string: " << a);
        CHECK(StringContains(cmd, a));
    }
}

TEST_CASE("custom ffmpeg path appears in command string") {
    auto j = BasicJob();
    j.ffmpegPath = "/usr/local/bin/ffmpeg";
    auto cmd = CommandBuilder::BuildCommandString(j);
    CHECK(StringContains(cmd, "/usr/local/bin/ffmpeg"));
}

// ── Video bitrate mode ────────────────────────────────────────────────────────
TEST_CASE("bitrate mode produces -b:v instead of -crf") {
    auto j = BasicJob();
    j.video.useCrf  = false;
    j.video.bitrate = 3000;
    auto args = CommandBuilder::BuildArguments(j);
    CHECK(Contains(args, "-b:v"));
    CHECK(Contains(args, "3000k"));
    CHECK_FALSE(Contains(args, "-crf"));
}

// ── Resolution ────────────────────────────────────────────────────────────────
TEST_CASE("resolution produces -vf scale=") {
    auto j = BasicJob();
    j.video.resolution = "1280x720";
    auto args = CommandBuilder::BuildArguments(j);
    CHECK(Contains(args, "-vf"));
    CHECK(Contains(args, "scale=1280x720"));
}

TEST_CASE("empty resolution produces no -vf flag") {
    auto j = BasicJob();
    j.video.resolution = "";
    auto args = CommandBuilder::BuildArguments(j);
    CHECK_FALSE(Contains(args, "-vf"));
}

} // TEST_SUITE
