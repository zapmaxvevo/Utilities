#include <doctest/doctest.h>
#include "core/ExtraArgsParser.h"

TEST_SUITE("ExtraArgsParser") {

TEST_CASE("empty string produces no tokens and no errors") {
    auto r = ExtraArgsParser::Parse("");
    CHECK(r.IsValid());
    CHECK(r.args.empty());
}

TEST_CASE("whitespace-only produces no tokens") {
    auto r = ExtraArgsParser::Parse("   \t  ");
    CHECK(r.IsValid());
    CHECK(r.args.empty());
}

TEST_CASE("simple flags without values") {
    auto r = ExtraArgsParser::Parse("-y -nostdin");
    REQUIRE(r.IsValid());
    REQUIRE(r.args.size() == 2);
    CHECK(r.args[0] == "-y");
    CHECK(r.args[1] == "-nostdin");
}

TEST_CASE("flag and value pair") {
    auto r = ExtraArgsParser::Parse("-pix_fmt yuv420p");
    REQUIRE(r.IsValid());
    REQUIRE(r.args.size() == 2);
    CHECK(r.args[0] == "-pix_fmt");
    CHECK(r.args[1] == "yuv420p");
}

TEST_CASE("double-quoted token preserves internal spaces") {
    auto r = ExtraArgsParser::Parse(R"(-metadata "title=My Video")");
    REQUIRE(r.IsValid());
    REQUIRE(r.args.size() == 2);
    CHECK(r.args[0] == "-metadata");
    CHECK(r.args[1] == "title=My Video");
}

TEST_CASE("single-quoted token preserves internal spaces") {
    auto r = ExtraArgsParser::Parse("-vf 'scale=1280:720,format=yuv420p'");
    REQUIRE(r.IsValid());
    REQUIRE(r.args.size() == 2);
    CHECK(r.args[0] == "-vf");
    CHECK(r.args[1] == "scale=1280:720,format=yuv420p");
}

TEST_CASE("mix of quoted and unquoted tokens") {
    auto r = ExtraArgsParser::Parse(R"(-movflags +faststart -metadata "comment=test" -level 4.0)");
    REQUIRE(r.IsValid());
    REQUIRE(r.args.size() == 6);
    CHECK(r.args[0] == "-movflags");
    CHECK(r.args[1] == "+faststart");
    CHECK(r.args[2] == "-metadata");
    CHECK(r.args[3] == "comment=test");
    CHECK(r.args[4] == "-level");
    CHECK(r.args[5] == "4.0");
}

TEST_CASE("unbalanced double quote returns error") {
    auto r = ExtraArgsParser::Parse(R"(-metadata "title=oops)");
    CHECK_FALSE(r.IsValid());
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].find("double") != std::string::npos);
}

TEST_CASE("unbalanced single quote returns error") {
    auto r = ExtraArgsParser::Parse("-vf 'scale=bad");
    CHECK_FALSE(r.IsValid());
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].find("single") != std::string::npos);
}

TEST_CASE("empty double-quoted string produces one empty token") {
    auto r = ExtraArgsParser::Parse(R"(-metadata "")");
    REQUIRE(r.IsValid());
    REQUIRE(r.args.size() == 2);
    CHECK(r.args[1].empty());
}

TEST_CASE("multiple spaces between tokens are collapsed") {
    auto r = ExtraArgsParser::Parse("-y    -i    input.mp4");
    REQUIRE(r.IsValid());
    REQUIRE(r.args.size() == 3);
    CHECK(r.args[0] == "-y");
    CHECK(r.args[1] == "-i");
    CHECK(r.args[2] == "input.mp4");
}

} // TEST_SUITE
