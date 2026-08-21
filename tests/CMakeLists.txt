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
