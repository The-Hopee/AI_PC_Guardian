add_library(guardian_common INTERFACE)
add_library(Guardian::Common ALIAS guardian_common)

target_link_libraries(guardian_common INTERFACE guardian_project_options)
target_include_directories(
    guardian_common
    INTERFACE
        "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>"
)
