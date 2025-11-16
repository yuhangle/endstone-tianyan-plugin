//
// Created by yuhang on 2025/11/6.
//

#ifndef TIANYAN_TIANYANPROTECT_H
#define TIANYAN_TIANYANPROTECT_H
#include <Global.h>

class TianyanProtect {
public:
    explicit TianyanProtect(endstone::Plugin &plugin) : plugin_(plugin){}

    // 设备ID黑名单初始化
    void deviceIDBlacklistInit() const {
        
        // 检查文件是否存在
        std::ifstream file(ban_id_path);
        if (!file.is_open()) {
            // 文件不存在，创建新文件
            if (std::ofstream outfile(ban_id_path); outfile.is_open()) {
                json empty_json = json::object();
                outfile << empty_json.dump(4);
                outfile.close();
                plugin_.getLogger().info(Tran.getLocal("Automatically create a device ID blacklist file"));
            } else {
                plugin_.getLogger().error(Tran.getLocal("Failed to create the device ID blacklist file"));
            }
            return;
        }

        // 文件存在，尝试读取内容
        try {
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();

            std::string content = buffer.str();
            if (content.empty()) {
                return;
            }

            json jsonData = json::parse(content);
            
            // 清空现有数据
            BanIDPlayers.clear();
            
            // 遍历JSON对象中的每个设备ID
            for (auto it = jsonData.begin(); it != jsonData.end(); ++it) {
                const std::string& device_id = it.key();

                if (json value = it.value(); value.contains("player_name")) {
                    TianyanCore::BanIDPlayer ban_player;
                    ban_player.device_id = device_id;
                    ban_player.player_name = value["player_name"];
                    ban_player.time = value["time"];
                    
                    if (value.contains("reason")) {
                        ban_player.reason = value["reason"];
                    }
                    
                    BanIDPlayers.push_back(ban_player);
                } else {
                    plugin_.getLogger().error(Tran.tr(Tran.getLocal("The data for device ID {} is missing the 'player_name' field")+","+device_id));
                }
            }
            
        } catch (const std::exception& e) {
            plugin_.getLogger().error(Tran.tr(Tran.getLocal("Error occurred while parsing the device ID blacklist file: {}")+","+ e.what()));
        }
    }

