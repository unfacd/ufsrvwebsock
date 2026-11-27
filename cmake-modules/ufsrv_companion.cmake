set(UFSRV_COMPANION_PROVIDER "system-installed" CACHE STRING
    "Companion library resolution provider: system-installed (find_package) or in-source (packages/ subtrees)")
set_property(CACHE UFSRV_COMPANION_PROVIDER PROPERTY STRINGS "in-source" "system-installed")

if(NOT UFSRV_COMPANION_PROVIDER STREQUAL "in-source" AND
   NOT UFSRV_COMPANION_PROVIDER STREQUAL "system-installed")
    message(FATAL_ERROR
        "ufsrv_companion: unknown UFSRV_COMPANION_PROVIDER='${UFSRV_COMPANION_PROVIDER}'.\n"
        "  Valid values: in-source, system-installed")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/providers/${UFSRV_COMPANION_PROVIDER}.cmake")
    message(FATAL_ERROR
        "ufsrv_companion: provider '${UFSRV_COMPANION_PROVIDER}' is not implemented.\n"
        "  Missing: ${CMAKE_CURRENT_LIST_DIR}/providers/${UFSRV_COMPANION_PROVIDER}.cmake")
endif()

if(DEFINED UFSRV_COMPANION_PROVIDER_LOCK AND
   NOT UFSRV_COMPANION_PROVIDER STREQUAL UFSRV_COMPANION_PROVIDER_LOCK)
    message(FATAL_ERROR
        "ufsrv_companion: UFSRV_COMPANION_PROVIDER changed from '${UFSRV_COMPANION_PROVIDER_LOCK}' "
        "to '${UFSRV_COMPANION_PROVIDER}' in an existing build directory.\n"
        "  Configure into a fresh build directory instead.")
endif()
set(UFSRV_COMPANION_PROVIDER_LOCK "${UFSRV_COMPANION_PROVIDER}" CACHE INTERNAL
    "Provider the build directory was configured with")

include("${CMAKE_CURRENT_LIST_DIR}/providers/${UFSRV_COMPANION_PROVIDER}.cmake")

if(_have_companion_libs)
    add_library(ufsrv_companion INTERFACE)
    target_link_libraries(ufsrv_companion INTERFACE
        uflib::uflib
        ufsrvrxlib::ufsrvrxlib
        ufnetcorelib::ufnetcorelib
        ufsrvcorelib::ufsrvcorelib)

    set(_ufsrv_companion_objects "")
    foreach(_lib IN ITEMS ufnetcorelib ufsrvcorelib ufsrvrxlib)
        if(TARGET ${_lib})
            get_target_property(_lib_type ${_lib} TYPE)
            if(_lib_type STREQUAL "OBJECT_LIBRARY")
                list(APPEND _ufsrv_companion_objects $<TARGET_OBJECTS:${_lib}>)
            endif()
        endif()
    endforeach()
    if(_ufsrv_companion_objects)
        target_sources(ufsrv_companion INTERFACE ${_ufsrv_companion_objects})
    endif()

    foreach(_vendored IN ITEMS hiredis curve25519)
        if(TARGET ${_vendored})
            target_link_libraries(ufsrv_companion INTERFACE ${_vendored})
        endif()
    endforeach()

    add_library(ufsrv::companion ALIAS ufsrv_companion)
endif()
