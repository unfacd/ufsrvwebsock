
#provide defaults for configuration items which the user did not pass value for via -D

if(NOT DEFINED CONFIG_UFSRVWEBSOCK_CONFIGFILE_ADJUNCT_SCHEMA)
    set(CONFIG_UFSRVWEBSOCK_CONFIGFILE_ADJUNCT_SCHEMA "adjunct_schema_ufsrvwebsock.lua" CACHE STRING "Lua adjunct schema definition for lua-base configuration files")
endif()

if(NOT DEFINED CONFIG_CONFIGFILE_DEFAULT_PID_DIR)
    set(CONFIG_CONFIGFILE_DEFAULT_PID_DIR "/var/run" CACHE STRING "Directory location of PID file")
endif()