# ── ufsrvwebsock executable ───────────────────────────────────────────────────
add_executable(ufsrvwebsock
    # ── top-level src/ ──────────────────────────────────────────────────
    main.c
    utils_console_handlers.c
    misc/ufsrv_linked_libs.c
    misc/ufsrv_version_self.c
    protocol_websocket_io.c
    protocol_websocket_routines.c
    protocol_websocket_session.c
    protocol_websockets.c
    scheduled_job_fences.c
    session_routing/session_auth_routing.c
    session_routing/session_auth_guard.c
    staged_messages.c

    # ── command_broadcast/ ──────────────────────────────────────────────
    command_broadcast/command_broadcast_call.c
    command_broadcast/command_broadcast_receipt.c
    command_broadcast/command_broadcast_session.c
    command_broadcast/command_broadcast_state.c
    command_broadcast/inter_broadcast_fence.c
    command_broadcast/inter_broadcast_location.c
    command_broadcast/inter_broadcast_user.c
    command_broadcast/inter_broadcast_user_message.c
    command_broadcast/intra_broadcast_fence.c
    command_broadcast/intra_broadcast_location.c
    command_broadcast/intra_broadcast_user.c
    command_broadcast/intra_broadcast_user_message.c

    # ── command_controllers/ ────────────────────────────────────────────
    command_controllers/call_command_controller.c
    command_controllers/fence_command_controller.c
    command_controllers/fence_command_controller_linkjoin.c
    command_controllers/location_command_controller.c
    command_controllers/location_command_utils.c
    command_controllers/message_command_controller.c
    command_controllers/message_command_utils.c
    command_controllers/receipt_command_controller.c
    command_controllers/state_command_controller.c
    command_controllers/user_command_controller.c
    command_controllers/utils_fence_command.c
    command_controllers/utils_user_command.c

    # ── command_delivery/ ───────────────────────────────────────────────
    command_delivery/fence_command_delivery.c
    command_delivery/message_command_delivery.c
    command_delivery/user_command_delivery.c
    command_delivery/sync_command_delivery.c
    command_delivery/state_command_delivery.c
    command_delivery/utils_command_delivery.c

    # ── command_parser/ ─────────────────────────────────────────────────
    command_parser/ufsrvcmd_callbacks.c
    command_parser/ufsrvcmd_parser.c
    command_parser/ufsrvcmd_user_callbacks.c

    # ── msgqueue_broadcast/ ─────────────────────────────────────────────
    msgqueue_broadcast/ufsrvcmd_broadcast.c

    # ── user_prefs/ ─────────────────────────────────────────────────────
    user_prefs/user_preferences.c
    user_prefs/user_prefs.c
    user_prefs/user_prefs_fence.c

    # ── websock_parser/ ─────────────────────────────────────────────────
    websock_parser/websocket_header_parser.c

    ufsrv_core/ufsrv_config/ufsrv_config_host.c
)

# ── Build-time version.c generation ────────────────────────────────────────────
add_custom_target(${PROJECT_NAME}_version
    COMMAND chmod 755 ${CMAKE_CURRENT_SOURCE_DIR}/version.c.sh
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/version.c.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/version.c
    COMMENT "Generating version.c with a fresh compilation timestamp"
    VERBATIM)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/version.c PROPERTIES GENERATED TRUE)
target_sources(ufsrvwebsock PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/version.c)
add_dependencies(ufsrvwebsock ${PROJECT_NAME}_version)

