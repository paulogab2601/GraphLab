cmake_minimum_required(VERSION 3.21)

project(GraphLab
    VERSION 0.1.0
    DESCRIPTION "Empirical and formal analysis of augmenting path bottlenecks in Ford-Fulkerson variants"
    HOMEPAGE_URL "https://github.com/paulogabriel/graphlab"
    LANGUAGES C CXX
)

# -----------------------------------------------------------------------------
# Language standards
# -----------------------------------------------------------------------------
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Output layout
# -----------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(GRAPHLAB_BUILD_TESTS         "Build unit tests"                                  ON)
option(GRAPHLAB_BUILD_UI            "Build the Qt6 UI (Phase 3+, requires Qt6)"         OFF)
option(GRAPHLAB_BUILD_CLI           "Build the standalone simulation CLI (Phase 2+)"    OFF)
option(GRAPHLAB_WARNINGS_AS_ERRORS  "Treat warnings as errors"                          OFF)

# -----------------------------------------------------------------------------
# Global compile options
# -----------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
    if(GRAPHLAB_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)
    if(GRAPHLAB_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(src/core)

if(GRAPHLAB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GRAPHLAB_BUILD_CLI)
    add_subdirectory(src/app)
endif()

if(GRAPHLAB_BUILD_UI)
    add_subdirectory(src/ui)
endif()

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "GraphLab ${PROJECT_VERSION}")
message(STATUS "  Build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C compiler        : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  CXX compiler      : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Tests             : ${GRAPHLAB_BUILD_TESTS}")
message(STATUS "  CLI               : ${GRAPHLAB_BUILD_CLI}")
message(STATUS "  UI                : ${GRAPHLAB_BUILD_UI}")
message(STATUS "  Warnings as errors: ${GRAPHLAB_WARNINGS_AS_ERRORS}")
message(STATUS "")
