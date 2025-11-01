//
// Created by yuhang on 2025/10/26.
//

#include "../include/TianyanCore.h"

TianyanCore::TianyanCore(DataBase database) : Database(std::move(database)) {};

long long TianyanCore::stringToTimestamp(const std::string& timestampStr) {
    try {
        // 使用 std::stoll 将字符串转换为 long long
        return std::stoll(timestampStr);
    } catch ([[maybe_unused]] const std::invalid_argument& e) {
        return -1; // 返回一个错误标志值
    } catch ([[maybe_unused]] const std::out_of_range& e) {
        return -1;
    }
}

std::string TianyanCore::timestampToString(long long timestamp) {

    const auto time = static_cast<std::time_t>(timestamp);

    // 使用 localtime_s (Windows) 或 localtime_r (Linux/POSIX) 进行线程安全转换
    // 条件编译确保跨平台兼容性
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

    // 创建一个字符数组来存放格式化后的字符串
    char buffer[20]; // YYYY-MM-DD HH:MM:SS + null terminator = 20 chars

    // 格式化时间
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d-%H:%M:%S", tmPtr);
    return {buffer};
}

int TianyanCore::recordLog(const LogData& logData) const {
    return Database.addLog(logData.id,logData.name,logData.pos_x,logData.pos_y,logData.pos_z,logData.world,logData.obj_id,logData.obj_name,logData.time,logData.type,logData.data);
}

int TianyanCore::recordLogs(const std::vector<LogData>& logDatas) const {
    // 将LogData转换为数据库所需的tuple格式
    std::vector<std::tuple<std::string, std::string, double, double, double, 
                           std::string, std::string, std::string, long long, std::string, std::string>> dbLogs;
    
    dbLogs.reserve(logDatas.size());
for (const auto&[id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, time, type, data] : logDatas) {
        dbLogs.emplace_back(id, name, pos_x, pos_y, pos_z,
                            world, obj_id, obj_name, time, type, data);
    }
    
    // 调用数据库的批量插入函数
    return Database.addLogs(dbLogs);
}

vector<TianyanCore::LogData> TianyanCore::searchLog(const pair<string, double>& key) const {
    std::vector<std::map<std::string, std::string>> result;
    Database.searchLog(result,key);
    if (result.empty()) {
        return {};
    }
    vector<LogData> LogDatas;
    for (const auto& data:result) {
        LogData oneLog;
        oneLog.id = data.at("id");oneLog.name = data.at("name");oneLog.pos_x = std::stod(data.at("pos_x"));oneLog.pos_y = std::stod(data.at("pos_y"));
        oneLog.pos_z = std::stod(data.at("pos_z"));oneLog.world = data.at("world");oneLog.obj_id = data.at("obj_id");oneLog.obj_name = data.at("obj_name");oneLog.time = std::stoll(data.at("time"));
        oneLog.type = data.at("type");oneLog.data = data.at("data");
        LogDatas.push_back(oneLog);
    }
    return LogDatas;
}

vector<TianyanCore::LogData> TianyanCore::searchLog(const pair<string, double>& key, const double x, const double y, const double z, const double r, const string& world) const {
    std::vector<std::map<std::string, std::string>> result;
    Database.searchLog(result,key);
    if (result.empty()) {
        return {};
    }
    vector<LogData> LogDatas;
    for (const auto& data:result) {
        LogData oneLog;
        oneLog.id = data.at("id");oneLog.name = data.at("name");oneLog.pos_x = std::stod(data.at("pos_x"));oneLog.pos_y = std::stod(data.at("pos_y"));
        oneLog.pos_z = std::stod(data.at("pos_z"));oneLog.world = data.at("world");oneLog.obj_id = data.at("obj_id");oneLog.obj_name = data.at("obj_name");oneLog.time = std::stoll(data.at("time"));
        oneLog.type = data.at("type");oneLog.data = data.at("data");
        
        // 首先检查世界是否匹配
        if (oneLog.world != world) {
            continue;
        }
        
        // 然后检查坐标是否在指定范围内
        const double dx = oneLog.pos_x - x;
        const double dy = oneLog.pos_y - y;
        const double dz = oneLog.pos_z - z;
        const double distanceSquared = dx * dx + dy * dy + dz * dz;

        if (const double radiusSquared = r * r; distanceSquared <= radiusSquared) {
            LogDatas.push_back(oneLog);
        }
    }
    return LogDatas;
}
