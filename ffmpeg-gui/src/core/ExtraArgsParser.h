#pragma once
#include <string>
#include <vector>

// ─── ExtraArgsParseResult ────────────────────────────────────────────────────
struct ExtraArgsParseResult {
    std::vector<std::string> args;
    std::vector<std::string> errors;
    bool IsValid() const { return errors.empty(); }
};

// ─── ExtraArgsParser ─────────────────────────────────────────────────────────
// Parses a single-line string of extra FFmpeg arguments into structured tokens.
//
// Supported:
//   - Whitespace-separated tokens
//   - Double-quoted tokens: -metadata "title=My video"
//   - Single-quoted tokens: -vf 'scale=1280:720,format=yuv420p'
//   - Spaces inside quotes are preserved
//
// Not supported (returns error):
//   - Unbalanced quotes
//   - Variable expansion, pipes, redirections, nested quoting
class ExtraArgsParser {
public:
    static ExtraArgsParseResult Parse(const std::string& input);
};