    // 封禁设备ID
    [[nodiscard]] bool BanDeviceID(const TianyanCore::BanIDPlayer& banPlayer) const {
        // 检查该设备ID是否已经存在于缓存中
        for (const auto& existingPlayer : BanIDPlayers) {
            if (existingPlayer.device_id == banPlayer.device_id) {
                plugin_.getLogger().error(Tran.tr(Tran.getLocal("The device ID {} already exists"), banPlayer.device_id));
                return false;
            }
        }

        try {
            // 读取现有文件内容
            std::ifstream file(ban_id_path);
            json jsonData;
            
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                file.close();

                if (std::string content = buffer.str(); !content.empty()) {
                    jsonData = json::parse(content);
                }
            }

            // 检查该设备ID是否已经存在于文件中
            if (jsonData.contains(banPlayer.device_id)) {
                plugin_.getLogger().error(Tran.tr(Tran.getLocal("The device ID {} already exists"), banPlayer.device_id));
                return false;
            }

            // 构造要添加的数据
            json newEntry;
            newEntry["player_name"] = banPlayer.player_name;
            
            if (banPlayer.reason.has_value()) {
                newEntry["reason"] = banPlayer.reason.value();
            } else {
                newEntry["reason"] = "";
            }

            newEntry["time"] = banPlayer.time;

            // 添加到JSON对象
            jsonData[banPlayer.device_id] = newEntry;

            // 写入文件
            if (std::ofstream outfile(ban_id_path); outfile.is_open()) {
                outfile << jsonData.dump(4);
                outfile.close();
                
                // 添加到缓存
                BanIDPlayers.push_back(banPlayer);
                
                plugin_.getLogger().info(Tran.tr(Tran.getLocal("Device ID {} banned successfully"),banPlayer.device_id));
                return true;
            }
            plugin_.getLogger().error(Tran.getLocal("Failed to write to the device ID blacklist file"));
            return false;
        } catch (const std::exception& e) {
            plugin_.getLogger().error(Tran.tr(Tran.getLocal("Error occurred while banning device ID {}: {}"), banPlayer.device_id, e.what()));
            return false;
        }
    }
    
    // 解封设备ID
    [[nodiscard]] bool UnbanDeviceID(const std::string& deviceId) const {
        bool foundInCache = false;
        
        // 检查该设备ID是否存在于缓存中并移除
        for (auto it = BanIDPlayers.begin(); it != BanIDPlayers.end();) {
            if (it->device_id == deviceId) {
                it = BanIDPlayers.erase(it);
                foundInCache = true;
            } else {
                ++it;
            }
        }
        
        if (!foundInCache) {
            plugin_.getLogger().error(Tran.tr(Tran.getLocal("Device ID {} not found in cache"), deviceId));
        }

        try {
            // 读取现有文件内容
            std::ifstream file(ban_id_path);
            json jsonData;
            
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                file.close();

                if (std::string content = buffer.str(); !content.empty()) {
                    jsonData = json::parse(content);
                }
            } else {
                plugin_.getLogger().error(Tran.getLocal("Failed to open the device ID blacklist file for reading"));
                return false;
            }

            // 检查该设备ID是否存在于文件中
            if (!jsonData.contains(deviceId)) {
                plugin_.getLogger().error(Tran.tr(Tran.getLocal("Device ID {} not found in file"), deviceId));
                return foundInCache; // 如果缓存中有就返回true，否则false
            }

            // 从JSON对象中移除
            jsonData.erase(deviceId);

            // 写入文件
            if (std::ofstream outfile(ban_id_path); outfile.is_open()) {
                outfile << jsonData.dump(4);
                outfile.close();
                
                plugin_.getLogger().info(Tran.tr(Tran.getLocal("Device ID {} unbanned successfully"), deviceId));
                return true;
            }
            plugin_.getLogger().error(Tran.getLocal("Failed to write to the device ID blacklist file"));
            return false;
        } catch (const std::exception& e) {
            plugin_.getLogger().error(Tran.tr(Tran.getLocal("Error occurred while unbanning device ID {}: {}"),deviceId, e.what()));
            return false;
        }
    }
    
    // 根据玩家名检查是否在黑名单中
    static bool isPlayerBanned(const std::string& playerName) {
        return std::ranges::any_of(BanIDPlayers, [&playerName](const TianyanCore::BanIDPlayer& banPlayer) {
            return banPlayer.player_name == playerName;
        });
    }
    
    // 根据设备ID查找被封禁的玩家信息
    static std::optional<TianyanCore::BanIDPlayer> getBannedPlayerByDeviceId(const std::string& deviceId) {
        const auto it = std::ranges::find_if(BanIDPlayers, [&deviceId](const TianyanCore::BanIDPlayer& banPlayer) {
            return banPlayer.device_id == deviceId;
        });
        
        if (it != BanIDPlayers.end()) {
            return *it;
        }
        return std::nullopt;
    }
    
    // 更新指定设备ID的玩家名称
    static bool updatePlayerNameForDeviceId(const std::string& deviceId, const std::string& newPlayerName) {
        try {
            // 更新缓存中的数据
            bool foundInCache = false;
            for (auto& banPlayer : BanIDPlayers) {
                if (banPlayer.device_id == deviceId) {
                    banPlayer.player_name = newPlayerName;
                    foundInCache = true;
                    break;
                }
            }
            
            if (!foundInCache) {
                return false;
            }

            // 读取现有文件内容
            std::ifstream file(ban_id_path);
            json jsonData;
            
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                file.close();

                if (std::string content = buffer.str(); !content.empty()) {
                    jsonData = json::parse(content);
                }
            } else {
                return false;
            }

            // 检查该设备ID是否存在于文件中
            if (!jsonData.contains(deviceId)) {
                return false;
            }

            // 更新JSON对象中的player_name
            jsonData[deviceId]["player_name"] = newPlayerName;

            // 写入文件
            if (std::ofstream outfile(ban_id_path); outfile.is_open()) {
                outfile << jsonData.dump(4);
                outfile.close();
                return true;
            }
            return false;
        } catch (const std::exception&) {
            return false;
        }
    }

    // 计算实体密度
    static TianyanCore::DensityResult calculateEntityDensity(endstone::Server &server,int size = 20) {
        TianyanCore::DensityResult result;
        
        try {
            
            // 收集所有维度中的实体信息
            struct ActorInfo {
                std::string type;
                double x{}, y{}, z{};
                std::string dim;
            };
            
            std::vector<ActorInfo> actor_info;
            
            // 遍历所有实体
            for (const auto& actor : server.getLevel()->getActors()) {
                ActorInfo info;
                info.type = actor->getType();
                info.x = actor->getLocation().getX();
                info.y = actor->getLocation().getY();
                info.z = actor->getLocation().getZ();
                info.dim = actor->getLocation().getDimension()->getName();
                actor_info.push_back(info);
            }
            
            // 如果没有实体，直接返回空结果
            if (actor_info.empty()) {
                return result;
            }
            
            // 实体密度计算
            int region_size = size;  // 定义区域立方体边长
            std::map<std::string, std::vector<std::tuple<double, double, double, std::string>>> dimension_groups;

            // 按维度分组实体
            for (const auto&[type, x, y, z, dim] : actor_info) {
                dimension_groups[dim].emplace_back(x, y, z, type);
            }

            int max_density = 0;
            struct RegionResult {
                std::string dim;
                double mid_x{}, mid_y{}, mid_z{};
                int count{};
                std::string entity_type;
                double entity_pos_x{}, entity_pos_y{}, entity_pos_z{};
            };
            std::optional<RegionResult> max_result;

            // 遍历每个维度计算密度
            for (const auto& [dim, positions] : dimension_groups) {
                std::map<std::tuple<int, int, int>, int> region_counts;
                std::map<std::tuple<int, int, int>, std::map<std::string, int>> region_entity_types;
                std::map<std::tuple<int, int, int>, std::vector<std::tuple<double, double, double, std::string>>> region_entities;

                // 统计每个区域的实体数量
                for (const auto& pos_info : positions) {
                    auto [x, y, z, type] = pos_info;
                    // 计算区域索引
                    int x_idx = static_cast<int>(std::floor(x / region_size));
                    int y_idx = static_cast<int>(std::floor(y / region_size));
                    int z_idx = static_cast<int>(std::floor(z / region_size));
                    std::tuple<int, int, int> region_key = std::make_tuple(x_idx, y_idx, z_idx);

                    region_counts[region_key]++;

                    // 记录区域中的实体类型
                    region_entity_types[region_key][type]++;

                    // 记录区域中的实体信息
                    region_entities[region_key].push_back(pos_info);
                }

                // 寻找当前维度最密集区域
                if (!region_counts.empty()) {
                    // 找到实体数量最多的区域
                    auto current_max_it = ranges::max_element(region_counts,
                                                              [](const auto& a, const auto& b) {
                                                                  return a.second < b.second;
                                                              });
                    
                    std::tuple<int, int, int> current_max_key = current_max_it->first;
                    int current_max_count = current_max_it->second;

                    // 寻找当前区域中数量最多的实体类型
                    const auto& entity_types_in_region = region_entity_types[current_max_key];
                    auto most_common_it = ranges::max_element(entity_types_in_region,
                                                              [](const auto& a, const auto& b) {
                                                                  return a.second < b.second;
                                                              });
                    
                    std::string most_common_entity = most_common_it->first;

                    // 从区域中选择一个该类型的实体位置
                    std::vector<std::tuple<double, double, double, std::string>> most_common_positions;
                    for (const auto& entity : region_entities[current_max_key]) {
                        if (auto [x, y, z, type] = entity; type == most_common_entity) {
                            most_common_positions.emplace_back(x, y, z, type);
                        }
                    }

                    // 选择一个位置
                    if (!most_common_positions.empty()) {
                        auto [random_x, random_y, random_z, random_type] = most_common_positions[0];

                        // 计算中点坐标
                        double mid_x = (std::get<0>(current_max_key) * region_size) + region_size / 2.0;
                        double mid_y = (std::get<1>(current_max_key) * region_size) + region_size / 2.0;
                        double mid_z = (std::get<2>(current_max_key) * region_size) + region_size / 2.0;

                        // 更新全局最大值
                        if (current_max_count > max_density) {
                            max_density = current_max_count;
                            RegionResult region_result;
                            region_result.dim = dim;
                            region_result.mid_x = mid_x;
                            region_result.mid_y = mid_y;
                            region_result.mid_z = mid_z;
                            region_result.count = current_max_count;
                            region_result.entity_type = most_common_entity;
                            region_result.entity_pos_x = random_x;
                            region_result.entity_pos_y = random_y;
                            region_result.entity_pos_z = random_z;
                            max_result = region_result;
                        }
                    }
                }
            }

            // 输出结果
            if (max_result.has_value()) {
                const auto&[dim, mid_x, mid_y, mid_z, count, entity_type, entity_pos_x, entity_pos_y, entity_pos_z] = max_result.value();
                result.dim = dim;
                result.mid_x = mid_x;
                result.mid_y = mid_y;
                result.mid_z = mid_z;
                result.count = count;
                result.entity_type = entity_type;
                
                result.entity_pos_x = entity_pos_x;
                result.entity_pos_y = entity_pos_y;
                result.entity_pos_z = entity_pos_z;
                result.entity_pos = fmt::format("({:.1f}, {:.1f}, {:.1f})", 
                    entity_pos_x, entity_pos_y, entity_pos_z);
            }
            
        } catch (const std::exception& e) {
            server.getLogger().error(e.what());
        }
        
        return result;
    }

private:
    endstone::Plugin &plugin_;
};


#endif //TIANYAN_TIANYANPROTECT_H