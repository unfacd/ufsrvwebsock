set(_ufsrv_companion_present "")
foreach(_lib IN ITEMS uflib ufsrvrxlib ufsrvcorelib ufnetcorelib)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/${_lib}/CMakeLists.txt")
        list(APPEND _ufsrv_companion_present "packages/${_lib}")
    endif()
endforeach()
if(_ufsrv_companion_present)
    message(FATAL_ERROR
        "ufsrv_companion: provider 'system-installed' with in-source copies present:\n"
        "  ${_ufsrv_companion_present}\n"
        "  Remove them, or configure with -DUFSRV_COMPANION_PROVIDER=in-source.")
endif()

find_package(uflib CONFIG REQUIRED)
find_package(ufsrvrxlib CONFIG REQUIRED)
find_package(ufsrvcorelib CONFIG REQUIRED)
find_package(ufnetcorelib CONFIG REQUIRED)

# Which flavour of the companion libraries to link. The packages export both as
# separate targets (_static / _shared), and until now this module always took the
# static one first — so installing a shared library changed nothing here. Shared
# is preferred because it is the only way two components that both embed uflib
# state can be avoided in one process; -DUFSRV_COMPANION_LINK_SHARED=OFF restores
# the old behaviour for a static-only prefix.
option(UFSRV_COMPANION_LINK_SHARED
    "Link the companion libraries as shared objects when the package exports one"
    ON)

foreach(_lib IN ITEMS uflib ufsrvrxlib ufsrvcorelib ufnetcorelib)
    if(NOT TARGET ${_lib}::${_lib})
        set(_ufsrv_companion_suffix "")
        if(UFSRV_COMPANION_LINK_SHARED AND TARGET ${_lib}::${_lib}_shared)
            set(_ufsrv_companion_suffix "_shared")
        elseif(TARGET ${_lib}::${_lib}_static)
            set(_ufsrv_companion_suffix "_static")
        elseif(TARGET ${_lib}::${_lib}_shared)
            set(_ufsrv_companion_suffix "_shared")
        else()
            message(FATAL_ERROR
                "ufsrv_companion: ${_lib} package exports no ${_lib}::${_lib} target "
                "(looked for ${_lib}::${_lib}_static and ${_lib}::${_lib}_shared).")
        endif()
        add_library(${_lib}::${_lib} INTERFACE IMPORTED)
        target_link_libraries(${_lib}::${_lib} INTERFACE ${_lib}::${_lib}${_ufsrv_companion_suffix})
        message(STATUS "${_lib}: linking ${_lib}${_ufsrv_companion_suffix}")
    endif()
    message(STATUS "${_lib}: system install (${${_lib}_VERSION} at ${${_lib}_DIR})")
endforeach()

set(_have_companion_libs TRUE)
set(_missing_companion_libs "")
