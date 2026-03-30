cmake_minimum_required(VERSION 3.16)
project(FFmpegGUI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── wxWidgets ──────────────────────────────────────────────────────────────────
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/core/FFmpegJob.cpp
    src/core/JobValidator.cpp
    src/core/ExtraArgsParser.cpp
    src/core/ContainerRules.cpp
    src/command/CommandBuilder.cpp
    src/process/FFmpegProcess.cpp
    src/process/WxProcessRunner.cpp
    src/process/ProgressParser.cpp
    src/gui/App.cpp
    src/gui/MainFrame.cpp
)

# WIN32 suppresses the console window on Windows; ignored on Linux/macOS.
add_executable(ffmpeg-gui WIN32 ${SOURCES})

target_include_directories(ffmpeg-gui PRIVATE src)
target_link_libraries(ffmpeg-gui PRIVATE ${wxWidgets_LIBRARIES})

if(MSVC)
    target_compile_options(ffmpeg-gui PRIVATE /W3)
else()
    target_compile_options(ffmpeg-gui PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
# Unit tests cover core/ and command/ layers only — no wxWidgets dependency.
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
    )
    FetchContent_MakeAvailable(doctest)

    # Sources shared with the main executable but wx-free
    set(TEST_CORE_SOURCES
        src/core/FFmpegJob.cpp
        src/core/JobValidator.cpp
        src/core/ExtraArgsParser.cpp
        src/core/ContainerRules.cpp
        src/command/CommandBuilder.cpp
    )

    add_executable(ffmpeg-tests
        ${TEST_CORE_SOURCES}
        tests/test_main.cpp
        tests/test_extra_args.cpp
        tests/test_container_rules.cpp
        tests/test_job_validator.cpp
        tests/test_command_builder.cpp
    )

    target_include_directories(ffmpeg-tests PRIVATE src)
    target_link_libraries(ffmpeg-tests PRIVATE doctest::doctest)

    if(MSVC)
        target_compile_options(ffmpeg-tests PRIVATE /W3)
    else()
        target_compile_options(ffmpeg-tests PRIVATE -Wall -Wextra)
    endif()

    include(CTest)
    enable_testing()
    add_test(NAME unit_tests COMMAND ffmpeg-tests)
endif()
