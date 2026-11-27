-- config.lua — ufsrvwebsock configuration (namespaced, three-tier)
--
-- Tier 1 — ufsrv scope: inherited by every ufsrv-class server
-- Tier 2 — ufnet scope: servers in the unfacd messaging network
-- Tier 3 — ufsrvwebsock scope: WebSocket-server-specific
--
-- Converted from the legacy flat-global format (etc/ufsrv-1.lua).
-- Validated against core_schema_ufsrv.lua + core_schema_ufnet.lua +
-- adjunct_schema_ufsrvwebsock.lua via core_schema_validator.lua.
--
-- Copyright (C) 2015-2026 unfacd works
-- SPDX-License-Identifier: AGPL-3.0-or-later

ufsrv = {

    server_run_mode         = "shadow",         -- "normal" | "shadow"
    server_cpu_affinity     = "system_override", -- "system_override" | "managed" | "user_override"
    ufsrv_geogroup          = 3,

    listeners = {
        { name = "realtime", protocol = "websocket", address = "127.0.0.1", port = 19701 },
    },
    command_console_port         = 19700,
    command_console_address = "127.0.0.1",

    -- ── Thread pools ─────────────────────────────────────────────────
    session_workers_thread_pool   = 2,
    ufsrv_workers_thread_pool     = 2,
    listener_workers_thread_pool  = 1,

    ssl_command_console = {
            is_enabled = true,
            certificate_file = "ufsrv_console_certificate.pem",
            key_file         = "ufsrv_console.key",
            is_client_certificate_required = false,
            allow_list_god = {"GOD_SHA1", "GOD_SHA2"},
            allow_list_browser = {},
            deny_list = {}
        },

    -- ── SSL — client connections ─────────────────────────────────────
        ssl_command_console_client = {
            certificate_file = "ufsrv_console_client_certificate_key.pem",
            key_file         = "ufsrv_console_client_key.key",
            is_enabled       = true
        },

    -- ── StatsD instrumentation backend ───────────────────────────────
    stats_backend = { address = "127.0.0.1", port = 8125 },

    -- ── User session timeouts (seconds) ──────────────────────────────
    user_timeouts = {
        unauthenticated_timeout = 60,
        connected_timeout       = 120,
        suspended_timeout       = 0,
        locationless_timeout    = 0
    },

    -- ── I/O buffer sizes (bytes) ──────────────────────────────────────
    buffer_sizes = {
        incoming_buffer_size    = 10240,
        incoming_buffer_maxsize = 1024000,
        outgoing_buffer_size    = 1024,
        holding_buffer_size     = 1024
    },

    -- ── Session memory-pool specs ────────────────────────────────────
    memory_specs_for_session = {
        sessions_per_allocation_group  = 1024,
        allocation_groups              = 10,
        allocation_threshold_trigger   = "10%"
    },

     -- ── File loader (concurrent registry + background I/O) ───────────
    fileloader = {
        is_enabled              = true,  -- disabled by default; set to 1 to enable
        registry_size        = 5,
        registry_hot_loaded_size  = 5,
        black_listed_files_registry = {"/etc/passwd", "/var/lib/secret.dat"},
        files_registry = {
            '{"value": "/opt/etc/ufsrv/assets/greeting.txt", "keys": ["greeting.txt"]}',
            '{"value": "/opt/etc/ufsrv/assets/palette.csv",  "keys": ["palette.csv"]}',
            '{"value": "/opt/etc/ufsrv/assets/numbers.txt",  "keys": ["numbers.txt"]}',
            '{"value": "/opt/etc/ufsrv/assets/manifest.json", "keys": ["manifest.json"]}',
            '{"value": "/opt/etc/ufsrv/assets/motd.txt",     "keys": ["motd.txt"]}'
        }
    },

    ratelimit = {
        is_enabled           = true,
        is_bypass_private_ip = true
    },

    -- ── Runtime (privilege drop) ─────────────────────────────────────
    sys_runtime = {
        run_as_user = "",
        chroot      = "/"
    },
    instrumentation_backend = {
                address =  "127.0.0.1" ,
                port    = 8125
    },

    logger = {
            driver = "file",
            prefix = "ufsrvwebsock",
       },

    -- ── Outbound e-mail provider (Mailgun-compatible HTTP API) ─────────
    -- api_key is an enc:v1 envelope produced with dev/encrypt_plaintext/
    -- encrypt_plaintext.py under the instance secret key, and decrypted at
    -- the point of use.  api_url is the local Mailgun-compatible relay:
    -- 172.19.0.1 is the ufsrv-dev bridge gateway, so containers reach the
    -- host-run relay.  Host-run processes use 127.0.0.1 instead.
    email = {
        api_key = "enc:v1:5c4acabedbc7960c5bc7e883:635a29385fc9c2b5e5090e95e0de99a24b247d:4a32286c90f1390ea6f3d24b9c320af7",
        api_url = "http://172.19.0.1:8095/v3/unfacd.io/messages"
    }
}

