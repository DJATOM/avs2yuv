cmake_minimum_required(VERSION 3.1)

project(avs2yuv VERSION 0.30)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

if(MSVC)
    message(FATAL_ERROR "Windows builds must be compiled using MinGW-w64, not MSVC.")
else()
    include(FindPkgConfig)
    if(MINGW)
        pkg_search_module(AVISYNTH avisynth>=3.5.0)
        if(AVISYNTH_FOUND)
            include_directories(${AVISYNTH_INCLUDE_DIRS})
        else()
            # the path on Windows itself, outside of pkg-config
            include_directories($ENV{AVISYNTH_SDK_PATH}/include)
        endif()
    else()
        pkg_search_module(AVISYNTH REQUIRED avisynth>=3.5.0)
        include_directories(${AVISYNTH_INCLUDE_DIRS})
    endif()
endif()

add_executable(avs2yuv avs2yuv.c)

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    set(SYSLIB "root")
else()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(SYSLIB "pthread" "dl")
    else()
        set(SYSLIB "pthread")
    endif()
endif()

if(NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-as-needed")
endif()

target_link_libraries(avs2yuv "${SYSLIB}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I. -Wall -O3 -ffast-math -fno-math-errno -fomit-frame-pointer")

# This is needed because trying to detect OSX versions in C is a PITA.
# The only occasion this option should be used is for OSX 10.6-10.11 on Intel.
# PPC versions of OSX have this enabled by default, right in avs2yuv.c.
# Most/all other OSes should already support clock_gettime.
option(OLD_TIME_BEHAVIOR "Use ftime() instead of clock_gettime() for timing." OFF)
if(${OLD_TIME_BEHAVIOR})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOLD_TIME_BEHAVIOR")
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86" OR
   CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR
   CMAKE_SYSTEM_PROCESSOR STREQUAL "x64" OR
   CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2 -mfpmath=sse")
endif()

include(GNUInstallDirs)
install(TARGETS avs2yuv
        RUNTIME DESTINATION bin)

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
