//
// Created by yuhang on 2025/5/17.
//

#include "tianyan_plugin.h"
#include "version.h"
#include "world_inspector.h"
#include <thread>
#include "global.h"
#include "tianyan_core.h"
#include "database_util.h"
#include "sqlite_backend.h"
#include "rust_backend.h"
#include <inventoryui_init.h>
#include "webui_adapter/log_query_impl.h"
#include "webui_adapter/language_impl.h"

// 为了方便在宏里调用，定义一个短函数
inline std::string T(const std::string& key) {
    return StaticTranslate::get(key);
}

//插件信息
ENDSTONE_PLUGIN("tianyan_plugin", TIANYAN_PLUGIN_VERSION, TianyanPlugin)
{
    description = "A plugin for endstone to record behavior";
    website = "https://github.com/yuhangle/endstone-tianyan-plugin";
    authors = {"yuhangle"};


        command("ty")
            .description(T("Check behavior logs at your current location"))
            .usages("/ty",
                    "/ty <r: float> <time: float>",
                    "/ty <r: float> <time: float> <source_id | source_name | target_id | target_name> <keywords: str>",
                    "/ty <r: float> <time: float> <action> <block_break | block_place | entity_damage | player_right_click_block | player_right_click_entity | entity_bomb | block_break_bomb | piston_extend | piston_retract | entity_die | player_pickup_item | player_drop_item | block_bomb | liquid_flow>"
                    )
            .permissions("ty.command.member");

        command("tyback")
            .description(T("Revert behaviors by time"))
            .usages("/tyback",
                    "/tyback <r: float> <time: float>",
                    "/tyback <r: float> <time: float> <source_id | source_name | target_id | target_name> <keywords: str>",
                    "/tyback <r: float> <time: float> <action> <block_break | block_place | player_right_click_block | block_break_bomb | entity_die>"
                    )
            .permissions("ty.command.op");

        command("tys")
            .description(T("Check behavior logs at server"))
            .usages("/tys",
                    "/tys <time: float>",
                    "/tys <time: float> <source_id | source_name | target_id | target_name> <keywords: str>",
                    "/tys <time: float> <action> <block_break | block_place | entity_damage | player_right_click_block | player_right_click_entity | entity_bomb | block_break_bomb | piston_extend | piston_retract | entity_die | player_pickup_item | player_drop_item | block_bomb | liquid_flow>"
                    )
            .permissions("ty.command.op");

        command("ban-id")
            .description(T("Ban player by device id"))
            .usages("/ban-id <device-id: str> [reason: str]"
                    )
            .permissions("ty.command.op");

        command("unban-id")
            .description(T("Unban player by device id"))
            .usages("/unban-id <device-id: str>"
                    )
            .permissions("ty.command.op");

        command("banlist-id")
            .description(T("List baned player by device id"))
            .usages("/banlist-id"
                    )
            .permissions("ty.command.op");

        command("tyclean")
            .description(T("Clean database"))
            .usages("/tyclean <time: int>"
                    )
            .permissions("ty.command.op");

        command("tyo")
            .description(T("View inventory of online player"))
            .usages("/tyo <player_name: player>"
                    )
            .permissions("ty.command.op");

        command("density")
            .description(T("Find the area with the highest entity density"))
            .usages("/density [size: int]"
                    )
            .permissions("ty.command.op");

        command("tymigrate")
            .description(T("Migrate database between SQLite and MySQL"))
            .usages("/tymigrate (sqlite|mysql)<opt_tym1> (sqlite|mysql)<opt_tym2>"
                    )
            .permissions("ty.command.op");

    permission("ty.command.member")
            .description(T("Allow users to use the /ty command."))
            .default_(endstone::PermissionDefault::True);
    permission("ty.command.op")
        .description("OP command.")
        .default_(endstone::PermissionDefault::Operator);
}

    //数据目录和配置文件检查
void TianyanPlugin::datafile_check() const {
    // 按逻辑分组排列 key 顺序
    ordered_json df_config = {
        // ---- 基础设置 ----
        {"language", "zh_CN"},
        // ---- 数据库 ----
        {"database_type", "sqlite"},
        {"mysql_host", "127.0.0.1"},
        {"mysql_port", 3306},
        {"mysql_user", "root"},
        {"mysql_password", ""},
        {"mysql_database", "endstone"},
        // ---- WebUI ----
        {"enable_web_ui", false},
        {"web_secret", "your_secret"},
        {"web_port", 8098},
        // ---- 反刷屏 ----
        {"10s_message_max", 6},
        {"10s_command_max", 12},
        // ---- 实体过滤 ----
        {"no_log_mobs", {"minecraft:zombie_pigman","minecraft:zombie","minecraft:skeleton","minecraft:bogged","minecraft:slime"}},
        // ---- 配置记录项 ----
        {"enforce_no_log_mobs", false},
        {"log_piston", true},
        {"log_block_bomb",true},
        {"log_entity_bomb",true},
        {"log_block_break",true},
        {"log_block_place",true},
        {"log_entity_damage",true},
        {"log_player_right_click_block",true},
        {"log_player_right_click_entity",true},
        {"log_entity_die",true},
        {"log_player_pickup_item",true},
        {"log_player_drop_item",true},
        {"log_liquid_flow",true}
    };

    auto write_config = [&](const ordered_json& config) {
        if (std::ofstream file(TianyanCore::config_path); file.is_open()) {
            file << config.dump(4);
            file.close();
            return true;
        }
        return false;
    };

    if (!(std::filesystem::exists(TianyanCore::dataPath))) {
        getLogger().info(Tran->getLocal("No data path,auto create"));
        std::filesystem::create_directory(TianyanCore::dataPath);
        if (!(std::filesystem::exists(TianyanCore::config_path))) {
            if (write_config(df_config)) {
                getLogger().info(Tran->getLocal("Config file created"));
            }
        }
    } else if (std::filesystem::exists(TianyanCore::dataPath)) {
        if (!(std::filesystem::exists(TianyanCore::config_path))) {
            if (write_config(df_config)) {
                getLogger().info(Tran->getLocal("Config file created"));
            }
        } else {
            bool need_update = false;
            ordered_json loaded_config;

            // 加载现有配置文件
            std::ifstream file(TianyanCore::config_path);
            file >> loaded_config;

            // 以 df_config 定义顺序为基准重建，缺失的 key 插入到对应分组位置
            ordered_json merged;
            for (auto& [key, value] : df_config.items()) {
                if (loaded_config.contains(key)) {
                    merged[key] = loaded_config[key];
                } else {
                    merged[key] = value;
                    getLogger().info(Tran->tr(Tran->getLocal("Config '{}' has update with default config"), key));
                    need_update = true;
                }
            }
            // 追加用户自定义的额外字段（不在 df_config 中）
            for (auto& [key, value] : loaded_config.items()) {
                if (!merged.contains(key)) {
                    merged[key] = value;
                }
            }
            loaded_config = std::move(merged);

            // 如果需要更新配置文件，则进行写入
            if (need_update) {
                if (write_config(loaded_config)) {
                    getLogger().info(Tran->getLocal("Config file update over"));
                }
            }
        }
    }
    if (!std::filesystem::exists(TianyanCore::language_path))
    {
        std::filesystem::create_directory(TianyanCore::language_path);
    }
    // 数据迁移
    migrateOldBanData();
}

void TianyanPlugin::migrateOldBanData()
{
    filesystem::path oldFile = filesystem::path(TianyanCore::dataPath) / "banidlist.json";
    filesystem::path newFile = filesystem::path(TianyanCore::dataPath) / "ban-id.json";

    if (filesystem::exists(newFile)) {
        std::cout << "[Tianyan] New ban data exists, skip migration.\n";
        return;
    }

    if (!filesystem::exists(oldFile)) {
        return;
    }

    json oldJson;
    {
        std::ifstream in(oldFile);
        if (!in.is_open()) {
            std::cerr << "[Tianyan] Failed to open old ban data.\n";
            return;
        }
        in >> oldJson;
    }

    json newJson = json::object();

    for (auto& [uuid, value] : oldJson.items()) {
        // 跳过空ID
        if (uuid.empty()) continue;

        std::string timeStr;
        if (value.contains("timestamp") && value["timestamp"].is_string()) {
            timeStr = value["timestamp"].get<std::string>();
            if (size_t dotPos = timeStr.find('.'); dotPos != std::string::npos) {
                timeStr = timeStr.substr(0, dotPos);
            }
            for (char& c : timeStr) {
                if (c == 'T') c = '-';
            }
        }

        newJson[uuid] = {
            { "player_name", "" },
            { "reason", "" },
            { "time", timeStr }
        };
    }

    std::ofstream out(newFile);
    if (!out.is_open()) {
        std::cerr << "[Tianyan] Failed to write new ban data.\n";
        return;
    }
    out << newJson.dump(4);

    std::cout << "[Tianyan] Ban data migration completed.\n";
}

