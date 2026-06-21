#include "rust_backend.h"

#include "database.hpp"

#include <chrono>
#include <iostream>
#include <vector>
#ifdef __linux__
#include <malloc.h>
#endif

// ============================================================
// C FFI declarations (from librust_mysql.a)
// ============================================================

extern "C" {
    // Lifecycle
    void* rust_mysql_new(const char* host, uint16_t port,
                         const char* user, const char* pass,
                         const char* db);
    void rust_mysql_free(void* handle);

    // Connection
    bool rust_mysql_connect(void* handle);
    bool rust_mysql_disconnect(void* handle);
    bool rust_mysql_is_connected(const void* handle);

    // Query (returns opaque result ptr, or nullptr on error)
    void* rust_mysql_query(void* handle, const char* sql);
    void* rust_mysql_query_params(void* handle, const char* sql,
                                   const char* const* params, int32_t param_count);
    void* rust_mysql_query_params_with_cancel(void* handle, const char* sql,
                                               const char* const* params,
                                               int32_t param_count,
                                               bool (*cancel_fn)(void*),
                                               void* cancel_ctx);
    int64_t rust_mysql_execute(void* handle, const char* sql);
    int64_t rust_mysql_execute_params(void* handle, const char* sql,
                                       const char* const* params,
                                       int32_t param_count);

    // Result access
    int32_t rust_mysql_result_rows(const void* result);
    int32_t rust_mysql_result_cols(const void* result);
    const char* rust_mysql_result_col_name(const void* result, int32_t col);
    const char* rust_mysql_result_value(const void* result,
                                         int32_t row, int32_t col);
    void rust_mysql_free_result(void* result);

    // Error & utility
    const char* rust_mysql_last_error(const void* handle);
    char* rust_mysql_generate_uuid();
    void rust_mysql_free_string(char* s);
}

// ============================================================
// Cancel callback trampoline
// ============================================================

static bool cancel_callback(void* ctx) {
    const auto* flag = static_cast<std::atomic<bool>*>(ctx);
    return flag && flag->load();
}

// ============================================================
// Construction / destruction
// ============================================================

RustBackend::RustBackend(const RustMySQLConfig& config) {
    handle_ = rust_mysql_new(
        config.host.c_str(),
        config.port,
        config.user.c_str(),
        config.password.c_str(),
        config.database.c_str()
    );
    if (handle_) {
        connected_ = rust_mysql_connect(handle_);
        if (!connected_) {
            std::cerr << "[Tianyan][RustMySQL] Connect failed: "
                      << lastError() << std::endl;
        }
    } else {
        std::cerr << "[Tianyan][RustMySQL] Failed to create handle" << std::endl;
    }
}

RustBackend::~RustBackend() {
    if (handle_) {
        rust_mysql_disconnect(handle_);
        rust_mysql_free(handle_);
    }
}

// ============================================================
// Error helper
// ============================================================

std::string RustBackend::lastError() const {
    if (!handle_) return "null handle";
    const char* err = rust_mysql_last_error(handle_);
    return err ? std::string(err) : "";
}

// ============================================================
// Result conversion helpers
// ============================================================

int RustBackend::resultToMaps(
    void* result,
    std::vector<std::map<std::string, std::string>>& out)
{
    if (!result) return -1;

    const int rows = rust_mysql_result_rows(result);
    const int cols = rust_mysql_result_cols(result);

    // Collect column names
    std::vector<std::string> col_names;
    col_names.reserve(cols);
    for (int c = 0; c < cols; ++c) {
        const char* name = rust_mysql_result_col_name(result, c);
        col_names.emplace_back(name ? name : "");
    }

    out.clear();
    out.reserve(rows);
    for (int r = 0; r < rows; ++r) {
        std::map<std::string, std::string> row;
        for (int c = 0; c < cols; ++c) {
            const char* val = rust_mysql_result_value(result, r, c);
            row[col_names[c]] = val ? val : "";
        }
        out.push_back(std::move(row));
    }

    rust_mysql_free_result(result);
    return 0;
}

