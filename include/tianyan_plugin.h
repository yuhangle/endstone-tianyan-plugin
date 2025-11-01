//
// Created by yuhang on 2025/5/17.
//
#pragma once

#include <endstone/plugin/plugin.h>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <fstream>
#include "translate.h"
#include "DataBase.hpp"
#include "TianyanCore.h"
#include <endstone/player.h>
#include <endstone/server.h>
#include <endstone/level/position.h>
#include <endstone/level/dimension.h>
#include <endstone/actor/actor.h>
#include <endstone/event/block/block_break_event.h>
#include <endstone/event/block/block_place_event.h>
#include <endstone/event/actor/actor_explode_event.h>
#include <endstone/event/actor/actor_knockback_event.h>
#include <endstone/event/actor/actor_death_event.h>
#include <endstone/event/player/player_interact_event.h>
#include <endstone/event/player/player_interact_actor_event.h>
#include <endstone/event/actor/actor_damage_event.h>
#include <endstone/event/block/block_piston_extend_event.h>
#include <endstone/event/block/block_piston_retract_event.h>
#include <endstone/block/block_data.h>
#include <endstone/block/block_state.h>
#include <endstone/block/block_face.h>
#include <endstone/color_format.h>
#include <endstone/command/command_sender.h>
#include <endstone/command/command_sender_wrapper.h>
#include <algorithm>
#include <endstone/inventory/item_stack.h>
#include <endstone/inventory/player_inventory.h>
#include <endstone/event/player/player_pickup_item_event.h>
#include <endstone/endstone.hpp>


using namespace std;
using namespace nlohmann;
//文件目录
inline string dataPath = "plugins/tianyan_data";
inline string dbPath = "plugins/tianyan_data/ty_data.db";
inline string config_path = "plugins/tianyan_data/config.json";
//配置变量
inline int max_message_in_10s;
inline int max_command_in_10s;
inline vector<string> no_log_mobs;
//日志缓存
inline vector<TianyanCore::LogData> logDataCache;
//语言
inline translate Tran;
//初始化其它实例
inline DataBase Database(dbPath);
inline TianyanCore tyCore(Database);

