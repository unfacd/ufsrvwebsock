# cmake-modules/ufsrv_companion_ide_sources.cmake
#
# IDE-only source indexing for companions consumed as system-installed packages.
#
# With UFSRV_COMPANION_PROVIDER=system-installed the companions contribute their
# headers (/usr/local/include) and their binary to the build — never their .c
# files. CLion therefore has no definition behind a call site: Go to Definition
# stops at the installed declaration, and the IDE has no setting to index an
# unrelated source tree on its own (CPP-18380 "Cannot attach other directory to
# current project" and CPP-22065 are still open; the documented answer is "put
# the sources in the project model").
#
# This module does exactly that, for companions only, without touching how they
# are resolved or linked:
#
#   * one throw-away OBJECT target per companion, <name>_ide, built
#     EXCLUDE_FROM_ALL — it is never linked into anything and never part of
#     'all', so no artifact changes and no source is compiled twice;
#   * the target links the installed package, so it inherits that package's
#     include dirs, compile definitions and dependencies, and the sources parse
#     against the same configuration the app uses;
#   * the source set comes from the package itself: a companion that ships
#     <pkg>-ide-sources.cmake next to its CMake config is the authority on what
#     it compiles, so nothing here needs editing when it grows a module. The
#     per-companion directory table below is the fallback for packages built
#     before that manifest existed, or installed from another machine where the
#     recorded absolute paths do not resolve;
#   * the checkout is located by path, never copied, so there is nothing to sync;
#   * when no source can be resolved the whole thing is skipped, and a machine
#     without the sibling repos configures exactly as before.
#
# Usage, after include(ufsrv_companion):
#
#   include(ufsrv_companion_ide_sources)
#   ufsrv_companion_ide_sources(ufsrvcorelib)
#   ufsrv_companion_ide_sources(ufnetcorelib)
#   ufsrv_companion_ide_sources(ufsrvrxlib)
#
# Override a checkout location with -DUFSRV_IDE_<NAME>_SOURCE_DIR=<path>.
# Only meaningful for the system-installed provider: with in-source the sources
# are already part of the project, and indexing a second copy would just give
# the IDE two definitions for everything.

include_guard(GLOBAL)

# ── Fallback layout: the .c files that make up the installed library ─────────
# Used only when the package ships no source manifest. Source directories are
# globbed recursively (a new file inside them is picked up on the next
# configure; a brand-new directory needs one line here). Extra sources are files
# the library compiles from outside those directories.
#
# uflib has no table on purpose: its compiled set is option-gated (k12, qr_code,
# db/zlog/ufconfig drivers, a try_compile probe) and a directory glob would add
# a dozen files the shipped library does not contain — including a stale
# src/version.c that duplicates the generated one. It relies on its manifest.

# ufsrvcorelib: mirrors the compiled set. src/ufsrv_core/ufsrv_config/ cannot be
# globbed wholesale — its composed/generated .c files are not part of the
# installed library and indexing them gives the IDE duplicate definitions.
set(_ufsrv_ide_companion_ufsrvcorelib_SOURCE_DIRS
    src/ufsrv_core/system
    src/ufsrv_core/configfile_processor
    src/ufsrv_core/instrumentation
    src/ufsrv_core/cli_configurator
    src/ufsrv_console_script)
set(_ufsrv_ide_companion_ufsrvcorelib_EXTRA_SOURCES
    src/ufsrv_core/ufsrv_config/ufsrv_config_pretty_printers.c
    src/ufsrv_core/ufsrv_config/ufsrv_config_secret.c
    src/ufsrv_core/ufsrv_config/ufsrv_config_stubs_as_string.c
    src/ufsrv_core/ufsrv_config/ufsrv_config_stubs_accessor_defs.c)

# ufnetcorelib: the two compiled modules. Their explicit target_sources() lists
# are exactly the recursive glob of these two directories (79 files).
set(_ufsrv_ide_companion_ufnetcorelib_SOURCE_DIRS
    src/ufnet_core
    src/ufnet_msgcore)

# ufsrvrxlib: one .c per module, plus the vendored libaco implementation it
# archives into libufsrvrxlib.a (aco is where the coroutine switch actually
# happens, so its definitions matter for navigation too).
set(_ufsrv_ide_companion_ufsrvrxlib_SOURCE_DIRS
    src/ufsrv_scheduler
    src/ufsrv_coroutine
    src/ufsrv_cancellation
    src/ufsrv_future
    packages/libaco)