// 读取配置文件
[[nodiscard]] ordered_json TianyanPlugin::read_config() const {
    std::ifstream i(TianyanCore::config_path);
    try {
        ordered_json j;
        i >> j;
        return j;
    } catch (ordered_json::parse_error& ex) { // 捕获解析错误
        getLogger().error( ex.what());
        ordered_json error_value = {
                {"error","error"}
        };
        return error_value;
    }
}

// 获取 BDS 服务端根目录
std::string TianyanPlugin::getServerRoot() {
    const auto data_folder = std::filesystem::absolute(TianyanCore::dataPath.data()).string();
    auto server_root = std::filesystem::path(data_folder).parent_path().parent_path().string();
    return server_root;
}


#ifdef _WIN32
    // Windows 下使用 HANDLE 记录进程
    static HANDLE g_web_handle = nullptr;
#else
    #include <unistd.h>
    #include <csignal>
    #include <sys/types.h>
    // Linux 下使用 pid_t
    static pid_t g_web_pid = 0;
#endif

namespace fs = std::filesystem;

void start_web_server(const std::string& pluginDir) {
    const fs::path base_path = fs::absolute(pluginDir).parent_path();
    const fs::path py_script = base_path / "WebUI" / "server.py";

    // 构造命令
#ifdef _WIN32
    const std::string cmd = "python \"" + py_script.string() + "\"";
#else
    const std::string cmd = "python3 " + py_script.string();
#endif

#ifdef _WIN32
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    // 创建一个临时 buffer
    char* cmd_buf = _strdup(cmd.c_str());

    if (CreateProcessA(nullptr, cmd_buf, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        g_web_handle = pi.hProcess; // 保存进程句柄用于后续关闭
        CloseHandle(pi.hThread);    // 直接关闭
        std::cout << "[Tianyan] WebUI started (PID: " << pi.dwProcessId << ")" << std::endl;
    } else {
        std::cerr << "[Tianyan] Failed to start WebUI. Error: " << GetLastError() << std::endl;
    }
    free(cmd_buf);
#else
    g_web_pid = fork();
    if (g_web_pid == 0) {
        setsid();
        execlp("python3", "python3", py_script.c_str(), NULL);
        _exit(1);
    }
#endif
}

void stop_web_server() {
#ifdef _WIN32
    if (g_web_handle != nullptr) {
        // 尝试关闭进程
        if (TerminateProcess(g_web_handle, 0)) {
            std::cout << "[Tianyan] Web server process terminated." << std::endl;
        } else {
            std::cerr << "[Tianyan] Failed to terminate web server. Error: " << GetLastError() << std::endl;
        }
        CloseHandle(g_web_handle); // 释放句柄资源
        g_web_handle = nullptr;
    }
#else
    if (g_web_pid > 0) {
        if (kill(g_web_pid, SIGTERM) == 0) {
            std::cout << "[Tianyan] Web server (PID: " << g_web_pid << ") terminated." << std::endl;
        } else {
            kill(g_web_pid, SIGKILL);
        }
        g_web_pid = 0;
    }
#endif
}
#ifdef _WIN32
void TianyanPlugin::dump_webui_log_once() const
{
    const string ready_file = string(TianyanCore::dataPath) + "/WebUI/ready";
    if (!filesystem::exists(ready_file))
    {
        return;
    }
    const string log_path = string(TianyanCore::dataPath) + "/logs/webui.log";
    std::ifstream file(log_path);
    if (!file.is_open()) {
        std::cerr << "[Tianyan] webui.log not found." << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::cout << line << std::endl;
    }
    std::error_code ec;
    std::filesystem::remove(ready_file, ec);
    getServer().getScheduler().cancelTask(windows_print_webui_log->getTaskId());
}
#endif

void TianyanPlugin::default_init_sqlite_() {
    // 默认使用 SQLite
    namespace fs = std::filesystem;
    try {
        const fs::path currentPath = fs::current_path();
        const fs::path fullPath = currentPath / TianyanCore::dbPath;
        db_backend_ = std::make_unique<SqliteBackend>(fullPath.string());
    } catch (const std::exception& e) {
        getLogger().error("Failed to create SQLite backend: {}", e.what());
        getServer().getPluginManager().disablePlugin(*this);
        return;
    }

    // 初始化数据库
    if (db_backend_->init_database() != 0) {
        getLogger().error("Database initialization failed!");
        getServer().getPluginManager().disablePlugin(*this);
        return;
    }
    tyCore = std::make_unique<TianyanCore>(*db_backend_);
    is_db_over = true;
}

void TianyanPlugin::onLoad()
{
    getLogger().info("onLoad is called");
    //初始化目录
    if (!(filesystem::exists(TianyanCore::dataPath))) {
        getLogger().info("No data path,auto create");
        filesystem::create_directory(TianyanCore::dataPath);
    }
    //获取服务器语言
    const string sever_lang = getServer().getLanguage().getLocale();
    TianyanCore::language_file = string(TianyanCore::dataPath) + "/language/"+sever_lang+".json";
    Tran = std::make_unique<translate>(TianyanCore::language_file);
    //加载语言
    const auto [fst, snd] = Tran->loadLanguage();
    getLogger().info(snd);
}

void TianyanPlugin::onEnable()
{
    getLogger().info("onEnable is called");

    // 确保目录和配置文件存在
    datafile_check();

    // 读取配置文件选择数据库后端
    ordered_json json_msg = read_config();
    try {
        if (json_msg.contains("database_type")) {
            db_type_ = json_msg["database_type"].get<std::string>();
        }
    } catch (const std::exception&) {}

    // 创建数据库后端
    if (db_type_ == "mysql") {
        RustMySQLConfig config;
        try {
            config.host = json_msg.value("mysql_host", std::string("127.0.0.1"));
            config.port = json_msg.value("mysql_port", 3306);
            config.user = json_msg.value("mysql_user", std::string("root"));
            config.password = json_msg.value("mysql_password", std::string(""));
            config.database = json_msg.value("mysql_database", std::string("endstone"));
        } catch (const std::exception& e) {
            getLogger().warning(std::string("MySQL config parse error: ") + e.what() + ", using defaults");
        }

        if (auto rust_backend = std::make_unique<RustBackend>(config); rust_backend->connected()) {
            db_backend_ = std::move(rust_backend);
            getLogger().info(Tran->getLocal("Using Rust MySQL database backend"));

            if (db_backend_->init_database() != 0) {
                getLogger().error(Tran->getLocal("MySQL database table initialization failed!"));
                getServer().getPluginManager().disablePlugin(*this);
                return;
            }
            is_db_over = true;
        } else {
            getLogger().warning(Tran->getLocal("Failed to connect to MySQL, falling back to SQLite database"));
            default_init_sqlite_();
        }
    }
    else if (db_type_ == "sqlite") {
        default_init_sqlite_();
    }

    // 清理上次可能的残留清理状态（服务端意外退出时遗留）
    if (yuhangle::clean_data_status == 2) {
        yuhangle::clean_data_status = 0;
        yuhangle::clean_data_message.clear();
        yuhangle::clean_data_sender_name.clear();
        yuhangle::clean_data_total = 0;
        yuhangle::clean_data_progress = 0;
    }

    // 创建核心逻辑层
    tyCore = std::make_unique<TianyanCore>(*db_backend_);

    // 读取剩余配置
    //预先设置默认值
    TianyanCore::max_message_in_10s = 6;
    TianyanCore::max_command_in_10s = 12;
    TianyanCore::no_log_mobs = {"minecraft:zombie_pigman","minecraft:zombie","minecraft:skeleton","minecraft:bogged","minecraft:slime"};
    try {
        string lang = "en_US";
        if (!json_msg.contains("error")) {
            TianyanCore::max_message_in_10s = json_msg["10s_message_max"];
            TianyanCore::max_command_in_10s = json_msg["10s_command_max"];
            TianyanCore::no_log_mobs = json_msg["no_log_mobs"];
            lang = json_msg["language"];
            TianyanCore::language_file = TianyanCore::language_path +lang+".json";
            TianyanCore::enable_web_ui = json_msg["enable_web_ui"];
            TianyanCore::config_enforce_no_log_mobs = json_msg["enforce_no_log_mobs"];
            TianyanCore::config_log_piston = json_msg["log_piston"];
            TianyanCore::config_log_block_bomb = json_msg["log_block_bomb"];
            TianyanCore::config_log_entity_bomb = json_msg["log_entity_bomb"];
            TianyanCore::config_log_block_break = json_msg["log_block_break"];
            TianyanCore::config_log_block_place = json_msg["log_block_place"];
            TianyanCore::config_log_entity_damage = json_msg["log_entity_damage"];
            TianyanCore::config_log_player_right_click_block = json_msg["log_player_right_click_block"];
            TianyanCore::config_log_player_right_click_entity = json_msg["log_player_right_click_entity"];
            TianyanCore::config_log_entity_die = json_msg["log_entity_die"];
            TianyanCore::config_log_player_pickup_item = json_msg["log_player_pickup_item"];
            TianyanCore::config_log_player_drop_item = json_msg["log_player_drop_item"];
            TianyanCore::config_log_liquid_flow = json_msg["log_liquid_flow"];
        } else {
            getLogger().error(Tran->getLocal("Config file error!Use default config"));
        }
    } catch (const std::exception& e) {
        getLogger().error(Tran->getLocal("Config file error!Use default config")+","+e.what());
    }
    Tran = std::make_unique<translate>(TianyanCore::language_file);
    Tran->loadLanguage();
    //定期写入
    getServer().getScheduler().runTaskTimer(*this, [&]() {logsCacheWrite();},0,60);
    //数据库清理后台检查
    getServer().getScheduler().runTaskTimer(*this,[&](){checkDatabaseCleanStatus();},0,20);
    //数据库迁移状态检查
    getServer().getScheduler().runTaskTimer(*this,[&](){checkMigrateStatus();},0,20);
    //后台查询任务检查
    getServer().getScheduler().runTaskTimer(*this,[&](){checkAsyncTasks();},0,20);
    //完成外部类初始化
    protect_ = std::make_unique<TianyanProtect>(this, Tran.get());
    eventListener_ = std::make_unique<EventListener>(this, Tran.get());
    menu_ = std::make_unique<Menu>(this, Tran.get());
    protect_->deviceIDBlacklistInit();

    //初始化在线玩家
    eventListener_->initOnlinePlayers();
    //注册事件
    registerEvent<endstone::BlockBreakEvent>(EventListener::onBlockBreak, endstone::EventPriority::Monitor);
    registerEvent<endstone::BlockPlaceEvent>(EventListener::onBlockPlace, endstone::EventPriority::Monitor);
    registerEvent<endstone::ActorDamageEvent>(EventListener::onActorDamage, endstone::EventPriority::Monitor);
    registerEvent<endstone::PlayerInteractEvent>(EventListener::onPlayerRightClickBlock, endstone::EventPriority::Monitor);
    registerEvent<endstone::PlayerInteractActorEvent>(EventListener::onPlayerRightClickActor, endstone::EventPriority::Monitor);
    registerEvent<endstone::ActorExplodeEvent>(EventListener::onActorBomb, endstone::EventPriority::Monitor);
    registerEvent<endstone::BlockExplodeEvent>(EventListener::onBlockBomb, endstone::EventPriority::Monitor);
    registerEvent<endstone::BlockPistonExtendEvent>(EventListener::onPistonExtend, endstone::EventPriority::Monitor);
    registerEvent<endstone::BlockPistonRetractEvent>(EventListener::onPistonRetract, endstone::EventPriority::Monitor);
    registerEvent<endstone::ActorDeathEvent>(EventListener::onActorDie, endstone::EventPriority::Monitor);
    registerEvent<endstone::PlayerDeathEvent>(EventListener::onPlayerDie, endstone::EventPriority::Monitor);
    registerEvent<endstone::BlockFromToEvent>(EventListener::onBlockFromTo, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerPickup, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerDropItem, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerJoin, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerLeave, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerSendMSG, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerSendCMD, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerTryJoin, *eventListener_, endstone::EventPriority::Monitor);
    constexpr string_view LOGO = R"(
_____   _
|_   _| (_)  __ _   _ _    _  _   __ _   _ _
| |   | | / _` | | ' \  | || | / _` | | ' \
|_|   |_| \__,_| |_||_|  \_, | \__,_| |_||_|
                        |__/
    )";
    getLogger().info(endstone::ColorFormat::Yellow + string(LOGO));
    const auto p_version = getServer().getPluginManager().getPlugin("tianyan_plugin")->getDescription().getVersion();
    getLogger().info(endstone::ColorFormat::Yellow + Tran->getLocal("Tianyan Plugin Version: ") + p_version);
    getLogger().info(endstone::ColorFormat::Yellow + Tran->getLocal("Repo: ")+"https://github.com/yuhangle/endstone-tianyan-plugin");
    getLogger().info("You can change the plugin’s language by editing the config file. Choose a language from the language folder.");
    if (TianyanCore::enable_web_ui)
    {
        // WebUI 后端
        tianyan::webui::WebUIConfig wcfg;
        {
            ordered_json cfg = read_config();
            wcfg.secret = cfg.value("web_secret", "your_secret");
            wcfg.port = cfg.value("web_port", 8098);

            // 兼容旧版 web_config.json（Python 后端遗留）
            // 如果存在且插件 config.json 中尚未迁移，则优先读取
            namespace fs = std::filesystem;
            auto legacy_path = fs::absolute(TianyanCore::dataPath) / "WebUI" / ".." / "web_config.json";
            // 我们改为检查更合理的路径
            if (fs::path old_cfg = fs::absolute(TianyanCore::dataPath).parent_path() / "web_config.json"; fs::exists(old_cfg) && !cfg.contains("web_secret")) {
                try {
                    std::ifstream ifs(old_cfg);
                    json old_json;
                    ifs >> old_json;
                    if (old_json.contains("secret"))
                        wcfg.secret = old_json["secret"].get<std::string>();
                    if (old_json.contains("backend_port"))
                        wcfg.port = old_json["backend_port"].get<int>();
                    getLogger().info("Migrated WebUI config from legacy web_config.json");
                } catch (...) {
                    // 忽略旧配置文件错误
                }
            }
            // 开发调试：设置 web_static_dir 指向 WebUI-Next/dist 则从磁盘提供静态文件，
            // 修改前端后只需 npm run build，无需重新编译 C++ 插件
            wcfg.static_dir = cfg.value("web_static_dir", "");
        }
        wcfg.thread_pool_size = 2;

        webui_server_ = std::make_unique<tianyan::webui::WebUIServer>(wcfg);
        webui_server_->setLogQueryService(
            std::make_unique<LogQueryImpl>(*db_backend_));
        webui_server_->setLanguageService(
            std::make_unique<LanguageServiceImpl>(TianyanCore::language_path));
        webui_server_->setMainThreadDispatcher(
            [this](std::function<void()> task) {
                getServer().getScheduler().runTask(*this, std::move(task));
            });

        if (webui_server_->start()) {
            getLogger().info("WebUI (C++ backend) started on port "
                + std::to_string(webui_server_->getPort()));
        } else {
            getLogger().error("Failed to start WebUI (C++ backend)");
        }
    }

    //初始化内嵌物品栏ui
    inventoryui::initialize_embedded(*this);
    //注册api
    const auto api_ptr = std::shared_ptr<ITianyanAPI>(this, [](ITianyanAPI*){
    });
    // 注册到服务管理器
    getServer().getServiceManager().registerService("TianyanAPI", api_ptr, *this, endstone::ServicePriority::Normal);
}

