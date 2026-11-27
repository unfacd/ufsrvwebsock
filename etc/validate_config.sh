#!/bin/bash
# validate_config.sh — validate a ufsrvwebsock config file against all schemas
#
# Usage:
#   ./etc/validate_config.sh [config_file] [--strict]
#   ./etc/validate_config.sh
#   ./etc/validate_config.sh etc/ufsrvwebsock.config.lua
#   ./etc/validate_config.sh etc/ufsrvwebsock.config.lua --strict
#
# Default config (relative to repo): etc/ufsrvwebsock.config.lua
#
# Exit codes:
#   0  config is valid
#   1  validation failed (or missing files)

set -euo pipefail

CONFIG="${1:-etc/ufsrvwebsock.config.lua}"
STRICT=false

for arg in "$@"; do
    case "$arg" in
        --strict) STRICT=true ;;
        -h|--help)
            echo "Usage: $0 [config_file] [--strict]"
            echo "  --strict   reject unrecognised attributes (blacklist mode)"
            exit 0
            ;;
    esac
done
if [[ "${1:-}" == "--strict" ]]; then
    CONFIG="etc/ufsrvwebsock.config.lua"
fi

SCHEMA_CORE="/opt/ufsrv/etc/lua/core_schema_ufsrv.lua"
SCHEMA_ADJUNCT="etc/adjunct_schema_ufsrvwebsock.lua"
VALIDATOR="/opt/ufsrv/etc/lua/core_schema_validator.lua"
LUA="${LUA:-/opt/bin/lua}"

for f in "$CONFIG" "$SCHEMA_CORE" "$SCHEMA_ADJUNCT" "$VALIDATOR"; do
    if [[ ! -f "$f" ]]; then
        echo "error: file not found: $f" >&2
        exit 1
    fi
done

echo "=== Validating $CONFIG ==="
if $STRICT; then
    echo "    mode: strict (unknown attributes are errors)"
else
    echo "    mode: non-strict (unknown attributes are stripped)"
fi

# Config files assign globals (ufsrv=..., ufnet=..., ufsrvwebsock=...) and may
# or may not end with `return { ... }`.  Load into a sandbox env so both styles
# work; prefer an explicit return value when present.
"$LUA" -e "
local v = dofile('${VALIDATOR}')

local function load_config(path)
    local env = {}
    setmetatable(env, { __index = _G })
    local chunk, err = loadfile(path, 't', env)
    if not chunk then
        error('failed to load config: ' .. tostring(err))
    end
    local ret = chunk()
    if type(ret) == 'table' then
        return ret
    end
    local cfg = {}
    for k, val in pairs(env) do
        if type(val) == 'table' and k ~= '_G' then
            cfg[k] = val
        end
    end
    return cfg
end

local cfg = load_config('${CONFIG}')
local opts = { strict = ${STRICT} }

local ok, result = pcall(function()
    return v.validate_config(cfg, '${SCHEMA_CORE}', '${SCHEMA_ADJUNCT}', opts)
end)

if not ok then
    io.stderr:write(' [FAIL] ' .. tostring(result) .. '\n')
    os.exit(1)
end

local scopes = {}
for k, _ in pairs(result) do
    scopes[#scopes + 1] = k
end
table.sort(scopes)
if #scopes == 0 then
    io.stderr:write(' [FAIL] config produced no scopes (empty or unloadable file?)\\n')
    os.exit(1)
end
for _, name in ipairs(scopes) do
    print(' [PASS] ' .. name .. ' scope')
end
print('All scopes validated successfully')
"

echo "Done — config is valid"