int RustBackend::queryToMaps(
    const std::string& sql,
    std::vector<std::map<std::string, std::string>>& result,
    const std::vector<std::string>& params) const
{
    if (!handle_) return -1;

    void* qr;
    if (params.empty()) {
        qr = rust_mysql_query(handle_, sql.c_str());
    } else {
        // Build C string array
        std::vector<const char*> c_params;
        c_params.reserve(params.size());
        for (const auto& p : params) {
            c_params.push_back(p.c_str());
        }
        qr = rust_mysql_query_params(handle_, sql.c_str(),
                                      c_params.data(),
                                      static_cast<int32_t>(c_params.size()));
    }
    if (!qr) {
        std::cerr << "[Tianyan][RustMySQL] query failed: " << lastError()
                  << "\n  SQL: " << sql.substr(0, 200) << std::endl;
        return -1;
    }
    return resultToMaps(qr, result);
}

int RustBackend::queryToMapsWithCancel(
    const std::string& sql,
    std::vector<std::map<std::string, std::string>>& result,
    const std::vector<std::string>& params,
    std::atomic<bool>* cancel) const
{
    if (!handle_ || !cancel) return queryToMaps(sql, result, params);
    if (cancel->load()) return -1;

    std::vector<const char*> c_params;
    c_params.reserve(params.size());
    for (const auto& p : params) {
        c_params.push_back(p.c_str());
    }

    void* qr = rust_mysql_query_params_with_cancel(
        handle_, sql.c_str(),
        c_params.data(), static_cast<int32_t>(c_params.size()),
        &cancel_callback, cancel);

    if (!qr) {
        if (cancel->load()) {
            // After the Rust side freed the large rows Vec, call malloc_trim(0)
            // to encourage glibc to return freed heap pages to the OS,
            // reducing RSS after a cancelled large query.
#ifdef __linux__
            ::malloc_trim(0);
#endif
            return -1; // cancelled
        }
        std::cerr << "[Tianyan][RustMySQL] cancel query failed: "
                  << lastError() << std::endl;
        return -1;
    }
    return resultToMaps(qr, result);
}

// ============================================================
// IDatabaseBackend implementation
// ============================================================

int RustBackend::init_database() {
    if (!handle_) return -1;

    // InnoDB 聚簇主键：自增整数 → 顺序写，无页碎片
    // uuid 改为普通列 + 唯一索引，不作为聚簇主键
    const auto create_sql =
        "CREATE TABLE IF NOT EXISTS LOGDATA ("
        "id_pk    BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        "uuid     VARCHAR(36)  NOT NULL, "
        "id       VARCHAR(255) DEFAULT NULL, "
        "name     VARCHAR(255) DEFAULT NULL, "
        "pos_x    DOUBLE       DEFAULT NULL, "
        "pos_y    DOUBLE       DEFAULT NULL, "
        "pos_z    DOUBLE       DEFAULT NULL, "
        "world    VARCHAR(255) DEFAULT NULL, "
        "obj_id   VARCHAR(255) DEFAULT NULL, "
        "obj_name VARCHAR(255) DEFAULT NULL, "
        "time     BIGINT       DEFAULT NULL, "
        "type     VARCHAR(255) DEFAULT NULL, "
        "data     TEXT         DEFAULT NULL, "
        "status   VARCHAR(255) DEFAULT NULL, "
        "UNIQUE KEY uk_uuid (uuid), "
        "KEY idx_logdata_time (time), "
        "KEY idx_logdata_pos  (pos_x, pos_y, pos_z)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci";

    if (rust_mysql_execute(handle_, create_sql) < 0) {
        std::cerr << "[Tianyan][RustMySQL] init_database create table failed: "
                  << lastError() << std::endl;
        return -1;
    }

    // 检查是否需要迁移旧表结构（uuid 主键 → 自增整数主键）
    std::vector<std::map<std::string, std::string>> check;
    if (queryToMaps("SHOW COLUMNS FROM LOGDATA LIKE 'id_pk'", check) != 0) {
        // 表不存在或查询失败，首次启动，无需迁移
        return 0;
    }
    if (!check.empty()) {
        // id_pk 列已存在，新表结构，无需迁移
        return 0;
    }

    // id_pk 列不存在 → 旧表结构（uuid PRIMARY KEY），自动迁移
    std::cerr << "[Tianyan][RustMySQL] Detected old schema (uuid PK), "
              << "migrating to auto-increment PK..." << std::endl;

    // 清理可能的残留旧表
    rust_mysql_execute(handle_, "DROP TABLE IF EXISTS LOGDATA_OLD");

    if (rust_mysql_execute(handle_, "RENAME TABLE LOGDATA TO LOGDATA_OLD") < 0) {
        std::cerr << "[Tianyan][RustMySQL] Schema migrate: rename failed" << std::endl;
        return -1;
    }

    if (rust_mysql_execute(handle_, create_sql) < 0) {
        std::cerr << "[Tianyan][RustMySQL] Schema migrate: create table failed, "
                  << "rolling back" << std::endl;
        rust_mysql_execute(handle_, "RENAME TABLE LOGDATA_OLD TO LOGDATA");
        return -1;
    }

    // 服务端 INSERT ... SELECT，不走 FFI，纯 MariaDB 内部操作
    // 不 ORDER BY，避免 filesort 写盘拖慢数十分钟
    const char* migrate_sql =
        "INSERT INTO LOGDATA "
        "(uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, "
        "time, type, data, status) "
        "SELECT uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, "
        "time, type, data, status "
        "FROM LOGDATA_OLD";

    int64_t inserted = rust_mysql_execute(handle_, migrate_sql);
    if (inserted < 0) {
        std::cerr << "[Tianyan][RustMySQL] Schema migrate: data copy failed, "
                  << "rolling back" << std::endl;
        rust_mysql_execute(handle_, "DROP TABLE IF EXISTS LOGDATA");
        rust_mysql_execute(handle_, "RENAME TABLE LOGDATA_OLD TO LOGDATA");
        return -1;
    }

    rust_mysql_execute(handle_, "DROP TABLE LOGDATA_OLD");
    std::cerr << "[Tianyan][RustMySQL] Schema migration complete: "
              << inserted << " rows migrated" << std::endl;

    return 0;
}