void TianyanPlugin::onDisable()
{
    inventoryui::shutdown();
    getLogger().info("onDisable is called");

    getServer().getScheduler().cancelTasks(*this);
    logsCacheWrite();

    // 清除剩余缓存
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        logDataCache.clear();
    }

    if (TianyanCore::enable_web_ui)
    {
        // 停止 C++ WebUI 后端
        if (webui_server_) {
            webui_server_->stop();
            webui_server_.reset();
        }
    }

    // NOTE: tyCore 和 db_backend_ 由插件析构时自动销毁。
    // 不可在此处手动 reset() —— logsCacheWrite 通过 detach() 线程异步访问它们，
    // 手动 reset 会造成 use-after-free。
}

bool TianyanPlugin::onCommand(endstone::CommandSender &sender, const endstone::Command &command, const std::vector<std::string> &args)
{
    if (command.getName() == "ty")
    {
        // 菜单
        if (args.empty()) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran->getLocal("Console not support menu"));
                return false;
            }
            menu_->tyMenu(*sender.asPlayer());
        }
        else if (args.size() >= 2) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran->getLocal("Console not support this command"));
                return false;
            }
            try {
                const double r = stod(args[0]);
                const double time = stod(args[1]);
                if (r > 100) {
                    sender.sendErrorMessage(Tran->getLocal("The radius cannot be greater than 100"));
                    return false;
                }
                if (time > 672) {
                    sender.sendErrorMessage(Tran->getLocal("The time cannot be greater than 672"));
                    return false;
                }
                const string search_key_type = args.size() > 2 ? args[2] : "";
                const string search_key = args.size() > 3 ? args[3] : "";
                const string world = sender.asPlayer()->getLocation().getDimension().getName();
                const double x = sender.asPlayer()->getLocation().getX();
                const double y = sender.asPlayer()->getLocation().getY();
                const double z = sender.asPlayer()->getLocation().getZ();

                // 提交后台查询任务
                auto cancel_flag = std::make_shared<std::atomic<bool>>(false);
                uint64_t task_id;
                {
                    std::lock_guard lock(async_tasks_mutex_);
                    // 取消该玩家之前的后台任务
                    for (auto& t : async_tasks_) {
                        if (t.player_name == sender.getName() && t.is_running && !t.cancelled) {
                            t.cancelled = true;
                            if (t.cancel_flag) *t.cancel_flag = true;
                            sender.sendMessage(endstone::ColorFormat::Yellow + Tran->getLocal("Previous query cancelled, starting a new one"));
                        }
                    }
                    task_id = next_task_id_++;
                    AsyncQueryTask task;
                    task.cancel_flag = cancel_flag;
                    task.id = task_id;
                    task.type = AsyncQueryTask::Type::Ty;
                    task.player_name = sender.getName();
                    task.is_running = true;
                    task.hours = time;
                    task.r = r;
                    task.world = world;
                    task.x = x;
                    task.y = y;
                    task.z = z;
                    task.key_type = search_key_type;
                    task.key = search_key;
                    async_tasks_.push_back(std::move(task));
                }

                sender.sendMessage(endstone::ColorFormat::Yellow + Tran->getLocal("Searching in the background, please wait"));

                std::thread([this, task_id, cancel_flag, hours = time, r, world, x, y, z]() {
                    if (cancel_flag->load()) return;
                    auto results = tyCore->searchLog({"", hours}, x, y, z, r, world, cancel_flag.get());
                    if (cancel_flag->load()) return;
                    std::lock_guard lock(async_tasks_mutex_);
                    for (auto& t : async_tasks_) {
                        if (t.id == task_id) {
                            t.results = std::move(results);
                            t.is_complete = true;
                            break;
                        }
                    }
                }).detach();

            } catch (const std::exception &e) {
                sender.sendErrorMessage(e.what());
            }
        }
    }
    else if (command.getName() == "tyback")
    {
        // 菜单
        if (args.empty()) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran->getLocal("Console not support menu"));
                return false;
            }
            menu_->tybackMenu(*sender.asPlayer());
        }
        else if (args.size() >= 2) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran->getLocal("Console not support this command"));
                return false;
            }
            try {
                const double r = stod(args[0]);
                if (r > 100) {
                    sender.sendErrorMessage(Tran->getLocal("The radius cannot be greater than 100"));
                    return false;
                }
                const double time = stod(args[1]);
                const string source_key_type = args.size() > 2 ? args[2] : "";
                const string source_key = args.size() > 3 ? args[3] : "";
                const string world = sender.asPlayer()->getLocation().getDimension().getName();
                const double x = sender.asPlayer()->getLocation().getX();
                const double y = sender.asPlayer()->getLocation().getY();
                const double z = sender.asPlayer()->getLocation().getZ();

                // 提交后台查询任务
                uint64_t task_id;
                auto cancel_flag = std::make_shared<std::atomic<bool>>(false);
                {
                    std::lock_guard lock(async_tasks_mutex_);
                    // 取消该玩家之前的后台任务
                    for (auto& t : async_tasks_) {
                        if (t.player_name == sender.getName() && t.is_running && !t.cancelled) {
                            t.cancelled = true;
                            if (t.cancel_flag) *t.cancel_flag = true;
                            sender.sendMessage(endstone::ColorFormat::Yellow + Tran->getLocal("Previous query cancelled, starting a new one"));
                        }
                    }
                    task_id = next_task_id_++;
                    AsyncQueryTask task;
                    task.id = task_id;
                    task.cancel_flag = cancel_flag;
                    task.type = AsyncQueryTask::Type::Tyback;
                    task.player_name = sender.getName();
                    task.is_running = true;
                    task.hours = time;
                    task.r = r;
                    task.world = world;
                    task.x = x;
                    task.y = y;
                    task.z = z;
                    task.key_type = source_key_type;
                    task.key = source_key;
                    async_tasks_.push_back(std::move(task));
                }

                sender.sendMessage(endstone::ColorFormat::Yellow + Tran->getLocal("Searching in the background, please wait"));

                std::thread([this, task_id, cancel_flag, hours = time, r, world, x, y, z]() {
                    if (cancel_flag->load()) return;
                    auto results = tyCore->searchLog({"", hours}, x, y, z, r, world, cancel_flag.get());
                    if (cancel_flag->load()) return;
                    std::lock_guard lock(async_tasks_mutex_);
                    for (auto& t : async_tasks_) {
                        if (t.id == task_id) {
                            t.results = std::move(results);
                            t.is_complete = true;
                            break;
                        }
                    }
                }).detach();

            } catch (const std::exception &e) {
                sender.sendErrorMessage(e.what());
            }
        }
    }
    else if (command.getName() == "tys")
    {
        // 菜单
        if (args.empty()) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran->getLocal("Console not support menu"));
                return false;
            }
            menu_->tysMenu(*sender.asPlayer());
        }
        else if (!args.empty()) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran->getLocal("Console not support this command"));
                return false;
            }
            try {
                // 提交后台查询任务
                {
                    const double time = stod(args[0]);
                    const string search_key_type = args.size() > 1 ? args[1] : "";
                    const string search_key = args.size() > 2 ? args[2] : "";
                    uint64_t task_id;
                    auto cancel_flag = std::make_shared<std::atomic<bool>>(false);
                    {
                        std::lock_guard lock(async_tasks_mutex_);
                        // 取消该玩家之前的后台任务
                        for (auto& t : async_tasks_) {
                            if (t.player_name == sender.getName() && t.is_running && !t.cancelled) {
                            if (t.cancel_flag) *t.cancel_flag = true;
                                t.cancelled = true;
                                sender.sendMessage(endstone::ColorFormat::Yellow + Tran->getLocal("Previous query cancelled, starting a new one"));
                            }
                        }
                        task_id = next_task_id_++;
                        AsyncQueryTask task;
                        task.id = task_id;
                        task.cancel_flag = cancel_flag;
                        task.type = AsyncQueryTask::Type::Tys;
                        task.player_name = sender.getName();
                        task.is_running = true;
                        task.hours = time;
                        task.key_type = search_key_type;
                        task.key = search_key;
                        async_tasks_.push_back(std::move(task));
                    }

                    sender.sendMessage(endstone::ColorFormat::Yellow + Tran->getLocal("Searching in the background, please wait"));

                    std::thread([this, task_id, cancel_flag, hours = time]() {
                        if (cancel_flag->load()) return;
                        auto results = tyCore->searchLog({"", hours}, cancel_flag.get());
                        if (cancel_flag->load()) return;
                        std::lock_guard lock(async_tasks_mutex_);
                        for (auto& t : async_tasks_) {
                            if (t.id == task_id) {
                                t.results = std::move(results);
                                t.is_complete = true;
                                break;
                            }
                        }
                    }).detach();
                }

            } catch (const std::exception &e) {
                sender.sendErrorMessage(e.what());
            }
        }
    }
    else if (command.getName() == "ban-id") {
        if (!args.empty()) {
            string device_id = args[0];
            const string reason = args.size() > 1 ? args[1] : "";
            string player_name = "Null";
            for (auto &player : getServer().getOnlinePlayers()) {
                if (player->getDeviceId() == device_id) {
                    player_name = player->getName();
                    player->kick(reason);
                }
            }
            TianyanCore::BanIDPlayer banIDPlayer = {player_name,device_id, reason, TianyanCore::timestampToString(std::time(nullptr))};
            auto status = protect_->BanDeviceID(banIDPlayer);
            if (sender.asPlayer()) {
                if (status) {
                    sender.sendMessage(Tran->tr(Tran->getLocal("Device ID {} banned successfully"),device_id));
                } else {
                    sender.sendErrorMessage(Tran->tr(Tran->getLocal("Error occurred while banning device ID {}: {}"), device_id, Tran->getLocal("View more in console")));
                }
            }
        }
    } else if (command.getName() == "unban-id") {
        if (!args.empty()) {
            const string& device_id = args[0];
            const auto status = protect_->UnbanDeviceID(device_id);
            if (sender.asPlayer()) {
                if (status) {
                    sender.sendMessage(Tran->tr(Tran->getLocal("Device ID {} unbanned successfully"), device_id));
                } else {
                    sender.sendErrorMessage(Tran->tr(Tran->getLocal("Error occurred while unbanning device ID {}: {}"), device_id, Tran->getLocal("View more in console")));
                }
            }
        }
    } else if (command.getName() == "banlist-id") {
        sender.sendMessage(Tran->getLocal("The list of baned device: "));
        if (BanIDPlayers.empty()) {
            sender.sendMessage(Tran->getLocal("Nothing"));
            return true;
        }
        for (auto &[player_name, device_id, reason, time] : BanIDPlayers) {
            sender.sendMessage(Tran->tr(Tran->getLocal("Device ID: {}"), device_id));
            sender.sendMessage(Tran->tr(Tran->getLocal("Player: {}"), player_name));
            sender.sendMessage(Tran->tr(Tran->getLocal("Reason: {}"), reason.value_or("")));
            sender.sendMessage(Tran->tr(Tran->getLocal("Time: {}"), time));
            sender.sendMessage("----------------------");
        }
    }
    else if (command.getName() == "tyclean") {
        if (!args.empty()) {
            if (yuhangle::clean_data_status == 2) {
                sender.sendErrorMessage(Tran->getLocal("A background operation is in progress. Please wait for it to complete"));
                return false;
            }
            int hours = db_util::stringToInt(args[0]);
            sender.sendMessage(endstone::ColorFormat::Yellow+Tran->tr(Tran->getLocal("Start cleaning logs older than {} hours"), args[0]));
            std::thread clean_thread([this, hours, sender_name = sender.getName()]() {
                runCleanup(hours, sender_name);
            });
            clean_thread.detach();
        }
    }
    else if (command.getName() == "tyo") {
        if (!sender.asPlayer()) {
            sender.sendErrorMessage(Tran->getLocal("Console not support menu"));
            return false;
        }
        if (!args.empty()) {
            const string& player_name = args[0];
            // 在线玩家查询
            if (auto player = getServer().getPlayer(player_name)) {
                if (!menu_->showPlayerInventoryUI(*sender.asPlayer(), *player)) {
                    menu_->showOnlinePlayerBag(sender, *player);
                }
                return true;
            }

            // 离线玩家查询
            {
                if (auto uuid = OfflineInventoryReader::findUUIDByName(player_name); !uuid) {
                    sender.sendErrorMessage(Tran->tr(Tran->getLocal("Player {} not found (offline)"), player_name));
                    sender.sendMessage(endstone::ColorFormat::Gray + Tran->getLocal("Tip: The player must have joined at least once to be cached."));
                } else {
                    // 提交后台查询任务
                    AsyncOfflineQueryTask task;
                    task.sender_name = sender.getName();
                    task.target_name = player_name;
                    task.player_uuid = *uuid;
                    task.world_path = OfflineInventoryReader::resolveWorldPath(getServerRoot());
                    task.is_running = true;
                    size_t task_idx;
                    {
                        std::lock_guard lock(async_offline_mutex_);
                        async_offline_tasks_.push_back(std::move(task));
                        task_idx = async_offline_tasks_.size() - 1;
                    }
                    std::thread t([this, task_idx]() {
                        std::string w_path, p_key;
                        {
                            std::lock_guard lock(async_offline_mutex_);
                            if (task_idx >= async_offline_tasks_.size()) return;
                            w_path = async_offline_tasks_[task_idx].world_path;
                            p_key = async_offline_tasks_[task_idx].player_uuid;
                        }
                        auto* json = wi_get_inventory_json(w_path.c_str(), p_key.c_str());
                        {
                            std::lock_guard lock(async_offline_mutex_);
                            if (task_idx < async_offline_tasks_.size()) {
                                if (json) {
                                    async_offline_tasks_[task_idx].result_json = json;
                                    wi_free_string(json);
                                }
                                async_offline_tasks_[task_idx].is_running = false;
                                async_offline_tasks_[task_idx].is_complete = true;
                            }
                        }
                    });
                    t.detach();
                    sender.sendMessage(endstone::ColorFormat::Yellow + Tran->getLocal("Querying offline inventory..."));
                }
            }
        }
        return true;
    }
    else if (command.getName() == "density") {
        if (!sender.asPlayer()) {
            int size = 20;
            if (!args.empty()) {
                size = db_util::stringToInt(args[0]);
            }
            if (auto result = TianyanProtect::calculateEntityDensity(getServer(), size);result.dim.has_value()) {
                std::string content = fmt::format(
                    "{}{} {},\n"
                    "{}:({:.1f}, {:.1f}, {:.1f}),\n"
                    "{}:{}\n"
                    "{}: {},\n"
                    "{}: {}",
                    endstone::ColorFormat::Yellow,
                    Tran->getLocal("Highest density region in dimension"), result.dim.value(),
                    Tran->getLocal("Midpoint coordinates"), result.mid_x.value(), result.mid_y.value(), result.mid_z.value(),
                    Tran->getLocal("Entity count"), result.count.value(),
                    Tran->getLocal("Most common entity"), result.entity_type.value(),
                    Tran->getLocal("Random entity position"), result.entity_pos.value()
                );
                sender.sendMessage(content);
            } else {
                sender.sendMessage(endstone::ColorFormat::Yellow + Tran->getLocal("No entities detected currently"));
            }
        } else {
            if (args.empty()) {
                menu_->findHighDensityRegion(*sender.asPlayer());
            } else {
                int size = db_util::stringToInt(args[0]);
                menu_->findHighDensityRegion(*sender.asPlayer(), size);
            }
        }
    }
    else if (command.getName() == "tymigrate") {
        if (args.size() < 2) {
            sender.sendErrorMessage("Usage: /tymigrate <sqlite|mysql> <sqlite|mysql>");
            return false;
        }
        const string& src = args[0];
        const string& dst = args[1];
        if ((src != "sqlite" && src != "mysql") || (dst != "sqlite" && dst != "mysql")) {
            sender.sendErrorMessage("Arguments must be 'sqlite' or 'mysql'");
            return false;
        }
        if (src == dst) {
            sender.sendErrorMessage("Source and target must be different");
            return false;
        }
        if (yuhangle::migrate_status == 1) {
            sender.sendErrorMessage(Tran->getLocal("A migration is already in progress"));
            return false;
        }
        if (yuhangle::clean_data_status == 2) {
            sender.sendErrorMessage(Tran->getLocal("A background operation is in progress. Please wait for it to complete"));
            return false;
        }
        sender.sendMessage(endstone::ColorFormat::Yellow + Tran->tr(Tran->getLocal("Starting migration from {} to {}"), src, dst));
        runMigration(src, dst, sender.getName());
    }
    return true;
}

