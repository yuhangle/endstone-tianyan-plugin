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
#include <endstone/form/controls/slider.h>


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
//任务
inline shared_ptr<endstone::Task> auto_write_task;
// 回溯状态缓存 - 存储需要标记为"reverted"的日志UUID和状态
inline vector<pair<string, string>> revertStatusCache;

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
        const auto now = std::chrono::steady_clock::now();

        // 查找玩家的上次触发时间
        if (lastTriggerTime.contains(playername)) {
            const auto lastTime = lastTriggerTime[playername];

            // 如果时间差小于 0.2 秒，不允许触发
            if (const auto elapsedTime = std::chrono::duration<double>(now - lastTime).count(); elapsedTime < 0.2) {
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
        logDataCache.clear();
    }
    
    // 批量更新回溯状态
    void updateRevertStatus() const {
        if (revertStatusCache.empty()) {
            return;
        }
        
        // 使用数据库的批量更新方法更新状态
        if (!Database.updateStatusesByUUIDs(revertStatusCache)) {
            getLogger().error("更新回溯状态失败");
        }
        
        // 清空缓存
        revertStatusCache.clear();
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
        auto_write_task = getServer().getScheduler().runTaskTimer(*this, [&]() {logsCacheWrite();},0,100);
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
                endstone::ModalForm tyMenu;
                tyMenu.setTitle(Tran.getLocal("Log Browser"));
                endstone::Slider radius;
                endstone::Slider Time;
                endstone::Dropdown keyType;
                endstone::TextInput keyWords;

                radius.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+Tran.getLocal("Radius"));radius.setDefaultValue(15);radius.setMin(1);radius.setMax(50);radius.setStep(1);
                Time.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+Tran.getLocal("Lookback Time (hours)"));Time.setDefaultValue(12);Time.setMin(1);Time.setMax(168);Time.setStep(1);
                keyType.setLabel(Tran.getLocal("Search By"));
                keyType.setOptions({Tran.getLocal("Source ID"),Tran.getLocal("Source Name"),Tran.getLocal("Target ID"),Tran.getLocal("Target Name"),Tran.getLocal("Action")});
                keyWords.setLabel(Tran.getLocal("Keywords"));
                keyWords.setPlaceholder(Tran.getLocal("Please enter keywords"));
                tyMenu.setControls({radius,Time,keyType,keyWords});
                tyMenu.setOnSubmit([=](endstone::Player *p,const string& response) {
                    json response_json = json::parse(response);
                    const int r = response_json[0];
                    const int time = response_json[1];
                    const int search_key_type = response_json[2];
                    const string search_key = response_json[3];
                    std::ostringstream cmd;
                    if (!search_key.empty()) {
                        string key_type;
                        if (search_key_type == 0) {key_type  = "source_id";} else if (search_key_type == 1) {key_type  = "source_name";} else if (search_key_type == 2) {key_type  = "target_id";}
                        else if (search_key_type == 3) {key_type  = "target_name";} else if (search_key_type == 4) {key_type  = "action";}
                        cmd << "ty " << r << " " << time << " " <<key_type << " " << "\"" << search_key << "\"" ;
                        (void)p->performCommand(cmd.str());
                    } else {
                        cmd << "ty " << r << " " << time;
                        (void)p->performCommand(cmd.str());
                    }
                });
                sender.asPlayer()->sendForm(tyMenu);
            }
            else if (args.size() >= 2) {
                try {
                    const double r = stod(args[0]);
                    const double time = stod(args[1]);
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
                                    if (logData.obj_name == search_key) {}
                                }
                            } else if (search_key_type == "action") {
                                for (auto &logData : searchData) {
                                    if (logData.type == search_key) {
                                        key_logData.push_back(logData);
                                    }
                                }
                            }
                            if (!key_logData.empty()) {
                                showLogMenu(*sender.asPlayer(), key_logData);
                                sender.sendMessage(endstone::ColorFormat::Yellow+Tran.getLocal("Display all logs about")+"` "+search_key+" `");
                            }
                            else {
                                sender.sendErrorMessage(Tran.getLocal("No log found"));
                            }
                        } else {
                            showLogMenu(*sender.asPlayer(), searchData);
                            sender.sendMessage(endstone::ColorFormat::Yellow+Tran.getLocal("Display all logs"));
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
                endstone::ModalForm tyMenu;
                tyMenu.setTitle(Tran.getLocal("Revert Menu"));
                endstone::Slider radius;
                endstone::Slider Time;
                endstone::Dropdown keyType;
                endstone::TextInput keyWords;

                radius.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+Tran.getLocal("Radius"));radius.setDefaultValue(15);radius.setMin(1);radius.setMax(50);radius.setStep(1);
                Time.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+Tran.getLocal("Lookback Time (hours)"));Time.setDefaultValue(12);Time.setMin(1);Time.setMax(168);Time.setStep(1);
                keyType.setLabel(Tran.getLocal("Search By"));
                keyType.setOptions({Tran.getLocal("Source ID"),Tran.getLocal("Source Name"),Tran.getLocal("Target ID"),Tran.getLocal("Target Name"),Tran.getLocal("Action")});
                keyWords.setLabel(Tran.getLocal("Keywords"));
                keyWords.setPlaceholder(Tran.getLocal("Please enter keywords"));
                tyMenu.setControls({radius,Time,keyType,keyWords});
                tyMenu.setOnSubmit([=](endstone::Player *p,const string& response) {
                    json response_json = json::parse(response);
                    const int r = response_json[0];
                    const int time = response_json[1];
                    const int search_key_type = response_json[2];
                    const string search_key = response_json[3];
                    std::ostringstream cmd;
                    if (!search_key.empty()) {
                        string key_type;
                        if (search_key_type == 0) {key_type  = "source_id";} else if (search_key_type == 1) {key_type  = "source_name";} else if (search_key_type == 2) {key_type  = "target_id";}
                        else if (search_key_type == 3) {key_type  = "target_name";} else if (search_key_type == 4) {key_type  = "action";}
                        if (key_type == "action") {
                            cmd << "tyback " << r << " " << time << " " <<key_type << " " << search_key ;
                        } else {
                            cmd << "tyback " << r << " " << time << " " <<key_type << " " << "\"" << search_key << "\"" ;
                        }
                        (void)p->performCommand(cmd.str());
                    } else {
                        cmd << "tyback " << r << " " << time;
                        (void)p->performCommand(cmd.str());
                    }
                });
                sender.asPlayer()->sendForm(tyMenu);
            }
            else if (args.size() >=2) {
                try {
                    const double r = stod(args[0]);
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
                                    string command_str;
                                    command_str = "setblock " + pos + " " + logData.obj_id + hand_block[1];
                                    endstone::CommandSenderWrapper wrapper_sender(sender, [&success_times](const endstone::Message &message) {success_times++;});
                                    (void)getServer().dispatchCommand(wrapper_sender,command_str);
                                    // 将已回溯的事件UUID和状态添加到缓存中
                                    revertStatusCache.emplace_back(logData.uuid, "reverted");
                                }
                            }
                            //放置方块的恢复
                            else if (logData.type == "block_place") {
                                string pos = std::to_string(logData.pos_x) + " " + std::to_string(logData.pos_y) + " " + std::to_string(logData.pos_z);
                                string command_str;
                                command_str = "setblock " + pos + " " + "minecraft:air";
                                endstone::CommandSenderWrapper wrapper_sender(sender, [&success_times](const endstone::Message &message) {success_times++;});
                                (void)getServer().dispatchCommand(wrapper_sender,command_str);
                                // 将已回溯的事件UUID和状态添加到缓存中
                                revertStatusCache.emplace_back(logData.uuid, "reverted");
                            }
                        }
                        // 执行批量更新回溯状态
                        updateRevertStatus();
                        if (success_times > 0) {
                            sender.sendMessage(Tran.getLocal("Revert times: ")+std::to_string(success_times));
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
        return true;
    }

    //日志展示菜单
    static void showLogMenu(endstone::Player &player, const std::vector<TianyanCore::LogData>& logDatas, int page = 0) {
        // 计算分页信息
        constexpr int logsPerPage = 30;
        const int totalPages = (static_cast<int>(logDatas.size()) + logsPerPage - 1) / logsPerPage;
        const int currentPage = std::min(page, std::max(0, totalPages - 1)); // 确保当前页在有效范围内
        
        endstone::ActionForm logMenu;
        logMenu.setTitle(fmt::format("{} - {} {}/{} {}", Tran.getLocal("Logs"), Tran.getLocal("The"),currentPage + 1, std::max(1, totalPages),Tran.getLocal("Page")));
        
        string showLogs;
        
        // 如果有日志数据，则显示当前页的日志
        if (!logDatas.empty() && currentPage < totalPages) {
            const int startIndex = currentPage * logsPerPage;
            const int endIndex = std::min(startIndex + logsPerPage, static_cast<int>(logDatas.size()));
            
            // 只显示当前页的日志（倒序显示）
            std::vector<TianyanCore::LogData> pageLogs;
            for (int i = endIndex - 1; i >= startIndex; --i) {
                pageLogs.push_back(logDatas[i]);
            }
            
            for (const auto& logData : pageLogs) {
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
                    logFields.emplace_back(Tran.getLocal("Target Name"), Tran.getLocal(logData.obj_name));
                }
                
                if (!logData.obj_id.empty()) {
                    logFields.emplace_back(Tran.getLocal("Target ID"), logData.obj_id);
                }
                
                // 添加数据信息
                if (!logData.data.empty()){
                    //对手持物品交互进行处理
                    if (logData.type == "player_right_click_block") {
                        auto hand_block = DataBase::splitString(logData.data);
                        logFields.emplace_back(Tran.getLocal("Item in Hand"), hand_block[0]);
                        if (hand_block[1] != "[]") {
                            logFields.emplace_back(Tran.getLocal("Data"), hand_block[1]);
                        }
                    } else {
                        if (logData.data != "[]") {
                            logFields.emplace_back(Tran.getLocal("Data"), logData.data);
                        }
                    }
                }

                //事件是否取消或被恢复
                if (logData.status == "canceled") {
                    logFields.emplace_back(Tran.getLocal("Status"), Tran.getLocal("This event has been canceled"));
                } else if (logData.status == "reverted") {
                    logFields.emplace_back(Tran.getLocal("Status"), Tran.getLocal("This event has been reverted"));
                }
                
                // 格式化输出
                for (const auto&[fst, snd] : logFields) {
                    showLogs += fmt::format("{}: {}\n", fst, snd);
                }
                
                showLogs += "--------------------------------\n\n";
            }
        } else {
            // 没有日志数据
            showLogs = Tran.getLocal("No log found");
        }
        
        logMenu.setContent(endstone::ColorFormat::Yellow + showLogs);
        
        // 设置翻页按钮
        std::vector<std::variant<endstone::Button, endstone::Divider, endstone::Header, endstone::Label>> controls;

        // 添加上一页按钮（如果有多页）
        if (totalPages > 1) {
            endstone::Button pageUp;
            pageUp.setText(Tran.getLocal("Page Up"));
            pageUp.setOnClick([=, &player](endstone::Player*) {
                // 如果是第一页，则跳转到最后一页；否则上一页
                const int prevPage = (currentPage == 0) ? (totalPages - 1) : (currentPage - 1);
                showLogMenu(player, logDatas, prevPage);
            });
            controls.emplace_back(pageUp);
        }

        // 添加下一页按钮（如果有多页）
        if (totalPages > 1) {
            endstone::Button pageDown;
            pageDown.setText(Tran.getLocal("Page Down"));
            pageDown.setOnClick([=, &player](endstone::Player*) {
                // 如果是最后一页，则回到第一页；否则下一页
                const int nextPage = (currentPage + 1) % totalPages;
                showLogMenu(player, logDatas, nextPage);
            });
            controls.emplace_back(pageDown);
        }
        
        logMenu.setControls(controls);
        player.sendForm(logMenu);
    }

    static void onBlockBreak(const endstone::BlockBreakEvent& event){
        TianyanCore::LogData logData;
        logData.uuid = DataBase::generate_uuid_v4(); // 生成UUID
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
            logData.status = "canceled";
        }
        const auto block_data = event.getBlock().getData();
        auto block_states = block_data->getBlockStates();
        logData.data = fmt::format("{}", block_states);
        logDataCache.push_back(logData);
    }

    static void onBlockPlace(const endstone::BlockPlaceEvent& event){
        TianyanCore::LogData logData;
        logData.uuid = DataBase::generate_uuid_v4(); // 生成UUID
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
            logData.status = "canceled";
        }
        const auto block_data = event.getBlockPlacedState().getData();
        auto block_states = block_data->getBlockStates();
        logData.data = fmt::format("{}", block_states);
        logDataCache.push_back(logData);
    }

    static void onActorDamage(const endstone::ActorDamageEvent& event){
        //无名的烂大街生物无需在意
        if (event.getActor().getNameTag().empty() && ranges::find(no_log_mobs, event.getActor().getType()) != no_log_mobs.end()) {
            return;
        }
        TianyanCore::LogData logData;
        logData.uuid = DataBase::generate_uuid_v4(); // 生成UUID

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
            logData.status = "canceled";
        }
        auto damage = event.getDamage();
        logData.data = fmt::format("{}", damage);
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
        logData.uuid = DataBase::generate_uuid_v4(); // 生成UUID
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
            logData.status = "canceled";
        }
        string hand_item;
        if (event.hasItem()) {
            hand_item = event.getItem()->getType().getId();
        } else {
            hand_item = "hand";
        }
        const auto block_data = event.getBlock()->getData();
        auto block_states = block_data->getBlockStates();
        logData.data = fmt::format("{},{}", hand_item,block_states);
        logDataCache.push_back(logData);
    }

    static void onPlayerRightClickActor(const endstone::PlayerInteractActorEvent& event){
        //无名的烂大街生物无需在意
        if (event.getActor().getNameTag().empty() && ranges::find(no_log_mobs, event.getActor().getType()) != no_log_mobs.end()) {
            return;
        }
        TianyanCore::LogData logData;
        logData.uuid = DataBase::generate_uuid_v4(); // 生成UUID
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
            logData.status = "canceled";
        }
        string hand_item;
        if (event.getPlayer().getInventory().getItemInMainHand()) {
            hand_item = event.getPlayer().getInventory().getItemInMainHand()->getType().getId();
        } else {
            hand_item = "hand";
        }
        logData.data = fmt::format("{}", hand_item);
        logDataCache.push_back(logData);
    }

    static void onActorBomb(const endstone::ActorExplodeEvent& event) {
        TianyanCore::LogData logData;
        logData.uuid = DataBase::generate_uuid_v4(); // 生成UUID
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
            logData.status = "canceled";
            logDataCache.push_back(logData);
        } else {
            logData.data = fmt::format("{}", block_num);
            logDataCache.push_back(logData);
            for (const auto& block : event.getBlockList()) {
                // 方块类型
                TianyanCore::LogData bomb_data;
                bomb_data.uuid = DataBase::generate_uuid_v4(); // 生成UUID
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
        logData.uuid = DataBase::generate_uuid_v4(); // 生成UUID
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
            logData.status = "canceled";
        }
        logData.data = fmt::format("{},{}", direct, event.getBlock().getData()->getBlockStates());


        logDataCache.push_back(logData);
    }

    static void onPistonRetract(const endstone::BlockPistonRetractEvent&event) {
        TianyanCore::LogData logData;
        logData.uuid = DataBase::generate_uuid_v4(); // 生成UUID
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
            logData.status = "canceled";
        }
        logData.data = fmt::format("{},{}", direct, event.getBlock().getData()->getBlockStates());

        logDataCache.push_back(logData);
    }

    static void onActorDie(const endstone::ActorDeathEvent&event) {
        //无名的烂大街生物无需在意
        if (event.getActor().getNameTag().empty() && ranges::find(no_log_mobs, event.getActor().getType()) != no_log_mobs.end()) {
            return;
        }
        TianyanCore::LogData logData;
        logData.uuid = DataBase::generate_uuid_v4(); // 生成UUID
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
        logData.uuid = DataBase::generate_uuid_v4(); // 生成UUID
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
            logData.status = "canceled";
        }
        logDataCache.push_back(logData);
    }

};