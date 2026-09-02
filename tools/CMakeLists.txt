add_library(
    guardian_tools STATIC
    src/logger.cpp
)
add_library(Guardian::Tools ALIAS guardian_tools)

target_include_directories(
    guardian_tools
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(
    guardian_tools
    PUBLIC guardian_project_options
)
