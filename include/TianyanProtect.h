//
// Created by yuhang on 2025/11/6.
//

#ifndef TIANYAN_TIANYANPROTECT_H
#define TIANYAN_TIANYANPROTECT_H
#include <Global.h>

class TianyanProtect {
public:
    explicit TianyanProtect(endstone::Plugin &plugin) : plugin_(plugin){}

    //设备ID黑名单初始化
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

private:
    endstone::Plugin &plugin_;
};


#endif //TIANYAN_TIANYANPROTECT_H