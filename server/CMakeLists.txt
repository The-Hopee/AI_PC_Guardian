add_executable(guardian-server src/main.cpp)
target_link_libraries(guardian-server PRIVATE Guardian::Common)

if(BUILD_TESTING)
    add_test(NAME guardian-server.starts COMMAND guardian-server)
    set_tests_properties(
        guardian-server.starts
        PROPERTIES PASS_REGULAR_EXPRESSION "AI PC Guardian Server v0\\.1"
    )
endif()
