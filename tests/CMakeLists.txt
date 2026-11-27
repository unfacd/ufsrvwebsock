# Master test CMakeLists.txt for ufsrvwebsock

add_compile_definitions(HAVE_CONFIG_H)
add_compile_definitions(MCPC_C23PTCH_UCHAR2 MCPC_C23PTCH_KW1 is_unix)

# Shared main() for all test executables
add_library(gtest_shared_main OBJECT main.cpp)
target_link_libraries(gtest_shared_main PUBLIC GTest::gtest GTest::gtest_main)
ufsrv_apply_build_options(gtest_shared_main)

function(add_c_test)
    set(options "")
    set(oneValueArgs NAME TEST_SOURCE)
    set(multiValueArgs C_SOURCES INCLUDE_DIRS LIBRARIES)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${TEST_NAME}
        ${TEST_TEST_SOURCE}
        $<TARGET_OBJECTS:gtest_shared_main>
        ${TEST_C_SOURCES}
    )
    set_target_properties(${TEST_NAME} PROPERTIES LINKER_LANGUAGE CXX)
    target_include_directories(${TEST_NAME} PRIVATE ${TEST_INCLUDE_DIRS})
    target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest GTest::gtest_main stdc++ m ${TEST_LIBRARIES})
    ufsrv_apply_build_options(${TEST_NAME})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

add_subdirectory(ufsrv_core)

add_subdirectory(session_auth)
