add_executable(guardian-foundation-tests version_test.cpp)
target_link_libraries(guardian-foundation-tests PRIVATE Guardian::Common)

add_test(NAME guardian.version COMMAND guardian-foundation-tests)

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
