#
# cmake-modules/ufsrv_standalone_fetch.cmake
#
# Standalone dependency fetch — invoked via:
#   cmake --build <build> --target standalone
#
# Clones the three companion libraries into packages/<lib> so the host can
# build self-sufficiently when the libs are neither system-installed nor
# present as git subtrees.  Idempotent and non-destructive: a library is
# skipped when its CMakeLists.txt already exists (never clobbered).  This is the only
# place that fetches, and it runs only when the target is invoked — no
# implicit detection logic.
#
# Overrides (passed via -D from the 'standalone' target, or set at -P time):
#   UFSRV_SOURCE_DIR               (required) host repo root
#   UFSRV_REPO_UFLIB / _UFSRVCORELIB / _UFNETCORELIB
#   UFSRV_REF_UFLIB  / _UFSRVCORELIB / _UFNETCORELIB
#

cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED UFSRV_SOURCE_DIR)
    message(FATAL_ERROR "ufsrv_standalone_fetch: UFSRV_SOURCE_DIR must be passed via -D")
endif()

# ── Defaults (central git.ro alias; overridable) ───────────────────────────────
if(NOT DEFINED UFSRV_REPO_UFLIB)
    set(UFSRV_REPO_UFLIB "git.ro:devops/uflib")
endif()
if(NOT DEFINED UFSRV_REPO_UFSRVRXLIB)
    set(UFSRV_REPO_UFSRVRXLIB "git.ro:devops/ufsrvrxlib")
endif()
if(NOT DEFINED UFSRV_REPO_UFSRVCORELIB)
    set(UFSRV_REPO_UFSRVCORELIB "git.ro:devops/ufsrvcorelib")
endif()
if(NOT DEFINED UFSRV_REPO_UFNETCORELIB)
    set(UFSRV_REPO_UFNETCORELIB "git.ro:devops/ufnetcorelib")
endif()
if(NOT DEFINED UFSRV_REF_UFLIB)
    set(UFSRV_REF_UFLIB "master")
endif()
if(NOT DEFINED UFSRV_REF_UFSRVRXLIB)
    set(UFSRV_REF_UFSRVRXLIB "master")
endif()
if(NOT DEFINED UFSRV_REF_UFSRVCORELIB)
    set(UFSRV_REF_UFSRVCORELIB "master")
endif()
if(NOT DEFINED UFSRV_REF_UFNETCORELIB)
    set(UFSRV_REF_UFNETCORELIB "master")
endif()

# ── Clone each library (idempotent, non-destructive) ───────────────────────────
foreach(_lib UFLIB UFSRVRXLIB UFSRVCORELIB UFNETCORELIB)
    string(TOLOWER "${_lib}" _lib_lower)
    set(_dst  "${UFSRV_SOURCE_DIR}/packages/${_lib_lower}")
    set(_repo "${UFSRV_REPO_${_lib}}")
    set(_ref  "${UFSRV_REF_${_lib}}")

    if(EXISTS "${_dst}/CMakeLists.txt")
        message(STATUS "standalone: ${_lib_lower} already present at packages/${_lib_lower} (skipping)")
        continue()
    endif()

    if("${_ref}" STREQUAL "")
        set(_branch_args)
    else()
        set(_branch_args "--branch=${_ref}")
    endif()

    message(STATUS "standalone: fetching ${_lib_lower} <- ${_repo} (${_ref})")
    execute_process(
        COMMAND git clone --depth=1 ${_branch_args} "${_repo}" "${_dst}"
        RESULT_VARIABLE _res
        TIMEOUT 600)
    if(NOT _res EQUAL 0)
        message(FATAL_ERROR "standalone: clone failed (${_lib_lower}) from ${_repo} — result ${_res}")
    endif()
    message(STATUS "standalone: fetched ${_lib_lower} -> packages/${_lib_lower}")
endforeach()

message(STATUS "standalone: done — re-run cmake configure to build against packages/")
