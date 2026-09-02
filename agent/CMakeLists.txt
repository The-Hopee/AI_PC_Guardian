add_library(
    guardian_agent_client STATIC
    src/telemetry_client.cpp
)
add_library(Guardian::AgentClient ALIAS guardian_agent_client)

target_include_directories(
    guardian_agent_client
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(
    guardian_agent_client
    PUBLIC
        Guardian::Common
        Guardian::Proto
        Guardian::Rpc
)

add_library(
    guardian_windows_collectors STATIC
    src/windows/system_metrics_collector.cpp
)
add_library(Guardian::WindowsCollectors ALIAS guardian_windows_collectors)

target_include_directories(
    guardian_windows_collectors
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(
    guardian_windows_collectors
    PUBLIC Guardian::Common
)

add_library(
    guardian_agent_runner STATIC
    src/telemetry_runner.cpp
    src/latest_snapshot_store.cpp
)
add_library(Guardian::AgentRunner ALIAS guardian_agent_runner)

target_include_directories(
    guardian_agent_runner
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(
    guardian_agent_runner
    PUBLIC
        Guardian::AgentClient
        Guardian::WindowsCollectors
    PRIVATE Guardian::Tools
)

add_library(
    guardian_agent_ipc_server STATIC
    src/local_ipc_server.cpp
)
add_library(Guardian::AgentIpcServer ALIAS guardian_agent_ipc_server)

target_include_directories(
    guardian_agent_ipc_server
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(
    guardian_agent_ipc_server
    PUBLIC Guardian::AgentRunner
    PRIVATE
        Advapi32
        Guardian::Ipc
        Guardian::Tools
        guardian_proto_messages
)

add_executable(guardian-agent src/main.cpp)
target_link_libraries(
    guardian-agent
    PRIVATE
        Guardian::AgentClient
        Guardian::AgentIpcServer
        Guardian::AgentRunner
        Guardian::Tools
        Guardian::WindowsCollectors
)

if(BUILD_TESTING)
    add_test(NAME guardian-agent.version COMMAND guardian-agent --version)
    set_tests_properties(
        guardian-agent.version
        PROPERTIES PASS_REGULAR_EXPRESSION "AI PC Guardian Agent v0\\.1"
    )

    add_test(NAME guardian-agent.help COMMAND guardian-agent --help)
    set_tests_properties(
        guardian-agent.help
        PROPERTIES PASS_REGULAR_EXPRESSION "Usage:.*guardian-agent"
    )
endif()
