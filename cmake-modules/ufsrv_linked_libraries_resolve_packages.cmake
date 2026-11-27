include(${CMAKE_CURRENT_LIST_DIR}/ufsrv_linked_libraries_registry.cmake)

message(STATUS "Package resolution (DEV_ENVIRONMENT_AA=${DEV_ENVIRONMENT_AA}):")

# ── resolve_external_library (resolution + IMPORTED target only) ──
function(resolve_external_library lib_name)
    string(TOUPPER "${lib_name}" lib_upper)

    if(DEFINED UFSRV_LINKED_LIB_${lib_upper} AND
       UFSRV_LINKED_LIB_${lib_upper} STREQUAL "OPT")
        set(lib_path   "${OPT_PACKAGE_${lib_upper}}")
        set(lib_source "OPT")
    else()
        find_library(UFSRV_LIB_${lib_upper} ${lib_name} REQUIRED)
        set(lib_path   "${UFSRV_LIB_${lib_upper}}")
        set(lib_source "SYSTEM")
    endif()

    set(ver_info "")
    if(DEFINED UFSRV_LINKED_LIB_${lib_upper}_VER)
        set(ver_info " (expected v${UFSRV_LINKED_LIB_${lib_upper}_VER})")
    endif()

    message(STATUS "  [${lib_source}]    ${lib_name} → ${lib_path}${ver_info}")

    add_library(${lib_name} SHARED IMPORTED)
    set_target_properties(${lib_name} PROPERTIES IMPORTED_LOCATION "${lib_path}")
    set(${lib_name}_LIBRARY "${lib_path}" PARENT_SCOPE)
    set(${lib_name}_SOURCE "${lib_source}" PARENT_SCOPE)
endfunction()

foreach(lib ssl crypto curl jemalloc utf8proc json-c mysqlclient zkgroup)
    resolve_external_library(${lib})
endforeach()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA REQUIRED IMPORTED_TARGET lua)
add_library(lua INTERFACE IMPORTED)
set_target_properties(lua PROPERTIES INTERFACE_LINK_LIBRARIES PkgConfig::LUA)

# ── Registry integrity ──
if(DEV_ENVIRONMENT_AA)
    message(STATUS "")
    message(STATUS "══════════════════════════════════════════════════════")
    message(STATUS "  DEV_ENVIRONMENT_AA = ON")
    message(STATUS "  Registry: ufsrv_linked_libraries_registry.cmake")
    if(DEFINED UFSRV_REGISTRY_CHECKSUM)
        message(STATUS "  Checksum: ${UFSRV_REGISTRY_CHECKSUM}")
    endif()
    message(STATUS "══════════════════════════════════════════════════════")
else()
    # External user: ensure registry file exists
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/ufsrv_linked_libraries_registry.cmake")
        set(_msg "Registry file not found:\n"
                 "  ${CMAKE_CURRENT_LIST_DIR}/ufsrv_linked_libraries_registry.cmake\n"
                 "  This file is required for library resolution.\n")
        set(_msg "${_msg}  If you have access to the ansible provisioning environment,\n")
        set(_msg "${_msg}  re-run cmake with:\n")
        set(_msg "${_msg}    -DDEV_ENVIRONMENT_AA=ON -DUFSRV_REGEN_LINKED_LIBS=ON\n")
        set(_msg "${_msg}  to see the regeneration procedure.")
        message(FATAL_ERROR "${_msg}")
    endif()
endif()

# ── Integrity check ──
if(DEV_ENVIRONMENT_AA AND UFSRV_CHECK_LINKED_LIBS)
    message(STATUS "")
    message(STATUS "── Registry integrity check ──")

    if(NOT DEFINED UFSRV_REGISTRY_CHECKSUM)
        set(_msg "  No checksum found in registry file.\n"
                 "  The file may have been hand-edited or is missing its generation stamp.\n")
        if(UFSRV_REGEN_LINKED_LIBS)
            set(_msg "${_msg}\n  To regenerate:\n"
                     "    cd <ansible_driven_dockerisation>\n"
                     "    ansible-playbook -i localhost, -c local provision_ufsrv.yml \\\n"
                     "      -e target_service=<type> --tags registry\n")
        else()
            set(_msg "${_msg}  Re-run ansible provision, or add -DUFSRV_REGEN_LINKED_LIBS=ON\n"
                     "  to see the regeneration procedure.\n")
        endif()
        message(FATAL_ERROR "${_msg}")
    endif()

    # Recompute hash of set() lines (excluding the checksum line itself)
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/ufsrv_linked_libraries_registry.cmake" _reg_lines)
    set(_reg_set_lines "")
    foreach(_line ${_reg_lines})
        if(_line MATCHES "^set\\(UFSRV_LINKED_LIB" OR _line MATCHES "^set\\(OPT_PACKAGE")
            set(_reg_set_lines "${_reg_set_lines}${_line}\n")
        endif()
    endforeach()
    string(SHA256 _computed_hash "${_reg_set_lines}")

    if(_computed_hash STREQUAL UFSRV_REGISTRY_CHECKSUM)
        message(STATUS "  ✓ Integrity verified (${_computed_hash})")
    else()
        set(_msg "  ✗ Registry integrity check FAILED.\n"
                 "    Stored:   ${UFSRV_REGISTRY_CHECKSUM}\n"
                 "    Computed: ${_computed_hash}\n"
                 "  The registry file has been modified since ansible generation.\n")
        if(UFSRV_REGEN_LINKED_LIBS)
            set(_msg "${_msg}\n"
                     "  To regenerate the registry from the ansible source of truth:\n"
                     "    cd ansible_driven_dockerisation\n"
                     "    ansible-playbook -i localhost, -c local provision_ufsrv.yml \\\n"
                     "      -e target_service=<type> --tags registry\n")
        else()
            set(_msg "${_msg}  Re-run ansible provision, or add -DUFSRV_REGEN_LINKED_LIBS=ON\n"
                     "  to see the regeneration procedure.\n")
        endif()
        message(FATAL_ERROR "${_msg}")
    endif()
endif()
