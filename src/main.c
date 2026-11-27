/**
 * Copyright (C) 2015-2026 unfacd works
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_UFSRVWEBSOCK_H
#include "config_ufsrvwebsock.h"
#endif

#include "main.h"
#include "main_protocols.h"
#include "ufsrv_linked_libs.h"

#include <string.h>
#include <time.h>

#include <bsd/libutil.h>
#include <uflib/utils_crypto.h>
#include <uflib/utils_file.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufsrvcorelib/ufsrv_core/system/ufsrv_signals.h>
#include "nportredird.h"
#include <signal.h>
#include <uflib/buffer_descriptor/buffer_descriptor.h>
#include <systemd/sd-daemon.h>
#include <ufsrvcorelib/ufsrv_core/command_console/command_console_thread.h>
#include <ufsrvcorelib/ufsrv_console_script/ufsrv_console_script.h>

#include <ufnetcorelib/ufnet_msgcore/msgqueue_backend/delegator_msgqueue_subscriber_thread.h>
#include <ufsrvcorelib/ufsrv_core/cli_configurator/ufsrv_cli_configurator.h>
#include "utils_console_handlers.h"
#include "ufsrv_type.h"
#include "version.h"
#include "ufsrv_core/protocol/protocol.h"
#include <ufsrvcorelib/ufsrv_core/system/cpu_affinity_policy_enum.h>
#include <ufsrvcorelib/ufsrv_core/system/utils_cpu.h>
#include <ufsrvcorelib/ufsrv_core/configfile_processor/configfile_processor.h>
#include <ufsrvcorelib/ufsrv_core/configfile_processor/configfile_validator_descriptor_type.h>
#include "ufsrv_core/ufsrv_config/generated/ufsrv_config_accessor_decls_generated.h"
#include "ufsrv_core/ufsrv_config/ufsrv_config_host.h"
#include <ufsrvcorelib/ufsrv_core/ufsrv_logger.h>
#include <ufsrvcorelib/ufsrv_core/main_utils.h>
#include <ufsrvcorelib/ufsrv_core/ufsrv_utils.h>
#include <ufnetcorelib/ratelimit_policy.h>
#include "generated/data_ufsrv_config_literal_to_field_map_priv_generated.h"

#define IS_MAIN_PROGRAM_CONTINUE_POST_CLI_PROCESSOR true
#define IS_MAIN_PROGRAM_CONTINUE_POST_LOGGER_PROCESSOR true

#ifndef APP_NAME_INJECTED
# define APP_NAME_INJECTED "ufsrvwebsock"
#endif

#include "data_protocols.h"

extern void ConfigSymbolsPatchCallbacks(void);

static void Banner(void) __attribute__ ((unused));
static bool sOpenLogger(void);
static bool sParseCommandLine(int argc, char *argv[], CommandLineArgsConfig *cli_configuration_provided);
static void sBootstrapConfigurationStores();
static void sLoadConfigurationStoresDefaults();
static void sProcessCliConfigurationJsonDump(const char *json_dump_file);
static void sProcessCliConfigOp(const CommandLineArgsConfig *cli);
static ConfFileSymbolTableEntry *sProvideConfigFileCoreSymbolsMap();
static ConfFileSymbolTableEntry *sProvideConfigFileAdjunctSymbolsMap();
static void sFinaliseApplicationConfig(void);

int main(int argc, char *argv[])
{
  struct pidfh *pid_handle = NULL;
  Socket *      s_ptr_console = NULL;
  char *        clientid;

  sBootstrapConfigurationStores();
  sLoadConfigurationStoresDefaults();

  UfsrvConfigSetWhen(&(UfsrvConfigItem){.value.stored_value.int_val = (int64_t)time(NULL)}, UFSRV_CONFIG_SOURCE_RUNTIME);

  CommandLineArgsConfig cli_configuration = {0};
  if (sParseCommandLine(argc, argv, &cli_configuration) != IS_MAIN_PROGRAM_CONTINUE_POST_CLI_PROCESSOR) return EXIT_SUCCESS;

  ConfigBootstrapCliDefaults(&cli_configuration);

  UfsrvConfigSetServerClass(&(UfsrvConfigItem){.value.stored_value.str_val = APP_NAME_INJECTED, .value.type_allocation_sz = sizeof(APP_NAME_INJECTED)}, UFSRV_CONFIG_SOURCE_RUNTIME);

  SeedRandom(UfsrvConfigUfsrvTimeAsPtr());
  UfsrvConfigSetServeridRuntimeRandom(&(UfsrvConfigItem){.value.stored_value.int_val = (unsigned)(GenerateRandomNumber() / 10000)}, UFSRV_CONFIG_SOURCE_RUNTIME);
  snprintf(UfsrvConfigServerDescriptiveName(), MEDIUMBUF, "%s-%d-%u", UfsrvConfigServerClass(), UfsrvConfigServerInstanceId(), UfsrvConfigServeridRuntimeRandom());
  {
    asprintf(&clientid, "%s-%d", UfsrvConfigServerClass(), UfsrvConfigServerInstanceId());
    openlog(clientid, LOG_NDELAY, LOG_USER);

    //DO NOT free it is retained by openlog
    //free (clientid);
  }

  syslog(LOG_INFO, "-:::::::::: %s (%s) ::::::::::- {instance id: '%d', serverid:'%u', compiled: '%s', by: '%s'}", APP_NAME_INJECTED, UfsrvVersion(), cli_configuration.instance_id, UfsrvConfigServeridRuntimeRandom(), UfsrvCompiledTimestamp(), UfsrvCompiledBy());

  UfsrvLinkedLibsLog();

  ConfigFileValidatorDescriptor config_file_validator_descriptor;
  ConfigProvideValidatorDescriptor(&config_file_validator_descriptor, false, CONFIG_UFSRVWEBSOCK_CONFIGFILE_ADJUNCT_SCHEMA, ConfigValidationFinding);
  config_file_validator_descriptor.core_symbols               = sProvideConfigFileCoreSymbolsMap();
  config_file_validator_descriptor.adjunct_symbols            = sProvideConfigFileAdjunctSymbolsMap();
  config_file_validator_descriptor.associative_lookup_map     = ufsrv_config_key_map;
  config_file_validator_descriptor.associative_lookup_map_sz  = UFSRV_CONFIG_KEY_MAP_COUNT;
  ConfigProcessFile(&config_file_validator_descriptor);

  // ── --config=<op> diagnostic (verify | verify-strict | generate) ──────
  if (cli_configuration.config_op_requested) {
    sProcessCliConfigOp(&cli_configuration);
    UfsrvConfigArgsDestruct(&cli_configuration);
    return EXIT_SUCCESS;
  }
  ConfigPopulateConfFileStoreFromLua(sProvideConfigFileCoreSymbolsMap());
  ConfigPopulateConfFileStoreFromLua(sProvideConfigFileAdjunctSymbolsMap());

  ConfigInjectOverridingCliArguments(&cli_configuration); //final step in configuration store value-setting

  if (!sOpenLogger()) {
    UfsrvConfigArgsDestruct(&cli_configuration);
    return EXIT_FAILURE;
  }

  ConfigBootstrapFileLoaderService();
  sFinaliseApplicationConfig();

  ConfigFinalizeSensitiveConfig();

  UfsrvConfigSetUfsrvTime(&(UfsrvConfigItem){.value.stored_value.int_val = time(NULL)}, UFSRV_CONFIG_SOURCE_RUNTIME);

  {
    CpuAffinityDescriptor cpu_desc_in = {
      .listener_worker_pool_sz = UfsrvConfigListenerWorkersThreadPool(),
      .worker_pool_sz          = UfsrvConfigSessionWorkersThreadPool()
                                 + UfsrvConfigUfsrvWorkersThreadPool(),
      .aux_worker_pool_sz      = 0,  /* TODO: timer + fileloader + msgqueue + console + schedulers */
    };
    CpuAffinityPolicy cpu_policy = UfsrvConfigServerCpuAffinity();

    UfsrvThreadCpuAffinityDescriptor *cpu_desc = UfsrvCpuAffinityInit(cpu_policy, &cpu_desc_in);
    if (cpu_desc) {
      /* TODO: split effective worker_pool_sz back into the two concrete pools */
      char *json = UfsrvCpuAffinityDescribe(cpu_desc);
      if (json) {
        syslog(LOG_INFO, "CPU affinity: %s", json);
        free(json);
      }
      free(cpu_desc);
    }
  }

  if (cli_configuration.dump_config_json_requested) {
    sProcessCliConfigurationJsonDump(cli_configuration.dump_config_json_path_is_default ? NULL : cli_configuration.dump_config_json_file);
    UfsrvConfigArgsDestruct(&cli_configuration);
    return EXIT_SUCCESS;
  }

  UfsrvConfigArgsDestruct(&cli_configuration);

  CollectionDescriptor listeners = UfsrvConfigListeners();
  if (!IsUfsrvListenersDescriptorValid(&listeners, ProtocolsRegistry)) {
    sd_notifyf(0, "STOPPING=1\nSTATUS=Invalid listener descriptor");
    return EXIT_FAILURE;
  }

  InitSignals();                                                                // signal handlers first

  if (UfsrvConfigServerProcessMode() == SERVER_PROCESS_MODE_DAEMON) pid_handle = UfsrvGoDaemon(); // daemonize BEFORE threads
  InitProtocolsForUfsrv(ProtocolsRegistry, UfsrvProtocolRegistrySize() - 1);
  const Protocol *websocket = UfsrvWebsockProtocol();
  if (!websocket->session_pool || !UfsrvWebsockStartServices()) {
    return EXIT_FAILURE;
  }

  UfsrvConfigSetServerpid(&(UfsrvConfigItem){.value.stored_value.int_val = getpid()}, UFSRV_CONFIG_SOURCE_RUNTIME);
  sd_notifyf(0, "STATUS=Starting ufsrv instance '%s'…\n", UfsrvConfigServerDescriptiveName());

  UfsrvConsoleRegisterHandlers();
  s_ptr_console = SetupCommandConsole();
  ConsoleThreadRegisterCommandHandler(UfsrvConsoleScriptResolve);

  if (websocket->protocol_controls.msgqueue_subscriber) {
    if (IS_EMPTY(CreateMessageQueueSubscriberListenerThread(websocket))) {
      syslog(LOG_ERR, "%s: INITIALISATION ERROR: TERMINATING...", __func__);
      if (IS_PRESENT(s_ptr_console))  free(s_ptr_console);
      return EXIT_FAILURE;
    }
  }

  int listener_status = 0;
  if (UfsrvConfigServerRunMode() != RUNMODE_SHADOW) {
    Socket **listener_sockets = calloc(listeners.collection_sz, sizeof(*listener_sockets));
    if (!listener_sockets) {
      syslog(LOG_ERR, "Could not allocate listener socket table");
      return EXIT_FAILURE;
    }

    bool prepared = UfsrvListenerForEach(&listeners, ^bool(UfsrvListenerDescriptor *listener, size_t index) {
      listener_sockets[index] = UfsrvListenerBootstrap(listener);
      return listener_sockets[index] != NULL;
    });

    if (!prepared) {
      for (size_t i = 0; i < listeners.collection_sz; ++i) {
        if (listener_sockets[i]) {
          close(listener_sockets[i]->sock);
          free(listener_sockets[i]);
        }
      }
      free(listener_sockets);
      syslog(LOG_ERR, "Failed to prepare listener sockets");
      sd_notifyf(0, "STOPPING=1\nSTATUS=Failed to prepare listener sockets");
      return EXIT_FAILURE;
    }

    UfsrvDropRootPrivileges(); // after all potentially privileged binds

    sd_notifyf(0, "STATUS=Starting %zu ufsrv listener(s)…\n", listeners.collection_sz);
    listener_status = UfsrvMainListenerStart(&listeners, listener_sockets, s_ptr_console);
    for (size_t i = 0; i < listeners.collection_sz; ++i) {
      free(listener_sockets[i]);
    }
    free(listener_sockets);
  } else {
    UfsrvDropRootPrivileges();
    sd_notifyf(0, "READY=1\nSTATUS=Starting ufsrv in shadow mode without application listeners");
    syslog(LOG_NOTICE, "%s: shadow mode enabled, application listeners are closed", __func__);
  }

  SessionsDelegator *sessions_delegator = GetUfsrvSessionsDelegator();

  sd_notifyf(0, "STATUS=Main thread join barrier reached…\n" "READY=1\n");
  syslog(LOG_NOTICE, "%s (pid:'%lu', listeners:'%d', session_workers:'%d'): Main thread join barrier reached....",
         __func__, pthread_self(), sessions_delegator->setsize, sessions_delegator->setsize);

  // ── Join barrier: drain all pools and wait for them to exit ────────────
  // NOTE: joining a pool blocks until it exits. Session workers exit via
  // up_status==0; the ufsrv workers, delegator and timer still need their
  // shutdown checks (see the signals design doc) — until those land, the
  // joins below will block.
  for (int i = 0; i < sessions_delegator->setsize; i++) {
    pthread_join(sessions_delegator->session_worker_ths[i], NULL);
  }

  WorkersConfigDescriptor *jobworkers = sessions_delegator->jobworkers_config_descriptor;
  if (IS_PRESENT(jobworkers)) {
    for (size_t i = 0; i < jobworkers->pool_sz; i++) {
      pthread_join(jobworkers->workers[i], NULL);
    }
  }

  pthread_join(sessions_delegator->session_delegator_thread, NULL);

  pthread_join(UfsrvConfigThreadHandleTimerDelegator(), NULL);

  if (pid_handle != NULL) {
    pidfile_close(pid_handle);
    pidfile_remove(pid_handle);
  }

  return listener_status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

