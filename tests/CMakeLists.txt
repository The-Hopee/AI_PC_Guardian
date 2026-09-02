add_executable(guardian-foundation-tests version_test.cpp)
target_link_libraries(guardian-foundation-tests PRIVATE Guardian::Common)

add_test(NAME guardian.version COMMAND guardian-foundation-tests)

add_executable(
    guardian-tools-logger-tests
    tools/logger_test.cpp
)
target_link_libraries(
    guardian-tools-logger-tests
    PRIVATE Guardian::Tools
)

add_test(
    NAME guardian.tools-logger
    COMMAND guardian-tools-logger-tests
)

add_executable(
    guardian-model-tests
    common/telemetry_event_test.cpp
)
target_link_libraries(guardian-model-tests PRIVATE Guardian::Common)

add_test(NAME guardian.telemetry-model COMMAND guardian-model-tests)

add_executable(
    guardian-proto-tests
    proto/telemetry_conversion_test.cpp
)
target_link_libraries(guardian-proto-tests PRIVATE Guardian::Proto)

add_test(NAME guardian.telemetry-conversion COMMAND guardian-proto-tests)

add_executable(
    guardian-local-agent-proto-tests
    proto/local_agent_proto_test.cpp
)
target_link_libraries(
    guardian-local-agent-proto-tests
    PRIVATE guardian_proto_messages
)

add_test(
    NAME guardian.local-agent-proto
    COMMAND guardian-local-agent-proto-tests
)

add_executable(
    guardian-ipc-framing-tests
    ipc/framing_test.cpp
)
target_link_libraries(
    guardian-ipc-framing-tests
    PRIVATE Guardian::Ipc
)

add_test(
    NAME guardian.ipc-framing
    COMMAND guardian-ipc-framing-tests
)

add_executable(
    guardian-server-service-tests
    server/telemetry_service_test.cpp
)
target_link_libraries(
    guardian-server-service-tests
    PRIVATE Guardian::ServerService
)

add_test(
    NAME guardian.telemetry-service
    COMMAND guardian-server-service-tests
)

add_executable(
    guardian-server-integration-tests
    server/telemetry_service_integration_test.cpp
)
target_link_libraries(
    guardian-server-integration-tests
    PRIVATE Guardian::ServerService
)

add_test(
    NAME guardian.telemetry-service-network
    COMMAND guardian-server-integration-tests
)

if(WIN32)
    add_executable(
        guardian-agent-client-tests
        agent/telemetry_client_test.cpp
    )
    target_link_libraries(
        guardian-agent-client-tests
        PRIVATE Guardian::AgentClient
    )

    add_test(
        NAME guardian.agent-client
        COMMAND guardian-agent-client-tests
    )

    add_executable(
        guardian-windows-collectors-tests
        agent/system_metrics_collector_test.cpp
    )
    target_link_libraries(
        guardian-windows-collectors-tests
        PRIVATE Guardian::WindowsCollectors
    )

    add_test(
        NAME guardian.windows-collectors
        COMMAND guardian-windows-collectors-tests
    )

    add_executable(
        guardian-agent-runner-tests
        agent/telemetry_runner_test.cpp
    )
    target_link_libraries(
        guardian-agent-runner-tests
        PRIVATE Guardian::AgentRunner
    )

    add_test(
        NAME guardian.agent-runner
        COMMAND guardian-agent-runner-tests
    )

    add_executable(
        guardian-latest-snapshot-store-tests
        agent/latest_snapshot_store_test.cpp
    )
    target_link_libraries(
        guardian-latest-snapshot-store-tests
        PRIVATE Guardian::AgentRunner
    )

    add_test(
        NAME guardian.latest-snapshot-store
        COMMAND guardian-latest-snapshot-store-tests
    )

    add_executable(
        guardian-agent-ipc-server-tests
        agent/local_ipc_server_test.cpp
    )
    target_link_libraries(
        guardian-agent-ipc-server-tests
        PRIVATE
            Guardian::AgentIpcServer
            Guardian::Ipc
            guardian_proto_messages
    )

    add_test(
        NAME guardian.agent-ipc-server
        COMMAND guardian-agent-ipc-server-tests
    )

    if(TARGET guardian-desktop)
        add_executable(
            guardian-desktop-ipc-tests
            desktop/local_agent_client_test.cpp
            ${PROJECT_SOURCE_DIR}/desktop/src/local_agent_client.cpp
            ${PROJECT_SOURCE_DIR}/desktop/src/system_metrics_view_model.cpp
            ${PROJECT_SOURCE_DIR}/desktop/include/guardian/desktop/local_agent_client.hpp
            ${PROJECT_SOURCE_DIR}/desktop/include/guardian/desktop/system_metrics_view_model.hpp
        )
        set_target_properties(
            guardian-desktop-ipc-tests
            PROPERTIES AUTOMOC ON
        )
        target_include_directories(
            guardian-desktop-ipc-tests
            PRIVATE ${PROJECT_SOURCE_DIR}/desktop/include
        )
        target_link_libraries(
            guardian-desktop-ipc-tests
            PRIVATE
                Guardian::AgentIpcServer
                Guardian::Common
                Guardian::Ipc
                Qt6::Core
                Qt6::Network
                Qt6::Qml
                guardian_proto_messages
        )

        add_test(
            NAME guardian.desktop-ipc-client
            COMMAND guardian-desktop-ipc-tests
        )
        set_tests_properties(
            guardian.desktop-ipc-client
            PROPERTIES
                ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
endif()

add_test(NAME guardian-server.version COMMAND guardian-server --version)
set_tests_properties(
    guardian-server.version
    PROPERTIES PASS_REGULAR_EXPRESSION "AI PC Guardian Server v0\\.1"
)

add_test(NAME guardian-server.help COMMAND guardian-server --help)
set_tests_properties(
    guardian-server.help
    PROPERTIES PASS_REGULAR_EXPRESSION "Usage:.*guardian-server"
)
