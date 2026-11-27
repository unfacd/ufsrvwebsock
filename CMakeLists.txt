# Build:  cmake --preset debug && cmake --build --preset debug
cmake_minimum_required(VERSION 3.18)

set(PROJECT_NAME "ufsrvwebsock")
#set(CMAKE_VERBOSE_MAKEFILE ON)

# project based cmake modules (use include(...module_file_name_))
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules)

MACRO(getVersionPatchLevel name)
    SET(VERSION_REGEX "^#define ${name} (.+)$")
    FILE(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/include/version.h"
            VERSION_BIT REGEX ${VERSION_REGEX})
    STRING(REGEX REPLACE ${VERSION_REGEX} "\\1" ${name} "${VERSION_BIT}")
ENDMACRO(getVersionPatchLevel)

getVersionPatchLevel(UFSRV_MAJOR)
getVersionPatchLevel(UFSRV_MINOR)
getVersionPatchLevel(UFSRV_PATCH)
SET(VERSION "${UFSRV_MAJOR}.${UFSRV_MINOR}.${UFSRV_PATCH}")
MESSAGE("Detected ufsrv version: ${VERSION}")

project(${PROJECT_NAME} LANGUAGES C CXX ASM)
project(${PROJECT_NAME} VERSION "${VERSION}")

# ── Shared ufsrv build options (canonical cmake-modules/ from ufnetcorelib) ─
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules)
set(UFSRV_C_WARNING_FLAGS
    -Wall -Wimplicit-fallthrough -Wno-unused-label -Wno-unused-variable
    -Wno-comment -Wno-nullability-completeness -Wno-unknown-attributes)
set(UFSRV_C_DEBUG_FLAGS_DEFAULT -ggdb3)
include(ufsrv_build_options)

option(UFSRV_ENABLE_TESTS "Enable unit tests via Google Test" OFF)

if(UFSRV_ENABLE_TESTS)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

option(UFSRV_CONFIG_COMPOSE
    "Enable the ufsrv_config composition targets (clone, generate, install, verify)" ON)

add_compile_definitions(
        HAVE_CONFIG_UFSRVWEBSOCK_H HAVE_CONFIG_UFSRVCORELIB_H HAVE_CONFIG_UFNETCORELIB_H
)
include(config_defaults_definitions)

# L3 implies L2, and either implies the base debug build switch.
option(UFSRVWEBSOCK_DEBUG_BUILD_L2 "Verbose level-2 debug logging" OFF)
option(UFSRVWEBSOCK_DEBUG_BUILD_L3 "Very verbose level-3 debug logging (per-object traces)" OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(UFSRVWEBSOCK_DEBUG_BUILD 1)
else()
    set(UFSRVWEBSOCK_DEBUG_BUILD 0)
endif()
if(UFSRVWEBSOCK_DEBUG_BUILD_L3)
    set(UFSRVWEBSOCK_DEBUG_L3 1)
    set(UFSRVWEBSOCK_DEBUG_L2 1)
else()
    set(UFSRVWEBSOCK_DEBUG_L3 0)
    if(UFSRVWEBSOCK_DEBUG_BUILD_L2)
        set(UFSRVWEBSOCK_DEBUG_L2 1)
    else()
        set(UFSRVWEBSOCK_DEBUG_L2 0)
    endif()
endif()
if(UFSRVWEBSOCK_DEBUG_L2 OR UFSRVWEBSOCK_DEBUG_L3)
    set(UFSRVWEBSOCK_DEBUG_BUILD 1)
endif()
add_compile_definitions(UFSRVWEBSOCK_DEBUG_BUILD=${UFSRVWEBSOCK_DEBUG_BUILD}
                        UFSRVWEBSOCK_DEBUG_BUILD_L2=${UFSRVWEBSOCK_DEBUG_L2}
                        UFSRVWEBSOCK_DEBUG_BUILD_L3=${UFSRVWEBSOCK_DEBUG_L3})

find_path(VALGRIND_INCLUDE_DIR valgrind/valgrind.h
    HINTS ${VALGRIND_ROOT}/include /opt/include)
include(CheckIncludeFile)
if(VALGRIND_INCLUDE_DIR)
    set(CMAKE_REQUIRED_INCLUDES "${VALGRIND_INCLUDE_DIR}")
endif()
check_include_file(valgrind/valgrind.h HAVE_VALGRIND)
check_include_file(valgrind/drd.h HAVE_VALGRIND_DRD)
check_include_file(valgrind/memcheck.h HAVE_VALGRIND_MEMCHECK)
unset(CMAKE_REQUIRED_INCLUDES)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/config_ufsrvwebsock.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/config_ufsrvwebsock.h
        @ONLY
)

# Generated config.h lands in the build root (${CMAKE_BINARY_DIR}); make it
# reachable to every target that does #include <config.h> under HAVE_CONFIG_H.
include_directories(${CMAKE_BINARY_DIR})

include(ufsrv_companion)

# ── IDE-only source indexing for system-installed companions ────────────────
# A system-installed companion contributes headers and a binary, never its .c
# files, so CLion has no definition to navigate to. Each companion's sibling
# checkout is registered under a throw-away OBJECT target that is never linked
# and never built — see cmake-modules/ufsrv_companion_ide_sources.cmake.
include(ufsrv_companion_ide_sources)
ufsrv_companion_ide_sources(uflib)
ufsrv_companion_ide_sources(ufsrvrxlib)
ufsrv_companion_ide_sources(ufsrvcorelib)
ufsrv_companion_ide_sources(ufnetcorelib)

if(UFSRV_ENABLE_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.0
    )
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_subdirectory(tests)
endif()

if(_have_companion_libs)
    add_subdirectory(src)
else()
    list(JOIN _missing_companion_libs ", " _missing_str)
    message(STATUS "ufsrvwebsock: ${_missing_str} did not resolve under provider '${UFSRV_COMPANION_PROVIDER}' — app target deferred.")
    if(_UFSRV_IN_SOURCE_DORMANT)
        message(WARNING
            "ufsrvwebsock: provider 'in-source' is dormant — no app target will be produced.\n"
            "  Reconfigure with -DUFSRV_COMPANION_PROVIDER=system-installed (the default).")
    else()
        message(STATUS "ufsrvwebsock: install the missing companion libraries into CMAKE_PREFIX_PATH, or use a preset that resolves them.")
    endif()
endif()