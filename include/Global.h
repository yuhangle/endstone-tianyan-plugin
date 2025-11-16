//
// Created by yuhang on 2025/11/6.
//

#ifndef TIANYAN_GLOBAL_H
#define TIANYAN_GLOBAL_H
#include "DataBase.hpp"
#include "TianyanCore.h"
#include "translate.h"
#include <nlohmann/json.hpp>
#include <condition_variable>
using namespace nlohmann;
//日志缓存
inline vector<TianyanCore::LogData> logDataCache;
//封禁设备ID玩家缓存
inline vector<TianyanCore::BanIDPlayer> BanIDPlayers;
//语言
inline translate Tran;
//初始化其它实例
inline DataBase Database(dbPath);
inline TianyanCore tyCore(Database);
//缓存锁
inline std::mutex cacheMutex;

#endif //TIANYAN_GLOBAL_H