void TianyanPlugin::runMigration(const std::string& source, const std::string& target, const std::string& sender_name) const
{
    namespace fs = std::filesystem;

    yuhangle::migrate_status = 1;
    yuhangle::migrate_message.clear();
    yuhangle::migrate_progress = 0;
    yuhangle::migrate_total = 0;
    yuhangle::migrate_source_type = source;
    yuhangle::migrate_target_type = target;
    yuhangle::migrate_sender_name = sender_name;

    std::thread migrate_thread([this, source, target]() {
        namespace fs1 = std::filesystem;
        try {
            // Build source and target backends
            std::unique_ptr<IDatabaseBackend> src_backend;
            std::unique_ptr<IDatabaseBackend> dst_backend;
            const fs1::path currentPath = fs1::current_path();
            const fs1::path fullPath = currentPath / TianyanCore::dbPath;

            if (source == "sqlite") {
                src_backend = std::make_unique<SqliteBackend>(fullPath.string());
            } else {
                // Read MySQL config from config.json
                ordered_json cfg = read_config();
                RustMySQLConfig mysql_cfg;
                mysql_cfg.host = cfg.value("mysql_host", std::string("127.0.0.1"));
                mysql_cfg.port = cfg.value("mysql_port", 3306);
                mysql_cfg.user = cfg.value("mysql_user", std::string("root"));
                mysql_cfg.password = cfg.value("mysql_password", std::string(""));
                mysql_cfg.database = cfg.value("mysql_database", std::string("endstone"));
                src_backend = std::make_unique<RustBackend>(mysql_cfg);
            }

            if (target == "sqlite") {
                dst_backend = std::make_unique<SqliteBackend>(fullPath.string());
            } else {
                ordered_json cfg = read_config();
                RustMySQLConfig mysql_cfg;
                mysql_cfg.host = cfg.value("mysql_host", std::string("127.0.0.1"));
                mysql_cfg.port = cfg.value("mysql_port", 3306);
                mysql_cfg.user = cfg.value("mysql_user", std::string("root"));
                mysql_cfg.password = cfg.value("mysql_password", std::string(""));
                mysql_cfg.database = cfg.value("mysql_database", std::string("endstone"));
                dst_backend = std::make_unique<RustBackend>(mysql_cfg);
            }

            // Init target
            if (dst_backend->init_database() != 0) {
                yuhangle::migrate_message = {"Target database init failed"};
                yuhangle::migrate_status = -1;
                return;
            }

            // Verify source has LOGDATA table
            {
                if (std::vector<std::map<std::string, std::string>> check; src_backend->querySQL("SELECT COUNT(*) AS cnt FROM LOGDATA", check) != 0) {
                    yuhangle::migrate_message = {
                        "Source " + source + " database has no LOGDATA table. "
                        "Make sure the source has been used before migrating."
                    };
                    yuhangle::migrate_status = -1;
                    return;
                }
            }

            // Get total count for progress tracking
            {
                std::vector<std::map<std::string, std::string>> cnt;
                if (src_backend->querySQL("SELECT COUNT(*) AS cnt FROM LOGDATA", cnt) != 0 || cnt.empty()) {
                    yuhangle::migrate_message = {"Failed to count rows for migration"};
                    yuhangle::migrate_status = -1;
                    return;
                }
                yuhangle::migrate_total = std::stoi(cnt[0]["cnt"]);
            }
            if (yuhangle::migrate_total == 0) {
                yuhangle::migrate_message = {"No data to migrate"};
                yuhangle::migrate_status = 2;
                return;
            }

            // MySQL 目标：先删二级索引，INSERT 阶段省去 B+Tree 实时维护
            if (!dst_backend->isSqlite()) {
                dst_backend->executeSQL("DROP INDEX uk_uuid ON LOGDATA");
                dst_backend->executeSQL("DROP INDEX idx_logdata_time ON LOGDATA");
                dst_backend->executeSQL("DROP INDEX idx_logdata_pos ON LOGDATA");
            }

            // Streaming migration: keyset pagination, no OFFSET tax
            constexpr int WRITE_BATCH = 50000;
            int64_t total_processed = 0;
            int64_t last_rowid = 0;
            std::vector<DatabaseLogEntry> batch;
            batch.reserve(WRITE_BATCH);

            auto t_read_total = std::chrono::duration<double>::zero();
            auto t_write_total = std::chrono::duration<double>::zero();

            while (true) {
                constexpr int PAGE_SIZE = 100000;
                std::vector<std::map<std::string, std::string>> page;
                auto t0 = std::chrono::high_resolution_clock::now();
                std::string page_sql = "SELECT *, rowid FROM LOGDATA WHERE rowid > " +
                                       std::to_string(last_rowid) +
                                       " ORDER BY rowid LIMIT " + std::to_string(PAGE_SIZE);
                if (src_backend->querySQL(page_sql, page) != 0) {
                    yuhangle::migrate_message = {"Failed to read data from source"};
                    yuhangle::migrate_status = -1;
                    return;
                }
                if (page.empty()) break;
                t_read_total += std::chrono::high_resolution_clock::now() - t0;

                last_rowid = std::stoll(page.back().at("rowid"));

                auto t1 = std::chrono::high_resolution_clock::now();
                for (const auto& row : page) {
                    DatabaseLogEntry entry;
                    entry.uuid = row.at("uuid");
                    entry.id = row.at("id");
                    entry.name = row.at("name");
                    entry.pos_x = std::stod(row.at("pos_x"));
                    entry.pos_y = std::stod(row.at("pos_y"));
                    entry.pos_z = std::stod(row.at("pos_z"));
                    entry.world = row.at("world");
                    entry.obj_id = row.at("obj_id");
                    entry.obj_name = row.at("obj_name");
                    entry.time = std::stoll(row.at("time"));
                    entry.type = row.at("type");
                    entry.data = row.at("data");
                    entry.status = row.at("status");
                    batch.push_back(std::move(entry));

                    if (batch.size() >= WRITE_BATCH) {
                        if (dst_backend->addLogs(batch) != 0) {
                            if (!dst_backend->isSqlite()) {
                                dst_backend->executeSQL(
                                    "ALTER TABLE LOGDATA "
                                    "ADD UNIQUE KEY uk_uuid (uuid), "
                                    "ADD KEY idx_logdata_time (time), "
                                    "ADD KEY idx_logdata_pos (pos_x, pos_y, pos_z)");
                            }
                            yuhangle::migrate_message = {"Write to target failed at row " + std::to_string(total_processed + 1)};
                            yuhangle::migrate_status = -1;
                            return;
                        }
                        batch.clear();
                    }
                    total_processed++;
                }

                if (!batch.empty()) {
                    if (dst_backend->addLogs(batch) != 0) {
                        if (!dst_backend->isSqlite()) {
                            dst_backend->executeSQL(
                                "ALTER TABLE LOGDATA "
                                "ADD UNIQUE KEY uk_uuid (uuid), "
                                "ADD KEY idx_logdata_time (time), "
                                "ADD KEY idx_logdata_pos (pos_x, pos_y, pos_z)");
                        }
                        yuhangle::migrate_message = {"Write to target failed at row " + std::to_string(total_processed)};
                        yuhangle::migrate_status = -1;
                        return;
                    }
                    batch.clear();
                }
                t_write_total += std::chrono::high_resolution_clock::now() - t1;

                yuhangle::migrate_progress = static_cast<int>(total_processed);
            }

            double read_s = t_read_total.count();
            double write_s = t_write_total.count();
            getLogger().info("[Tianyan] Migrate timing - SQLite read: {}s, MySQL write: {}s, ratio: {}x",
                             read_s, write_s, read_s > 0 ? write_s / read_s : 0);

            // MySQL 目标：重建二级索引
            if (!dst_backend->isSqlite()) {
                if (dst_backend->executeSQL(
                        "ALTER TABLE LOGDATA "
                        "ADD UNIQUE KEY uk_uuid (uuid), "
                        "ADD KEY idx_logdata_time (time), "
                        "ADD KEY idx_logdata_pos (pos_x, pos_y, pos_z)") != 0) {
                    yuhangle::migrate_message = {"Failed to rebuild indexes on target"};
                    yuhangle::migrate_status = -1;
                    return;
                }
            }

            yuhangle::migrate_progress = yuhangle::migrate_total;

            // Swap active backend if target matches a different db_type
            // (keep the migration result for the status checker to finalize)
            yuhangle::migrate_message = {
                "Migration complete",
                std::to_string(yuhangle::migrate_total) + " entries migrated"
            };
            yuhangle::migrate_status = 2;

        } catch (const std::exception& e) {
            yuhangle::migrate_message = {"Migration error: " + std::string(e.what())};
            yuhangle::migrate_status = -1;
        }
    });
    migrate_thread.detach();
}

