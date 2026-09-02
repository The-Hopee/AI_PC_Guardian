add_library(
    guardian_server_service STATIC
    src/telemetry_service.cpp
)
add_library(Guardian::ServerService ALIAS guardian_server_service)

target_include_directories(
    guardian_server_service
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(
    guardian_server_service
    PUBLIC
        Guardian::Proto
        Guardian::Rpc
    PRIVATE Guardian::Tools
)

add_executable(guardian-server src/main.cpp)
target_link_libraries(
    guardian-server
    PRIVATE
        Guardian::ServerService
        Guardian::Tools
)
