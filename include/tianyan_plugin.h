//
// Created by yuhang on 2025/5/17.
//
#pragma once

#include <endstone/plugin/plugin.h>
#include <string>
#include <filesystem>
#include <fstream>
#include "translate.h"
#include "DataBase.hpp"
#include "TianyanCore.h"
#include <endstone/player.h>
#include <endstone/server.h>
#include <endstone/event/block/block_break_event.h>
#include <endstone/event/block/block_place_event.h>
#include <endstone/event/actor/actor_explode_event.h>
#include <endstone/event/actor/actor_death_event.h>
#include <endstone/event/player/player_interact_event.h>
#include <endstone/event/player/player_interact_actor_event.h>
#include <endstone/event/actor/actor_damage_event.h>
#include <endstone/event/block/block_piston_extend_event.h>
#include <endstone/event/block/block_piston_retract_event.h>
#include <endstone/color_format.h>
#include <algorithm>
#include <endstone/event/player/player_pickup_item_event.h>
#include <endstone/endstone.hpp>
#include "TianyanProtect.h"
#include "Global.h"
#include "EventListener.h"
#include "Menu.h"

class TianyanPlugin : public endstone::Plugin {
public:
        //数据目录和配置文件检查
    void datafile_check() const {
        json df_config = {
            {"10s_message_max", 6},
            {"10s_command_max", 12},
            {"no_log_mobs", {"minecraft:zombie_pigman","minecraft:zombie","minecraft:skeleton","minecraft:bogged","minecraft:slime"}}
        };

        if (!(std::filesystem::exists(dataPath))) {
            getLogger().info(Tran.getLocal("No data path,auto create"));
            std::filesystem::create_directory(dataPath);
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
    [[nodiscard]] json read_config() const {
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

    void onLoad() override
    {
        getLogger().info("onLoad is called");
        //初始化目录
        if (!(filesystem::exists(dataPath))) {
            getLogger().info("No data path,auto create");
            filesystem::create_directory(dataPath);
        }
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
            Database = DataBase(finalPathStr);
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

    //缓存写入机制
    void logsCacheWrite() const {
        if (logDataCache.empty()) {
            return;
        }
        //检查写入状态
        if (tyCore.recordLogs(logDataCache)) {
            getLogger().error(Tran.getLocal("Failed to write cached logs"));
            if (logDataCache.size() > 100000) {
                getLogger().error("Unable to write a large volume of logs; cached logs will be discarded");
                logDataCache.clear();
            }
        } else {
            logDataCache.clear();
        }
    }

    //检查异步的数据库清理状态
    void checkDatabaseCleanStatus() const {
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

    // 批量更新回溯状态
    void updateRevertStatus() const {
        if (revertStatusCache.empty()) {
            return;
        }

        // 使用数据库的批量更新方法更新状态
        if (!Database.updateStatusesByUUIDs(revertStatusCache)) {
            getLogger().error("Update revert status failed");
            if (revertStatusCache.size() > 100000) {
                getLogger().error("Unable to write a large volume of logs; cached logs will be discarded");
                revertStatusCache.clear();
            }
        } else {
            // 写入成功才清空缓存
            revertStatusCache.clear();
        }
    }
    void onEnable() override
    {
        getLogger().info("onEnable is called");
        (void)Database.init_database();
        datafile_check();
                //进行一个配置文件的读取
        json json_msg = read_config();
        try {
            if (!json_msg.contains("error")) {
                max_message_in_10s = json_msg["10s_message_max"];
                max_command_in_10s = json_msg["10s_command_max"];
                no_log_mobs = json_msg["no_log_mobs"];
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
        //定期写入
        auto_write_task = getServer().getScheduler().runTaskTimer(*this, [&]() {logsCacheWrite();},0,60);
        getServer().getScheduler().runTaskTimer(*this,[&](){checkDatabaseCleanStatus();},0,20);
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
    }

    void onDisable() override
    {
        getLogger().info("onDisable is called");
        logsCacheWrite();
        auto_write_task->cancel();
    }

    bool onCommand(endstone::CommandSender &sender, const endstone::Command &command, const std::vector<std::string> &args) override
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
                                if (searchData.size() > 5000) {
                                    sender.sendErrorMessage(Tran.getLocal("Too many logs, please narrow the search range,display only 5,001 logs"));
                                }
                            }
                            else {
                                sender.sendErrorMessage(Tran.getLocal("No log found"));
                            }
                        } else {
                            Menu::showLogMenu(*sender.asPlayer(), searchData);
                            sender.sendMessage(endstone::ColorFormat::Yellow+Tran.getLocal("Display all logs"));
                            //提示数据过大
                            if (searchData.size() > 5000) {
                                sender.sendErrorMessage(Tran.getLocal("Too many logs, please narrow the search range,display only 5,001 logs"));
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
                    if (const auto searchData = tyCore.searchLog({"",time},x, y, z, r, world); searchData.empty()) {
                        sender.sendErrorMessage(Tran.getLocal("No log found"));
                    } else {
                        //回溯逻辑
                        int success_times = 0;
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
                                string command_str;
                                command_str = "setblock " + pos + " " + logData.obj_id + logData.data;
                                endstone::CommandSenderWrapper wrapper_sender(sender, [&success_times](const endstone::Message &message) {success_times++;});
                                (void)getServer().dispatchCommand(wrapper_sender,command_str);
                                // 将已回溯的事件UUID和状态添加到缓存中
                                revertStatusCache.emplace_back(logData.uuid, "reverted");
                            }
                            //对玩家右键方块的状态复原
                            else if (logData.type == "player_right_click_block") {
                                //右键方块存在状态
                                if (auto hand_block = DataBase::splitString(logData.data); hand_block[1] != "[]") {
                                    string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                                    std::ostringstream cmd;
                                    cmd << "setblock " << pos << " " << logData.obj_id << hand_block[1];
                                    endstone::CommandSenderWrapper wrapper_sender(sender, [&success_times](const endstone::Message &message) {success_times++;});
                                    (void)getServer().dispatchCommand(wrapper_sender,cmd.str());
                                    // 将已回溯的事件UUID和状态添加到缓存中
                                    revertStatusCache.emplace_back(logData.uuid, "reverted");
                                }
                            }
                            //放置方块的恢复
                            else if (logData.type == "block_place") {
                                string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                                std::ostringstream cmd;
                                cmd << "setblock " << pos << " minecraft:air";
                                endstone::CommandSenderWrapper wrapper_sender(sender, [&success_times](const endstone::Message &message) {success_times++;});
                                (void)getServer().dispatchCommand(wrapper_sender,cmd.str());
                                // 将已回溯的事件UUID和状态添加到缓存中
                                revertStatusCache.emplace_back(logData.uuid, "reverted");
                            }
                            //复活吧我的生物
                            else if (logData.type == "entity_die") {
                                string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                                std::string obj_id = logData.obj_id;
                                if (size_t vpos = obj_id.find("villager_v2"); vpos != std::string::npos) {
                                    obj_id.replace(vpos, 11, "villager");
                                }
                                std::ostringstream cmd;
                                cmd << "summon " << obj_id << " " << pos;
                                endstone::CommandSenderWrapper wrapper_sender(sender, [&success_times](const endstone::Message &message) {success_times++;});
                                (void)getServer().dispatchCommand(wrapper_sender,cmd.str());
                                // 将已回溯的事件UUID和状态添加到缓存中
                                revertStatusCache.emplace_back(logData.uuid, "reverted");
                            }
                        }
                        // 执行批量更新回溯状态
                        updateRevertStatus();
                        if (success_times > 0) {
                            sender.sendMessage(Tran.getLocal("Revert times: ")+std::to_string(success_times));
                            //提示数据过大
                            if (searchData.size() > 5000) {
                                sender.sendErrorMessage(Tran.getLocal("Too many logs, please narrow the search range,display only 5,001 logs"));
                            }
                        } else {
                            sender.sendMessage(Tran.getLocal("Nothing happened"));
                        }
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
                                if (searchData.size() > 5000) {
                                    sender.sendErrorMessage(Tran.getLocal("Too many logs, please narrow the search range,display only 5,001 logs"));
                                }
                            }
                            else {
                                sender.sendErrorMessage(Tran.getLocal("No log found"));
                            }
                        } else {
                            Menu::showLogMenu(*sender.asPlayer(), searchData);
                            sender.sendMessage(endstone::ColorFormat::Yellow+Tran.getLocal("Display all logs"));
                            //提示数据过大
                            if (searchData.size() > 5000) {
                                sender.sendErrorMessage(Tran.getLocal("Too many logs, please narrow the search range,display only 5,001 logs"));
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
                TianyanCore::BanIDPlayer banIDPlayer = {player_name,device_id, reason};
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
                sender.sendErrorMessage(Tran.getLocal("Nothing"));
                return true;
            }
            for (auto &[player_name, device_id, reason] : BanIDPlayers) {
                sender.sendMessage(Tran.tr(Tran.getLocal("Device ID: {}"), device_id));
                sender.sendMessage(Tran.tr(Tran.getLocal("Player: {}"), player_name));
                sender.sendMessage(Tran.tr(Tran.getLocal("Reason: {}"), reason.value_or("")));
                sender.sendMessage("----------------------");
            }
        }
        else if (command.getName() == "tyclean") {
            if (!args.empty()) {
                int hours = DataBase::stringToInt(args[0]);
                clean_data_sender_name = sender.getName();
                sender.sendMessage(endstone::ColorFormat::Yellow+Tran.tr(Tran.getLocal("Start cleaning logs older than {} hours"), args[0]));
                std::thread clean_thread([hours]() {
                    (void)Database.cleanDataBase(hours);
                });
                clean_thread.detach();
            }
        }
        return true;
    }

private:
    std::unique_ptr<TianyanProtect> protect_;
    std::unique_ptr<EventListener> eventListener_;
    std::unique_ptr<Menu> menu_;
    string clean_data_sender_name;
};