int RustBackend::addLog(
    const std::string& uuid, const std::string& id, const std::string& name,
    const double pos_x, const double pos_y, const double pos_z,
    const std::string& world, const std::string& obj_id,
    const std::string& obj_name, const long long time,
    const std::string& type, const std::string& data,
    const std::string& status)
{
    if (!handle_) return -1;

    const std::string sql =
        "INSERT IGNORE INTO LOGDATA (uuid, id, name, pos_x, pos_y, pos_z, "
        "world, obj_id, obj_name, time, type, data, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    const std::vector params = {
        uuid, id, name,
        std::to_string(pos_x), std::to_string(pos_y), std::to_string(pos_z),
        world, obj_id, obj_name,
        std::to_string(time), type, data, status
    };

    std::vector<const char*> c_params;
    c_params.reserve(params.size());
    for (const auto& p : params) {
        c_params.push_back(p.c_str());
    }

    const int64_t ret = rust_mysql_execute_params(
        handle_, sql.c_str(),
        c_params.data(), static_cast<int32_t>(c_params.size()));
    return ret >= 0 ? 0 : -1;
}

int RustBackend::addLogs(const std::vector<DatabaseLogEntry>& entries) {
    if (entries.empty()) return 0;
    if (!handle_) return -1;

    // Multi-row INSERT: one FFI call per 5000 rows instead of per-row
    constexpr int SUB_BATCH = 4000; // 4000 × 13 = 52000 params, well under MySQL limit

    // bulk insert 优化：跳过唯一约束检查，redo log 降频
    rust_mysql_execute(handle_,
        "SET SESSION unique_checks=0, foreign_key_checks=0, "
        "innodb_flush_log_at_trx_commit=2");
    rust_mysql_execute(handle_, "START TRANSACTION");

    for (size_t i = 0; i < entries.size(); i += SUB_BATCH) {
        constexpr int COLS = 13;
        const size_t end = std::min(i + static_cast<size_t>(SUB_BATCH), entries.size());

        std::string sql =
            "INSERT IGNORE INTO LOGDATA "
            "(uuid, id, name, pos_x, pos_y, pos_z, "
            "world, obj_id, obj_name, time, type, data, status) VALUES ";

        std::vector<std::string> params;
        params.reserve((end - i) * COLS);

        for (size_t j = i; j < end; ++j) {
            if (j > i) sql += ",";
            sql += "(?,?,?,?,?,?,?,?,?,?,?,?,?)";
            const auto& [uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, time, type, data, status] = entries[j];
            params.emplace_back(uuid);
            params.emplace_back(id);
            params.emplace_back(name);
            params.emplace_back(std::to_string(pos_x));
            params.emplace_back(std::to_string(pos_y));
            params.emplace_back(std::to_string(pos_z));
            params.emplace_back(world);
            params.emplace_back(obj_id);
            params.emplace_back(obj_name);
            params.emplace_back(std::to_string(time));
            params.emplace_back(type);
            params.emplace_back(data);
            params.emplace_back(status);
        }

        std::vector<const char*> c_params;
        c_params.reserve(params.size());
        for (const auto& p : params) {
            c_params.push_back(p.c_str());
        }

        if (rust_mysql_execute_params(handle_, sql.c_str(),
                                       c_params.data(),
                                       static_cast<int32_t>(c_params.size())) < 0) {
            rust_mysql_execute(handle_,
                "SET SESSION unique_checks=1, foreign_key_checks=1, "
                "innodb_flush_log_at_trx_commit=1");
            rust_mysql_execute(handle_, "ROLLBACK");
            return -1;
        }
    }

    rust_mysql_execute(handle_, "COMMIT");
    // 恢复 session 变量
    rust_mysql_execute(handle_,
        "SET SESSION unique_checks=1, foreign_key_checks=1, "
        "innodb_flush_log_at_trx_commit=1");
    return 0;
}