void TianyanPlugin::checkMigrateStatus()
{
    namespace fs = std::filesystem;
    if (yuhangle::migrate_status == 0) return;

    const auto player = getServer().getPlayer(yuhangle::migrate_sender_name);
    const auto green = endstone::ColorFormat::Green;
    const auto red = endstone::ColorFormat::Red;

    if (yuhangle::migrate_status == 2) {
        if (yuhangle::migrate_total > 0) {
            std::string detail = Tran->tr(Tran->getLocal("{0} entries migrated from {1} to {2}"),
                                           std::to_string(yuhangle::migrate_total),
                                           yuhangle::migrate_source_type,
                                           yuhangle::migrate_target_type);
            std::string msg = green + Tran->getLocal("Migration completed") + " " + detail;
            if (player) player->sendMessage(msg);
            getLogger().info(msg);
        } else {
            std::string msg = green + Tran->getLocal("Migration completed");
            if (player) player->sendMessage(msg);
            getLogger().info(msg);
        }
        // Swap active backend to match target type
        bool need_swap = false;
        if (yuhangle::migrate_target_type == "sqlite" && db_type_ != "sqlite") need_swap = true;
        if (yuhangle::migrate_target_type == "mysql" && db_type_ != "mysql") need_swap = true;

        if (need_swap) {
            getLogger().info(Tran->tr(Tran->getLocal("Switching active database backend to {}"), yuhangle::migrate_target_type));

            // Suspend writes
            is_db_over = false;

            // Recreate core with the new backend type
            if (yuhangle::migrate_target_type == "sqlite") {
                const fs::path fullPath = fs::current_path() / TianyanCore::dbPath;
                db_backend_ = std::make_unique<SqliteBackend>(fullPath.string());
                db_backend_->init_database();
            } else {
                json cfg = read_config();
                RustMySQLConfig mysql_cfg;
                mysql_cfg.host = cfg.value("mysql_host", std::string("127.0.0.1"));
                mysql_cfg.port = cfg.value("mysql_port", 3306);
                mysql_cfg.user = cfg.value("mysql_user", std::string("root"));
                mysql_cfg.password = cfg.value("mysql_password", std::string(""));
                mysql_cfg.database = cfg.value("mysql_database", std::string("endstone"));
                db_backend_ = std::make_unique<RustBackend>(mysql_cfg);
            }

            // Update db_type string for next server start
            db_type_ = yuhangle::migrate_target_type;

            // Persist to config.json
            try {
                ordered_json cfg;
                if (std::ifstream in(TianyanCore::config_path); in.is_open()) {
                    in >> cfg;
                }
                cfg["database_type"] = db_type_;
                if (std::ofstream out(TianyanCore::config_path); out.is_open()) {
                    out << cfg.dump(4);
                }
            } catch (const std::exception& e) {
                getLogger().error("Failed to update config.json: {}", e.what());
            }

            // Rebuild core with new backend
            tyCore = std::make_unique<TianyanCore>(*db_backend_);
            is_db_over = true;
            getLogger().info(Tran->tr(Tran->getLocal("Active backend switched to {}"), yuhangle::migrate_target_type));

            // Restart WebUI to pick up new database config
            if (TianyanCore::enable_web_ui && webui_server_) {
                getLogger().info(Tran->getLocal("Restarting WebUI for new database backend..."));
                webui_server_->stop();
                if (!webui_server_->start()) {
                    getLogger().error("Failed to restart WebUI (C++ backend)");
                }
            }
        }

        yuhangle::migrate_status = 0;
        yuhangle::migrate_message.clear();
    }
    else if (yuhangle::migrate_status == -1) {
        std::string err_msg = red + Tran->getLocal("Migration failed") + ": ";
        for (const auto& msg : yuhangle::migrate_message) {
            err_msg += Tran->getLocal(msg) + " ";
        }
        if (player) player->sendErrorMessage(err_msg);
        getLogger().error(err_msg);

        yuhangle::migrate_status = 0;
        yuhangle::migrate_message.clear();
    }
}

    //缓存写入机制
