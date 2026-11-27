#
# cmake-modules/ufsrv_companion_libraries.cmake
#
# Companion-library resolution (uflib, ufsrvcorelib, ufnetcorelib) and the
# explicit 'standalone' fetch target.
#
# Modes:
#   UFSRV_STANDALONE_MODE=OFF (default) — system install or in-source subtree.
#   UFSRV_STANDALONE_MODE=ON           — resolve ONLY from packages/ (self-sufficient);
#                                        any system-installed companion library is
#                                        ignored in favour of packages/.
#
# _have_companion_libs gates the app target: any companion library that fails to
# resolve clears it.  In standalone mode a missing packages/<lib> defers (rather
# than erroring) so `cmake --build . --target standalone` can populate it.
#

# ── Standalone fetch target (explicit; idempotent) ─────────────────────────────
set(UFSRV_REPO_UFLIB "git.ro:uflib" CACHE STRING "git repository for uflib (standalone mode)")
set(UFSRV_REPO_UFSRVRXLIB "git.ro:ufsrvrxlib" CACHE STRING "git repository for ufsrvrxlib (standalone mode)")
set(UFSRV_REPO_UFSRVCORELIB "git.ro:ufsrvcorelib" CACHE STRING "git repository for ufsrvcorelib (standalone mode)")
set(UFSRV_REPO_UFNETCORELIB "git.ro:ufnetcorelib" CACHE STRING "git repository for ufnetcorelib (standalone mode)")
set(UFSRV_REF_UFLIB "master" CACHE STRING "git ref for uflib (standalone mode)")
set(UFSRV_REF_UFSRVCORELIB "master" CACHE STRING "git ref for ufsrvcorelib (standalone mode)")
set(UFSRV_REF_UFNETCORELIB "master" CACHE STRING "git ref for ufnetcorelib (standalone mode)")

add_custom_target(standalone
    COMMAND ${CMAKE_COMMAND}
        "-DUFSRV_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DUFSRV_REPO_UFLIB=${UFSRV_REPO_UFLIB}"
        "-DUFSRV_REPO_UFSRVRXLIB=${UFSRV_REPO_UFSRVRXLIB}"
        "-DUFSRV_REPO_UFSRVCORELIB=${UFSRV_REPO_UFSRVCORELIB}"
        "-DUFSRV_REPO_UFNETCORELIB=${UFSRV_REPO_UFNETCORELIB}"
        "-DUFSRV_REF_UFLIB=${UFSRV_REF_UFLIB}"
        "-DUFSRV_REF_UFSRVCORELIB=${UFSRV_REF_UFSRVCORELIB}"
        "-DUFSRV_REF_UFNETCORELIB=${UFSRV_REF_UFNETCORELIB}"
        -P "${CMAKE_SOURCE_DIR}/cmake-modules/ufsrv_standalone_fetch.cmake"
    COMMENT "Fetch companion libraries (uflib, ufsrvcorelib, ufnetcorelib) into packages/"
    VERBATIM)

option(UFSRV_STANDALONE_MODE
    "Self-sufficient build: resolve companion libraries only from packages/ (overrides system installs)"
    OFF)

# Tracks whether all companion libs resolved; the app target is gated on this.
set(_have_companion_libs TRUE)
set(_missing_companion_libs "")

