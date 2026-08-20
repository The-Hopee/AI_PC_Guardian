add_executable(guardian-agent src/main.cpp)
target_link_libraries(guardian-agent PRIVATE Guardian::Common)

if(BUILD_TESTING)
    add_test(NAME guardian-agent.starts COMMAND guardian-agent)
    set_tests_properties(
        guardian-agent.starts
        PROPERTIES PASS_REGULAR_EXPRESSION "AI PC Guardian Agent v0\\.1"
    )
endif()