void TianyanPlugin::logsCacheWrite() const
{
    if (logDataCache.empty()) {
        return;
    }
    //避开数据清理
    if (yuhangle::clean_data_status == 2) {
        return;
    }
    // 创建局部副本以避免在写入过程中数据被修改
    std::vector<TianyanCore::LogData> localCache;

    // 将当前缓存数据复制到局部变量中
    {
        std::lock_guard lock(cacheMutex);  // 互斥锁
        localCache.swap(logDataCache);
    }

    // 异步写入
    std::thread logsCacheWrite_thread ([this,localCache]() mutable {
        // 检查写入状态
        if (tyCore->recordLogs(localCache)) {
            //超过100万仍无法写入，则清空缓存
            if (localCache.size() > 1000000) {
                std::cerr << "Unable to write a large volume of logs; cached logs will be discarded" << endl;
                localCache.clear();
            } else {
                //将写入失败的数据放回去
                std::lock_guard lock(cacheMutex);
                logDataCache.insert(logDataCache.begin(), localCache.begin(), localCache.end());
            }
        }
    });
    logsCacheWrite_thread.detach();
}

void TianyanPlugin::runCleanup(const double hours, const std::string& sender_name) const
{
    // 设置清理状态
    yuhangle::clean_data_status = 2;
    yuhangle::clean_data_sender_name = sender_name;
    yuhangle::clean_data_message.clear();
    yuhangle::clean_data_progress = 0;
    yuhangle::clean_data_total = 0;

    // 计算时间阈值（秒）
    const long long current_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const long long threshold = current_time - static_cast<long long>(hours * 3600);

    try {
        // 统计待删除行数
        const int64_t to_delete = db_backend_->getCleanCount(threshold);
        if (to_delete < 0) {
            yuhangle::clean_data_status = -1;
            yuhangle::clean_data_message = {"Failed to count rows for cleanup"};
            return;
        }
        if (to_delete == 0) {
            yuhangle::clean_data_message = {"Time elapsed: ", "0", "Number of cleaned logs: ", "0"};
            yuhangle::clean_data_status = 1;
            return;
        }

        yuhangle::clean_data_total = to_delete;
        const auto start_time = std::chrono::high_resolution_clock::now();

        // MySQL 模式：表重建策略
        if (!db_backend_->isSqlite()) {
            const int64_t kept = db_backend_->cleanupByRebuild(threshold);
            if (kept < 0) {
                yuhangle::clean_data_status = -1;
                yuhangle::clean_data_message = {"MySQL rebuild cleanup failed"};
                return;
            }
            const auto end_time = std::chrono::high_resolution_clock::now();
            const double seconds = std::chrono::duration<double>(end_time - start_time).count();
            getLogger().info("[Tianyan] MySQL rebuild kept: {} rows in {}s", kept, seconds);
            yuhangle::clean_data_message = {
                "Time elapsed: ", std::to_string(seconds),
                "Number of cleaned logs: ", std::to_string(to_delete)
            };
            yuhangle::clean_data_status = 1;
            return;
        }

        // SQLite 模式：独立连接 + 分批 DELETE
        if (!db_backend_->beginCleanup()) {
            yuhangle::clean_data_status = -1;
            yuhangle::clean_data_message = {"Failed to open cleanup connection"};
            return;
        }

        // 分批 DELETE
        int64_t total_deleted = 0;
        int batch_count = 0;
        while (total_deleted < to_delete) {
            const int deleted = db_backend_->cleanupDeleteBatch(threshold, CLEANUP_BATCH_SIZE);
            if (deleted < 0) {
                yuhangle::clean_data_status = -1;
                yuhangle::clean_data_message = {
                    "Batch delete failed after deleting " +
                    std::to_string(total_deleted) + " rows"};
                return;
            }
            if (deleted == 0) break;

            total_deleted += deleted;
            yuhangle::clean_data_progress = total_deleted;
            batch_count++;

            if (batch_count % CLEANUP_CHECKPOINT_INTERVAL == 0) {
                db_backend_->cleanupCheckpoint();
            }
        }

        // 结束清理（VACUUM 或只 checkpoint）
        if (total_deleted > 0) {
            if (total_deleted >= 100000) {
                db_backend_->endCleanup();
            } else {
                db_backend_->abortCleanup();
            }
        }

        const auto end_time = std::chrono::high_resolution_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        const double seconds = std::chrono::duration<double>(duration).count();

        yuhangle::clean_data_message = {
            "Time elapsed: ", std::to_string(seconds),
            "Number of cleaned logs: ", std::to_string(total_deleted)
        };
        yuhangle::clean_data_status = 1;

    } catch (const std::exception& e) {
        yuhangle::clean_data_status = -1;
        yuhangle::clean_data_message = {"Cleanup exception: " + std::string(e.what())};
    }
}