const Protocol *
UfsrvWebsockProtocol(void)
{
  return &ProtocolsRegistry[WEBSOCKET_PROTOCOL_INDEX];
}

size_t
UfsrvProtocolRegistrySize()
{
  return sizeof(ProtocolsRegistry) / sizeof(ProtocolsRegistry[0]);
}

static void
sFinaliseApplicationConfig(void)
{
  ConfigPrepareApplicationRateLimitPolicy();
}

static bool
sOpenLogger(void)
{
  static char logger_file_path[PATH_MAX];

  const char *driver = UfsrvConfigLoggerDriver();
  UfLoggerConfig logger_config = *ProvideDefaultLoggerConfig();

  logger_config.category       = UfsrvConfigLoggerPrefix();
  logger_config.format_pattern = UfsrvConfigLoggerFormat();

  if (strcmp(driver, "file") == 0) {
    FileInfo logger_dir_info = {0};
    char logger_dir[PATH_MAX];

    int dir_len = snprintf(logger_dir, sizeof(logger_dir), "%s/var/log", UfsrvConfigConfigDir());
    if (dir_len < 0 || (size_t)dir_len >= sizeof(logger_dir)) {
      syslog(LOG_ERR, "%s: logger directory path is too long", __func__);
      return !IS_MAIN_PROGRAM_CONTINUE_POST_LOGGER_PROCESSOR;
    }

    if (GetFileInfo(logger_dir, &logger_dir_info, 0) != 0 || !logger_dir_info.is_directory) {
      syslog(LOG_ERR, "%s: logger directory '%s' does not exist or is not a directory", __func__, logger_dir);
      return !IS_MAIN_PROGRAM_CONTINUE_POST_LOGGER_PROCESSOR;
    }

    int path_len = snprintf(logger_file_path, sizeof(logger_file_path), "%s/%s.log", logger_dir, UfsrvConfigServerClass());
    if (path_len < 0 || (size_t)path_len >= sizeof(logger_file_path)) {
      syslog(LOG_ERR, "%s: logger file path is too long", __func__);
      return !IS_MAIN_PROGRAM_CONTINUE_POST_LOGGER_PROCESSOR;
    }
    logger_config.destination = UF_LOGGER_DESTINATION_FILE;
    logger_config.file_path   = logger_file_path;
  }
  else if (strcmp(driver, "syslog") == 0) {
    logger_config.destination = UF_LOGGER_DESTINATION_SYSLOG;
  }
  else if (strcmp(driver, "stderr") == 0) {
    logger_config.destination = UF_LOGGER_DESTINATION_STDERR;
  }
  else if (strcmp(driver, "stdout") == 0) {
    logger_config.destination = UF_LOGGER_DESTINATION_STDOUT;
  }
  else {
    syslog(LOG_ERR, "%s: unsupported logger driver '%s' (expected syslog|stderr|stdout|file)", __func__, driver);
    return !IS_MAIN_PROGRAM_CONTINUE_POST_LOGGER_PROCESSOR;
  }

  UfLogger *logger_ptr = OpenLogger(&logger_config);
  if (logger_ptr == NULL) {
    syslog(LOG_ERR, "%s: could not open logger (driver '%s')", __func__, driver);
    return !IS_MAIN_PROGRAM_CONTINUE_POST_LOGGER_PROCESSOR;
  }

  UfsrvConfigSetUfLogger(&(UfsrvConfigItem){.value = {.stored_value.ptr_val = logger_ptr}}, UFSRV_CONFIG_SOURCE_RUNTIME);

  return IS_MAIN_PROGRAM_CONTINUE_POST_LOGGER_PROCESSOR;
}

