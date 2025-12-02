//
// Created by yuhang on 2025/5/17.
//

#include "tianyan_plugin.h"
#include "version.h"
#include <thread>

    //数据目录和配置文件检查
void TianyanPlugin::datafile_check() const {
    json df_config = {
        {"language","zh_CN"},
        {"10s_message_max", 6},
        {"10s_command_max", 12},
        {"no_log_mobs", {"minecraft:zombie_pigman","minecraft:zombie","minecraft:skeleton","minecraft:bogged","minecraft:slime"}}
    };

    if (!(std::filesystem::exists(dataPath))) {
        getLogger().info(Tran.getLocal("No data path,auto create"));
        std::filesystem::create_directory(dataPath);
        std::filesystem::create_directory(language_path);
        if (!(std::filesystem::exists(config_path))) {
            if (std::ofstream file(config_path); file.is_open()) {
                file << df_config.dump(4);
                file.close();
                getLogger().info(Tran.getLocal("Config file created"));
            }
        }
    } else if (std::filesystem::exists(dataPath)) {
        if (!(std::filesystem::exists(config_path))) {
            if (std::ofstream file(config_path); file.is_open()) {
                file << df_config.dump(4);
                file.close();
                getLogger().info(Tran.getLocal("Config file created"));
            }
        } else {
            bool need_update = false;
            json loaded_config;

            // 加载现有配置文件
            std::ifstream file(config_path);
            file >> loaded_config;

            // 检查配置完整性并更新
            for (auto& [key, value] : df_config.items()) {
                if (!loaded_config.contains(key)) {
                    loaded_config[key] = value;
                    getLogger().info(Tran.tr(Tran.getLocal("Config '{}' has update with default config"), key));
                    need_update = true;
                }
            }

            // 如果需要更新配置文件，则进行写入
            if (need_update) {
                if (std::ofstream outfile(config_path); outfile.is_open()) {
                    outfile << loaded_config.dump(4);
                    outfile.close();
                    getLogger().info(Tran.getLocal("Config file update over"));
                }
            }
        }
    }
}

