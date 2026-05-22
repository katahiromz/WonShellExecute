# CMakeLists.txt --- CMake project settings
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.10)

# project name and languages
project(WonShellExecute CXX RC)

# libshlexec.a
add_library(shlexec STATIC shlexec.cpp utils.cpp osinfo.cpp)
target_link_libraries(shlexec comctl32)

# tests.exe
add_executable(tests tests.cpp)
target_link_libraries(tests PRIVATE shlexec shlwapi)
