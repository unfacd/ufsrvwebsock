-- /opt/redis/redis-cli -p 22001 SCRIPT LOAD "$(cat redis_fence_claim.lua)"
--
-- Per-fence mutation claim. One claim per fence record, held for the duration of a mutation,
-- released by token; the deadline is the crash backstop, never the mechanism.
--
-- KEYS[1] - UF:<fid>                    record; its existence gates every claim
-- KEYS[2] - UF_LOCK_CLAIM               zset, member = fid, score = deadline in ms
-- KEYS[3] - UF_LOCK_CLAIM_META:<fid>    hash: token, serverid, geogroup, reqid, deadline
-- ARGV[1] - mode: claim | renew | release | inspect | reap
-- ARGV[2] - fid
-- ARGV[3] - token
-- ARGV[4] - ttl in ms
-- ARGV[5] - serverid       } claimant identity, returned to whoever is refused
-- ARGV[6] - ufsrv_geogroup }
-- ARGV[7] - reqid
-- ARGV[8] - reap limit (reap only)
--
-- Reply: { status, fid, token, deadline, serverid, geogroup, reqid }
--   status: granted | already_locked | no_such_fence | not_holder | released | free | held | reaped

local rec  = KEYS[1]
local zkey = KEYS[2]
local mkey = KEYS[3]

local mode = ARGV[1]
local fid  = ARGV[2]

-- TIME first: no write may follow a non-deterministic command on the older replica paths
local t     = redis.call('TIME')
local now   = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)

local function holder()
  local deadline = tonumber(redis.call('ZSCORE', zkey, fid))
  if not deadline then return nil end

  local meta = redis.call('HMGET', mkey, 'token', 'serverid', 'geogroup', 'reqid')

  return deadline, meta
end

local function reply(status, deadline, meta)
  meta = meta or {}

  return { status, fid,
           meta[1] or '',
           tostring(deadline or 0),
           meta[2] or '',
           meta[3] or '',
           meta[4] or '' }
end

if mode == 'claim' then
  if redis.call('EXISTS', rec) == 0 then return reply('no_such_fence') end

  local deadline, meta = holder()

  if deadline then
    if deadline > now then return reply('already_locked', deadline, meta) end

    -- expired: the holder never released, drop it and let this claimant take over
    redis.call('ZREM', zkey, fid)
    redis.call('DEL', mkey)
  end

  local expires = now + tonumber(ARGV[4])

  redis.call('ZADD', zkey, expires, fid)
  redis.call('HSET', mkey,
             'token',    ARGV[3],
             'serverid', ARGV[5],
             'geogroup', ARGV[6],
             'reqid',    ARGV[7],
             'deadline', expires)

  return reply('granted', expires, { ARGV[3], ARGV[5], ARGV[6], ARGV[7] })

elseif mode == 'renew' then
  local deadline, meta = holder()

  if not deadline then return reply('not_holder') end
  if meta[1] ~= ARGV[3] then return reply('not_holder', deadline, meta) end

  local expires = now + tonumber(ARGV[4])

  redis.call('ZADD', zkey, expires, fid)
  redis.call('HSET', mkey, 'deadline', expires)

  return reply('granted', expires, meta)

elseif mode == 'release' then
  local deadline, meta = holder()

  if not deadline then return reply('not_holder') end
  if meta[1] ~= ARGV[3] then return reply('not_holder', deadline, meta) end

  redis.call('ZREM', zkey, fid)
  redis.call('DEL', mkey)

  return reply('released', deadline, meta)

elseif mode == 'inspect' then
  local deadline, meta = holder()

  if not deadline then return reply('free') end
  if deadline <= now then return reply('free', deadline, meta) end

  return reply('held', deadline, meta)

elseif mode == 'reap' then
  local limit   = tonumber(ARGV[8])
  local expired = redis.call('ZRANGEBYSCORE', zkey, '-inf', now, 'LIMIT', 0, limit)
  local count   = 0

  for i = 1, #expired do
    local stale_fid = expired[i]

    redis.call('ZREM', zkey, stale_fid)
    redis.call('DEL', 'UF_LOCK_CLAIM_META:' .. stale_fid)
    count = count + 1
  end

  return { 'reaped', tostring(count), '', '', '', '', '' }
end

return { 'bad_mode', mode, '', '', '', '', '' }