/**
 * @brief Parse CLI arguments via ufnetcorelib ufsrv_configurator.
 *
 * Sets defaults, parses argv, handles --help/--version, applies overrides.
 * @return true if main() should continue, false if the process should exit.
 *
 */
static bool
sParseCommandLine(int argc, char *argv[], CommandLineArgsConfig *cli_configuration_provided)
{
  UfsrvConfigParseArgs(argc, argv, cli_configuration_provided);

  // Short-circuit: diagnostic flags honoured regardless of errors

  if (cli_configuration_provided->help_requested) {
    UfsrvConfigPrintUsage(stdout, argv[0], NULL);
    UfsrvConfigArgsDestruct(cli_configuration_provided);
    return !IS_MAIN_PROGRAM_CONTINUE_POST_CLI_PROCESSOR;
  }

  if (cli_configuration_provided->version_requested) {
    printf("%s %s\n", APP_NAME_INJECTED, UfsrvVersion());
    UfsrvConfigArgsDestruct(cli_configuration_provided);
    return !IS_MAIN_PROGRAM_CONTINUE_POST_CLI_PROCESSOR;
  }

  if (cli_configuration_provided->result.state == UFSRV_ARGS_ERROR) {
    fprintf(stderr, "%s\n", cli_configuration_provided->result.message);
    UfsrvConfigArgsDestruct(cli_configuration_provided);
    return !IS_MAIN_PROGRAM_CONTINUE_POST_CLI_PROCESSOR;;
  }

#if 0
  if (cfg.validate_config_requested) {
    const char *cf     = cfg.validate_config_file ? cfg.validate_config_file : "/opt/ufsrv/etc/ufsrv.conf.lua";
    int         errors = ConfFileValidate(cf, "etc/core_schema_ufsrv.lua", "etc/adjunct_schema_ufsrvwebsock.lua",
                                          "etc/core_schema_validator.lua", g_ufsrv_config_symbols, g_app_config_symbols,
                                          sOnValidationFinding, NULL, 0);
    fprintf(stderr, "Validation complete: %d error(s)\n", errors);
    UfsrvConfigArgsDestruct(&cfg);
    exit(errors > 0 ? EXIT_FAILURE : EXIT_SUCCESS);
  } if (cfg.dump_config_json_requested) {
    const char *cf = cfg.config_file ? cfg.config_file : CONFIG_FILE_NAME;
    char        config_path[_POSIX_PATH_MAX];
    snprintf(config_path, sizeof(config_path), "%s/%s", *UfsrvConfigConfigDir() ? UfsrvConfigConfigDir() : CONFIG_DIR,
             cf);
    ConfFile cp = ConfFileInit(config_path);
    if (!cp) {
      fprintf(stderr, "Failed to load %s\n", config_path);
      UfsrvConfigArgsDestruct(&cfg);
      exit(EXIT_FAILURE);
    }
    ConfFileResolveSymbols(cp, g_ufsrv_config_symbols, NULL);
    ConfFileResolveSymbols(cp, g_ufnet_config_symbols, NULL);
    ConfFileResolveSymbols(cp, g_app_config_symbols, NULL);
    ConfFileDumpJson(g_ufsrv_config_symbols, cfg.dump_config_json_file);
    fprintf(stderr, "Config dumped to %s\n", cfg.dump_config_json_file);
    ConfFileDestroy(cp);
    UfsrvConfigArgsDestruct(&cfg);
    exit(EXIT_SUCCESS);
  }

  // ── Parse errors — only relevant if no short-circuit fired ──

  if (cfg.result.state == UFSRV_ARGS_ERROR) {
    fprintf(stderr, "%s\n", cfg.result.message);
    UfsrvConfigArgsDestruct(&cfg);
    return false;
  }

  // ═════════════════════════════════════════════════════════════════════
  // Pass 3 — CLI overrides via new generated setters.
  // CLI values win over defaults and config file.
  // ═════════════════════════════════════════════════════════════════════
  {
    UfsrvConfigItem item;
    item.layer = UFSRV_CONFIG_SOURCE_CLI;

    if (cfg.config_dir) {
      item.type                       = UFSRV_CONFIG_VALUE_STR;
      item.value.stored_value.str_val = cfg.config_dir;
      UfsrvConfigSetConfigDir(&item, UFSRV_CONFIG_SOURCE_CLI);
    }
    if (cfg.config_file) {
      item.type                       = UFSRV_CONFIG_VALUE_STR;
      item.value.stored_value.str_val = cfg.config_file;
      UfsrvConfigSetConfigFile(&item, UFSRV_CONFIG_SOURCE_CLI);
    }
    if (cfg.server_mode == UFSRV_MODE_DAEMON) {
      item.type                       = UFSRV_CONFIG_VALUE_ENUM;
      item.value.stored_value.int_val = RUNNING_MODE_DAEMON;
      UfsrvConfigSetRunningMode(&item, UFSRV_CONFIG_SOURCE_CLI);
    }
    if (cfg.server_mode == UFSRV_MODE_TERMINAL) {
      item.type                       = UFSRV_CONFIG_VALUE_ENUM;
      item.value.stored_value.int_val = RUNNING_MODE_TERM;
      UfsrvConfigSetRunningMode(&item, UFSRV_CONFIG_SOURCE_CLI);
    }

    if (cfg.sandbox_protocol) {
      // TODO: wire sandbox protocol mode into the protocol registry
    }

    // Fallback defaults for CLI — only if not already set
    // (ufsrv_config_store carries compiled defaults from Pass 1)
  } if (!*UfsrvConfigConfigDir()) {
    UfsrvConfigItem item            = {.type = UFSRV_CONFIG_VALUE_STR};
    item.value.stored_value.str_val = CONFIG_DIR;
    UfsrvConfigSetConfigDir(&item, UFSRV_CONFIG_SOURCE_DEFAULTS);
  } if (!*UfsrvConfigConfigFile()) {
    UfsrvConfigItem item            = {.type = UFSRV_CONFIG_VALUE_STR};
    item.value.stored_value.str_val = CONFIG_FILE_NAME;
    UfsrvConfigSetConfigFile(&item, UFSRV_CONFIG_SOURCE_DEFAULTS);
  }
#endif

  return IS_MAIN_PROGRAM_CONTINUE_POST_CLI_PROCESSOR;
}

