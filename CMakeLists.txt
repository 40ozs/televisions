cmake_minimum_required(VERSION 3.22)

project(TelevisionsAudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at external/JUCE. Fetch the pinned version with:\n"
        "  git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git external/JUCE")
endif()

add_subdirectory(external/JUCE)

enable_testing()
add_subdirectory(VintageFilmAudio)
