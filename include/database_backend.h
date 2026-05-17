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
                          std::atomic<bool>* cancel = nullptr) = 0;

    virtual int searchLog(std::vector<std::map<std::string, std::string>>& result,
                          const std::pair<std::string, double>& key,
                          double x, double y, double z, double r,
                          const std::string& world,
                          std::atomic<bool>* cancel = nullptr) = 0;

    virtual bool updateStatusesByUUIDs(
        const std::vector<std::pair<std::string, std::string>>& pairs) = 0;

    virtual bool cleanDataBase(double hours) = 0;

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
