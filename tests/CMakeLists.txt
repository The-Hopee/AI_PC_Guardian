add_executable(guardian-foundation-tests version_test.cpp)
target_link_libraries(guardian-foundation-tests PRIVATE Guardian::Common)

add_test(NAME guardian.version COMMAND guardian-foundation-tests)