/**
 * @brief Dump a JSON representation of the config-file map to stderr/file.
 *
 * @param json_dump_file File name to dump the JSON stream into.
 */
static void
sProcessCliConfigurationJsonDump(const char *json_dump_file)
{
  //ConfFileDumpJson(sProvideConfigFileCoreSymbolsMap(), sProvideConfigFileAdjunctSymbolsMap(), NULL, 0, json_dump_file);
  //fprintf(stderr, "Lua Config dumped to: %s\n", json_dump_file);

  BufferDescriptor config_file = {0};

  if (JsonFormatRuntimeConfigStore(&config_file) == 0) {
    fprintf(stderr, "Runtime Config dumped to: %s\n", config_file.data);
  }
  BufferDescriptorRelease(&config_file);

  if (JsonFormatConfigurationFileConfigStore(&config_file) == 0) {
    fprintf(stderr, "ConfigFile dumped to: %s\n", config_file.data);
  }
  BufferDescriptorRelease(&config_file);


}

/**
 * @brief Skeletal handler for the --config=<op> diagnostic option.
 *
 * Placeholder — the real config-file verify / verify-strict / generate logic
 * is wired later.  Prints the requested operation; the caller frees the CLI
 * argument state and exits.
 */
static void
sProcessCliConfigOp(const CommandLineArgsConfig *cli)
{
  const char *op;
  switch (cli->config_file_op) {
    case CLI_CONFIGFILE_OP_VERIFY_STRICT: op = "verify-strict"; break;
    case CLI_CONFIGFILE_OP_GENERATE:      op = "generate";      break;
    case CLI_CONFIGFILE_OP_VERIFY:
    default:                              op = "verify";        break;
  }

  syslog(LOG_NOTICE, "config op '%s': skeletal (not yet implemented)", op);
  fprintf(stderr, "config op '%s': not yet implemented\n", op);
}