//检查异步的数据库清理状态
void TianyanPlugin::checkDatabaseCleanStatus() const {
    const int status = yuhangle::clean_data_status.load();
    if (status == 0) return;

    const auto player = getServer().getPlayer(yuhangle::clean_data_sender_name);
    const auto green = endstone::ColorFormat::Green;

    if (status == 2) {
        return;
    }

    if (status == 1) {
        // 成功
        const auto& msg = yuhangle::clean_data_message;
        const std::string elapsed = msg.size() > 1 ? msg[1] : "0";
        const std::string count = msg.size() > 3 ? msg[3] : "0";

        if (player && yuhangle::clean_data_sender_name != "Server") {
            player->sendMessage(green + Tran->getLocal("Database clean over"));
            player->sendMessage(green + Tran->getLocal("Time elapsed: ") + elapsed + "s");
            player->sendMessage(green + Tran->getLocal("Number of cleaned logs: ") + count);
        }
        getLogger().info(green + Tran->getLocal("Database clean over"));
        getLogger().info(green + Tran->getLocal("Time elapsed: ") + elapsed + "s");
        getLogger().info(green + Tran->getLocal("Number of cleaned logs: ") + count);

    } else if (status == -1) {
        // 失败
        if (player && yuhangle::clean_data_sender_name != "Server") {
            player->sendErrorMessage(Tran->getLocal("Database clean error"));
            for (const auto& info : yuhangle::clean_data_message) {
                player->sendErrorMessage(info);
                getLogger().error(info);
            }
        } else {
            getLogger().error(Tran->getLocal("Database clean error"));
            for (const auto& info : yuhangle::clean_data_message) {
                getLogger().error(info);
            }
        }
    }

    // 重置状态
    yuhangle::clean_data_status = 0;
    yuhangle::clean_data_message.clear();
    yuhangle::clean_data_sender_name.clear();
    yuhangle::clean_data_total = 0;
    yuhangle::clean_data_progress = 0;
}

