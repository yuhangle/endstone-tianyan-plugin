//
// Created by yuhang on 2025/5/17.
//

#include "tianyan_plugin.h"
#include "version.h"
#include <thread>
#include "global.h"
#include  "tianyan_core.h"

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
                    "/ty <r: float> <time: float> <action> <block_break | block_place | entity_damage | player_right_click_block | player_right_click_entity | entity_bomb | block_break_bomb | piston_extend | piston_retract | entity_die | player_pickup_item | player_drop_item>"
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
                    "/tys <time: float> <action> <block_break | block_place | entity_damage | player_right_click_block | player_right_click_entity | entity_bomb | block_break_bomb | piston_extend | piston_retract | entity_die | player_pickup_item | player_drop_item>"
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

    permission("ty.command.member")
            .description(T("Allow users to use the /ty command."))
            .default_(endstone::PermissionDefault::True);
    permission("ty.command.op")
        .description("OP command.")
        .default_(endstone::PermissionDefault::Operator);
}

    //数据目录和配置文件检查
void TianyanPlugin::datafile_check() const {
    json df_config = {
        {"language","zh_CN"},
        {"enable_web_ui",false},
        {"10s_message_max", 6},
        {"10s_command_max", 12},
        {"no_log_mobs", {"minecraft:zombie_pigman","minecraft:zombie","minecraft:skeleton","minecraft:bogged","minecraft:slime"}}
    };

    if (!(std::filesystem::exists(TianyanCore::dataPath))) {
        getLogger().info(Tran->getLocal("No data path,auto create"));
        std::filesystem::create_directory(TianyanCore::dataPath);
        if (!(std::filesystem::exists(TianyanCore::config_path))) {
            if (std::ofstream file(TianyanCore::config_path); file.is_open()) {
                file << df_config.dump(4);
                file.close();
                getLogger().info(Tran->getLocal("Config file created"));
            }
        }
    } else if (std::filesystem::exists(TianyanCore::dataPath)) {
        if (!(std::filesystem::exists(TianyanCore::config_path))) {
            if (std::ofstream file(TianyanCore::config_path); file.is_open()) {
                file << df_config.dump(4);
                file.close();
                getLogger().info(Tran->getLocal("Config file created"));
            }
        } else {
            bool need_update = false;
            json loaded_config;

            // 加载现有配置文件
            std::ifstream file(TianyanCore::config_path);
            file >> loaded_config;

            // 检查配置完整性并更新
            for (auto& [key, value] : df_config.items()) {
                if (!loaded_config.contains(key)) {
                    loaded_config[key] = value;
                    getLogger().info(Tran->tr(Tran->getLocal("Config '{}' has update with default config"), key));
                    need_update = true;
                }
            }

            // 如果需要更新配置文件，则进行写入
            if (need_update) {
                if (std::ofstream outfile(TianyanCore::config_path); outfile.is_open()) {
                    outfile << loaded_config.dump(4);
                    outfile.close();
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
[[nodiscard]] json TianyanPlugin::read_config() const {
    std::ifstream i(TianyanCore::config_path);
    try {
        json j;
        i >> j;
        return j;
    } catch (json::parse_error& ex) { // 捕获解析错误
        getLogger().error( ex.what());
        json error_value = {
                {"error","error"}
        };
        return error_value;
    }
}


#ifdef _WIN32
    #include <windows.h>
    // Windows 下使用 HANDLE 记录进程
    static HANDLE g_web_handle = NULL;
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
    std::string cmd = "python \"" + py_script.string() + "\"";
#else
    std::string cmd = "python3 " + py_script.string();
#endif

#ifdef _WIN32
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    // 创建一个临时 buffer
    char* cmd_buf = _strdup(cmd.c_str());

    if (CreateProcessA(NULL, cmd_buf, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
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
    if (g_web_handle != NULL) {
        // 尝试关闭进程
        if (TerminateProcess(g_web_handle, 0)) {
            std::cout << "[Tianyan] Web server process terminated." << std::endl;
        } else {
            std::cerr << "[Tianyan] Failed to terminate web server. Error: " << GetLastError() << std::endl;
        }
        CloseHandle(g_web_handle); // 释放句柄资源
        g_web_handle = NULL;
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
#ifdef __linux__
    namespace fs = std::filesystem;
    try {
        // 获取当前路径
        const fs::path currentPath = fs::current_path();

        // 子目录路径
        const fs::path subdir = TianyanCore::dbPath;

        // 拼接路径
        const fs::path fullPath = currentPath / subdir;

        // 如果需要将最终路径转换为 string 类型
        const std::string finalPathStr = fullPath.string();

        // 使用完整路径重新初始化Database
        Database = std::make_unique<yuhangle::Database>(finalPathStr);
        tyCore = std::make_unique<TianyanCore>(*Database);
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << std::endl;
    }
#endif
}

void TianyanPlugin::onEnable()
{
    getLogger().info("onEnable is called");
    (void)Database->init_database();
    datafile_check();
    //进行一个配置文件的读取
    json json_msg = read_config();
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
    registerEvent<endstone::BlockPistonExtendEvent>(EventListener::onPistonExtend, endstone::EventPriority::Monitor);
    registerEvent<endstone::BlockPistonRetractEvent>(EventListener::onPistonRetract, endstone::EventPriority::Monitor);
    registerEvent<endstone::ActorDeathEvent>(EventListener::onActorDie, endstone::EventPriority::Monitor);
    registerEvent<endstone::PlayerDeathEvent>(EventListener::onPlayerDie, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerPickup, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerDropItem, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerJoin, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerLeave, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerSendMSG, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerSendCMD, *eventListener_, endstone::EventPriority::Monitor);
    registerEvent(&EventListener::onPlayerTryJoin, *eventListener_, endstone::EventPriority::Monitor);
    const string LOGO = R"(
_____   _
|_   _| (_)  __ _   _ _    _  _   __ _   _ _
| |   | | / _` | | ' \  | || | / _` | | ' \
|_|   |_| \__,_| |_||_|  \_, | \__,_| |_||_|
                        |__/
    )";
    getLogger().info(endstone::ColorFormat::Yellow+LOGO);
    const auto p_version = getServer().getPluginManager().getPlugin("tianyan_plugin")->getDescription().getVersion();
    getLogger().info(endstone::ColorFormat::Yellow + Tran->getLocal("Tianyan Plugin Version: ") + p_version);
    getLogger().info(endstone::ColorFormat::Yellow + Tran->getLocal("Repo: ")+"https://github.com/yuhangle/endstone-tianyan-plugin");
    getLogger().info("You can change the plugin’s language by editing the config file. Choose a language from the language folder.");
    if (TianyanCore::enable_web_ui)
    {
        start_web_server(TianyanCore::dbPath);
#ifdef _WIN32
        windows_print_webui_log = getServer().getScheduler().runTaskTimer(*this, [&]() {dump_webui_log_once();},0,20);
#endif
    }
    //注册api
    const auto api_ptr = std::shared_ptr<ITianyanAPI>(this, [](ITianyanAPI*){
    });
    // 注册到服务管理器
    getServer().getServiceManager().registerService("TianyanAPI", api_ptr, *this, endstone::ServicePriority::Normal);
}

void TianyanPlugin::onDisable()
{
    getLogger().info("onDisable is called");
    logsCacheWrite();
    if (TianyanCore::enable_web_ui)
    {
        stop_web_server();
    }
    getServer().getScheduler().cancelTasks(*this);
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
                {
                    std::lock_guard lock(async_tasks_mutex_);
                    for (const auto& t : async_tasks_) {
                        if (t.player_name == sender.getName() && t.is_running) {
                            sender.sendErrorMessage(Tran->getLocal("A background operation is in progress. Please wait for it to complete"));
                            return false;
                        }
                    }
                    AsyncQueryTask task;
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

                std::thread([this, player_name = sender.getName(), hours = time, r, world, x, y, z]() {
                    auto results = tyCore->searchLog({"", hours}, x, y, z, r, world);
                    std::lock_guard lock(async_tasks_mutex_);
                    for (auto& t : async_tasks_) {
                        if (t.player_name == player_name && t.is_running) {
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
                // 检查是否有进行中的后台任务
                {
                    std::lock_guard lock(async_tasks_mutex_);
                    for (const auto& t : async_tasks_) {
                        if (t.player_name == sender.getName() && t.is_running) {
                            sender.sendErrorMessage(Tran->getLocal("A background operation is in progress. Please wait for it to complete"));
                            return false;
                        }
                    }
                }
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
                {
                    std::lock_guard lock(async_tasks_mutex_);
                    AsyncQueryTask task;
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

                std::thread([this, player_name = sender.getName(), hours = time, r, world, x, y, z]() {
                    auto results = tyCore->searchLog({"", hours}, x, y, z, r, world);
                    std::lock_guard lock(async_tasks_mutex_);
                    for (auto& t : async_tasks_) {
                        if (t.player_name == player_name && t.is_running) {
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
                const double time = stod(args[0]);

                // 提交后台查询任务
                {
                    const string search_key_type = args.size() > 1 ? args[1] : "";
                    const string search_key = args.size() > 2 ? args[2] : "";
                    std::lock_guard lock(async_tasks_mutex_);
                    for (const auto& t : async_tasks_) {
                        if (t.player_name == sender.getName() && t.is_running) {
                            sender.sendErrorMessage(Tran->getLocal("A background operation is in progress. Please wait for it to complete"));
                            return false;
                        }
                    }
                    AsyncQueryTask task;
                    task.type = AsyncQueryTask::Type::Tys;
                    task.player_name = sender.getName();
                    task.is_running = true;
                    task.hours = time;
                    task.key_type = search_key_type;
                    task.key = search_key;
                    async_tasks_.push_back(std::move(task));
                }

                sender.sendMessage(endstone::ColorFormat::Yellow + Tran->getLocal("Searching in the background, please wait"));

                std::thread([this, player_name = sender.getName(), hours = time]() {
                    auto results = tyCore->searchLog({"", hours});
                    std::lock_guard lock(async_tasks_mutex_);
                    for (auto& t : async_tasks_) {
                        if (t.player_name == player_name && t.is_running) {
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
            int hours = yuhangle::Database::stringToInt(args[0]);
            clean_data_sender_name = sender.getName();
            sender.sendMessage(endstone::ColorFormat::Yellow+Tran->tr(Tran->getLocal("Start cleaning logs older than {} hours"), args[0]));
            std::thread clean_thread([this,hours]() {
                (void)Database->cleanDataBase(hours);
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
            if (auto player = getServer().getPlayer(player_name)) {
                menu_->showOnlinePlayerBag(sender, *player);
            } else {
                sender.sendErrorMessage(Tran->tr(Tran->getLocal("Player {} not found"), player_name));
            }
        }
    }
    else if (command.getName() == "density") {
        if (!sender.asPlayer()) {
            int size = 20;
            if (!args.empty()) {
                size = yuhangle::Database::stringToInt(args[0]);
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
                int size = yuhangle::Database::stringToInt(args[0]);
                menu_->findHighDensityRegion(*sender.asPlayer(), size);
            }
        }
    }
    return true;
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

//检查异步的数据库清理状态
void TianyanPlugin::checkDatabaseCleanStatus() const {
    if (yuhangle::clean_data_status == 0) {
        return;
    }
    const auto player = getServer().getPlayer(clean_data_sender_name);
    const auto green = endstone::ColorFormat::Green;
    if (yuhangle::clean_data_status == 1) {
        if (clean_data_sender_name!="Server" && player) {
            player->sendMessage(green+Tran->getLocal("Database clean over"));
            player->sendMessage(green+Tran->getLocal(yuhangle::clean_data_message[0]) + yuhangle::clean_data_message[1] + "s");
            player->sendMessage(green+Tran->getLocal(yuhangle::clean_data_message[2])+yuhangle::clean_data_message[3]);
            getLogger().info(green+Tran->getLocal("Database clean over"));
            getLogger().info(green+Tran->getLocal(yuhangle::clean_data_message[0]) + yuhangle::clean_data_message[1] + "s");
            getLogger().info(green+Tran->getLocal(yuhangle::clean_data_message[2])+yuhangle::clean_data_message[3]);
        } else {
            getLogger().info(green+Tran->getLocal("Database clean over"));
            getLogger().info(green+Tran->getLocal(yuhangle::clean_data_message[0]) + yuhangle::clean_data_message[1] + "s");
            getLogger().info(green+Tran->getLocal(yuhangle::clean_data_message[2])+yuhangle::clean_data_message[3]);
        }
    } else if (yuhangle::clean_data_status == -1) {
        if (clean_data_sender_name!="Server" && player) {
            player->sendErrorMessage(Tran->getLocal("Database clean error"));
            getLogger().error(Tran->getLocal("Database clean error"));
            for (const string& info : yuhangle::clean_data_message) {
                player->sendErrorMessage(info);
                getLogger().error(info);
            }
        } else {
            getLogger().error(Tran->getLocal("Database clean error"));
            for (const string& info : yuhangle::clean_data_message) {
                getLogger().error(info);
            }
        }
    }
    yuhangle::clean_data_status = 0;
    yuhangle::clean_data_message.clear();
}

//检查后台查询任务
void TianyanPlugin::checkAsyncTasks() {
    // 收集已完成的任务
    vector<AsyncQueryTask> completed;
    {
        std::lock_guard lock(async_tasks_mutex_);
        for (auto it = async_tasks_.begin(); it != async_tasks_.end();) {
            if (it->is_complete) {
                completed.push_back(std::move(*it));
                it = async_tasks_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 在服务器线程上处理每个已完成任务
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
            for (auto& logData : std::ranges::reverse_view(task.results)) {
                if (logData.status == "canceled" || logData.status == "reverted") {
                    continue;
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
                    if (auto hand_block = yuhangle::Database::splitString(logData.data); hand_block[1] != "[]") {
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
        if (!Database->updateStatusesByUUIDs(localCache)) {
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
        const std::string config_path = "plugins/tianyan_data/config.json";
        const std::string lang_dir = "plugins/tianyan_data/language/";
        std::string lang = "en_US";
        if (fs::exists(config_path)) {
            std::ifstream i(config_path);
            json j;
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