# CMake generated Testfile for 
# Source directory: /home/maxi/program/utilities/ffmpeg-gui
# Build directory: /home/maxi/program/utilities/ffmpeg-gui/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit_tests "/home/maxi/program/utilities/ffmpeg-gui/build/ffmpeg-tests")
set_tests_properties(unit_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/maxi/program/utilities/ffmpeg-gui/CMakeLists.txt;80;add_test;/home/maxi/program/utilities/ffmpeg-gui/CMakeLists.txt;0;")
subdirs("_deps/doctest-build")