int RustBackend::searchLog(
    std::vector<std::map<std::string, std::string>>& result,
    const std::pair<std::string, double>& key,
    std::atomic<bool>* cancel)
{
    // 行数上限SQL层
    const std::string sql =
        "SELECT * FROM LOGDATA WHERE "
        "(name LIKE ? OR type LIKE ? OR data LIKE ?) AND "
        "time >= UNIX_TIMESTAMP() - ? "
        "ORDER BY time LIMIT 100000";

    const std::vector params = {
        "%" + key.first + "%",
        "%" + key.first + "%",
        "%" + key.first + "%",
        std::to_string(static_cast<long long>(key.second * 3600))
    };

    if (cancel) {
        return queryToMapsWithCancel(sql, result, params, cancel);
    }
    return queryToMaps(sql, result, params);
}

int RustBackend::searchLog(
    std::vector<std::map<std::string, std::string>>& result,
    const std::pair<std::string, double>& key,
    const double x, const double y, const double z, const double r,
    const std::string& world,
    std::atomic<bool>* cancel)
{
    const std::string sql =
        "SELECT * FROM LOGDATA WHERE "
        "(name LIKE ? OR type LIKE ? OR data LIKE ?) AND "
        "time >= UNIX_TIMESTAMP() - ? AND "
        "world = ? AND "
        "pos_x >= ? AND pos_x <= ? AND "
        "pos_y >= ? AND pos_y <= ? AND "
        "pos_z >= ? AND pos_z <= ? AND "
        "(POW(pos_x - ?, 2) + POW(pos_y - ?, 2) + POW(pos_z - ?, 2)) <= ? "
        "ORDER BY time LIMIT 100000";

    const std::vector params = {
        "%" + key.first + "%",
        "%" + key.first + "%",
        "%" + key.first + "%",
        std::to_string(static_cast<long long>(key.second * 3600)),
        world,
        std::to_string(x - r), std::to_string(x + r),
        std::to_string(y - r), std::to_string(y + r),
        std::to_string(z - r), std::to_string(z + r),
        std::to_string(x), std::to_string(y), std::to_string(z),
        std::to_string(r * r)
    };

    if (cancel) {
        return queryToMapsWithCancel(sql, result, params, cancel);
    }
    return queryToMaps(sql, result, params);
}

bool RustBackend::updateStatusesByUUIDs(
    const std::vector<std::pair<std::string, std::string>>& pairs)
{
    if (pairs.empty()) return true;
    if (!handle_) return false;

    rust_mysql_execute(handle_, "START TRANSACTION");

    for (const auto& [uuid, status] : pairs) {
        std::vector c_params = {status.c_str(), uuid.c_str()};
        if (rust_mysql_execute_params(
                handle_,
                "UPDATE LOGDATA SET status = ? WHERE uuid = ?",
                c_params.data(), 2) < 0) {
            rust_mysql_execute(handle_, "ROLLBACK");
            return false;
        }
    }

    rust_mysql_execute(handle_, "COMMIT");
    return true;
}