ufnet = {

    geoip_service = { address = "127.0.0.1", port = 19801 },

    -- ── Message queue (inter/intra-instance messaging) ───────────────
    msgqueue = {
        is_enabled = false,
        address    = "ufsrvmsgqueue.unfacd.com",
        port       = 6380
    },

    db_backend = {
        port     = 19800,
        address  = "db.ufsrv.unfacd.com",
        username = "ufsrv_user",
        password = "c*"
    },

    persistence_backend = {
        address = "ufsrvpersistance.unfacd.com",
        port    = 19705,
        mode    = "tcp",
        timeout = 500000
    },

    cache_backend_usrmsg = {
        address = "usrmsg.cachebackend.unfacd.io",
        port    = 22001,
        mode    = "tcp",
        timeout = 500000
    },

    cache_backend_fence = {
        address = "fence.cachebackend.unfacd.io",
        port    = 32001,
        mode    = "tcp",
        timeout = 500000
    }
}

ufsrvwebsock = {

    -- ── Google Cloud service accounts ────────────────────────────────
    -- Each value is an enc:v1 envelope of the service-account document,
    -- decrypted at the point of use with the instance secret key.
    cloud_service_accounts = {
        gpc_firebase_messaging = "enc:v1:070123ea54704c9928e94142:e4917f829671087d9f21e31f55557dd6ec82629a427c6d55b7c6c02c6e6f9d4c68e662993574512720ac9f7a148aa18342594d242ffb1fd84656d9649675b30ecf36fd80758417313984429f95bcf5fe44e03691f5f97c9785952590e765872d125555a105066af0f669fee7ffbf9b6b99cabfc61d157c65978c9a35dfe0b6362199f836b76ba53cb02877e47f9ea08856fa1be438fd90f735ed633ace362d67e1ebb03a9b3fe5920dcd4529a1f8de6e43f9f27405649d129dd7ff6950f953a6a817c6d1b544a2fc04698e058b2b3d53064f32c8cb183a608bc5d719bcdad57c1b66a2c361c3f1d1ba8502c020d7d477a8b0126e99264377ac75e8bb3d150c2d43c38a00a1b82b5f7df68518410f8c0f3f02d5f245c3851dc9c3c610f5634e7d4254a91d7c747501c420a38f1df1be069e8d45781bcbb5a2881709e91a480580c37b0b0c15eb35001c468435955054b17cf61532f5ed14c04973d61740088b2e0fee2c38118e0a19f0060403209a2be887109c7ac348f7e4fed7b3b362d383267ae5e2cc06475b48bc151a3dce4bb645c2ecf4020d292873cba1cb9fcb39e885950c685de76a865320ba9f14c05132aa4eee4c7625242411b8094951b3def6a7c9f729ef01230733c814b9c8edbeef26ebf41d14ac9f6aede8d818d5105d86cf0f456fd3efebd9efdd89e9ae099d365bb402aae0e8200ce6a9bd3fd1f57499836387b9e67e86ea27cd48ff0b4f594836bd72ab99bd0825b6b9103b7d537b4b8034f3e40ba935a994cb616519a02ff7bad6fa28c6b1be1e1f05857c03f7bd2e06813f76711351be444e39ea3b870346511c26632980e8337583cf2280ec8ca3167ca6c0b1913183a87cef28233ab16e09696d0131d1e9f60ef234b000cab5b9bb96c1bcd07f70092cc68c3216dc093bc7429e2febc7f51253a669e101b8e5ced6674d750548d417cfc6fdc4cfd11bee96c7414fe2d0ec66774465a350de1726f7ecad4f3fc96e3e1a38c0f65033262bf4ad35d7c56d959e1f5337204b4eaff37a178de4472a455c0978f66e3c07195ae9ec97aa72a0abcc7bd64ed0e2fd4cb19ac0aa50b721beaf25e15d5be56bf4d690fb6709bc57884e4f2d1457ee11ad6a56f6e86a02dd0d3146dbccac035d3320e52ba51dcec59a240af3943cc2e1a7774400d5fb6a9effa51869c250b6e2ff83d44e6aee0655bd0fecb3cb47473373e2732dbbfae5229034531b6cc83e2188bc899865cf9b6d5fa3591ff2a868d3b0b96cc360443d4fbffce10e2c7c29343c228b98e416f660d8212dcbfe6e8fd50c181cc009a0f0581a4e1444e53d16d914fa8ae22598aa38b9105b880296236b66982d1d0e93e5806e93df71372b7dde929565cf58c5882290871ff32e21a60af87325ed0190bfeb4a969918d13e207f917566a27dce09de1a77511657b56e8a0bd46443ba0bd1d3e5d90fcb322aae54bf26b554308bda5da14d6739a3cab50489e910f4e4e5ad3c9565d56169f81b3eb3b15a7e0eeba4822d19a1318484cb16f58fccde9e508efe908615476295d9871f2e33f38229222fd55fa36193d34bb103fd786eb92fbc76b6d7c35f40adc476df8cd77d75edb1673b589a9b839ebfff67eebd7ca5cde47eadcd75ea3405f8846234f71c4bb46305190a8fdfd52aea20a4eef255d4f158321b747d5c80fb0e48307ef7636fb536d3c86f5afa42253fbd06af2458d50f7eb04cdb3425d78c867d802fcadea0927d9ec639a356469d5a24c2478374198803edd3103a47119e08cb66adea22ab12c9458cea2945509b50e4296b586853dbba0ef9f83c98cd0ed9effb25b0919e484e219f4c83ea31db6f74a408b3acdd6e2124bfe53dcb9f4877e15c73238157e99849cc70fc14567b55b6d0a2b3b66a7c5f011e81748b220a9feb8867fac826909f800dfd23abe338147c02827de8c75b52cb8a466a1c39388ba9a049f0865df2d319d9ac15213bff4f902f90e755fe0764dc0ff006ca3abea4166686b784ef43c9aa987851a74f9a9d6de43e5f19e3810d90e59e1532fd83718b208d449d1126cdd9e9ecc22664b971b996a1b8d5a5630cccc8bb6fea09f71d7b5c7f8b503f806a1e3367763996615aacdcae6416f0b39d15f5a4dcbe8d6487026d55d55a66a64f8b8b12e768e0a0b522bc04d1c800733a47403884ebc22382860aa80c9929b82c4b24faf7a74d55010b088b6bfa09d9f90cbc81ef8d86fb80e02cac6cd944d9b65df50d9aca896b95f3a148460d383638d11e35a281ecf4156e0836161dd35b771c20afe8df01c463701e3ce59ef780d39d9d12c9b3d5819f57d6202292247948f6f014c37cdabd8a2e49255a54f8c8edd2905c90b2d4d15f67e6e482ff8e1db7995715d8b3c4335922ade0140112360f063e41133c18de0a9c890ea4a670c0bd8a8051848b6295ec4d490cd3556a506a89a2dd25c2106ad2fa5b319a4a1e0cfbd8885dfe5eb4875a54200734c6c85762e8d90df159862bd76f52e92705a73d5b7841337e21398ba13ab039d5b397fc6d85c9f054e529301c7932357e680aaf3a0bdeee4704e14b1fd658453733aa6f402f6aaa76e69ea2a1e562d377cb360f69f8e71c46600b363cd27bbb72fd8b8f6f41831c2796353ec53cad9412c8036616db6fcc889545e497eb3f0e8f62a1d842b42a538269e6abc032c3bed56d05a57bde0634492057658357f42e8e5646a5cd468c4a43dbf347bf10be73eb71906dce5818b85e1858c5d5b404a083dd6bd67ebb79886772202ff3e7ef505199e421dbeaa07806066b28bc16a86c40c17c77a5b773f23b78f835f028b981a2abc5db963025edd4e3abd182cbe3fba887d9fd0384fc2d65f56c5364ac9c98a9a4:7a15b1fdeba9b382920340ba0a6dc378",
        gpc_integrity          = "enc:v1:fe962f71ada49979880438fb:c131d293162a535d0dcabd2de381e2140d46dd8b6be94ea11e68d491bc16c8aac09a0e7c6a788a025d3f93db35d0a721665590d0cefb1ba11b6ee8dfc90d81ff81fea540f153939bcc11fb4efd43245afcfe4e9d9dfa124507e89f64c184638dfdd3e469a3205ccfc6f14c2f239fb7893d8c311c41f0db304f68bdba4ee990c81ef1755a64b19a12d97ea7a968c13edea828d0b72697fe0a762b9d368e397b6bc62c5f25b5060fd194d2f65fcf3bf15cf2d9e658ed91c25ac26fd0a2421f2198138d75bbb0a9e4f8202a723132b32d395b67ee14f6118d82ce919d5e08dbc188a43c409fd5dd370a43d3b6f373746718dba2f633f1b8f1c32ff093ced001e25975e2068264b6a990c7a6cc17359b2594473e4ced1a2bf409f4262ee6d94d12a8e6a759179af9bfa2dd9d269c5042d94a97aea02351f908556c33c9aa371abec0cdde16c7f1c91b0b7489384cb45f406ac7dab29933ec99bb8a176a3d695649b5a2edb92d6ab723c1f844b1b11d4700c22c241f7415baa543842c9e49cfc2cf3b026073364c2d542e98f815e45a6408fc0ae9c8966c11a320bd75275c2ce0eb07c860bfcebf817118d6252da1cc7a3a02acd48c54128883af1b7ad1400e50cc6f5e04c86012117653f7fac2a4b0a27ad2d6f478a3fd65a244c8f6648d3abca4b63087e996086a9db4f3cf36d5d3c7c9cf78cee2e700fd7bb4f306c50c56b96502fa750f3b75bc8a556888c1cca2eadb9680c2efa5424ed5c27881b9c6842298cb2c531ad4b6ad52b0caf51efae5da64b8251f19db838305fdde8926b15acaab30df05af22035a4a7ae19aa2056422d7eaf7ca5ecb6934b8ba47faa304491a1c6146db4439beee87006d876058fbc039096734c5acde2132dd3ab963f66cfb834a82419bded0651fd358ec7667250f51c97a26c68f72fe2e8ef93faf27a19454c49d20f55e7ae7395e68a0bacb2a252c3129ff2a01bb868bd89fda8de7f7a8f8d1768b32b387d163ab1a889bf3c6275c5550b7f452d23b891f5f923f7b22e14338542748a557ff90be6b9bb1d2d27dac5bc5cbc64168903311bddd4e970e72d08793d0e144a86e9fd24467ca51485ba2484c93eef9be45cca8a61bb2999ed956902d7baa9ccd00bf693581c541a81bfe127eafa43ce91dbb45b4f2d452d206178cd32219ee04a8c52dfbfec774d87e1bf08fa32ddefb03ca005af199a1406634c3b20a2fd934d984feb8a47a28d99ba082c4ad45bf847318fa5cdcbc266d8eb8e29f357a942b98a6c6878f463ac072d38a25d8f27ec908f311fbd6879be1377ba55bba6f8081be3603e5cfe3e9e4e5188fd64cdbf3802188a9ccd989cb9134e1537b09e4172197a88b05ecc4cbd8b901904ec309e313d15645d0050bc6ba69cfc8c358e38141743eaa50c805ec681de806cd4259a1e4b0e82331107cc488b73e48286e8736057bb08d1ec7d025ef8e4b22516ba8f6be4223576e36f5e2961b4ab6c0a5b4306a154040baac5cf40316d3d82b42d24e2eac7bf7a134504c78acbd88fde8ed6989e2c8aaf972ef0ab82765ded48a04a98b55a2f7ca5e532ec28de36448dd4de090e2a4e00701d6d77a76b92f936bb38ca45882831d039d4179371521f1d7860b09ad3c37df0bb08931d8cdf5966e045fecab81fd9b35cc8c069c8e5d8d6661dd7c0df001088d8f2882c496eba90957c77c4c87001ec31e9f6767cdfbbc0a9146475cd7b2867707dac9107681a3bf9f9ba9e37a013a6c8a7f37828aadb19b5ff37d59c69771306decf1c7805c83e207a75e721972a5fd514eae534680fd6ad21ee7ef44bad8ffab378c04a7b1db3d10f9883951110bdb94b3d5fdc842d8c9f1ea284004cbec7699aa4c3d25c75a9e9ac2bee718ea5e301fbc1f86ba55231ce18ff74877ca302c36b7b1f2b2d410532ec2b29e1f87af43d30eca98765ad1b8e05ed0093057922257013f798cac004221174a3dc4f88301fdea7391d287f08269dc5dd0e606ac04de0ecf5ba82302a2f0a230040615ab644beb1dbeb58f31fd799bf7443394120ad7e857dc67b81cbfb225efbc9795da69e6ed385e97dce9c9b22bdfb97e0f1af6a224ea726c33a3a25ae6237abb5f9be6a3a27d293efaad57d64e7e207ecc4fb0bce7dad601b0a33a15b3324982dfc980b4a7f1320ae0b8baf57cd27f0b0d777525f55c224b101bbe25c4c83ce01078716d3678e96ca74d66c9c77f44552f802bdbfb7cca8a210746811e8a01045882671771514d3146e8adf62d7dc2431c1250272e4e2bb9e456657064e3aa59188472bfe0f38c02ebd1bfdebee449f3dc45ed710a143c5759a5b57c99dc9f7373246df50f657bfbfd45542f6039e7bd034babb25be55da0c78b7a755b01d9284ec7eb6df4e0c335fa3f1bbac37b82019b935db5369acc48c266e35c3b85573d465e0f6c5d23cfe093ee6915c654eb24cc0178845905854fc61d1db2cc77f051f885a80f7c8563357573fe6ad82d564d3c65bbae4485fdc5f587a7f58d2498c2153920fd9c91242c1862e44d5a9e8227dad4f58069500b522fa53df7aed555318a192fd8be823287bc637568e179a00e21c65d0d83e212bd7560d8b2163e3472322a92230a4d1b497f9bc9bc390ac762ce3db449498c77b3c8aa2420f18217d28be390199cf2c5b1f16891f26a0efd9f0a0f9ea23a542284dea91c5af65f4aea042d715e6b1d110b78cc8bb5a30cea0f2d620b5ee5c9061c9c511b7e5a02020c8a4adf68a551d79542671d9409b4daba937e74d8425fc7579ebc719190f1ab669f940dca86b7beb9286edd5d9cfdae8169097d2d0fa49a8f3e3d5b4b01eb918e1a10a79f54ff5cd18664ccfe0c2c9cfb8050be123ac5:220c5cd1e30bb73495cd0081c490c5eb"
    },
    websocket = {
         max_frame_size              = 65536,
         max_message_size            = 1048576,
         heartbeat_interval_secs     = 30,
         close_timeout_secs          = 10,
         max_connections_per_session = 1
    }
}

-- optional but good practice
return {
  ufsrv = ufsrv,
  ufnet = ufnet,
  ufsrvwebsock = ufsrvwebsock,
}
