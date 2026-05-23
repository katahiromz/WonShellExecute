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

# statically link
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # using Clang
    set(CMAKE_C_FLAGS "-static")
    set(CMAKE_CXX_FLAGS "-static")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # using GCC
    set(CMAKE_C_FLAGS "-static")
    set(CMAKE_CXX_FLAGS "-static")
elseif (MSVC)
    # replace "/MD" with "/MT" (building without runtime DLLs)
    set(CompilerFlags
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    foreach(CompilerFlags ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlags} "${${CompilerFlags}}")
    endforeach()
endif()

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