int64_t RustBackend::getCleanCount(const long long timestamp) {
    if (!handle_) return -1;

    std::vector<std::map<std::string, std::string>> result;
    const int rc = queryToMaps(
        "SELECT COUNT(*) AS cnt FROM LOGDATA WHERE time < ?",
        result, {std::to_string(timestamp)});

    if (rc != 0 || result.empty()) return -1;
    const auto it = result[0].find("cnt");
    if (it == result[0].end()) return -1;
    try {
        return std::stoll(it->second);
    } catch (...) {
        return -1;
    }
}

int RustBackend::deleteBatch(const long long timestamp, const int limit) {
    if (limit <= 0) return 0;
    if (!handle_) return -1;

    // 自增主键 + time 索引，LIMIT 直接走索引范围扫描无需排序
    const std::string ts_str = std::to_string(timestamp);
    const std::string lim_str = std::to_string(limit);

    const std::vector c_params = {ts_str.c_str(), lim_str.c_str()};

    const int64_t affected = rust_mysql_execute_params(
        handle_,
        "DELETE FROM LOGDATA WHERE time < ? LIMIT ?",
        c_params.data(),
        static_cast<int32_t>(c_params.size()));

    if (affected < 0) {
        std::cerr << "[Tianyan][RustMySQL] deleteBatch failed: "
                  << lastError() << std::endl;
        return -1;
    }
    return static_cast<int>(affected);
}

int64_t RustBackend::cleanupByRebuild(const long long threshold) {
    if (!handle_) return -1;

    // Step 1: 创建新表（同结构，清空旧残留）
    rust_mysql_execute(handle_, "DROP TABLE IF EXISTS LOGDATA_NEW");

    // 直接用完整建表语句创建新表，不走 LIKE 复制
    const char* new_table_sql =
        "CREATE TABLE LOGDATA_NEW ("
        "id_pk    BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        "uuid     VARCHAR(36)  NOT NULL, "
        "id       VARCHAR(255) DEFAULT NULL, "
        "name     VARCHAR(255) DEFAULT NULL, "
        "pos_x    DOUBLE       DEFAULT NULL, "
        "pos_y    DOUBLE       DEFAULT NULL, "
        "pos_z    DOUBLE       DEFAULT NULL, "
        "world    VARCHAR(255) DEFAULT NULL, "
        "obj_id   VARCHAR(255) DEFAULT NULL, "
        "obj_name VARCHAR(255) DEFAULT NULL, "
        "time     BIGINT       DEFAULT NULL, "
        "type     VARCHAR(255) DEFAULT NULL, "
        "data     TEXT         DEFAULT NULL, "
        "status   VARCHAR(255) DEFAULT NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci";
    if (rust_mysql_execute(handle_, new_table_sql) < 0) {
        std::cerr << "[Tianyan][RustMySQL] rebuild: create table failed" << std::endl;
        return -1;
    }

    // Step 2: bulk insert 优化
    rust_mysql_execute(handle_,
        "SET SESSION unique_checks=0, foreign_key_checks=0, "
        "innodb_flush_log_at_trx_commit=2");

    // Step 3: INSERT SELECT 保留需要的数据（含 id_pk 保持 auto_increment 连续）
    const std::string insert_sql =
        "INSERT INTO LOGDATA_NEW "
        "(id_pk, uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, "
        "time, type, data, status) "
        "SELECT id_pk, uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, "
        "time, type, data, status "
        "FROM LOGDATA WHERE time >= " + std::to_string(threshold);

    const int64_t kept = rust_mysql_execute(handle_, insert_sql.c_str());
    if (kept < 0) {
        std::cerr << "[Tianyan][RustMySQL] rebuild: INSERT SELECT failed" << std::endl;
        rust_mysql_execute(handle_,
            "SET SESSION unique_checks=1, foreign_key_checks=1, "
            "innodb_flush_log_at_trx_commit=1");
        rust_mysql_execute(handle_, "DROP TABLE IF EXISTS LOGDATA_NEW");
        return -1;
    }

    // Step 4: 重建索引（一次 ALTER 批量构建，比逐行维护快 10-50 倍）
    if (rust_mysql_execute(handle_,
            "ALTER TABLE LOGDATA_NEW "
            "ADD UNIQUE KEY uk_uuid (uuid), "
            "ADD KEY idx_logdata_time (time), "
            "ADD KEY idx_logdata_pos (pos_x, pos_y, pos_z)") < 0) {
        std::cerr << "[Tianyan][RustMySQL] rebuild: add index failed" << std::endl;
        rust_mysql_execute(handle_,
            "SET SESSION unique_checks=1, foreign_key_checks=1, "
            "innodb_flush_log_at_trx_commit=1");
        rust_mysql_execute(handle_, "DROP TABLE IF EXISTS LOGDATA_NEW");
        return -1;
    }

    // 恢复 session 变量
    rust_mysql_execute(handle_,
        "SET SESSION unique_checks=1, foreign_key_checks=1, "
        "innodb_flush_log_at_trx_commit=1");

    // Step 5: 原子交换表名（DDL，< 1ms）
    if (rust_mysql_execute(handle_,
            "RENAME TABLE LOGDATA TO LOGDATA_OLD, "
            "LOGDATA_NEW TO LOGDATA") < 0) {
        std::cerr << "[Tianyan][RustMySQL] rebuild: RENAME failed" << std::endl;
        rust_mysql_execute(handle_, "DROP TABLE IF EXISTS LOGDATA_NEW");
        return -1;
    }

    // Step 6: 删旧表
    rust_mysql_execute(handle_, "DROP TABLE LOGDATA_OLD");

    std::cerr << "[Tianyan][RustMySQL] rebuild complete, kept: "
              << kept << " rows" << std::endl;
    return kept;
}

