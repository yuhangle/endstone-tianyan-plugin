#pragma once

#include <atomic>
#include <map>
#include <string>
#include <vector>

struct DatabaseLogEntry {
    std::string uuid;
    std::string id;
    std::string name;
    double pos_x = 0;
    double pos_y = 0;
    double pos_z = 0;
    std::string world;
    std::string obj_id;
    std::string obj_name;
    long long time = 0;
    std::string type;
    std::string data;
    std::string status;
};

class IDatabaseBackend {
public:
    virtual ~IDatabaseBackend() = default;

    virtual int init_database() = 0;

    virtual int addLog(const std::string& uuid,
                       const std::string& id,
                       const std::string& name,
                       double pos_x, double pos_y, double pos_z,
                       const std::string& world,
                       const std::string& obj_id,
                       const std::string& obj_name,
                       long long time,
                       const std::string& type,
                       const std::string& data,
                       const std::string& status) = 0;

    virtual int addLogs(const std::vector<DatabaseLogEntry>& entries) = 0;

    virtual int searchLog(std::vector<std::map<std::string, std::string>>& result,
                          const std::pair<std::string, double>& key,
                          std::atomic<bool>* cancel) = 0;

    int searchLog(std::vector<std::map<std::string, std::string>>& result,
                  const std::pair<std::string, double>& key) {
        return searchLog(result, key, nullptr);
    }

    virtual int searchLog(std::vector<std::map<std::string, std::string>>& result,
                          const std::pair<std::string, double>& key,
                          double x, double y, double z, double r,
                          const std::string& world,
                          std::atomic<bool>* cancel) = 0;

    int searchLog(std::vector<std::map<std::string, std::string>>& result,
                  const std::pair<std::string, double>& key,
                  double x, double y, double z, double r,
                  const std::string& world) {
        return searchLog(result, key, x, y, z, r, world, nullptr);
    }

    virtual bool updateStatusesByUUIDs(
        const std::vector<std::pair<std::string, std::string>>& pairs) = 0;

    virtual int64_t getCleanCount(long long timestamp) = 0;

    virtual int deleteBatch(long long timestamp, int limit) = 0;

    // MySQL 表重建清理：CREATE INSERT RENAME DROP，替代逐行 DELETE
    // 返回 -1 表示不支持（SQLite 不实现此方法）
    virtual int64_t cleanupByRebuild(long long threshold) { return -1; }

    // 三阶段清理 API（SQLite 使用独立连接；MySQL 退化为 deleteBatch + no-op）
    virtual bool beginCleanup() { return true; }

    virtual int cleanupDeleteBatch(long long timestamp, int limit) {
        return deleteBatch(timestamp, limit);
    }

    virtual bool cleanupCheckpoint() { return true; }

    // checkpoint + 关连接，不做 VACUUM（小量清理跳过全库重建）
    virtual bool abortCleanup() { return true; }

    virtual bool endCleanup() { return true; }

    // 供 runCleanup 判断是否需要 WAL checkpoint
    [[nodiscard]] virtual bool isSqlite() const { return false; }

    virtual std::string generateUuid() = 0;

    // === Generic SQL operations ===

    virtual int executeSQL(const std::string& sql) = 0;

    virtual int querySQL(const std::string& sql,
                         std::vector<std::map<std::string, std::string>>& result) = 0;

    virtual int updateSQL(const std::string& table,
                          const std::string& set_clause,
                          const std::string& where_clause) = 0;

    // === Value operations ===

    virtual bool isValueExists(const std::string& tableName,
                               const std::string& columnName,
                               const std::string& value) = 0;

    virtual bool updateValue(const std::string& tableName,
                             const std::string& targetColumn,
                             const std::string& newValue,
                             const std::string& conditionColumn,
                             const std::string& conditionValue) = 0;

    virtual bool updateStatusByUUID(const std::string& uuid,
                                    const std::string& newStatus) = 0;

    virtual int getAllLog(std::vector<std::map<std::string, std::string>>& result) = 0;
};
