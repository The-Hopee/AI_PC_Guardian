add_library(
    guardian_common STATIC
    src/telemetry_event.cpp
)
add_library(Guardian::Common ALIAS guardian_common)

target_link_libraries(
    guardian_common
    PUBLIC guardian_project_options
)

target_include_directories(
    guardian_common
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>"
)
