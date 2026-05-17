#include "mysql_backend.h"

#include "database.hpp"

#include <endstone_mysql_api/mysql_api.h>
#include <chrono>

int MysqlBackend::init_database() {
    return mysql_->init_logdata_table();
}

int MysqlBackend::addLog(const std::string& uuid,
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
    return mysql_->add_log(uuid, id, name, pos_x, pos_y, pos_z, world,
                           obj_id, obj_name, time, type, data, status);
}

int MysqlBackend::addLogs(const std::vector<DatabaseLogEntry>& entries) {
    if (entries.empty()) {
        return 0;
    }

    std::vector<mysql_api::LogEntry> mysql_entries;
    mysql_entries.reserve(entries.size());
    for (const auto& e : entries) {
        mysql_entries.push_back({
            .uuid = e.uuid,
            .id = e.id,
            .name = e.name,
            .pos_x = e.pos_x,
            .pos_y = e.pos_y,
            .pos_z = e.pos_z,
            .world = e.world,
            .obj_id = e.obj_id,
            .obj_name = e.obj_name,
            .time = e.time,
            .type = e.type,
            .data = e.data,
            .status = e.status
        });
    }

    return mysql_->add_logs(mysql_entries);
}

int MysqlBackend::searchLog(std::vector<std::map<std::string, std::string>>& result,
                            const std::pair<std::string, double>& key,
                            std::atomic<bool>* cancel) {
    if (cancel && *cancel) return -1;
    auto qr = mysql_->search_logs(key.first, key.second);
    if (cancel && *cancel) return -1;
    result.clear();
    result.reserve(qr.rows.size());
    for (auto& row : qr.rows) {
        result.push_back(std::move(static_cast<std::map<std::string, std::string>&>(row)));
    }
    return 0;
}

int MysqlBackend::searchLog(std::vector<std::map<std::string, std::string>>& result,
                            const std::pair<std::string, double>& key,
                            const double x, const double y, const double z, const double r,
                            const std::string& world,
                            std::atomic<bool>* cancel) {
    if (cancel && *cancel) return -1;
    auto qr = mysql_->search_logs_by_pos(key.first, key.second, x, y, z, r, world);
    if (cancel && *cancel) return -1;
    result.clear();
    result.reserve(qr.rows.size());
    for (auto& row : qr.rows) {
        result.push_back(std::move(static_cast<std::map<std::string, std::string>&>(row)));
    }
    return 0;
}

bool MysqlBackend::updateStatusesByUUIDs(
    const std::vector<std::pair<std::string, std::string>>& pairs) {
    return mysql_->update_statuses_by_uuids(pairs);
}

bool MysqlBackend::cleanDataBase(const double hours) {
    yuhangle::clean_data_status = 2;
    auto start_time = std::chrono::high_resolution_clock::now();

    const long long current_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const long long threshold = current_time - static_cast<long long>(hours * 3600);

    // Count rows to be deleted
    auto count_result = mysql_->query(
        "SELECT COUNT(*) AS cnt FROM LOGDATA WHERE time < ?",
        {std::to_string(threshold)});
    int deleted_count = 0;
    if (!count_result.rows.empty()) {
        auto it = count_result.rows[0].find("cnt");
        if (it != count_result.rows[0].end()) {
            deleted_count = std::stoi(it->second);
        }
    }

    // Delete
    int affected = mysql_->execute(
        "DELETE FROM LOGDATA WHERE time < ?",
        {std::to_string(threshold)});

    if (affected < 0) {
        yuhangle::clean_data_message.clear();
        yuhangle::clean_data_message.emplace_back("MySQL delete failed");
        yuhangle::clean_data_status = -1;
        return false;
    }

    // Timing
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double seconds = std::chrono::duration<double>(duration).count();

    yuhangle::clean_data_message.clear();
    yuhangle::clean_data_message.emplace_back("Time elapsed: ");
    yuhangle::clean_data_message.emplace_back(std::to_string(seconds));
    yuhangle::clean_data_message.emplace_back("Number of cleaned logs: ");
    yuhangle::clean_data_message.emplace_back(std::to_string(deleted_count));

    yuhangle::clean_data_status = 1;
    return true;
}

std::string MysqlBackend::generateUuid() {
    return mysql_->generate_uuid();
}

int MysqlBackend::executeSQL(const std::string& sql) {
    int affected = mysql_->execute(sql, {});
    return affected >= 0 ? 0 : affected;
}

int MysqlBackend::querySQL(const std::string& sql,
                           std::vector<std::map<std::string, std::string>>& result) {
    return queryResultToMaps(result, sql, {});
}

int MysqlBackend::updateSQL(const std::string& table,
                            const std::string& set_clause,
                            const std::string& where_clause) {
    const std::string sql = "UPDATE " + table + " SET " + set_clause + " WHERE " + where_clause;
    int affected = mysql_->execute(sql, {});
    return affected >= 0 ? 0 : affected;
}

bool MysqlBackend::isValueExists(const std::string& tableName,
                                 const std::string& columnName,
                                 const std::string& value) {
    auto qr = mysql_->query(
        "SELECT COUNT(*) AS cnt FROM " + tableName + " WHERE " + columnName + " = ?",
        {value});
    if (!qr.rows.empty()) {
        auto it = qr.rows[0].find("cnt");
        if (it != qr.rows[0].end()) {
            return std::stoi(it->second) > 0;
        }
    }
    return false;
}

bool MysqlBackend::updateValue(const std::string& tableName,
                               const std::string& targetColumn,
                               const std::string& newValue,
                               const std::string& conditionColumn,
                               const std::string& conditionValue) {
    const std::string sql = "UPDATE " + tableName +
                            " SET " + targetColumn + " = ?" +
                            " WHERE " + conditionColumn + " = ?";
    int affected = mysql_->execute(sql, {newValue, conditionValue});
    return affected > 0;
}

bool MysqlBackend::updateStatusByUUID(const std::string& uuid,
                                      const std::string& newStatus) {
    return mysql_->update_status_by_uuid(uuid, newStatus);
}

int MysqlBackend::getAllLog(std::vector<std::map<std::string, std::string>>& result) {
    return queryResultToMaps(result, "", {});
}

int MysqlBackend::queryResultToMaps(
    std::vector<std::map<std::string, std::string>>& result,
    const std::string& sql,
    const std::vector<std::string>& params) const
{
    auto qr = mysql_->query(sql, params);
    result.clear();
    result.reserve(qr.rows.size());
    for (auto& row : qr.rows) {
        result.push_back(std::move(static_cast<std::map<std::string, std::string>&>(row)));
    }
    return 0;
}
