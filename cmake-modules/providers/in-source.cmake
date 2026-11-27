set(UFSRV_REPO_UFLIB "git.ro:devops/uflib" CACHE STRING "git repository for uflib (standalone mode)")
set(UFSRV_REPO_UFSRVRXLIB "git.ro:devops/ufsrvrxlib" CACHE STRING "git repository for ufsrvrxlib (standalone mode)")
set(UFSRV_REPO_UFSRVCORELIB "git.ro:devops/ufsrvcorelib" CACHE STRING "git repository for ufsrvcorelib (standalone mode)")
set(UFSRV_REPO_UFNETCORELIB "git.ro:devops/ufnetcorelib" CACHE STRING "git repository for ufnetcorelib (standalone mode)")
set(UFSRV_REF_UFLIB "master" CACHE STRING "git ref for uflib (standalone mode)")
set(UFSRV_REF_UFSRVRXLIB "master" CACHE STRING "git ref for ufsrvrxlib (standalone mode)")
set(UFSRV_REF_UFSRVCORELIB "master" CACHE STRING "git ref for ufsrvcorelib (standalone mode)")
set(UFSRV_REF_UFNETCORELIB "master" CACHE STRING "git ref for ufnetcorelib (standalone mode)")

option(UFSRV_STANDALONE_MODE
    "Self-sufficient build: resolve companion libraries only from packages/ (overrides system installs)"
    OFF)

add_custom_target(standalone
    COMMAND ${CMAKE_COMMAND}
        "-DUFSRV_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DUFSRV_REPO_UFLIB=${UFSRV_REPO_UFLIB}"
        "-DUFSRV_REPO_UFSRVRXLIB=${UFSRV_REPO_UFSRVRXLIB}"
        "-DUFSRV_REPO_UFSRVCORELIB=${UFSRV_REPO_UFSRVCORELIB}"
        "-DUFSRV_REPO_UFNETCORELIB=${UFSRV_REPO_UFNETCORELIB}"
        "-DUFSRV_REF_UFLIB=${UFSRV_REF_UFLIB}"
        "-DUFSRV_REF_UFSRVRXLIB=${UFSRV_REF_UFSRVRXLIB}"
        "-DUFSRV_REF_UFSRVCORELIB=${UFSRV_REF_UFSRVCORELIB}"
        "-DUFSRV_REF_UFNETCORELIB=${UFSRV_REF_UFNETCORELIB}"
        -P "${CMAKE_SOURCE_DIR}/cmake-modules/ufsrv_standalone_fetch.cmake"
    COMMENT "Fetch companion libraries (uflib, ufsrvrxlib, ufsrvcorelib, ufnetcorelib) into packages/"
    VERBATIM)

set(_have_companion_libs TRUE)
set(_missing_companion_libs "")

if(UFSRV_STANDALONE_MODE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/uflib/CMakeLists.txt")
        set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(packages/uflib ${CMAKE_BINARY_DIR}/packages/uflib)
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

if(UFSRV_STANDALONE_MODE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/ufsrvrxlib/CMakeLists.txt")
        set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(packages/ufsrvrxlib ${CMAKE_BINARY_DIR}/packages/ufsrvrxlib)
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

if(UFSRV_STANDALONE_MODE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/ufsrvcorelib/CMakeLists.txt")
        set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(packages/ufsrvcorelib ${CMAKE_BINARY_DIR}/packages/ufsrvcorelib)
        message(STATUS "ufsrvcorelib: standalone (packages/ subtree — OBJECT lib compiled into the app)")
    else()
        set(_have_companion_libs FALSE)
        list(APPEND _missing_companion_libs ufsrvcorelib)
        message(STATUS "ufsrvcorelib: ABSENT from packages/ — 'standalone' target will fetch it")
    endif()
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/ufsrvcorelib/CMakeLists.txt")
    set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(packages/ufsrvcorelib ${CMAKE_BINARY_DIR}/packages/ufsrvcorelib)
    message(STATUS "ufsrvcorelib: in-source (packages/ subtree — OBJECT lib compiled into the app)")
else()
    set(_have_companion_libs FALSE)
    list(APPEND _missing_companion_libs ufsrvcorelib)
    set(_UFSRV_IN_SOURCE_DORMANT TRUE)
    message(WARNING
        "ufsrv_companion: provider 'in-source' is not operational in this tree — "
        "packages/ufsrvcorelib/ is not present.\n"
        "  The in-source wiring is retained for a future restored subtree; configure with "
        "-DUFSRV_COMPANION_PROVIDER=system-installed (the default) to build.\n"
        "  To restore the subtree: git subtree add --prefix=packages/ufsrvcorelib "
        "ufsrvcorelib master --squash\n"
        "  Or populate packages/ with: -DUFSRV_STANDALONE_MODE=ON, then "
        "--target standalone.")
endif()

if(UFSRV_STANDALONE_MODE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/ufnetcorelib/CMakeLists.txt")
        set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(packages/ufnetcorelib ${CMAKE_BINARY_DIR}/packages/ufnetcorelib)
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
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/ufnetcorelib/CMakeLists.txt")
    set(_PACKAGE_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(packages/ufnetcorelib ${CMAKE_BINARY_DIR}/packages/ufnetcorelib)
    target_include_directories(ufnetcorelib BEFORE PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
        "${CMAKE_SOURCE_DIR}/packages/ufsrvcorelib/include"
        "${CMAKE_SOURCE_DIR}/packages/ufsrvcorelib/include/ufsrvcorelib")
    message(STATUS "ufnetcorelib: in-source (packages/ subtree — OBJECT lib compiled into the app)")
else()
    set(_have_companion_libs FALSE)
    list(APPEND _missing_companion_libs ufnetcorelib)
    set(_UFSRV_IN_SOURCE_DORMANT TRUE)
    message(WARNING
        "ufsrv_companion: provider 'in-source' is not operational in this tree — "
        "packages/ufnetcorelib/ is not present.\n"
        "  The in-source wiring is retained for a future restored subtree; configure with "
        "-DUFSRV_COMPANION_PROVIDER=system-installed (the default) to build.\n"
        "  To restore the subtree: git subtree add --prefix=packages/ufnetcorelib "
        "ufnetcorelib master --squash\n"
        "  Or populate packages/ with: -DUFSRV_STANDALONE_MODE=ON, then "
        "--target standalone.")
endif()
