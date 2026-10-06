//
// Created by yuhang on 2025/10/26.
//

#include "../include/tianyan_core.h"
#include <stdexcept>

#include "global.h"

TianyanCore::TianyanCore(IDatabaseBackend& db) : db_backend_(db) {};

long long TianyanCore::stringToTimestamp(const std::string& timestampStr) {
    try {
        return std::stoll(timestampStr);
    } catch ([[maybe_unused]] const std::invalid_argument& e) {
        return -1;
    } catch ([[maybe_unused]] const std::out_of_range& e) {
        return -1;
    }
}

std::string TianyanCore::timestampToString(const long long timestamp) {
    const auto time = static_cast<std::time_t>(timestamp);

#ifdef _WIN32
    std::tm tmBuf{};
    const errno_t err = localtime_s(&tmBuf, &time);
    const std::tm* tmPtr = (err == 0) ? &tmBuf : nullptr;
#else
    std::tm tmBuf{};
    const std::tm* tmPtr = localtime_r(&time, &tmBuf);
#endif

    if (!tmPtr) {
        return "INVALID_TIMESTAMP";
    }

    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d-%H:%M:%S", tmPtr);
    return {buffer};
}

int TianyanCore::recordLog(const LogData& logData) const {
    if (!is_db_over) {return 1;}
    return db_backend_.addLog(logData.uuid, logData.id, logData.name,
                              logData.pos_x, logData.pos_y, logData.pos_z,
                              logData.world, logData.obj_id, logData.obj_name,
                              logData.time, logData.type, logData.data, logData.status);
}

int TianyanCore::recordLogs(const std::vector<LogData>& logDatas) const {
    if (!is_db_over) {return 1;}
    std::vector<DatabaseLogEntry> entries;
    entries.reserve(logDatas.size());
    for (const auto& [uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, time, type, data, status] : logDatas) {
        entries.push_back({uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, time, type, data, status});
    }
    return db_backend_.addLogs(entries);
}

vector<TianyanCore::LogData> TianyanCore::searchLog(const pair<string, double>& key, atomic<bool>* cancel) const {
    if (!is_db_over) {return {};}
    std::vector<std::map<std::string, std::string>> result;
    if (cancel && *cancel) return {};
    if (db_backend_.searchLog(result, key, cancel) != 0) return {};
    if (cancel && *cancel) return {};
    if (result.empty()) {
        return {};
    }
    vector<LogData> LogDatas;
    LogDatas.reserve(result.size());
    for (auto& data : result) {
        LogData oneLog;
        try {
            // 字符串直接移入，避免每行 14 次深拷贝（大结果集的主要拷贝开销）
            oneLog.uuid = std::move(data.at("uuid"));
            oneLog.id = std::move(data.at("id"));
            oneLog.name = std::move(data.at("name"));
            oneLog.pos_x = std::stod(data.at("pos_x"));
            oneLog.pos_y = std::stod(data.at("pos_y"));
            oneLog.pos_z = std::stod(data.at("pos_z"));
            oneLog.world = std::move(data.at("world"));
            oneLog.obj_id = std::move(data.at("obj_id"));
            oneLog.obj_name = std::move(data.at("obj_name"));
            oneLog.time = std::stoll(data.at("time"));
            oneLog.type = std::move(data.at("type"));
            oneLog.data = std::move(data.at("data"));
            oneLog.status = std::move(data.at("status"));
        } catch (const std::exception&) {
            // 跳过因数据库脏数据导致的异常行（如 Inf/NaN 坐标）
            continue;
        }
        LogDatas.push_back(std::move(oneLog));
    }
    return LogDatas;
}

vector<TianyanCore::LogData> TianyanCore::searchLog(const pair<string, double>& key,
                                                     const double x, const double y, const double z,
                                                     const double r, const string& world,
                                                     atomic<bool>* cancel) const {
    if (!is_db_over) {return {};}
    std::vector<std::map<std::string, std::string>> result;
    if (cancel && *cancel) return {};
    if (db_backend_.searchLog(result, key, x, y, z, r, world, cancel) != 0) return {};
    if (cancel && *cancel) return {};
    if (result.empty()) {
        return {};
    }
    vector<LogData> LogDatas;
    LogDatas.reserve(result.size());
    for (auto& data : result) {
        LogData oneLog;
        try {
            // 字符串直接移入，避免每行 14 次深拷贝（大结果集的主要拷贝开销）
            oneLog.uuid = std::move(data.at("uuid"));
            oneLog.id = std::move(data.at("id"));
            oneLog.name = std::move(data.at("name"));
            oneLog.pos_x = std::stod(data.at("pos_x"));
            oneLog.pos_y = std::stod(data.at("pos_y"));
            oneLog.pos_z = std::stod(data.at("pos_z"));
            oneLog.world = std::move(data.at("world"));
            oneLog.obj_id = std::move(data.at("obj_id"));
            oneLog.obj_name = std::move(data.at("obj_name"));
            oneLog.time = std::stoll(data.at("time"));
            oneLog.type = std::move(data.at("type"));
            oneLog.data = std::move(data.at("data"));
            oneLog.status = std::move(data.at("status"));
        } catch (const std::exception&) {
            // 跳过因数据库脏数据导致的异常行（如 Inf/NaN 坐标）
            continue;
        }
        LogDatas.push_back(std::move(oneLog));
    }
    return LogDatas;
}

int TianyanCore::recordPlayerSendMSG(const string& player_name) {
    const auto now = std::chrono::steady_clock::now();
    auto& timestamps = playerMessageTimes[player_name];
    timestamps.push_back(now);

    constexpr auto tenSeconds = std::chrono::seconds(10);
    std::erase_if(timestamps,
                  [now, tenSeconds](const TimePoint& t) {
                      return (now - t) > tenSeconds;
                  });

    return static_cast<int>(timestamps.size());
}

bool TianyanCore::checkPlayerSendMSG(const string& player_name) {
    const auto now = std::chrono::steady_clock::now();
    constexpr auto tenSeconds = std::chrono::seconds(10);

    const auto it = playerMessageTimes.find(player_name);
    if (it == playerMessageTimes.end()) {
        return false;
    }

    auto& timestamps = it->second;
    std::erase_if(timestamps,
                  [now, tenSeconds](const TimePoint& t) {
                      return (now - t) > tenSeconds;
                  });

    return timestamps.size() > max_message_in_10s;
}

int TianyanCore::recordPlayerSendCMD(const string& player_name) {
    const auto now = std::chrono::steady_clock::now();
    auto& timestamps = playerCommandTimes[player_name];
    timestamps.push_back(now);

    constexpr auto tenSeconds = std::chrono::seconds(10);
    std::erase_if(timestamps,
                  [now, tenSeconds](const TimePoint& t) {
                      return (now - t) > tenSeconds;
                  });

    return static_cast<int>(timestamps.size());
}

bool TianyanCore::checkPlayerSendCMD(const string& player_name) {
    const auto now = std::chrono::steady_clock::now();
    constexpr auto tenSeconds = std::chrono::seconds(10);

    const auto it = playerCommandTimes.find(player_name);
    if (it == playerCommandTimes.end()) {
        return false;
    }

    auto& timestamps = it->second;
    std::erase_if(timestamps,
                  [now, tenSeconds](const TimePoint& t) {
                      return (now - t) > tenSeconds;
                  });

    return timestamps.size() > max_command_in_10s;
}