//检查后台查询任务
void TianyanPlugin::checkAsyncTasks() {
    // 收集已完成的任务
    vector<AsyncQueryTask> completed;
    {
        std::lock_guard lock(async_tasks_mutex_);
        for (auto it = async_tasks_.begin(); it != async_tasks_.end();) {
            if (it->cancelled) {
                it = async_tasks_.erase(it);
            } else if (it->is_complete) {
                completed.push_back(std::move(*it));
                it = async_tasks_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 在服务器线程上处理每个已完成任务

    // 处理已完成的后台离线背包查询
    vector<AsyncOfflineQueryTask> offline_completed;
    {
        std::lock_guard lock(async_offline_mutex_);
        for (auto it = async_offline_tasks_.begin(); it != async_offline_tasks_.end();) {
            if (it->is_complete) {
                offline_completed.push_back(std::move(*it));
                it = async_offline_tasks_.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto& task : offline_completed) {
        const auto player = getServer().getPlayer(task.sender_name);
        if (!player) continue;

        if (task.result_json.empty()) {
            player->sendErrorMessage(Tran->tr(Tran->getLocal("Player {} not found (offline)"), task.target_name));
            continue;
        }

        menu_->showOfflinePlayerInventoryEncoded(*player, task.target_name, task.world_path, task.player_uuid);
    }

    // 处理其他后台查询任务
    for (auto& task : completed) {
        const auto player = getServer().getPlayer(task.player_name);
        if (!player) continue;

        if (task.type == AsyncQueryTask::Type::Ty || task.type == AsyncQueryTask::Type::Tys) {
            if (task.results.empty()) {
                player->sendErrorMessage(Tran->getLocal("No log found"));
                continue;
            }

            if (!task.key_type.empty() && !task.key.empty()) {
                vector<TianyanCore::LogData> filtered;
                if (task.key_type == "source_id") {
                    for (auto& log : task.results) {
                        if (log.id == task.key) filtered.push_back(log);
                    }
                } else if (task.key_type == "source_name") {
                    for (auto& log : task.results) {
                        if (log.name == task.key) filtered.push_back(log);
                    }
                } else if (task.key_type == "target_id") {
                    for (auto& log : task.results) {
                        if (log.obj_id == task.key) filtered.push_back(log);
                    }
                } else if (task.key_type == "target_name") {
                    for (auto& log : task.results) {
                        if (log.obj_name == task.key) filtered.push_back(log);
                    }
                } else if (task.key_type == "action") {
                    for (auto& log : task.results) {
                        if (log.type == task.key) filtered.push_back(log);
                    }
                }

                if (!filtered.empty()) {
                    menu_->showLogMenu(*player, filtered);
                    player->sendMessage(endstone::ColorFormat::Yellow + Tran->getLocal("Display all logs about") + "` " + task.key + " `");
                } else {
                    player->sendErrorMessage(Tran->getLocal("No log found"));
                }
            } else {
                menu_->showLogMenu(*player, task.results);
                player->sendMessage(endstone::ColorFormat::Yellow + Tran->getLocal("Display all logs"));
            }
        } else if (task.type == AsyncQueryTask::Type::Tyback) {
            if (task.results.empty()) {
                player->sendErrorMessage(Tran->getLocal("No log found"));
                continue;
            }

            int success_times = 0;
            int failed_times = 0;

            // 收集 block_place 的位置，跳过该位置上冗余的 player_right_click_block
            std::unordered_set<std::string> placed_positions;
            for (const auto& ld : task.results) {
                if (ld.type == "block_place") {
                    placed_positions.insert(std::to_string(ld.pos_x) + "," +
                                            std::to_string(ld.pos_y) + "," +
                                            std::to_string(ld.pos_z));
                }
            }

            for (auto& logData : std::ranges::reverse_view(task.results)) {
                if (logData.status == "canceled" || logData.status == "reverted") {
                    continue;
                }
                if (logData.type == "player_right_click_block") {
                    string p = std::to_string(logData.pos_x) + "," +
                               std::to_string(logData.pos_y) + "," +
                               std::to_string(logData.pos_z);
                    if (placed_positions.contains(p)) continue;
                }
                if (!task.key_type.empty() && !task.key.empty()) {
                    if (task.key_type == "source_id" && logData.id != task.key) continue;
                    if (task.key_type == "source_name" && logData.name != task.key) continue;
                    if (task.key_type == "target_id" && logData.obj_id != task.key) continue;
                    if (task.key_type == "target_name" && logData.obj_name != task.key) continue;
                    if (task.key_type == "action" && logData.type != task.key) continue;
                }
                endstone::CommandSenderWrapper wrapper_sender(*player,
                    [](const endstone::Message&) {},
                    [](const endstone::Message&) {}
                );
                if (logData.type == "block_break" || logData.type == "block_break_bomb" || (logData.type == "actor_bomb" && logData.id == "minecraft:tnt")) {
                    string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                    std::ostringstream cmd;
                    cmd << "setblock " << pos << " " << logData.obj_id << logData.data;
                    if (getServer().dispatchCommand(wrapper_sender, cmd.str())) {
                        TianyanCore::revertStatusCache.emplace_back(logData.uuid, "reverted");
                        success_times++;
                    } else {
                        failed_times++;
                    }
                } else if (logData.type == "player_right_click_block") {
                    if (auto hand_block = db_util::splitString(logData.data); hand_block[1] != "[]") {
                        static constexpr std::array<std::string_view, 12> skipKeywords = {
                            "chest", "sign", "command", "shulker_box",
                            "dispenser", "dropper", "hopper", "barrel",
                            "furnace", "smoker", "frame", "shelf"
                        };
                        auto containsKeyword = [&logData](const std::string_view kw) {
                            return logData.obj_id.find(kw) != std::string::npos;
                        };
                        if (ranges::any_of(skipKeywords, containsKeyword)) {
                            continue;
                        }
                        string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                        std::ostringstream cmd;
                        cmd << "setblock " << pos << " " << logData.obj_id << hand_block[1];
                        if (getServer().dispatchCommand(wrapper_sender, cmd.str())) {
                            TianyanCore::revertStatusCache.emplace_back(logData.uuid, "reverted");
                            success_times++;
                        } else {
                            failed_times++;
                        }
                    }
                } else if (logData.type == "block_place") {
                    string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                    std::ostringstream cmd;
                    cmd << "setblock " << pos << " minecraft:air";
                    if (getServer().dispatchCommand(wrapper_sender, cmd.str())) {
                        TianyanCore::revertStatusCache.emplace_back(logData.uuid, "reverted");
                        success_times++;
                    } else {
                        failed_times++;
                    }
                } else if (logData.type == "entity_die") {
                    string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                    string obj_id = logData.obj_id;
                    if (size_t vpos = obj_id.find("villager_v2"); vpos != std::string::npos) {
                        obj_id.replace(vpos, 11, "villager");
                    }
                    if (obj_id == "minecraft:player") {
                        continue;
                    }
                    std::ostringstream cmd;
                    cmd << "summon " << obj_id << " " << pos;
                    if (getServer().dispatchCommand(wrapper_sender, cmd.str())) {
                        TianyanCore::revertStatusCache.emplace_back(logData.uuid, "reverted");
                        success_times++;
                    } else {
                        failed_times++;
                    }
                }
            }
            updateRevertStatus();
            if (success_times > 0) {
                player->sendMessage(endstone::ColorFormat::Green + Tran->getLocal("Revert times: ") + std::to_string(success_times + failed_times));
                player->sendMessage(endstone::ColorFormat::Green + Tran->getLocal("Success: ") + std::to_string(success_times));
                player->sendMessage(endstone::ColorFormat::Green + Tran->getLocal("Failed: ") + std::to_string(failed_times));
            } else {
                player->sendMessage(endstone::ColorFormat::Green + Tran->getLocal("Nothing happened"));
                player->sendMessage(endstone::ColorFormat::Green + Tran->getLocal("Failed: ") + std::to_string(failed_times));
            }
        }
    }
}

// 批量更新回溯状态
void TianyanPlugin::updateRevertStatus() const
{
    if (TianyanCore::revertStatusCache.empty()) {
        return;
    }
    if (yuhangle::clean_data_status == 2) {
        return;
    }
    vector<pair<string, string>> localCache;
    {
        std::lock_guard lock(cacheMutex);  // 互斥锁
        localCache.swap(TianyanCore::revertStatusCache);
    }
    //异步写入
    std::thread updateRevertStatus_thread ([this,localCache]()mutable {
        if (!db_backend_->updateStatusesByUUIDs(localCache)) {
            std::cerr << "Update revert status failed" << std::endl;
            if (localCache.size() > 1000000) {
                std::cerr << "Unable to write a large volume of logs; cached logs will be discarded" << endl;
                localCache.clear();
            } else {
                TianyanCore::revertStatusCache.insert(TianyanCore::revertStatusCache.begin(), localCache.begin(), localCache.end());
            }
        }
    });
    updateRevertStatus_thread.detach();
}

// 专门用于注册期的翻译

std::string StaticTranslate::get(const std::string& key) {
    try {
        namespace fs = std::filesystem;
        constexpr auto lang_dir   = "plugins/tianyan_data/language/";
        std::string lang = "en_US";
        if (constexpr auto config_path = "plugins/tianyan_data/config.json"; fs::exists(config_path)) {
            std::ifstream i(config_path);
            ordered_json j;
            i >> j;
            if (j.contains("language")) {
                lang = j["language"].get<std::string>();
            }
        }

        if (std::string lang_file = lang_dir + lang + ".json"; fs::exists(lang_file)) {
            std::ifstream f(lang_file);
            if (json res = json::parse(f); res.contains(key)) {
                return res[key].get<std::string>();
            }
        }
    } catch (...) {
    }
    return key;
}

//api接口

// 转换函数
std::vector<tianyan::LogData> TianyanPlugin::processLogConversion(const std::vector<TianyanCore::LogData>& source, const int limit) {
    std::vector<tianyan::LogData> result;
    if (source.empty()) return result;

    // 确定转换条数
    const size_t count = (limit > 0 && source.size() > static_cast<size_t>(limit)) ? static_cast<size_t>(limit) : source.size();
    result.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        const auto& [uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, time, type, data, status] = source[i];
        result.push_back({
            uuid, id, name,
            pos_x, pos_y, pos_z,
            world, obj_id, obj_name,
            time, type, data, status
        });
    }
    return result;
}

// --- 同步版本实现 ---
std::vector<tianyan::LogData> TianyanPlugin::getLogDataSync(double hours, const int limit) const
{
    if (!isCompatible()) return {};
    const auto searchData = tyCore->searchLog({"", hours});
    return processLogConversion(searchData, limit);
}

// --- 异步版本实现 ---
std::future<std::vector<tianyan::LogData>> TianyanPlugin::getLogDataAsync(double hours) {
    if (!isCompatible()) return {};
    return std::async(std::launch::async, [this, hours]() {
        const auto searchData = tyCore->searchLog({"", hours});
        return processLogConversion(searchData, -1);
    });
}

std::vector<tianyan::LogData> TianyanPlugin::getLogDataSyncImpl(double seconds, const int limit) {
    const auto searchData = tyCore->searchLog({"", seconds});
    return processLogConversion(searchData, limit);
}