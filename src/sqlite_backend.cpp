#include "sqlite_backend.h"

int SqliteBackend::init_database() {
    return db_.init_database();
}

int SqliteBackend::addLog(const std::string& uuid,
                          const std::string& id,
                          const std::string& name,
                          const double pos_x, const double pos_y, const double pos_z,
                          const std::string& world,
                          const std::string& obj_id,
                          const std::string& obj_name,
                          const long long time,
                          const std::string& type,
                          const std::string& data,
                          const std::string& status) {
    return db_.addLog(uuid, id, name, pos_x, pos_y, pos_z, world,
                      obj_id, obj_name, time, type, data, status);
}

int SqliteBackend::addLogs(const std::vector<DatabaseLogEntry>& entries) {
    // Convert DatabaseLogEntry vector to the tuple format expected by Database::addLogs
    std::vector<std::tuple<std::string, std::string, std::string, double, double, double,
                           std::string, std::string, std::string, long long,
                           std::string, std::string, std::string>> dbLogs;
    dbLogs.reserve(entries.size());
    for (const auto& e : entries) {
        dbLogs.emplace_back(e.uuid, e.id, e.name,
                            e.pos_x, e.pos_y, e.pos_z,
                            e.world, e.obj_id, e.obj_name,
                            e.time, e.type, e.data, e.status);
    }
    return db_.addLogs(dbLogs);
}

int SqliteBackend::searchLog(std::vector<std::map<std::string, std::string>>& result,
                             const std::pair<std::string, double>& key,
                             std::atomic<bool>* cancel) {
    return db_.searchLog(result, key, cancel);
}

int SqliteBackend::searchLog(std::vector<std::map<std::string, std::string>>& result,
                             const std::pair<std::string, double>& key,
                             const double x, const double y, const double z, const double r,
                             const std::string& world,
                             std::atomic<bool>* cancel) {
    return db_.searchLog(result, key, x, y, z, r, world, cancel);
}

bool SqliteBackend::updateStatusesByUUIDs(
    const std::vector<std::pair<std::string, std::string>>& pairs) {
    return db_.updateStatusesByUUIDs(pairs);
}

bool SqliteBackend::cleanDataBase(const double hours) {
    return db_.cleanDataBase(hours);
}

std::string SqliteBackend::generateUuid() {
    return db_util::generate_uuid_v4();
}

int SqliteBackend::executeSQL(const std::string& sql) {
    return db_.executeSQL(sql);
}

int SqliteBackend::querySQL(const std::string& sql,
                            std::vector<std::map<std::string, std::string>>& result) {
    return db_.querySQL(sql, result);
}

int SqliteBackend::updateSQL(const std::string& table,
                             const std::string& set_clause,
                             const std::string& where_clause) {
    return db_.updateSQL(table, set_clause, where_clause);
}

bool SqliteBackend::isValueExists(const std::string& tableName,
                                  const std::string& columnName,
                                  const std::string& value) {
    return db_.isValueExists(tableName, columnName, value);
}

bool SqliteBackend::updateValue(const std::string& tableName,
                                const std::string& targetColumn,
                                const std::string& newValue,
                                const std::string& conditionColumn,
                                const std::string& conditionValue) {
    return db_.updateValue(tableName, targetColumn, newValue, conditionColumn, conditionValue);
}

bool SqliteBackend::updateStatusByUUID(const std::string& uuid,
                                       const std::string& newStatus) {
    return db_.updateStatusByUUID(uuid, newStatus);
}

int SqliteBackend::getAllLog(std::vector<std::map<std::string, std::string>>& result) {
    return db_.getAllLog(result);
}
