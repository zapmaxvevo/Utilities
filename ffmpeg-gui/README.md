# ffmpeg-gui

Visual frontend for FFmpeg built with C++17, wxWidgets and CMake.

The app lets you configure common FFmpeg jobs from a GUI, see the equivalent
command in real time, copy it, export it as a script, execute it, and inspect
logs, status, and progress without dropping to a terminal.

## Requirements

| Dependency | Linux | Windows |
|---|---|---|
| CMake >= 3.16 | `apt install cmake` | cmake.org |
| C++17 compiler | GCC/Clang | MSVC 2019+ or MinGW-w64 |
| wxWidgets >= 3.0 | `apt install libwxgtk3.2-dev` | wxWidgets installer + set `wxWidgets_ROOT_DIR` |
| ffmpeg | `apt install ffmpeg` | ffmpeg.org -> add to PATH |

## Build

### Linux

```bash
cd ffmpeg-gui
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/ffmpeg-gui
```

### Windows (MSVC)

```bat
cmake -B build -G "Visual Studio 17 2022" -DwxWidgets_ROOT_DIR=C:\wxWidgets
cmake --build build --config Release
build\Release\ffmpeg-gui.exe
```

### Windows (MinGW)

```bat
cmake -B build -G "MinGW Makefiles" -DwxWidgets_ROOT_DIR=C:\wxWidgets
cmake --build build
build\ffmpeg-gui.exe
```

## Architecture

```text
src/
├── core/           Domain model + validation (FFmpegJob, JobValidator)
├── command/        Pure command generation: BuildArguments() + BuildCommandString()
├── process/        Process lifecycle, availability check, cancelation, progress parsing
└── gui/            wxWidgets UI (App, MainFrame)
```

### Data flow

```text
UI widgets
    └─► BuildJobFromUI()  →  FFmpegJob
                                 │
                                 ├─► JobValidator::Validate()
                                 │
                    ┌────────────┴────────────┐
                    ▼                         ▼
          CommandBuilder                CommandBuilder
          ::BuildCommandString()        ::BuildArguments()
          (display only)                (execution args)
                                              │
                           FFmpegProcess::CheckAvailable() / Start()
                                              │
                                       wxExecute (async)
                                              │
                                       wxTimer::OnPollTimer()
                                              │
                              PollOutput() → ProgressParser::Feed()
                                              │
                     log area + status label + progress bar + recent history
```

**Why this split?**

- `FFmpegJob` is the central job description shared by the app.
- `JobValidator` keeps business rules out of UI event handlers.
- `CommandBuilder` owns both the visible command and the executable argv.
- `FFmpegProcess` owns subprocess state, availability checks, and cancelation.
- `ProgressParser` isolates FFmpeg stderr parsing from the GUI.
- `MainFrame` reads widgets, updates the model, and renders results.

## Current features

- Input / output file picker
- Output container selector with output extension sync
- Manual FFmpeg binary path field plus file picker
- Presets for common jobs:
  Web MP4, Web MP4 720p, HEVC MKV, audio extraction, small-file compression, stream copy
- Video codec: `libx264`, `libx265`, `libvpx-vp9`, `libsvtav1`, `copy`
- Audio codec: `aac`, `libmp3lame`, `libopus`, `flac`, `copy`
- Audio-only extraction (`-vn`)
- CRF or bitrate quality mode
- Resolution presets or original size
- Trim by start time plus either end time or duration
- Extra args text field appended to the generated command
- Live command preview with shell-style quoting for display/copy
- Copy command to clipboard
- Export command as `.sh` or `.bat`
- Recent command history in the current session
- Validation errors and warnings before execution
- FFmpeg availability check before launch
- Execute / Stop controls
- Process status: Idle / Running / Finished / Failed / Canceled
- Progress bar with parsed progress details
- Real-time log output polled from the subprocess

## Validation and execution behavior

Before launching FFmpeg, the app currently validates:

- input/output presence
- input file existence
- input and output not being the same path
- output extension matching the selected container
- trim time parsing for `start`, `end`, and `duration`
- mutual exclusion of end time and duration
- `end > start`
- `duration > 0`
- invalid combinations such as video copy plus scaling
- bitrate / CRF sanity checks
- MP3 container paired with a compatible audio codec

Warnings are shown for cases that may still be valid but risky, such as:

- stream copy together with trim
- audio-only export to a video-oriented container

Execution behavior:

- the visible command is generated separately from the executable argument list
- FFmpeg is launched with structured argv, not by concatenating a shell string
- Stop first requests graceful termination, then escalates if the process does not exit
- progress is parsed from FFmpeg stderr updates and reflected in the GUI

## Portability notes

| Risk | Detail |
|---|---|
| `wxProcess::IsErrorAvailable()` | Uses `select()` on Unix and `PeekNamedPipe()` on Windows. Both are non-blocking. |
| `wxEXEC_MAKE_GROUP_LEADER` | On Windows this flag is ignored, so process-group behavior should be verified with real runs. |
| ffmpeg binary path | The app accepts a custom binary path and quotes it when checking availability, but end-to-end Windows behavior still needs validation. |
| Console window (Windows) | Suppressed via `WIN32` in `add_executable`. |
| Visible command quoting | `BuildCommandString()` uses shell-style quoting for display only; execution still uses structured argv. |
| Progress parsing | Progress is parsed from stderr `\r` updates; this works well for common runs but depends on FFmpeg's text output format. |
| Stop behavior | The app requests a graceful stop first and escalates if needed; exact behavior should still be validated on Windows. |

## Current limitations

- Container selection syncs extension and validates consistency, but the command builder still does not emit an explicit `-f <container>`.
- The visible command is much more robust than before, but it is still only a best-effort shell representation across platforms.
- Extra args are currently tokenized by whitespace in the GUI, so quoted multi-word values are not preserved as a single argument.
- Recent history is in-memory only and stores commands, not full jobs.
- Exported `.sh` scripts are written as plain text and are not marked executable automatically.
- Progress percentage is inferred from FFmpeg log output, not from structured metadata.
- There are still no automated tests for `core/`, `command/`, or `process/`.
- Windows support is designed for, but not yet verified with real end-to-end runs.

## Extending

| Feature | Where to add |
|---|---|
| Better extra-args parsing | Parse quoted tokens in `gui/` before filling `FFmpegJob::extraArgs` |
| Persistent presets / history | Add a `presets/` or `storage/` layer and load/save from `gui/` |
| Input metadata (`ffprobe`) | New probe/service layer, then surface in `gui/` |
| Job queue | New `queue/` layer owning a list of `FFmpegJob` |
| Explicit container output (`-f`) | Extend `command/` and align `core/` validation rules |
| Tests | Add unit coverage for `core/`, `command/`, and `process/ProgressParser` |
