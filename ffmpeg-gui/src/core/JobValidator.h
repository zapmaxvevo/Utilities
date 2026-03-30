#pragma once
#include <string>
#include <vector>
#include "FFmpegJob.h"

// ─── ValidationResult ────────────────────────────────────────────────────────
struct ValidationResult {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    bool IsValid() const { return errors.empty(); }
};

// ─── JobValidator ────────────────────────────────────────────────────────────
// Pure domain layer – no wx, no I/O (except file-existence check), no process calls.
class JobValidator
{
public:
    static ValidationResult Validate(const FFmpegJob& job);

private:
    // Returns seconds >= 0 for valid time, -1.0 for empty (not set), -2.0 for parse error.
    static double ParseTimeSec(const std::string& s);
};