// 读取配置文件
[[nodiscard]] json TianyanPlugin::read_config() const {
    std::ifstream i(config_path);
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

void TianyanPlugin::onLoad()
{
    getLogger().info("onLoad is called");
    //初始化目录
    if (!(filesystem::exists(dataPath))) {
        getLogger().info("No data path,auto create");
        filesystem::create_directory(dataPath);
    }
    //获取服务器语言
    const string sever_lang = getServer().getLanguage().getLocale();
    language_file = dataPath + "/language/"+sever_lang+".json";
    Tran = translate(language_file);
    //加载语言
    const auto [fst, snd] = Tran.loadLanguage();
    getLogger().info(snd);
#ifdef __linux__
    namespace fs = std::filesystem;
    try {
        // 获取当前路径
        const fs::path currentPath = fs::current_path();

        // 子目录路径
        const fs::path subdir = dbPath;

        // 拼接路径
        const fs::path fullPath = currentPath / subdir;

        // 如果需要将最终路径转换为 string 类型
        const std::string finalPathStr = fullPath.string();

        // 使用完整路径重新初始化Database
        Database = yuhangle::Database(finalPathStr);
        tyCore = TianyanCore(Database);
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
    (void)Database.init_database();
    datafile_check();
    //进行一个配置文件的读取
    json json_msg = read_config();
    try {
        string lang = "en_US";
        if (!json_msg.contains("error")) {
            max_message_in_10s = json_msg["10s_message_max"];
            max_command_in_10s = json_msg["10s_command_max"];
            no_log_mobs = json_msg["no_log_mobs"];
            lang = json_msg["language"];
            language_file = dataPath + "/language/"+lang+".json";
        } else {
            getLogger().error(Tran.getLocal("Config file error!Use default config"));
            max_message_in_10s = 6;
            max_command_in_10s = 12;
            no_log_mobs = {"minecraft:zombie_pigman","minecraft:zombie","minecraft:skeleton","minecraft:bogged","minecraft:slime"};
        }
    } catch (const std::exception& e) {
        max_message_in_10s = 6;
        max_command_in_10s = 12;
        no_log_mobs = {"minecraft:zombie_pigman","minecraft:zombie","minecraft:skeleton","minecraft:bogged","minecraft:slime"};
        getLogger().error(Tran.getLocal("Config file error!Use default config")+","+e.what());
    }
    Tran = translate(language_file);
    Tran.loadLanguage();
    translate::checkLanguageCommon(language_file,dataPath+"/language/lang.json");
    //定期写入
    getServer().getScheduler().runTaskTimer(*this, [&]() {logsCacheWrite();},0,60);
    //数据库清理后台检查
    getServer().getScheduler().runTaskTimer(*this,[&](){checkDatabaseCleanStatus();},0,20);
    //tyback命令后台检查
    getServer().getScheduler().runTaskTimer(*this,[&](){checkTybackSearchThread();},0,20);
    //完成外部类初始化
    protect_ = std::make_unique<TianyanProtect>(*this);
    eventListener_ = std::make_unique<EventListener>(*this);
    menu_ = std::make_unique<Menu>(*this);
    protect_->deviceIDBlacklistInit();
    //注册事件
    registerEvent<endstone::BlockBreakEvent>(EventListener::onBlockBreak);
    registerEvent<endstone::BlockPlaceEvent>(EventListener::onBlockPlace);
    registerEvent<endstone::ActorDamageEvent>(EventListener::onActorDamage);
    registerEvent<endstone::PlayerInteractEvent>(EventListener::onPlayerRightClickBlock);
    registerEvent<endstone::PlayerInteractActorEvent>(EventListener::onPlayerRightClickActor);
    registerEvent<endstone::ActorExplodeEvent>(EventListener::onActorBomb);
    registerEvent<endstone::BlockPistonExtendEvent>(EventListener::onPistonExtend);
    registerEvent<endstone::BlockPistonRetractEvent>(EventListener::onPistonRetract);
    registerEvent<endstone::ActorDeathEvent>(EventListener::onActorDie);
    registerEvent<endstone::PlayerPickupItemEvent>(EventListener::onPlayPickup);
    registerEvent<endstone::PlayerDeathEvent>(EventListener::onPlayerDie);
    registerEvent(&EventListener::onPlayerJoin, *eventListener_);
    registerEvent(&EventListener::onPlayerSendMSG, *eventListener_);
    registerEvent(&EventListener::onPlayerSendCMD, *eventListener_);
    registerEvent(&EventListener::onPlayerTryJoin, *eventListener_);
    const string LOGO = R"(
_____   _
|_   _| (_)  __ _   _ _    _  _   __ _   _ _
| |   | | / _` | | ' \  | || | / _` | | ' \
|_|   |_| \__,_| |_||_|  \_, | \__,_| |_||_|
                        |__/
    )";
    getLogger().info(endstone::ColorFormat::Yellow+LOGO);
    const auto p_version = getServer().getPluginManager().getPlugin("tianyan_plugin")->getDescription().getVersion();
    getLogger().info(endstone::ColorFormat::Yellow + Tran.getLocal("Tianyan Plugin Version: ") + p_version);
    getLogger().info(endstone::ColorFormat::Yellow + Tran.getLocal("Repo: ")+"https://github.com/yuhangle/endstone-tianyan-plugin");
    getLogger().info("You can change the plugin’s language by editing the config file. Choose a language from the language folder.");
}

void TianyanPlugin::onDisable()
{
    getLogger().info("onDisable is called");
    logsCacheWrite();
}

bool TianyanPlugin::onCommand(endstone::CommandSender &sender, const endstone::Command &command, const std::vector<std::string> &args)
{
    if (command.getName() == "ty")
    {
        // 菜单
        if (args.empty()) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran.getLocal("Console not support menu"));
                return false;
            }
            Menu::tyMenu(*sender.asPlayer());
        }
        else if (args.size() >= 2) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran.getLocal("Console not support this command"));
                return false;
            }
            try {
                const double r = stod(args[0]);
                const double time = stod(args[1]);
                if (r > 100) {
                    sender.sendErrorMessage(Tran.getLocal("The radius cannot be greater than 100"));
                    return false;
                }
                if (time > 672) {
                    sender.sendErrorMessage(Tran.getLocal("The time cannot be greater than 672"));
                    return false;
                }
                const string search_key_type = args.size() > 2 ? args[2] : "";
                const string search_key = args.size() > 3 ? args[3] : "";
                const string world = sender.asPlayer()->getLocation().getDimension()->getName();
                const double x = sender.asPlayer()->getLocation().getX();
                const double y = sender.asPlayer()->getLocation().getY();
                const double z = sender.asPlayer()->getLocation().getZ();
                if (const auto searchData = tyCore.searchLog({"",time},x, y, z, r, world); searchData.empty()) {
                    sender.sendErrorMessage(Tran.getLocal("No log found"));
                } else {
                    if (!search_key_type.empty() && !search_key.empty()) {
                        vector<TianyanCore::LogData> key_logData;
                        if (search_key_type == "source_id") {
                            for (auto &logData : searchData) {
                                if (logData.id == search_key) {
                                    key_logData.push_back(logData);
                                }
                            }
                        } else if (search_key_type == "source_name") {
                            for (auto &logData : searchData) {
                                if (logData.name == search_key) {
                                    key_logData.push_back(logData);
                                }
                            }
                        } else if (search_key_type == "target_id") {
                            for (auto &logData : searchData) {
                                if (logData.obj_id == search_key) {
                                    key_logData.push_back(logData);
                                }
                            }
                        } else if (search_key_type == "target_name") {
                            for (auto &logData : searchData) {
                                if (logData.obj_name == search_key) {
                                    key_logData.push_back(logData);
                                }
                            }
                        } else if (search_key_type == "action") {
                            for (auto &logData : searchData) {
                                if (logData.type == search_key) {
                                    key_logData.push_back(logData);
                                }
                            }
                        }
                        if (!key_logData.empty()) {
                            Menu::showLogMenu(*sender.asPlayer(), key_logData);
                            sender.sendMessage(endstone::ColorFormat::Yellow+Tran.getLocal("Display all logs about")+"` "+search_key+" `");
                            //提示数据过大
                            if (searchData.size() > 9999) {
                                sender.sendErrorMessage(Tran.getLocal("Too many logs, please narrow the search range,display only 10,000 logs"));
                            }
                        }
                        else {
                            sender.sendErrorMessage(Tran.getLocal("No log found"));
                        }
                    } else {
                        Menu::showLogMenu(*sender.asPlayer(), searchData);
                        sender.sendMessage(endstone::ColorFormat::Yellow+Tran.getLocal("Display all logs"));
                        //提示数据过大
                        if (searchData.size() > 9999) {
                            sender.sendErrorMessage(Tran.getLocal("Too many logs, please narrow the search range,display only 10,000 logs"));
                        }
                    }
                }

            } catch (const std::exception &e) {
                //返回错误给玩家
                sender.sendErrorMessage(e.what());
            }
        }
    }
    else if (command.getName() == "tyback")
    {
        // 菜单
        if (args.empty()) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran.getLocal("Console not support menu"));
                return false;
            }
            Menu::tybackMenu(*sender.asPlayer());
        }
        else if (args.size() >=2) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran.getLocal("Console not support this command"));
                return false;
            }
            try {
                //玩家有后台tyback操作未完成阻止使用
                if (tyback_cache.player_name == sender.getName() && tyback_cache.status == false) {
                    sender.sendErrorMessage(Tran.getLocal("A background operation is in progress. Please wait for it to complete"));
                    return false;
                }
                const double r = stod(args[0]);
                if (r > 100) {
                    sender.sendErrorMessage(Tran.getLocal("The radius cannot be greater than 100"));
                    return false;
                }
                const double time = stod(args[1]);
                const string source_key_type = args.size() > 2 ? args[2] : "";
                const string source_key = args.size() > 3 ? args[3] : "";
                const string world = sender.asPlayer()->getLocation().getDimension()->getName();
                const double x = sender.asPlayer()->getLocation().getX();
                const double y = sender.asPlayer()->getLocation().getY();
                const double z = sender.asPlayer()->getLocation().getZ();
                if (auto searchData = tyCore.searchLog({"",time},x, y, z, r, world); searchData.empty()) {
                    sender.sendErrorMessage(Tran.getLocal("No log found"));
                } else {
                    //数据过大，启动后台模式
                    if (searchData.size() > 9999 && tyback_cache.status == false) {
                        sender.sendMessage(Tran.getLocal("Too many logs. Loading in the background to avoid lag — please wait"));
                        //不允许多个后台任务
                        if (tyback_cache.is_running) {
                            sender.sendErrorMessage(Tran.getLocal("A background operation is in progress. Please wait for it to complete"));
                            return false;
                        }
                        tyback_cache.player_name = sender.asPlayer()->getName();
                        tyback_cache.time = time;tyback_cache.r = r;
                        std::thread tyback_thread( [this,time, x, y, z, r, world]() {
                            tyback_cache.is_running = true;
                            const auto searchData_ = tyCore.searchLog({"",time},x, y, z, r, world, true);
                            tyback_cache.logDatas = searchData_;
                            tyback_cache.status = true;
                        });
                        tyback_thread.detach();
                        return true;
                    }
                    if (tyback_cache.status == true) {
                        searchData = tyback_cache.logDatas;
                    }
                    //回溯逻辑
                    int success_times = 0;int failed_times = 0;
                    for (auto& logData : std::ranges::reverse_view(searchData)) {
                        //跳过取消掉和已回溯的事件
                        if (logData.status == "canceled" || logData.status == "reverted") {
                            continue;
                        }
                        //输入了查找指定源
                        if (!source_key_type.empty() && !source_key.empty()) {
                            //查询ID而事件非源ID
                            if (source_key_type == "source_id" && logData.id != source_key) {
                                continue;
                            }
                            //查询名称而事件非源名称
                            if (source_key_type == "source_name" && logData.name != source_key) {
                                continue;
                            }
                            //查询ID而事件非目标ID
                            if (source_key_type == "target_id" && logData.obj_id != source_key) {
                                continue;
                            }
                            //查询名称而事件非目标名称
                            if (source_key_type == "target_name" && logData.obj_name != source_key) {
                                continue;
                            }
                            //查询类型而事件非指定类型
                            if (source_key_type == "action" && logData.type != source_key) {
                                continue;
                            }
                        }
                        //破坏和爆炸方块的恢复
                        if (logData.type == "block_break" || logData.type == "block_break_bomb" || (logData.type == "actor_bomb" && logData.id == "minecraft:tnt")) {
                            string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                            std::ostringstream cmd;
                            cmd << "setblock " << pos << " " << logData.obj_id << logData.data;
                            endstone::CommandSenderWrapper wrapper_sender(sender,
                                [&success_times](const endstone::Message &message) {success_times++;},
                                [&failed_times](const endstone::Message &message) {failed_times++;}
                                );
                            // 将已回溯的事件UUID和状态添加到缓存中
                            if (getServer().dispatchCommand(wrapper_sender,cmd.str())) {
                                revertStatusCache.emplace_back(logData.uuid, "reverted");
                            }
                        }
                        //对玩家右键方块的状态复原
                        else if (logData.type == "player_right_click_block") {
                            //右键方块存在状态
                            if (auto hand_block = yuhangle::Database::splitString(logData.data); hand_block[1] != "[]") {
                                string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                                std::ostringstream cmd;
                                cmd << "setblock " << pos << " " << logData.obj_id << hand_block[1];
                                endstone::CommandSenderWrapper wrapper_sender(sender,
                                [&success_times](const endstone::Message &message) {success_times++;},
                                [&failed_times](const endstone::Message &message) {failed_times++;}
                                );
                                if (getServer().dispatchCommand(wrapper_sender,cmd.str())) {
                                    revertStatusCache.emplace_back(logData.uuid, "reverted");
                                }
                            }
                        }
                        //放置方块的恢复
                        else if (logData.type == "block_place") {
                            string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                            std::ostringstream cmd;
                            cmd << "setblock " << pos << " minecraft:air";
                            endstone::CommandSenderWrapper wrapper_sender(sender,
                            [&success_times](const endstone::Message &message) {success_times++;},
                            [&failed_times](const endstone::Message &message) {failed_times++;}
                            );
                            if (getServer().dispatchCommand(wrapper_sender,cmd.str())) {
                                revertStatusCache.emplace_back(logData.uuid, "reverted");
                            }
                        }
                        //复活吧我的生物
                        else if (logData.type == "entity_die") {
                            string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                            std::string obj_id = logData.obj_id;
                            if (size_t vpos = obj_id.find("villager_v2"); vpos != std::string::npos) {
                                obj_id.replace(vpos, 11, "villager");
                            }
                            // 人被杀，就会死
                            if (obj_id == "minecraft:player") {
                                continue;
                            }
                            std::ostringstream cmd;
                            cmd << "summon " << obj_id << " " << pos;
                            endstone::CommandSenderWrapper wrapper_sender(sender,
                            [&success_times](const endstone::Message &message) {success_times++;},
                            [&failed_times](const endstone::Message &message) {failed_times++;}
                            );
                            if (getServer().dispatchCommand(wrapper_sender,cmd.str())) {
                                revertStatusCache.emplace_back(logData.uuid, "reverted");
                            }
                        }
                    }
                    // 执行批量更新回溯状态
                    updateRevertStatus();
                    auto green = endstone::ColorFormat::Green;
                    if (success_times > 0) {
                        sender.sendMessage(green + Tran.getLocal("Revert times: ")+std::to_string(success_times+failed_times));
                        sender.sendMessage(green + Tran.getLocal("Success: ")+std::to_string(success_times));
                        sender.sendMessage(green + Tran.getLocal("Failed: ")+std::to_string(failed_times));
                    } else {
                        sender.sendMessage(green + Tran.getLocal("Nothing happened"));
                        sender.sendMessage(green + Tran.getLocal("Failed: ")+std::to_string(failed_times));
                    }
                    //清理后台缓存
                    tyback_cache = {false};
                }
            } catch (const std::exception &e) {
                //返回错误给玩家
                sender.sendErrorMessage(e.what());
            }
        }
    }
    else if (command.getName() == "tys")
    {
        // 菜单
        if (args.empty()) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran.getLocal("Console not support menu"));
                return false;
            }
            Menu::tysMenu(*sender.asPlayer());
        }
        else if (!args.empty()) {
            if (!sender.asPlayer()) {
                sender.sendErrorMessage(Tran.getLocal("Console not support this command"));
                return false;
            }
            try {
                const double time = stod(args[0]);
                const string search_key_type = args.size() > 1 ? args[1] : "";
                const string search_key = args.size() > 2 ? args[2] : "";
                const string world = sender.asPlayer()->getLocation().getDimension()->getName();
                if (const auto searchData = tyCore.searchLog({"",time}); searchData.empty()) {
                    sender.sendErrorMessage(Tran.getLocal("No log found"));
                } else {
                    if (!search_key_type.empty() && !search_key.empty()) {
                        vector<TianyanCore::LogData> key_logData;
                        if (search_key_type == "source_id") {
                            for (auto &logData : searchData) {
                                if (logData.id == search_key) {
                                    key_logData.push_back(logData);
                                }
                            }
                        } else if (search_key_type == "source_name") {
                            for (auto &logData : searchData) {
                                if (logData.name == search_key) {
                                    key_logData.push_back(logData);
                                }
                            }
                        } else if (search_key_type == "target_id") {
                            for (auto &logData : searchData) {
                                if (logData.obj_id == search_key) {
                                    key_logData.push_back(logData);
                                }
                            }
                        } else if (search_key_type == "target_name") {
                            for (auto &logData : searchData) {
                                if (logData.obj_name == search_key) {
                                    key_logData.push_back(logData);
                                }
                            }
                        } else if (search_key_type == "action") {
                            for (auto &logData : searchData) {
                                if (logData.type == search_key) {
                                    key_logData.push_back(logData);
                                }
                            }
                        }
                        if (!key_logData.empty()) {
                            Menu::showLogMenu(*sender.asPlayer(), key_logData);
                            sender.sendMessage(endstone::ColorFormat::Yellow+Tran.getLocal("Display all logs about")+"` "+search_key+" `");
                            //提示数据过大
                            if (searchData.size() > 9999) {
                                sender.sendErrorMessage(Tran.getLocal("Too many logs, please narrow the search range,display only 10,000 logs"));
                            }
                        }
                        else {
                            sender.sendErrorMessage(Tran.getLocal("No log found"));
                        }
                    } else {
                        Menu::showLogMenu(*sender.asPlayer(), searchData);
                        sender.sendMessage(endstone::ColorFormat::Yellow+Tran.getLocal("Display all logs"));
                        //提示数据过大
                        if (searchData.size() > 9999) {
                            sender.sendErrorMessage(Tran.getLocal("Too many logs, please narrow the search range,display only 10,000 logs"));
                        }
                    }
                }

            } catch (const std::exception &e) {
                //返回错误给玩家
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
                    sender.sendMessage(Tran.tr(Tran.getLocal("Device ID {} banned successfully"),device_id));
                } else {
                    sender.sendErrorMessage(Tran.tr(Tran.getLocal("Error occurred while banning device ID {}: {}"), device_id, Tran.getLocal("View more in console")));
                }
            }
        }
    } else if (command.getName() == "unban-id") {
        if (!args.empty()) {
            const string& device_id = args[0];
            const auto status = protect_->UnbanDeviceID(device_id);
            if (sender.asPlayer()) {
                if (status) {
                    sender.sendMessage(Tran.tr(Tran.getLocal("Device ID {} unbanned successfully"), device_id));
                } else {
                    sender.sendErrorMessage(Tran.tr(Tran.getLocal("Error occurred while unbanning device ID {}: {}"), device_id, Tran.getLocal("View more in console")));
                }
            }
        }
    } else if (command.getName() == "banlist-id") {
        sender.sendMessage(Tran.getLocal("The list of baned device: "));
        if (BanIDPlayers.empty()) {
            sender.sendMessage(Tran.getLocal("Nothing"));
            return true;
        }
        for (auto &[player_name, device_id, reason, time] : BanIDPlayers) {
            sender.sendMessage(Tran.tr(Tran.getLocal("Device ID: {}"), device_id));
            sender.sendMessage(Tran.tr(Tran.getLocal("Player: {}"), player_name));
            sender.sendMessage(Tran.tr(Tran.getLocal("Reason: {}"), reason.value_or("")));
            sender.sendMessage(Tran.tr(Tran.getLocal("Time: {}"), time));
            sender.sendMessage("----------------------");
        }
    }
    else if (command.getName() == "tyclean") {
        if (!args.empty()) {
            if (clean_data_status == 2) {
                sender.sendErrorMessage(Tran.getLocal("A background operation is in progress. Please wait for it to complete"));
                return false;
            }
            int hours = yuhangle::Database::stringToInt(args[0]);
            clean_data_sender_name = sender.getName();
            sender.sendMessage(endstone::ColorFormat::Yellow+Tran.tr(Tran.getLocal("Start cleaning logs older than {} hours"), args[0]));
            std::thread clean_thread([hours]() {
                (void)Database.cleanDataBase(hours);
            });
            clean_thread.detach();
        }
    }
    else if (command.getName() == "tyo") {
        if (!sender.asPlayer()) {
            sender.sendErrorMessage(Tran.getLocal("Console not support menu"));
            return false;
        }
        if (!args.empty()) {
            const string& player_name = args[0];
            if (auto player = getServer().getPlayer(player_name)) {
                Menu::showOnlinePlayerBag(sender, *player);
            } else {
                sender.sendErrorMessage(Tran.tr(Tran.getLocal("Player {} not found"), player_name));
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
                    Tran.getLocal("Highest density region in dimension"), result.dim.value(),
                    Tran.getLocal("Midpoint coordinates"), result.mid_x.value(), result.mid_y.value(), result.mid_z.value(),
                    Tran.getLocal("Entity count"), result.count.value(),
                    Tran.getLocal("Most common entity"), result.entity_type.value(),
                    Tran.getLocal("Random entity position"), result.entity_pos.value()
                );
                sender.sendMessage(content);
            } else {
                sender.sendMessage(endstone::ColorFormat::Yellow + Tran.getLocal("No entities detected currently"));
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
void TianyanPlugin::logsCacheWrite() {
    if (logDataCache.empty()) {
        return;
    }
    //避开数据清理
    if (clean_data_status == 2) {
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
    std::thread logsCacheWrite_thread ([localCache]() mutable {
        // 检查写入状态
        if (tyCore.recordLogs(localCache)) {
            //超过10万仍无法写入，则清空缓存
            if (localCache.size() > 100000) {
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
    if (clean_data_status == 0) {
        return;
    }
    const auto player = getServer().getPlayer(clean_data_sender_name);
    const auto green = endstone::ColorFormat::Green;
    if (clean_data_status == 1) {
        if (clean_data_sender_name!="Server" && player) {
            player->sendMessage(green+Tran.getLocal("Database clean over"));
            player->sendMessage(green+Tran.getLocal(clean_data_message[0]) + clean_data_message[1] + "s");
            player->sendMessage(green+Tran.getLocal(clean_data_message[2])+clean_data_message[3]);
            getLogger().info(green+Tran.getLocal("Database clean over"));
            getLogger().info(green+Tran.getLocal(clean_data_message[0]) + clean_data_message[1] + "s");
            getLogger().info(green+Tran.getLocal(clean_data_message[2])+clean_data_message[3]);
        } else {
            getLogger().info(green+Tran.getLocal("Database clean over"));
            getLogger().info(green+Tran.getLocal(clean_data_message[0]) + clean_data_message[1] + "s");
            getLogger().info(green+Tran.getLocal(clean_data_message[2])+clean_data_message[3]);
        }
    } else if (clean_data_status == -1) {
        if (clean_data_sender_name!="Server" && player) {
            player->sendErrorMessage(Tran.getLocal("Database clean error"));
            getLogger().error(Tran.getLocal("Database clean error"));
            for (const string& info : clean_data_message) {
                player->sendErrorMessage(info);
                getLogger().error(info);
            }
        } else {
            getLogger().error(Tran.getLocal("Database clean error"));
            for (const string& info : clean_data_message) {
                getLogger().error(info);
            }
        }
    }
    clean_data_status = 0;
    clean_data_message.clear();
}

//检查tyback后台
void TianyanPlugin::checkTybackSearchThread() {
    if (tyback_cache.status == false) {
        return;
    }
    if (const auto player = getServer().getPlayer(tyback_cache.player_name)) {
        player->sendMessage(endstone::ColorFormat::Green+Tran.getLocal("Background log loading completed. You may now resume your operation"));
        std::ostringstream cmd;
        cmd << "tyback " << tyback_cache.r << " " << tyback_cache.time;
        tyback_cache.is_running = false;
        (void)player->performCommand(cmd.str());
    } else {
        tyback_cache = {false};
    }
}

// 批量更新回溯状态
void TianyanPlugin::updateRevertStatus() {
    if (revertStatusCache.empty()) {
        return;
    }
    if (clean_data_status == 2) {
        return;
    }
    vector<pair<string, string>> localCache;
    {
        std::lock_guard lock(cacheMutex);  // 互斥锁
        localCache.swap(revertStatusCache);
    }
    //异步写入
    std::thread updateRevertStatus_thread ([localCache]()mutable {
        if (!Database.updateStatusesByUUIDs(localCache)) {
            std::cerr << "Update revert status failed" << std::endl;
            if (localCache.size() > 100000) {
                std::cerr << "Unable to write a large volume of logs; cached logs will be discarded" << endl;
                localCache.clear();
            } else {
                revertStatusCache.insert(revertStatusCache.begin(), localCache.begin(), localCache.end());
            }
        }
    });
    updateRevertStatus_thread.detach();
}


//插件信息
ENDSTONE_PLUGIN("tianyan_plugin", TIANYAN_PLUGIN_VERSION, TianyanPlugin)
{
    description = "A plugin for endstone to record behavior";
    website = "https://github.com/yuhangle/endstone-tianyan-plugin";
    authors = {"yuhangle"};


        command("ty")
            .description(Tran.getLocal("Check behavior logs at your current location"))
            .usages("/ty",
                    "/ty <r: float> <time: float>",
                    "/ty <r: float> <time: float> <source_id | source_name | target_id | target_name> <keywords: str>",
                    "/ty <r: float> <time: float> <action> <block_break | block_place | entity_damage | player_right_click_block | player_right_click_entity | entity_bomb | block_break_bomb | piston_extend | piston_retract | entity_die | player_pickup_item>"
                    )
            .permissions("ty.command.member");

        command("tyback")
            .description(Tran.getLocal("Revert behaviors by time"))
            .usages("/tyback",
                    "/tyback <r: float> <time: float>",
                    "/tyback <r: float> <time: float> <source_id | source_name | target_id | target_name> <keywords: str>",
                    "/tyback <r: float> <time: float> <action> <block_break | block_place | player_right_click_block | block_break_bomb | entity_die>"
                    )
            .permissions("ty.command.op");

        command("tys")
            .description(Tran.getLocal("Check behavior logs at server"))
            .usages("/tys",
                    "/tys <time: float>",
                    "/tys <time: float> <source_id | source_name | target_id | target_name> <keywords: str>",
                    "/tys <time: float> <action> <block_break | block_place | entity_damage | player_right_click_block | player_right_click_entity | entity_bomb | block_break_bomb | piston_extend | piston_retract | entity_die | player_pickup_item>"
                    )
            .permissions("ty.command.op");

        command("ban-id")
            .description(Tran.getLocal("Ban player by device id"))
            .usages("/ban-id <device-id: str> [reason: str]"
                    )
            .permissions("ty.command.op");

        command("unban-id")
            .description(Tran.getLocal("Unban player by device id"))
            .usages("/unban-id <device-id: str>"
                    )
            .permissions("ty.command.op");

        command("banlist-id")
            .description(Tran.getLocal("List baned player by device id"))
            .usages("/banlist-id"
                    )
            .permissions("ty.command.op");

        command("tyclean")
            .description(Tran.getLocal("Clean database"))
            .usages("/tyclean <time: int>"
                    )
            .permissions("ty.command.op");

        command("tyo")
            .description(Tran.getLocal("View inventory of online player"))
            .usages("/tyo <player_name: player>"
                    )
            .permissions("ty.command.op");

        command("density")
            .description(Tran.getLocal("Find the area with the highest entity density"))
            .usages("/density [size: int]"
                    )
            .permissions("ty.command.op");

    permission("ty.command.member")
            .description("Allow users to use the /ty command.")
            .default_(endstone::PermissionDefault::True);
    permission("ty.command.op")
        .description("OP command.")
        .default_(endstone::PermissionDefault::Operator);
}