// 存储每个玩家的上次触发时间
inline std::unordered_map<string, std::chrono::steady_clock::time_point> lastTriggerTime;


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
                        getLogger().info(Tran.getLocal("Config '{}' has update with default config")+","+ key);
                        need_update = true;
                    }
                }

                // 如果需要更新配置文件，则进行写入
                if (need_update) {
                    std::ofstream outfile(config_path);
                    if (outfile.is_open()) {
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

    // 检查是否允许触发事件
    static bool canTriggerEvent(const string& playername) {
        auto now = std::chrono::steady_clock::now();

        // 查找玩家的上次触发时间
        if (lastTriggerTime.contains(playername)) {
            auto lastTime = lastTriggerTime[playername];
            auto elapsedTime = std::chrono::duration<double>(now - lastTime).count(); // 计算时间差

            // 如果时间差小于 0.2 秒，不允许触发
            if (elapsedTime < 0.2) {
                return false;
            }
        }

        // 更新玩家的上次触发时间为当前时间
        lastTriggerTime[playername] = now;
        return true;
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
        if (tyCore.recordLogs(logDataCache)) {
            getLogger().error("写入错误");
        };
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
        getServer().getScheduler().runTaskTimerAsync(*this, [&]() {logsCacheWrite();},0,100);
        //注册事件
        registerEvent<endstone::BlockBreakEvent>(onBlockBreak);
        registerEvent<endstone::BlockPlaceEvent>(onBlockPlace);
        registerEvent<endstone::ActorDamageEvent>(onActorDamage);
        registerEvent<endstone::PlayerInteractEvent>(onPlayerRightClickBlock);
        registerEvent<endstone::PlayerInteractActorEvent>(onPlayerRightClickActor);
        registerEvent<endstone::ActorExplodeEvent>(onActorBomb);
        registerEvent<endstone::BlockPistonExtendEvent>(onPistonExtend);
        registerEvent<endstone::BlockPistonRetractEvent>(onPistonRetract);
        registerEvent<endstone::ActorDeathEvent>(onActorDie);
        registerEvent<endstone::PlayerPickupItemEvent>(onPlayPickup);
    }

    void onDisable() override
    {
        getLogger().info("onDisable is called");
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
                //待施工
            }
            else if (args.size() == 2) {
                try {
                    const double r = stod(args[0]);
                    const double time = stod(args[1]);
                    const string world = sender.asPlayer()->getLocation().getDimension()->getName();
                    const double x = sender.asPlayer()->getLocation().getX();
                    const double y = sender.asPlayer()->getLocation().getY();
                    const double z = sender.asPlayer()->getLocation().getZ();
                    if (const auto searchData = tyCore.searchLog({"",time},x, y, z, r, world); searchData.empty()) {
                        sender.sendErrorMessage(Tran.getLocal("No log found"));
                    } else {
                        showLogMenu(*sender.asPlayer(), searchData);
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
                //待施工
            }
            else if (args.size() == 2) {
                try {
                    const double r = stod(args[0]);
                    const double time = stod(args[1]);
                    const string world = sender.asPlayer()->getLocation().getDimension()->getName();
                    const double x = sender.asPlayer()->getLocation().getX();
                    const double y = sender.asPlayer()->getLocation().getY();
                    const double z = sender.asPlayer()->getLocation().getZ();
                    if (const auto searchData = tyCore.searchLog({"",time},x, y, z, r, world); searchData.empty()) {
                        sender.sendErrorMessage(Tran.getLocal("No log found"));
                    } else {
                        //回溯逻辑
                        for (auto& logData:searchData) {
                            //跳过取消掉的事件
                            if (logData.data == "canceled") {
                                continue;
                            }
                            //破坏和爆炸方块的恢复
                            if (logData.type == "block_break" | logData.type == "block_break_bomb" | (logData.type == "actor_bomb" && logData.id == "minecraft:tnt")) {
                                string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                                string command_str;
                                command_str = "setblock " + pos + " " + logData.obj_id + logData.data;
                                endstone::CommandSenderWrapper wrapper_sender(sender);
                                (void)getServer().dispatchCommand(wrapper_sender,command_str);
                            }
                            //对玩家右键方块的状态复原
                            else if (logData.type == "player_right_click_block") {
                                //右键方块存在状态
                                if (auto hand_block = DataBase::splitString(logData.data); hand_block[1] != "[]") {
                                    string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                                    string command_str;
                                    command_str = "setblock " + pos + " " + logData.obj_id + hand_block[1];
                                    endstone::CommandSenderWrapper wrapper_sender(sender);
                                    (void)getServer().dispatchCommand(wrapper_sender,command_str);
                                }
                            }
                        }
                    }
                } catch (const std::exception &e) {
                    //返回错误给玩家
                    sender.sendErrorMessage(e.what());
                }
            }
        }
        return true;
    }

    //日志展示菜单
    static void showLogMenu(endstone::Player &player, const std::vector<TianyanCore::LogData>& logDatas) {
        endstone::ActionForm logMenu;
        logMenu.setTitle(Tran.getLocal("Logs"));
        string showLogs;
        for (const auto& logData : std::ranges::reverse_view(logDatas)) {
            // 构建要显示的日志信息
            std::vector<std::pair<std::string, std::string>> logFields;
            
            // 名字不为空时添加源名称
            if (!logData.name.empty()) {
                logFields.emplace_back(Tran.getLocal("Source Name"), logData.name);
            }
            
            // 根据是否是玩家添加不同类型的信息
            if (logData.id == "minecraft:player") {
                logFields.emplace_back(Tran.getLocal("Source Type"), Tran.getLocal("Player"));
            }
            else if (!logData.id.empty()){
                logFields.emplace_back(Tran.getLocal("Source ID"), logData.id);
            }
            
            // 添加行为类型
            logFields.emplace_back(Tran.getLocal("Action"), Tran.getLocal(logData.type));

            // 添加位置信息
            logFields.emplace_back(Tran.getLocal("Position"),
                                  fmt::format("{:.2f},{:.2f},{:.2f}", logData.pos_x, logData.pos_y, logData.pos_z));
            
            // 添加时间信息
            logFields.emplace_back(Tran.getLocal("Time"), TianyanCore::timestampToString(logData.time));
            
            // 根据目标名称和ID是否存在添加相应信息
            if (!logData.obj_name.empty()) {
                logFields.emplace_back(Tran.getLocal("Target Name"), logData.obj_name);
            }
            
            if (!logData.obj_id.empty()) {
                logFields.emplace_back(Tran.getLocal("Target ID"), logData.obj_id);
            }
            
            // 添加数据信息
            if (logData.data == "canceled") {
                logFields.emplace_back(Tran.getLocal("Data"), Tran.getLocal("This event has been canceled"));
            } else if (!logData.data.empty()){
                //对手持物品交互进行处理
                if (logData.type == "player_right_click_block") {
                    auto hand_block = DataBase::splitString(logData.data);
                    logFields.emplace_back(Tran.getLocal("Item in Hand"), hand_block[0]);
                    logFields.emplace_back(Tran.getLocal("Data"), hand_block[1]);
                } else {
                    logFields.emplace_back(Tran.getLocal("Data"), logData.data);
                }
            }
            
            // 格式化输出
            for (const auto&[fst, snd] : logFields) {
                showLogs += fmt::format("{}: {}\n", fst, snd);
            }
            
            showLogs += "--------------------------------\n\n";
        }
        logMenu.setContent(endstone::ColorFormat::Yellow + showLogs);
        player.sendForm(logMenu);
    }

    static void onBlockBreak(const endstone::BlockBreakEvent& event){
        TianyanCore::LogData logData;
        logData.id = event.getPlayer().getType();
        logData.name = event.getPlayer().getName();
        logData.pos_x = event.getBlock().getX();
        logData.pos_y = event.getBlock().getY();
        logData.pos_z = event.getBlock().getZ();
        logData.world = event.getBlock().getLocation().getDimension()->getName();
        logData.obj_id = event.getBlock().getType();
        logData.time = std::time(nullptr);
        logData.type = "block_break";
        if (event.isCancelled()) {
            logData.data = "canceled";
        } else {
            const auto block_data = event.getBlock().getData();
            auto block_states = block_data->getBlockStates();
            logData.data = fmt::format("{}", block_states);
        }
        logDataCache.push_back(logData);
    }

    static void onBlockPlace(const endstone::BlockPlaceEvent& event){
        TianyanCore::LogData logData;
        logData.id = event.getPlayer().getType();
        logData.name = event.getPlayer().getName();
        logData.pos_x = event.getBlockPlacedState().getX();
        logData.pos_y = event.getBlockPlacedState().getY();
        logData.pos_z = event.getBlockPlacedState().getZ();
        logData.world = event.getBlockPlacedState().getLocation().getDimension()->getName();
        logData.obj_id = event.getBlockPlacedState().getType();
        logData.time = std::time(nullptr);
        logData.type = "block_place";
        if (event.isCancelled()) {
            logData.data = "canceled";
        } else {
            const auto block_data = event.getBlockPlacedState().getData();
            auto block_states = block_data->getBlockStates();
            logData.data = fmt::format("{}", block_states);
        }
        logDataCache.push_back(logData);
    }

    static void onActorDamage(const endstone::ActorDamageEvent& event){
        //无名的烂大街生物无需在意
        if (event.getActor().getNameTag().empty() && ranges::find(no_log_mobs, event.getActor().getType()) != no_log_mobs.end()) {
            return;
        }
        TianyanCore::LogData logData;

        //实体造成伤害
        if (event.getDamageSource().getActor()) {
            logData.id = event.getDamageSource().getActor()->getType();
            logData.name = event.getDamageSource().getActor()->getName();
            logData.type = "entity_damage";
        }
        //其余伤害
        else {
            logData.id = event.getDamageSource().getType();
            logData.type = "damage";
        }
        logData.pos_x = event.getActor().getLocation().getX();
        logData.pos_y = event.getActor().getLocation().getY();
        logData.pos_z = event.getActor().getLocation().getZ();
        logData.world = event.getActor().getLocation().getDimension()->getName();
        logData.obj_id = event.getActor().getType();
        logData.obj_name = event.getActor().getName();
        logData.time = std::time(nullptr);
        if (event.isCancelled()) {
            logData.data = "canceled";
        } else {
            auto damage = event.getDamage();
            logData.data = fmt::format("{}", damage);
        }
        logDataCache.push_back(logData);
    }

    static void onPlayerRightClickBlock(const endstone::PlayerInteractEvent& event) {
        TianyanCore::LogData logData;
        if (!event.getBlock()) {
            return;
        }
        if (!canTriggerEvent(event.getPlayer().getName())) {
            return;
        }
        logData.id = event.getPlayer().getType();
        logData.name = event.getPlayer().getName();
        logData.pos_x = event.getBlock()->getX();
        logData.pos_y = event.getBlock()->getY();
        logData.pos_z = event.getBlock()->getZ();
        logData.world = event.getPlayer().getLocation().getDimension()->getName();
        logData.obj_id = event.getBlock()->getType();
        logData.time = std::time(nullptr);
        logData.type = "player_right_click_block";
        if (event.isCancelled()) {
            logData.data = "canceled";
        } else {
            string hand_item;
            if (event.hasItem()) {
                hand_item = event.getItem()->getType().getId();
            } else {
                hand_item = "hand";
            }
            const auto block_data = event.getBlock()->getData();
            auto block_states = block_data->getBlockStates();
            logData.data = fmt::format("{},{}", hand_item,block_states);
        }
        logDataCache.push_back(logData);
    }

    static void onPlayerRightClickActor(const endstone::PlayerInteractActorEvent& event){
        //无名的烂大街生物无需在意
        if (event.getActor().getNameTag().empty() && ranges::find(no_log_mobs, event.getActor().getType()) != no_log_mobs.end()) {
            return;
        }
        TianyanCore::LogData logData;
        logData.id = event.getPlayer().getType();
        logData.name = event.getPlayer().getName();
        logData.pos_x = event.getActor().getLocation().getX();
        logData.pos_y = event.getActor().getLocation().getY();
        logData.pos_z = event.getActor().getLocation().getZ();
        logData.world = event.getActor().getLocation().getDimension()->getName();
        logData.obj_id = event.getActor().getType();
        logData.obj_name = event.getActor().getName();
        logData.time = std::time(nullptr);
        logData.type = "player_right_click_entity";
        if (event.isCancelled()) {
            logData.data = "canceled";
        }
        string hand_item;
        if (event.getPlayer().getInventory().getItemInMainHand()) {
            hand_item = event.getPlayer().getInventory().getItemInMainHand()->getType().getTranslationKey();
        } else {
            hand_item = "hand";
        }
        logData.data = fmt::format("{}", hand_item);
        logDataCache.push_back(logData);
    }

    static void onActorBomb(const endstone::ActorExplodeEvent& event) {
        TianyanCore::LogData logData;
        logData.id = event.getActor().getType();
        logData.name = event.getActor().getName();
        logData.pos_x = event.getLocation().getX();
        logData.pos_y = event.getLocation().getY();
        logData.pos_z = event.getLocation().getZ();
        logData.world = event.getLocation().getDimension()->getName();
        if (!event.getBlockList().empty()) {
            logData.obj_name = "Block";
        }
        logData.time = std::time(nullptr);
        logData.type = "entity_bomb";
        auto block_num = event.getBlockList().size();
        if (event.isCancelled()) {
            logData.data = "canceled";
            logDataCache.push_back(logData);
        } else {
            logData.data = fmt::format("{}", block_num);
            logDataCache.push_back(logData);
            for (const auto& block : event.getBlockList()) {
                // 方块类型
                TianyanCore::LogData bomb_data;
                bomb_data.id = event.getActor().getType();
                bomb_data.name = event.getActor().getName();
                bomb_data.pos_x = block->getX();
                bomb_data.pos_y = block->getY();
                bomb_data.pos_z = block->getZ();
                bomb_data.world = block->getLocation().getDimension()->getName();
                bomb_data.obj_id = block->getType();
                bomb_data.time = std::time(nullptr);
                bomb_data.type = "block_break_bomb";
                bomb_data.data = fmt::format("{}", block->getData()->getBlockStates());
                logDataCache.push_back(bomb_data);
            }
        }
    }

    static void onPistonExtend(const endstone::BlockPistonExtendEvent&event) {
        TianyanCore::LogData logData;
        logData.id = event.getBlock().getType();
        logData.pos_x = event.getBlock().getX();
        logData.pos_y = event.getBlock().getY();
        logData.pos_z = event.getBlock().getZ();
        logData.world = event.getBlock().getLocation().getDimension()->getName();
        logData.time = std::time(nullptr);
        logData.type = "piston_extend";
        auto Face = event.getDirection();
        string direct;
        if (Face == endstone::BlockFace::Down) {
            direct = "down";
        } else if (Face == endstone::BlockFace::Up) {
            direct = "up";
        } else if (Face == endstone::BlockFace::North) {
            direct = "north";
        } else if (Face == endstone::BlockFace::South) {
            direct = "south";
        } else if (Face == endstone::BlockFace::West) {
            direct = "west";
        } else if (Face == endstone::BlockFace::East) {
            direct = "east";
        }
        if (event.isCancelled()) {
            logData.data = "canceled";
        } else {
            logData.data = fmt::format("{},{}", direct, event.getBlock().getData()->getBlockStates());
        }

        logDataCache.push_back(logData);
    }

    static void onPistonRetract(const endstone::BlockPistonRetractEvent&event) {
        TianyanCore::LogData logData;
        logData.id = event.getBlock().getType();
        logData.pos_x = event.getBlock().getX();
        logData.pos_y = event.getBlock().getY();
        logData.pos_z = event.getBlock().getZ();
        logData.world = event.getBlock().getLocation().getDimension()->getName();
        logData.time = std::time(nullptr);
        logData.type = "piston_retract";
        auto Face = event.getDirection();
        string direct;
        if (Face == endstone::BlockFace::Down) {
            direct = "down";
        } else if (Face == endstone::BlockFace::Up) {
            direct = "up";
        } else if (Face == endstone::BlockFace::North) {
            direct = "north";
        } else if (Face == endstone::BlockFace::South) {
            direct = "south";
        } else if (Face == endstone::BlockFace::West) {
            direct = "west";
        } else if (Face == endstone::BlockFace::East) {
            direct = "east";
        }
        if (event.isCancelled()) {
            logData.data = "canceled";
        } else {
            logData.data = fmt::format("{},{}", direct, event.getBlock().getData()->getBlockStates());
        }

        logDataCache.push_back(logData);
    }

    static void onActorDie(const endstone::ActorDeathEvent&event) {
        //无名的烂大街生物无需在意
        if (event.getActor().getNameTag().empty() && ranges::find(no_log_mobs, event.getActor().getType()) != no_log_mobs.end()) {
            return;
        }
        TianyanCore::LogData logData;
        logData.pos_x = event.getActor().getLocation().getX();
        logData.pos_y = event.getActor().getLocation().getY();
        logData.pos_z = event.getActor().getLocation().getZ();
        logData.world = event.getActor().getLocation().getDimension()->getName();
        logData.obj_id = event.getActor().getType();
        logData.obj_name = event.getActor().getName();
        logData.time = std::time(nullptr);
        logData.type = "entity_die";
        logDataCache.push_back(logData);
    }

    static void onPlayPickup(const endstone::PlayerPickupItemEvent&event) {
        TianyanCore::LogData logData;
        logData.id = event.getPlayer().getType();
        logData.name = event.getPlayer().getName();
        logData.pos_x = event.getPlayer().getLocation().getX();
        logData.pos_y = event.getPlayer().getLocation().getY();
        logData.pos_z = event.getPlayer().getLocation().getZ();
        logData.world = event.getPlayer().getLocation().getDimension()->getName();
        logData.obj_id = event.getItem().getItemStack()->getType().getId();
        logData.obj_name = event.getItem().getName();
        logData.time = std::time(nullptr);
        logData.type = "player_pickup_item";
        if (event.isCancelled()) {
            logData.data = "canceled";
        }
        logDataCache.push_back(logData);
    }

};