/**
 * @brief Bootstrap the runtime and config-file config stores.
 */
static void
sBootstrapConfigurationStores()
{
  BootstrapRuntimeConfigStore();
  BootstrapConfigurationFileConfiStore();
}

/**
 * @brief Load defaults into the config-file and runtime config stores.
 */
static void
sLoadConfigurationStoresDefaults()
{
#define IS_FORCED_RELOAD_FALSE false

  LoadConfigurationFileConfiStoreDefaults(IS_FORCED_RELOAD_FALSE);
  LoadRuntimeConfigStoreDefaults(IS_FORCED_RELOAD_FALSE);

#undef IS_FORCED_RELOAD_FALSE
}

/**
* Under SysV init, init was passive. It ran your /etc/init.d/ufsrvwebsock start script and then walked away — it had no way to track the daemon afterward. So the daemon had to do all the work itself:

1. fork + setsid — detach from the controlling terminal (so a terminal SIGHUP doesn't kill it, and it's no longer in a terminal's foreground process group).
2. second fork — guarantee it's not a session leader, so it can never re-acquire a controlling terminal.
3. chdir("/"), redirect stdin/stdout/stderr to /dev/null — because those point at the now-gone terminal.
4. write a pidfile — so the later init.d/ufsrvwebsock status/stop script could find the daemon by reading the PID out of a file.

Why systemd doesn't want you to do any of that

systemd is an active supervisor. When it runs ExecStart, it:

- spawns the process itself, into a dedicated cgroup,
- gives it a clean context — no controlling terminal to detach from, stdout/stderr already go to the journal, SIGHUP doesn't mean "terminal hung up",
- tracks the process tree directly through the cgroup — it always knows exactly which PID is yours,
- handles restart, resource limits, and cleanup (kills the whole cgroup on stop).

So all four SysV jobs are redundant: no terminal to escape, no /dev/null redirection needed, no pidfile needed to find you, no parent-exit signal needed to say "I'm up."

That's the whole point of the Type= field:

┌──────────────────┬───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│      Type=       │                                                           What the daemon does                                                            │
├──────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ forking          │ Legacy: double-fork, parent exits, systemd reads PIDFile= to find the child. Exists only for old daemons that insist on self-daemonizing. │
├──────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ simple (default) │ Stay in foreground; the ExecStart process is the service.                                                                                 │
├──────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│ notify (yours)   │ Stay in foreground; additionally call sd_notify(READY=1) when ready.                                                                      │
└──────────────────┴───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘

The server already sends READY=1 at main.c:204 and 213 — that's the systemd-native replacement for "the parent exits, so init knows I'm up."

So the direct answer

Under systemd, the server should not call GoDaemon(). It should run in the foreground as the single process systemd spawned, and say "READY" via sd_notify. If it does fork:

- systemd loses track of which child is the real daemon (that's the fragile Type=forking + PIDFile= guessing game),
- your Type=notify + READY=1 gets confused because the notifying process and the surviving child aren't the same PID,
- restart/stop/cgroup limits no longer line up with the real process.

The fork path in the code (if (… == SERVER_PROCESS_MODE_DAEMON) GoDaemon()) isn't wrong to have — it's the SysV fallback. The rule is simply: the deployed systemd unit must set server_process_mode to foreground (not DAEMON), so GoDaemon() doesn't fire, and systemd gets a clean foreground process.

One-line mental model

- SysV: "daemonize yourself — fork, detach, write a pidfile — because nobody's watching you."
- systemd: "stay in the foreground and tell me when you're ready — I'm watching you."
 */

#include "data_config_symbols_core_priv.h"
#include "data_config_symbols_adjunct_priv.h"

/**
 * @brief Return the core (ufsrv) config symbol table.
 *
 * @return Pointer to the core config symbol table.
 */
static ConfFileSymbolTableEntry *
sProvideConfigFileCoreSymbolsMap()
{
  return &g_ufsrv_config_symbols[0];
}

/**
 * @brief Return the adjunct (app) config symbol table.
 *
 * @return Pointer to the adjunct config symbol table.
 */
static ConfFileSymbolTableEntry *
sProvideConfigFileAdjunctSymbolsMap()
{
  return &g_app_config_symbols[0];
}

