#pragma once

#include "database_backend.h"

#include <memory>

namespace mysql_api {
class MySQLAPI;
}

class MysqlBackend : public IDatabaseBackend {
public:
    explicit MysqlBackend(std::shared_ptr<mysql_api::MySQLAPI> mysql)
        : mysql_(std::move(mysql)) {}

    int init_database() override;

    int addLog(const std::string& uuid,
               const std::string& id,
               const std::string& name,
               double pos_x, double pos_y, double pos_z,
               const std::string& world,
               const std::string& obj_id,
               const std::string& obj_name,
               long long time,
               const std::string& type,
               const std::string& data,
               const std::string& status) override;

    int addLogs(const std::vector<DatabaseLogEntry>& entries) override;

    int searchLog(std::vector<std::map<std::string, std::string>>& result,
                  const std::pair<std::string, double>& key,
                  std::atomic<bool>* cancel = nullptr) override;

    int searchLog(std::vector<std::map<std::string, std::string>>& result,
                  const std::pair<std::string, double>& key,
                  double x, double y, double z, double r,
                  const std::string& world,
                  std::atomic<bool>* cancel = nullptr) override;

    bool updateStatusesByUUIDs(
        const std::vector<std::pair<std::string, std::string>>& pairs) override;

    bool cleanDataBase(double hours) override;

    std::string generateUuid() override;

    int executeSQL(const std::string& sql) override;

    int querySQL(const std::string& sql,
                 std::vector<std::map<std::string, std::string>>& result) override;

    int updateSQL(const std::string& table,
                  const std::string& set_clause,
                  const std::string& where_clause) override;

    bool isValueExists(const std::string& tableName,
                       const std::string& columnName,
                       const std::string& value) override;

    bool updateValue(const std::string& tableName,
                     const std::string& targetColumn,
                     const std::string& newValue,
                     const std::string& conditionColumn,
                     const std::string& conditionValue) override;

    bool updateStatusByUUID(const std::string& uuid,
                            const std::string& newStatus) override;

    int getAllLog(std::vector<std::map<std::string, std::string>>& result) override;

private:
    std::shared_ptr<mysql_api::MySQLAPI> mysql_;

    int queryResultToMaps(
        std::vector<std::map<std::string, std::string>>& result,
        const std::string& sql,
        const std::vector<std::string>& params = {}) const;
};
