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
)

if(NOT WIN32)
    add_executable(guardian-server src/main.cpp)
    target_link_libraries(guardian-server PRIVATE Guardian::ServerService)

    if(BUILD_TESTING)
        add_test(NAME guardian-server.starts COMMAND guardian-server)
        set_tests_properties(
            guardian-server.starts
            PROPERTIES PASS_REGULAR_EXPRESSION "AI PC Guardian Server v0\\.1"
        )
    endif()
endif()
