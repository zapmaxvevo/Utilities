# ffmpeg-gui — Context for Claude

## What this is

C++17 desktop GUI wrapping FFmpeg. Built with wxWidgets 3.2 + CMake.
Portfolio project. Priority: solid architecture and testable code over visual polish.

## Build

```bash
# Configure (first time or after CMakeLists changes)
cmake -B build -S . -DBUILD_TESTS=ON

# Build everything
cmake --build build -j$(nproc)

# Run unit tests
./build/ffmpeg-tests

# Run the app
./build/ffmpeg-gui
```

Tests use **doctest** fetched via CMake FetchContent. No wx dependency in the test binary.

## Architecture — strict layer separation

```
src/
  core/           — pure domain, no wx, no process calls
    FFmpegJob.h         domain model (VideoConfig, AudioConfig, TrimConfig, FFmpegJob)
    JobValidator        validates an FFmpegJob → ValidationResult{errors, warnings}
    ExtraArgsParser     tokenises extra-args string (handles quoting); no shell semantics
    ContainerRules      container-level policy: IsAudioOnly, codec warnings, -f flag policy

  command/        — turns FFmpegJob into CLI representation, no wx
    CommandBuilder      BuildArguments() → argv for exec; BuildCommandString() → visible string

  process/        — process lifecycle, wx-aware
    IProcessRunner      pure interface: Launch/Kill/IsRunning/Poll{Stdout,Stderr}/onTerminate
    WxProcessRunner     concrete wx implementation (all wx types stay in this .cpp)
    FFmpegProcess       state machine + SIGTERM→SIGKILL escalation; uses IProcessRunner
    ProgressParser      parses FFmpeg stderr: separates \r progress lines from \n log lines

  gui/            — wxWidgets UI, reads from domain, never builds FFmpeg flags directly
    MainFrame           single frame; BuildJobFromUI() → FFmpegJob → validates → executes
```

**Key rule**: the GUI never constructs FFmpeg flags. It fills `FFmpegJob` and hands it to `CommandBuilder`.

## Domain model (FFmpegJob)

```cpp
struct FFmpegJob {
    std::string              ffmpegPath;       // default "ffmpeg"
    std::string              inputFile, outputFile, outputContainer;
    VideoConfig              video;            // codec, crf/bitrate, resolution, copy
    AudioConfig              audio;            // codec, bitrate, copy, extractOnly(-vn)
    TrimConfig               trim;             // enabled, startTime, endTime, duration
    std::vector<std::string> extraArgs;        // pre-tokenised by ExtraArgsParser
};
```

## ContainerRules policy

- `IsAudioOnly(c)` → true for mp3, wav, flac, ogg, opus, m4a, aiff, aac
- `ShouldEmitFormatFlag(c)` → true only for "opus" (extension not always auto-detected)
- `VideoCodecWarning / AudioCodecWarning` → string warnings for bad codec/container combos
- MP3 + non-libmp3lame is a **hard error** in JobValidator (not just a warning)

## ExtraArgsParser rules

Handles: simple tokens, double-quoted, single-quoted, spaces inside quotes.
Returns error for unbalanced quotes.
Does NOT support: variable expansion, pipes, redirections, nested quoting.
The GUI validates before execution; BuildJobFromUI silently uses what parsed (errors caught in OnExecute).

## Process abstraction

`IProcessRunner` is the extension point for a future Win32 runner.
`WxProcessRunner` is the only concrete impl today — all wx types live in `WxProcessRunner.cpp`.
`FFmpegProcess` owns the state machine and kill escalation; uses `IProcessRunner` via `unique_ptr`.
`CheckAvailable` stays in `FFmpegProcess.cpp` and uses wxExecute sync (acceptable for now).

## Tests

```
tests/
  test_main.cpp          defines DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
  test_extra_args.cpp    ExtraArgsParser — 11 cases
  test_container_rules.cpp  ContainerRules — 24 cases
  test_job_validator.cpp    JobValidator — 18 cases
  test_command_builder.cpp  CommandBuilder — 14 cases
```

77 assertions, 0 failures. No GUI tests (by design).

## Roadmap

### Done
- **Fase 1** — Hardening: real time validation, container authority, shell quoting, CheckAvailable fix, trim radio buttons
- **Fase 2** — Practical features: extra args field, quick presets, export script (.sh/.bat), command history (in-memory), manual ffmpeg path
- **Fase Core Hardening** — ExtraArgsParser, ContainerRules, IProcessRunner/WxProcessRunner abstraction, unit tests

### Next (Fase 3 — wrapper intelligence)
- `ffprobe` integration: read input file metadata (duration → accurate progress, resolution, codecs)
- Input file info panel in GUI
- Codec/container validation matrix (expand ContainerRules)
- Custom resolution input (not just presets)
- "No audio" option (-an) for video output

### Later (Fase 4 — v1 solid)
- Persist history between sessions (JSON or INI)
- Real Windows validation (WxProcessRunner on Win32, path quoting, ffmpeg.exe)
- Packaging / installers
- User-facing documentation

## wxWidgets notes

- Use `wxString::FromUTF8(std::string)` for any raw bytes from subprocess output
- Never do `wxString += char` for bytes >= 0x80 — triggers `FromHi8bit` assert
- `wxSpinCtrl` with `wxSize(60,-1)` is too small on GTK; use `wxDefaultSize`
- `wxEXEC_ASYNC` with `const char* const*` argv works; sync+capture overload needs `wxString`
- `wxGauge` range 0-1000 gives smooth updates (avoids 1% jumps)
- `wxSizerFlags(n).Expand().CenterVertical()` in a horizontal sizer → warning; remove CenterVertical
- Radio button groups: first button needs `wxRB_GROUP` flag

## Conventions

- New containers/codec rules → add to `ContainerRules.cpp` only
- New validation rules → add to `JobValidator.cpp` only
- New command-line flags → add to `CommandBuilder.cpp` `BuildArgs()` only
- GUI widget additions → update `MainFrame.h` IDs enum + widget members + `BuildUI()` + bindings
- Tests live in `tests/`; add cases to existing suite files; no test for GUI layer
