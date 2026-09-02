add_library(
    guardian_ipc STATIC
    src/framing.cpp
)
add_library(Guardian::Ipc ALIAS guardian_ipc)

target_include_directories(
    guardian_ipc
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(
    guardian_ipc
    PUBLIC guardian_project_options
)
