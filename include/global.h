//
// Created by yuhang on 2025/11/6.
//

#ifndef TIANYAN_GLOBAL_H
#define TIANYAN_GLOBAL_H
#include "tianyan_core.h"
#include <nlohmann/json.hpp>
#include <condition_variable>
using namespace nlohmann;
//日志缓存
inline vector<TianyanCore::LogData> logDataCache;
//封禁设备ID玩家缓存
inline vector<TianyanCore::BanIDPlayer> BanIDPlayers;
//缓存锁
inline std::mutex cacheMutex;
//数据库状态
inline std::atomic is_db_over = false;

#endif //TIANYAN_GLOBAL_H