std::string RustBackend::generateUuid() {
    char* uuid_cstr = rust_mysql_generate_uuid();
    if (!uuid_cstr) return "";
    std::string result(uuid_cstr);
    rust_mysql_free_string(uuid_cstr);
    return result;
}

int RustBackend::executeSQL(const std::string& sql) {
    if (!handle_) return -1;
    const int64_t affected = rust_mysql_execute(handle_, sql.c_str());
    return affected >= 0 ? 0 : -1;
}

int RustBackend::querySQL(
    const std::string& sql,
    std::vector<std::map<std::string, std::string>>& result)
{
    return queryToMaps(sql, result);
}

int RustBackend::updateSQL(
    const std::string& table,
    const std::string& set_clause,
    const std::string& where_clause)
{
    if (!handle_) return -1;
    const std::string sql =
        "UPDATE " + table + " SET " + set_clause + " WHERE " + where_clause;
    const int64_t affected = rust_mysql_execute(handle_, sql.c_str());
    return affected >= 0 ? 0 : -1;
}

bool RustBackend::isValueExists(
    const std::string& tableName,
    const std::string& columnName,
    const std::string& value)
{
    std::vector<std::map<std::string, std::string>> result;
    const int rc = queryToMaps(
        "SELECT COUNT(*) AS cnt FROM " + tableName +
        " WHERE " + columnName + " = ?",
        result, {value});

    if (rc != 0 || result.empty()) return false;
    const auto it = result[0].find("cnt");
    if (it == result[0].end()) return false;
    return std::stoi(it->second) > 0;
}

bool RustBackend::updateValue(
    const std::string& tableName,
    const std::string& targetColumn,
    const std::string& newValue,
    const std::string& conditionColumn,
    const std::string& conditionValue)
{
    if (!handle_) return false;

    const std::string sql = "UPDATE " + tableName +
                            " SET " + targetColumn + " = ?" +
                            " WHERE " + conditionColumn + " = ?";

    const std::vector c_params = {newValue.c_str(), conditionValue.c_str()};
    const int64_t affected = rust_mysql_execute_params(
        handle_, sql.c_str(), c_params.data(), 2);
    return affected > 0;
}

bool RustBackend::updateStatusByUUID(
    const std::string& uuid,
    const std::string& newStatus)
{
    if (!handle_) return false;

    const std::vector c_params = {newStatus.c_str(), uuid.c_str()};
    const int64_t affected = rust_mysql_execute_params(
        handle_,
        "UPDATE LOGDATA SET status = ? WHERE uuid = ?",
        c_params.data(), 2);
    return affected >= 0;
}

int RustBackend::getAllLog(
    std::vector<std::map<std::string, std::string>>& result)
{
    return queryToMaps("SELECT * FROM LOGDATA", result);
}
