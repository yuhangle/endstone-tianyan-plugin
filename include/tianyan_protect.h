//
// Created by yuhang on 2025/11/6.
//

#ifndef TIANYAN_TIANYANPROTECT_H
#define TIANYAN_TIANYANPROTECT_H
#include <endstone/endstone.hpp>
#include "global.h"

class TianyanProtect {
public:
    explicit TianyanProtect(endstone::Plugin &plugin) : plugin_(plugin){}

    // 设备ID黑名单初始化
    void deviceIDBlacklistInit() const;

    // 封禁设备ID
    [[nodiscard]] bool BanDeviceID(const TianyanCore::BanIDPlayer& banPlayer) const;
    
    // 解封设备ID
    [[nodiscard]] bool UnbanDeviceID(const std::string& deviceId) const;
    
    // 根据玩家名检查是否在黑名单中
    static bool isPlayerBanned(const std::string& playerName);
    
    // 根据设备ID查找被封禁的玩家信息
    static std::optional<TianyanCore::BanIDPlayer> getBannedPlayerByDeviceId(const std::string& deviceId);
    
    // 更新指定设备ID的玩家名称
    static bool updatePlayerNameForDeviceId(const std::string& deviceId, const std::string& newPlayerName);

    // 计算实体密度
    static TianyanCore::DensityResult calculateEntityDensity(endstone::Server &server,int size = 20);

private:
    endstone::Plugin &plugin_;
};


#endif //TIANYAN_TIANYANPROTECT_H