# ── uflib ──────────────────────────────────────────────────────────────────────
if(UFSRV_STANDALONE_MODE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/uflib/CMakeLists.txt")
        set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(packages/uflib)
        message(STATUS "uflib: standalone (packages/ subtree — OBJECT lib compiled into the app)")
    else()
        set(_have_companion_libs FALSE)
        list(APPEND _missing_companion_libs uflib)
        message(STATUS "uflib: ABSENT from packages/ — 'standalone' target will fetch it")
    endif()
else()
    find_package(uflib REQUIRED)
    message(STATUS "Found uflib ${uflib_VERSION} at ${uflib_DIR}")
endif()

# ── ufsrvrxlib ──────────────────────────────────────────────────────────────────────
if(UFSRV_STANDALONE_MODE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/ufsrvrxlib/CMakeLists.txt")
        set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(packages/ufsrvrxlib)
        message(STATUS "ufsrvrxlib: standalone (packages/ subtree — OBJECT lib compiled into the app)")
    else()
        set(_have_companion_libs FALSE)
        list(APPEND _missing_companion_libs ufsrvrxlib)
        message(STATUS "ufsrvrxlib: ABSENT from packages/ — 'standalone' target will fetch it")
    endif()
else()
    find_package(ufsrvrxlib REQUIRED)
    message(STATUS "Found ufsrvrxlib ${ufsrvrxlib_VERSION} at ${ufsrvrxlib_DIR}")
endif()

# ── ufsrvcorelib ───────────────────────────────────────────────────────────────
option(UFSRVCORELIB_USE_SYSTEM "Link against system-installed ufsrvcorelib" OFF)
if(UFSRV_STANDALONE_MODE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/ufsrvcorelib/CMakeLists.txt")
        set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(packages/ufsrvcorelib)
        message(STATUS "ufsrvcorelib: standalone (packages/ subtree — OBJECT lib compiled into the app)")
    else()
        set(_have_companion_libs FALSE)
        list(APPEND _missing_companion_libs ufsrvcorelib)
        message(STATUS "ufsrvcorelib: ABSENT from packages/ — 'standalone' target will fetch it")
    endif()
elseif(UFSRVCORELIB_USE_SYSTEM)
    find_package(ufsrvcorelib REQUIRED)
    message(STATUS "ufsrvcorelib: system install (${ufsrvcorelib_VERSION} at ${ufsrvcorelib_DIR})")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/ufsrvcorelib/CMakeLists.txt")
    set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(packages/ufsrvcorelib)
    message(STATUS "ufsrvcorelib: in-source (packages/ subtree — OBJECT lib compiled into the app)")
else()
    message(FATAL_ERROR
        "ufsrvcorelib not found at packages/ufsrvcorelib/ (in-source requested, UFSRVCORELIB_USE_SYSTEM=OFF).\n"
        "  To fix:  git subtree add --prefix=packages/ufsrvcorelib ufsrvcorelib master --squash\n"
        "  Or set -DUFSRVCORELIB_USE_SYSTEM=ON to link against a system install.\n"
        "  Or set -DUFSRV_STANDALONE_MODE=ON for a self-sufficient packages/ build.")
endif()

# ── ufnetcorelib ───────────────────────────────────────────────────────────────
option(UFNETCORELIB_USE_SYSTEM "Link against system-installed ufnetcorelib" OFF)
if(UFSRV_STANDALONE_MODE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/ufnetcorelib/CMakeLists.txt")
        set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(packages/ufnetcorelib)
        # ufnetcorelib includes ufsrvcorelib type headers but doesn't link against it.
        # src/include is listed first so the composed <ufsrvcorelib/.../ufsrv_config/*>
        # shim + generated headers resolve to the LOCAL (composed) tree, shadowing the
        # subtree's committed N=1 generated copies; canonical type headers fall through
        # to packages/ufsrvcorelib/include.
        target_include_directories(ufnetcorelib BEFORE PRIVATE
            "${CMAKE_SOURCE_DIR}/src/include"
            "${CMAKE_SOURCE_DIR}/packages/ufsrvcorelib/include"
            "${CMAKE_SOURCE_DIR}/packages/ufsrvcorelib/include/ufsrvcorelib")
        message(STATUS "ufnetcorelib: standalone (packages/ subtree — OBJECT lib compiled into the app)")
    else()
        set(_have_companion_libs FALSE)
        list(APPEND _missing_companion_libs ufnetcorelib)
        message(STATUS "ufnetcorelib: ABSENT from packages/ — 'standalone' target will fetch it")
    endif()
elseif(UFNETCORELIB_USE_SYSTEM)
    find_package(ufnetcorelib REQUIRED)
    message(STATUS "ufnetcorelib: system install (${ufnetcorelib_VERSION} at ${ufnetcorelib_DIR})")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/ufnetcorelib/CMakeLists.txt")
    set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(packages/ufnetcorelib)
    # ufnetcorelib includes ufsrvcorelib type headers but doesn't link against it.
    # src/include is listed first so the composed <ufsrvcorelib/.../ufsrv_config/*>
    # shim + generated headers resolve to the LOCAL (composed) tree, shadowing the
    # subtree's committed N=1 generated copies; canonical type headers fall through
    # to packages/ufsrvcorelib/include.
    target_include_directories(ufnetcorelib BEFORE PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
        "${CMAKE_SOURCE_DIR}/packages/ufsrvcorelib/include"
        "${CMAKE_SOURCE_DIR}/packages/ufsrvcorelib/include/ufsrvcorelib")
    message(STATUS "ufnetcorelib: in-source (packages/ subtree — OBJECT lib compiled into the app)")
else()
    message(FATAL_ERROR
        "ufnetcorelib not found at packages/ufnetcorelib/ (in-source requested, UFNETCORELIB_USE_SYSTEM=OFF).\n"
        "  To fix:  git subtree add --prefix=packages/ufnetcorelib ufnetcorelib master --squash\n"
        "  Or set -DUFNETCORELIB_USE_SYSTEM=ON to link against a system install.\n"
        "  Or set -DUFSRV_STANDALONE_MODE=ON for a self-sufficient packages/ build.")
endif()
