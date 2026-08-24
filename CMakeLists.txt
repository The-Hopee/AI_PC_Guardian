cmake_minimum_required(VERSION 3.25...4.3)

project(
    AI_PC_Guardian
    VERSION 0.1.0
    DESCRIPTION "Cross-platform PC diagnostics foundation"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    if(NOT MSVC OR MSVC_VERSION LESS 1930)
        message(FATAL_ERROR "Windows builds require MSVC from Visual Studio 2022 or newer.")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
       OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
        message(FATAL_ERROR "Linux builds require GCC 11 or newer.")
    endif()
else()
    message(FATAL_ERROR "ARCH-001 currently supports Windows and Linux only.")
endif()

include(CTest)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/guardian/version.hpp"
    @ONLY
)

add_library(guardian_project_options INTERFACE)
target_compile_features(guardian_project_options INTERFACE cxx_std_17)

if(MSVC)
    target_compile_options(guardian_project_options INTERFACE /W4 /permissive- /utf-8)
else()
    target_compile_options(guardian_project_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(common)
add_subdirectory(proto)
add_subdirectory(server)

if(WIN32)
    add_subdirectory(agent)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
