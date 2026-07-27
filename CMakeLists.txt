cmake_minimum_required(VERSION 3.22)

project(TelevisionsAudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# JUCE acquisition (ADR-014, amended): prefer the pinned local checkout at
# external/JUCE (offline-deterministic, inspectable); on a fresh clone where
# it is absent, fall back to FetchContent at the SAME pinned tag so
# `cmake -B build` works out of the box.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    message(STATUS "JUCE not found at external/JUCE — fetching pinned tag 8.0.8 "
                   "(one-time; to pin locally instead: git clone --depth 1 --branch 8.0.8 "
                   "https://github.com/juce-framework/JUCE.git external/JUCE)")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.8
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
    # Test targets include JUCE module headers from this path; keep both
    # acquisition modes equivalent.
    set(VFA_JUCE_MODULES_DIR "${juce_SOURCE_DIR}/modules" CACHE INTERNAL "")
endif()
if(NOT DEFINED VFA_JUCE_MODULES_DIR)
    set(VFA_JUCE_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/modules" CACHE INTERNAL "")
endif()

enable_testing()
add_subdirectory(VintageFilmAudio)
