add_c_test(
    NAME session_auth_guard_tests
    TEST_SOURCE session_auth_guard_tests.cpp
    C_SOURCES session_auth_guard_fixture.c ${PROJECT_SOURCE_DIR}/src/session_routing/session_auth_guard.c
    INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/session_routing ${PROJECT_BINARY_DIR}
    LIBRARIES uflib::uflib PkgConfig::UTF8PROC pthread
)

add_c_test(
    NAME session_auth_lookup_tests
    TEST_SOURCE session_auth_lookup_tests.cpp
    C_SOURCES ${PROJECT_SOURCE_DIR}/src/session_routing/session_auth_guard.c
    INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/session_routing ${PROJECT_BINARY_DIR}
    LIBRARIES uflib::uflib PkgConfig::UTF8PROC pthread
)
target_link_options(session_auth_lookup_tests PRIVATE -Wl,--wrap=malloc)
