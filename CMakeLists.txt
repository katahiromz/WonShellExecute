# CMakeLists.txt --- CMake project settings
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.10)

# enable testing
enable_testing()

# project name, version, and languages
project(WonShellExecute VERSION 0.0 LANGUAGES CXX RC)

# support unicode
add_definitions(-DUNICODE -D_UNICODE)

# libshlexec.a
add_library(shlexec STATIC shlexec.cpp utils.cpp osinfo.cpp)

# test1.exe
add_executable(test1 test1.cpp shlexec_res.rc)
target_link_libraries(test1 PRIVATE shlexec shlwapi userenv)

# test2.exe
add_executable(test2 test2.cpp shlexec_res.rc)
target_link_libraries(test2 PRIVATE shlexec shlwapi userenv)

# add test
add_test(NAME test2 COMMAND test2)
