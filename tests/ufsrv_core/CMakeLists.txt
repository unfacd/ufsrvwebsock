add_c_test(
    NAME ufsrv_core_smoke_tests
    TEST_SOURCE smoke_test.cpp
)

# Version accessor tests — exercise the app version.c generation pipeline.
# The generated version.c (src/version.c.sh -> <build>/src/version.c) is
# compiled directly into the test so the getter symbols resolve without
# pulling in the full ufsrvwebsock executable.
add_c_test(
    NAME ufsrv_core_version_tests
    TEST_SOURCE version_tests.cpp
    C_SOURCES ${PROJECT_BINARY_DIR}/src/version.c
    INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/include
)
if(TARGET ufsrvwebsock_version)
    add_dependencies(ufsrv_core_version_tests ufsrvwebsock_version)
endif()
