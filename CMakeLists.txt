cmake_minimum_required(VERSION 3.20)
project(hive LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure build outputs are placed in a single bin/lib folder
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Options
option(HIVE_BUILD_TESTS "Build unit tests" ON)

# Dependencies via FetchContent
include(FetchContent)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# SFML (for desktop UI)
include(FetchSFML)

add_subdirectory(engine)
add_subdirectory(ui-desktop)
if(HIVE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()