# ── Implementation ───────────────────────────────────────────────────────────
function(ufsrv_companion_ide_sources _base)
    set(_target "${_base}_ide")
    if(TARGET ${_target})
        return()
    endif()

    if(NOT UFSRV_COMPANION_PROVIDER STREQUAL "system-installed")
        message(STATUS
            "ufsrv companion IDE index: skipped for ${_base} — provider is "
            "'${UFSRV_COMPANION_PROVIDER}', sources are already in the project")
        return()
    endif()

    string(TOUPPER "${_base}" _upper)
    set(_dir_var "UFSRV_IDE_${_upper}_SOURCE_DIR")
    if(NOT DEFINED ${_dir_var})
        set(${_dir_var} "$ENV{HOME}/repos/1.ufsrv/${_base}" CACHE PATH
            "${_base} checkout — indexed for IDE navigation only, never built")
    endif()
    set(_src "${${_dir_var}}")

    # Source set, first choice: the manifest the package itself publishes. The
    # owner of the source set knows exactly which files it compiles, so it ships
    # that list next to its CMake config (<pkg>_DIR/<base>-ide-sources.cmake).
    # This is not a guess: it survives new modules, renamed files, conditionals
    # and test-only sources without any edit here. Paths in it are the builder's
    # absolute paths, so entries that do not exist on this machine are dropped.
    set(_sources "")
    set(_origin "")
    set(_manifest "${${_base}_DIR}/${_base}-ide-sources.cmake")
    if(DEFINED ${_base}_DIR AND EXISTS "${_manifest}")
        include("${_manifest}")
        if(DEFINED ${_upper}_IDE_SOURCE_DIR AND EXISTS "${${_upper}_IDE_SOURCE_DIR}/src")
            set(_src "${${_upper}_IDE_SOURCE_DIR}")
            set(${_dir_var} "${_src}" CACHE PATH
                "${_base} checkout — indexed for IDE navigation only, never built" FORCE)
        endif()
        foreach(_file IN LISTS ${_upper}_IDE_SOURCES)
            # Compiled C/C++ translation units only: a manifest may also list
            # assembler or data files, which the IDE index target has no use for.
            string(REGEX MATCH "\\.(c|cc|cpp|cxx)$" _is_cxx "${_file}")
            if(EXISTS "${_file}" AND _is_cxx)
                list(APPEND _sources "${_file}")
            endif()
        endforeach()
        if(_sources)
            set(_origin "package manifest")
        endif()
    endif()

    # Second choice: the directory table below, globbed from the checkout. This
    # is the fallback for packages built before the manifest existed and for
    # machines where the builder's paths do not resolve.
    if(NOT _sources)
        if(NOT EXISTS "${_src}/src")
            message(STATUS
                "ufsrv companion IDE index: ${_base} not indexed — no checkout at '${_src}'")
            return()
        endif()
        foreach(_rel IN LISTS _ufsrv_ide_companion_${_base}_SOURCE_DIRS)
            if(EXISTS "${_src}/${_rel}")
                file(GLOB_RECURSE _found CONFIGURE_DEPENDS "${_src}/${_rel}/*.c")
                list(APPEND _sources ${_found})
            endif()
        endforeach()
        foreach(_rel IN LISTS _ufsrv_ide_companion_${_base}_EXTRA_SOURCES)
            if(EXISTS "${_src}/${_rel}")
                list(APPEND _sources "${_src}/${_rel}")
            endif()
        endforeach()
        if(_sources)
            set(_origin "directory table")
        endif()
    endif()
    list(REMOVE_DUPLICATES _sources)

    if(NOT _sources)
        message(STATUS
            "ufsrv companion IDE index: ${_base} not indexed — no sources resolved from "
            "'${_src}' (package ships no usable manifest and no fallback layout is declared)")
        return()
    endif()

    # Private include layout: the libraries compile internal headers by bare name
    # (<configfile_processor.h>, <ufsrvscriptlib_eval.h>), so the source tree's
    # own directories must be on the path. Source headers are listed before the
    # package's installed ones, so they win for this target.
    set(_includes "${_src}/include" "${_src}/include/${_base}" "${_src}/src")
    file(GLOB_RECURSE _include_tree LIST_DIRECTORIES true "${_src}/include/*")
    foreach(_dir IN LISTS _include_tree)
        if(IS_DIRECTORY "${_dir}")
            list(APPEND _includes "${_dir}")
        endif()
    endforeach()
    file(GLOB_RECURSE _src_tree LIST_DIRECTORIES true "${_src}/src/*")
    foreach(_dir IN LISTS _src_tree)
        if(IS_DIRECTORY "${_dir}")
            list(APPEND _includes "${_dir}")
        endif()
    endforeach()
    # Vendored packages (csonpath, ufsrvscriptlib, hiredis) ship headers either
    # at their root or under include/, sometimes namespaced one level deeper
    # (packages/ufsrvcorelib/include/ufsrvcorelib/ufsrvcorelib_defs.h).
    file(GLOB _packages LIST_DIRECTORIES true "${_src}/packages/*")
    if(_packages)
        # Headers are also reached by a package-qualified path (<libaco/aco.h>),
        # which resolves against the packages root.
        list(APPEND _includes "${_src}/packages")
    endif()
    foreach(_pkg IN LISTS _packages)
        if(NOT IS_DIRECTORY "${_pkg}")
            continue()
        endif()
        list(APPEND _includes "${_pkg}")
        if(IS_DIRECTORY "${_pkg}/include")
            list(APPEND _includes "${_pkg}/include")
            file(GLOB_RECURSE _pkg_include_tree LIST_DIRECTORIES true "${_pkg}/include/*")
            foreach(_dir IN LISTS _pkg_include_tree)
                if(IS_DIRECTORY "${_dir}")
                    list(APPEND _includes "${_dir}")
                endif()
            endforeach()
        endif()
    endforeach()

    # Lua: ufnetcorelib's headers pull in <lua.h>/<lauxlib.h> unconditionally,
    # and it is a host dependency (never vendored). Resolve it the same way the
    # build does — pkg-config first, path search as a fallback. Without it every
    # ufnetcorelib TU fails at the first #include and the index gets nothing.
    if(NOT _ufsrv_ide_lua_includes)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(_ufsrv_ide_pc_lua QUIET lua)
            set(_ufsrv_ide_lua_includes ${_ufsrv_ide_pc_lua_INCLUDE_DIRS})
        endif()
        if(NOT _ufsrv_ide_lua_includes)
            find_path(_ufsrv_ide_lua_inc lua.h PATH_SUFFIXES lua5.4 lua54 lua5.3 lua)
            if(_ufsrv_ide_lua_inc)
                set(_ufsrv_ide_lua_includes "${_ufsrv_ide_lua_inc}")
            endif()
        endif()
    endif()
    if(_ufsrv_ide_lua_includes)
        list(APPEND _includes ${_ufsrv_ide_lua_includes})
    endif()

    add_library(${_target} OBJECT EXCLUDE_FROM_ALL ${_sources})
    target_include_directories(${_target} PRIVATE ${_includes})
    # Each library's sources are written against its own generated config header,
    # guarded by HAVE_CONFIG_<NAME>_H. ufsrvapi defines the ufsrvcore/ufnet ones
    # globally; uflib's is not exported by its package, so set it here for the
    # index target — otherwise the library's own .c files do not even parse.
    target_compile_definitions(${_target} PRIVATE HAVE_CONFIG_${_upper}_H)
    # Inherit the installed packages' include dirs, config definitions and
    # dependencies. The companions depend on one another (ufnetcorelib compiles
    # against ufsrvcorelib's generated <config_ufsrvcorelib.h>, ufsrvcorelib
    # against uflib), so link every companion package that resolved — never a
    # target that is built here.
    foreach(_dep IN ITEMS uflib ufsrvcorelib ufnetcorelib ufsrvrxlib)
        if(TARGET ${_dep}::${_dep})
            target_link_libraries(${_target} PRIVATE ${_dep}::${_dep})
        endif()
    endforeach()
    # -fblocks / -include stdatomic.h / sanitizers: parse the sources the way the
    # real build does, so the indexed definitions match what ships.
    if(COMMAND ufsrv_apply_build_options)
        ufsrv_apply_build_options(${_target})
    endif()
    set_target_properties(${_target} PROPERTIES C_STANDARD 17)

    list(LENGTH _sources _count)
    message(STATUS
        "ufsrv companion IDE index: ${_base} → target '${_target}' (${_count} sources via ${_origin}, ${_src})")
endfunction()