# ── Include directories ────────────────────────────────────────────────────────
target_include_directories(ufsrvwebsock
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${VALGRIND_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/session_routing
        ${CMAKE_CURRENT_SOURCE_DIR}/command_controllers
        ${CMAKE_CURRENT_SOURCE_DIR}/command_broadcast
        ${CMAKE_CURRENT_SOURCE_DIR}/command_parser
        ${CMAKE_CURRENT_SOURCE_DIR}/msgqueue_broadcast
        ${CMAKE_CURRENT_SOURCE_DIR}/user_prefs
        ${CMAKE_CURRENT_SOURCE_DIR}/websock_parser
        ${CMAKE_CURRENT_SOURCE_DIR}/lexer
)

# ── Inject executable name as a compile-time string ─────────────────────────
target_compile_definitions(ufsrvwebsock PRIVATE APP_NAME_INJECTED="${PROJECT_NAME}")

# ── Linked-library resolution ──────────────────────────────────────────────────
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
include(${CMAKE_SOURCE_DIR}/cmake-modules/ufsrv_linked_libraries_resolve_packages.cmake)

# ── Link ───────────────────────────────────────────────────────────────────────
set(PROJECT_LINK_LIBS
    jemalloc utf8proc lua ${CMAKE_THREAD_LIBS_INIT} json-c curl ssl crypto
    mysqlclient zkgroup dl systemd bsd BlocksRuntime m resolv)
target_link_libraries(ufsrvwebsock PUBLIC
    ufsrv::companion
    ${PROJECT_LINK_LIBS})

# ── Build-options + digest on the executable ───────────────────────────────────
ufsrv_apply_build_options(ufsrvwebsock)
ufsrv_build_summary(ufsrvwebsock)
set_target_properties(ufsrvwebsock PROPERTIES OUTPUT_NAME "ufsrvwebsock")

if(UFSRV_CONFIG_COMPOSE)
    # One implementation of the pipeline lives in ufsrvcorelib and is exported by
    # its package config; this host carries only site data (ufsrv_config_site.cmake).
    if(NOT DEFINED ufsrvcorelib_HOST_MODULES_DIR OR ufsrvcorelib_HOST_MODULES_DIR STREQUAL "")
        message(FATAL_ERROR
            "ufsrv_config: ufsrvcorelib does not export ufsrvcorelib_HOST_MODULES_DIR.\n"
            "  Reinstall ufsrvcorelib (>= the release that ships cmake-modules/ufsrv_config/host).")
    endif()
    set(_UFSRV_CONFIG_HOST_MODULES_DIR "${ufsrvcorelib_HOST_MODULES_DIR}")

    include("${_UFSRV_CONFIG_HOST_MODULES_DIR}/ufsrv_config_clean_host.cmake")

    include("${_UFSRV_CONFIG_HOST_MODULES_DIR}/ufsrv_config_bootstrap_host.cmake")

    add_custom_target(ufsrv_config_compose_host
        COMMAND ${CMAKE_COMMAND}
            "-DUFSRV_CONFIG_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DUFSRV_CONFIG_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DUFSRV_CONFIG_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DUFSRV_CONFIG_HOST_MODULES_DIR=${_UFSRV_CONFIG_HOST_MODULES_DIR}"
            "-DUFSRV_CONFIG_HOST_SITE_FILE=${CMAKE_SOURCE_DIR}/cmake-modules/ufsrv_config/host/ufsrv_config_site.cmake"
            -P "${_UFSRV_CONFIG_HOST_MODULES_DIR}/ufsrv_config_compose_host.cmake"
        COMMENT "ufsrv_config: compose (clone → generate → compose → deep-install → rewrite)"
        VERBATIM)

    add_custom_target(ufsrv_config_check_host
        COMMAND ${CMAKE_COMMAND}
            "-DUFSRV_CONFIG_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${_UFSRV_CONFIG_HOST_MODULES_DIR}/ufsrv_config_check_host.cmake"
        COMMENT "ufsrv_config: integrity check (deep tree complete)"
        VERBATIM)
    add_dependencies(ufsrvwebsock ufsrv_config_check_host)
endif()

set(_UFSRV_CONFIG_DPR "${CMAKE_SOURCE_DIR}/src/ufsrv_core/ufsrv_config")
target_sources(ufsrvwebsock PRIVATE
    "${_UFSRV_CONFIG_DPR}/ufsrv_config.c"
    "${_UFSRV_CONFIG_DPR}/ufsrv_config_generic_callbacks.c"
    "${_UFSRV_CONFIG_DPR}/generated/ufsrv_config_getters_setters_generated.c")
target_include_directories(ufsrvwebsock PRIVATE
    "${CMAKE_SOURCE_DIR}/src/include/ufsrvcorelib"
    "${_UFSRV_CONFIG_DPR}/generated"
    "${_UFSRV_CONFIG_DPR}")
