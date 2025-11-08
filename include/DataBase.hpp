//
// Created by yuhang on 2025/3/25.
//

#ifndef TIANYAN_DATABASE_H
#define TIANYAN_DATABASE_H

#include <iostream>
#include <sqlite3.h>
#include <fstream>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <map>
#include <random>
#include <chrono>

//数据库清理输出语句缓存
inline std::vector<std::string> clean_data_message;
inline int clean_data_status;//0:未开始 1:成功 -1:失败

class DataBase {
public:
    // 构造时需要指定数据库文件名
    explicit DataBase(std::string  db_filename) : db_filename(std::move(db_filename)) {}


    // 函数用于检查文件是否存在
    static bool fileExists(const std::string& filename) {
        std::ifstream f(filename.c_str());
        return f.good();
    }

    // 初始化数据库
    [[nodiscard]] int init_database() const {
        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db);
        if (rc) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
            return rc;
        }

        // 创建 LOGDATA 表，添加uuid字段作为主键
        const std::string create_logdata_table = "CREATE TABLE IF NOT EXISTS LOGDATA ("
                                        "uuid TEXT PRIMARY KEY, "
                                        "id TEXT, "
                                        "name TEXT, "
                                        "pos_x REAL, pos_y REAL, pos_z REAL, "
                                        "world TEXT, "
                                        "obj_id TEXT, "
                                        "obj_name TEXT, "
                                        "time INTEGER, "
                                        "type TEXT, "
                                        "data TEXT, "
                                        "status TEXT)";
        rc = sqlite3_exec(db, create_logdata_table.c_str(), nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "创建 logdata 表失败: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return rc;
        }

        sqlite3_close(db);
        return SQLITE_OK;
    }

    ///////////////////// 通用操作 /////////////////////

    // 通用执行 SQL 命令（用于添加、删除、修改操作）
    [[nodiscard]] int executeSQL(const std::string &sql) const {
        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db);
        if (rc) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
            return rc;
        }
        rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL 执行失败: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_close(db);
        return rc;
    }

// 通用查询 SQL（select查询使用回调函数返回结果为 vector<map<string, string>>）
    static int queryCallback(void* data, int argc, char** argv, char** azColName) {
        auto* result = static_cast<std::vector<std::map<std::string, std::string>>*>(data);
        std::map<std::string, std::string> row;
        for (int i = 0; i < argc; i++) {
            row[azColName[i]] = argv[i] ? argv[i] : "NULL";
        }
        result->push_back(row);
        return 0;
    }

    int querySQL(const std::string &sql, std::vector<std::map<std::string, std::string>> &result) const {
        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db);
        if (rc) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
            return rc;
        }
        rc = sqlite3_exec(db, sql.c_str(), queryCallback, &result, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL 查询失败: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_close(db);
        return rc;
    }

// 批量查询 SQL（select查询使用回调函数返回结果为 vector<map<string, string>>）
    static int queryCallback_many_dict(void* data, int argc, char** argv, char** azColName) {
        auto* result = static_cast<std::vector<std::map<std::string, std::string>>*>(data);
        std::map<std::string, std::string> row;
        for (int i = 0; i < argc; i++) {
            row[azColName[i]] = argv[i] ? argv[i] : "NULL";
        }
        result->push_back(row);
        return 0;
    }

    int querySQL_many(const std::string &sql, std::vector<std::map<std::string, std::string>> &result) const {
        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db);
        if (rc) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
            return rc;
        }
        rc = sqlite3_exec(db, sql.c_str(), queryCallback_many_dict, &result, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL 查询失败: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_close(db);
        return rc;
    }

    [[nodiscard]] int updateSQL(const std::string &table, const std::string &set_clause, const std::string &where_clause) const {
        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db);
        if (rc) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
            return rc;
        }

        const std::string sql = "UPDATE " + table + " SET " + set_clause + " WHERE " + where_clause + ";";
        rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);

        if (rc != SQLITE_OK) {
            std::cerr << "SQL 更新失败: " << sqlite3_errmsg(db) << std::endl;
        }

        sqlite3_close(db);
        return rc;
    }

    // 清理数据库中超过指定时间的数据
    [[nodiscard]] bool cleanDataBase(double hours) const {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db);
        if (rc) {
            std::ostringstream ss;
            ss << "Can not open database: " << sqlite3_errmsg(db);
            clean_data_message.push_back(ss.str());
            clean_data_status = -1;
            return false;
        }

        // 获取当前时间戳（秒）
        const long long currentTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // 计算时间阈值（秒）
        const long long timeThreshold = currentTime - static_cast<long long>(hours * 3600);

        // 查询将要删除的记录数
        const std::string countSql = "SELECT COUNT(*) FROM LOGDATA WHERE time < " + std::to_string(timeThreshold) + ";";
        sqlite3_stmt* countStmt;
        rc = sqlite3_prepare_v2(db, countSql.c_str(), -1, &countStmt, nullptr);
        if (rc != SQLITE_OK) {
            std::ostringstream ss;
            ss << "SQL 预处理失败: " << sqlite3_errmsg(db);
            clean_data_message.push_back(ss.str());
            sqlite3_close(db);
            clean_data_status = -1;
            return false;
        }

        int deletedCount = 0;
        if (sqlite3_step(countStmt) == SQLITE_ROW) {
            deletedCount = sqlite3_column_int(countStmt, 0);
        }
        sqlite3_finalize(countStmt);

        // 开始事务以提高性能
        rc = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            std::ostringstream ss;
            ss << "Can not begin transaction: " << sqlite3_errmsg(db);
            clean_data_message.push_back(ss.str());
            sqlite3_close(db);
            clean_data_status = -1;
            return false;
        }

        // 删除超过指定时间的数据
        const std::string deleteSql = "DELETE FROM LOGDATA WHERE time < " + std::to_string(timeThreshold) + ";";
        rc = sqlite3_exec(db, deleteSql.c_str(), nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            std::ostringstream ss;
            ss << "SQL delete failed: " << sqlite3_errmsg(db);
            clean_data_message.push_back(ss.str());
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            clean_data_status = -1;
            return false;
        }

        // 提交事务
        rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            std::ostringstream ss;
            ss << "Can not commit: " << sqlite3_errmsg(db);
            clean_data_message.push_back(ss.str());
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            clean_data_status = -1;
            return false;
        }

        // 整理数据库以释放空间
        rc = sqlite3_exec(db, "VACUUM;", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            std::ostringstream ss;
            ss << "Database vacuum failed: " << sqlite3_errmsg(db);
            clean_data_message.push_back(ss.str());
            sqlite3_close(db);
            clean_data_status = -1;
            return false;
        }

        // 计算耗时
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();

        // 添加耗时和清理日志数信息
        clean_data_message.emplace_back("Time elapsed: ");
        clean_data_message.emplace_back(std::to_string(seconds));
        clean_data_message.emplace_back("Number of cleaned logs: ");
        clean_data_message.emplace_back(std::to_string(deletedCount));

        sqlite3_close(db);
        clean_data_status = 1;
        return true;
    }

    // 查找指定表中是否存在指定值
    [[nodiscard]] bool isValueExists(const std::string &tableName, const std::string &columnName, const std::string &value) const {
        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db); // 打开数据库
        if (rc) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        std::string sql;

        // 构造 SQL 查询语句
        sql = "SELECT COUNT(*) FROM " + tableName + " WHERE " + columnName + " = ?;";

        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL 预处理失败: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return false;
        }

        // 绑定参数
        sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_STATIC);

        // 执行查询并获取结果
        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0); // 获取 COUNT(*) 的结果
            exists = (count > 0);
        }

        // 清理资源
        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return exists;
    }

    // 修改指定表中的指定数据的指定值
    [[nodiscard]] bool updateValue(const std::string &tableName,
                     const std::string &targetColumn,
                     const std::string &newValue,
                     const std::string &conditionColumn,
                     const std::string &conditionValue) const {
        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db); // 打开数据库
        if (rc) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        std::string sql;
            // 构造 SQL 更新语句
            sql = "UPDATE " + tableName +
                  " SET " + targetColumn + " = ?" +
                  " WHERE " + conditionColumn + " = ?;";

        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL 预处理失败: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return false;
        }

        // 绑定参数
        sqlite3_bind_text(stmt, 1, newValue.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, conditionValue.c_str(), -1, SQLITE_STATIC);

        // 执行 SQL 语句
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "SQL 更新失败: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }

        // 清理资源
        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return true;
    }

    // 根据UUID更新指定记录的状态
    [[nodiscard]] bool updateStatusByUUID(const std::string &uuid, const std::string &newStatus) const {
        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db); // 打开数据库
        if (rc) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        const std::string sql = "UPDATE LOGDATA SET status = ? WHERE uuid = ?;";
        
        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL 预处理失败: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return false;
        }

        // 绑定参数
        sqlite3_bind_text(stmt, 1, newStatus.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, uuid.c_str(), -1, SQLITE_STATIC);

        // 执行 SQL 语句
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "SQL 更新失败: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }

        // 清理资源
        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return true;
    }

    // 批量更新状态
    [[nodiscard]] bool updateStatusesByUUIDs(const std::vector<std::pair<std::string, std::string>>& uuidStatusPairs) const {
        if (uuidStatusPairs.empty()) {
            return true;
        }

        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db);
        if (rc) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        // 开始事务以提高性能
        rc = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "无法开始事务: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return false;
        }

        const std::string sql = "UPDATE LOGDATA SET status = ? WHERE uuid = ?;";
        
        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL 预处理失败: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return false;
        }

        // 遍历所有UUID和状态对并更新
        for (const auto& [uuid, status] : uuidStatusPairs) {
            // 绑定参数
            sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, uuid.c_str(), -1, SQLITE_STATIC);

            // 执行 SQL 语句
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                std::cerr << "SQL 更新失败: " << sqlite3_errmsg(db) << std::endl;
                sqlite3_finalize(stmt);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_close(db);
                return false;
            }
            
            // 重置语句以供下次使用
            sqlite3_reset(stmt);
        }
        
        // 提交事务
        rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "无法提交事务: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        }

        // 清理资源
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return true;
    }

    ///////////////////// LOGDATA 表操作 /////////////////////

    [[nodiscard]] int addLog(const std::string& uuid,
                             const std::string& id,
                             const std::string& name,
                             const double pos_x, const double pos_y, const double pos_z,
                             const std::string& world,
                             const std::string& obj_id,
                             const std::string& obj_name,
                             const long long time,
                             const std::string& type,
                             const std::string& data,
                             const std::string& status) const {
        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db);
        if (rc) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
            return rc;
        }

        const std::string sql = "INSERT INTO LOGDATA (uuid, id, name, pos_x, pos_y, pos_z, "
                          "world, obj_id, obj_name, time, type, data, status) VALUES (?, ?, "
                          "?, ?, ?, ?, "
                          "?, ?, ?, ?, ?, ?, ?);";

        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL 预处理失败: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return rc;
        }

        // 绑定参数
        sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, pos_x);
        sqlite3_bind_double(stmt, 5, pos_y);
        sqlite3_bind_double(stmt, 6, pos_z);
        sqlite3_bind_text(stmt, 7, world.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, obj_id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 9, obj_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 10, time);
        sqlite3_bind_text(stmt, 11, type.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 12, data.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 13, status.c_str(), -1, SQLITE_STATIC);
        
        // 执行 SQL 语句
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "SQL 插入失败: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return rc;
        }
        
        // 清理资源
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return SQLITE_OK;
    }

    // 批量插入日志数据
    [[nodiscard]] int addLogs(const std::vector<std::tuple<std::string, std::string, std::string, double, double, double, 
                             std::string, std::string, std::string, long long, std::string, std::string, std::string>>& logs) const {
        if (logs.empty()) {
            return SQLITE_OK; // 空数据直接返回成功
        }

        sqlite3* db;
        int rc = sqlite3_open(db_filename.c_str(), &db);
        if (rc) {
            std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
            return rc;
        }

        // 开始事务以提高性能
        rc = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "无法开始事务: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return rc;
        }

        const std::string sql = "INSERT INTO LOGDATA (uuid, id, name, pos_x, pos_y, pos_z, "
                          "world, obj_id, obj_name, time, type, data, status) VALUES (?, ?, "
                          "?, ?, ?, ?, "
                          "?, ?, ?, ?, ?, ?, ?);";

        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL 预处理失败: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return rc;
        }

        // 遍历所有日志数据并插入
        for (const auto& log : logs) {
            const auto& [uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, time, type, data, status] = log;

            // 绑定参数
            sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, 4, pos_x);
            sqlite3_bind_double(stmt, 5, pos_y);
            sqlite3_bind_double(stmt, 6, pos_z);
            sqlite3_bind_text(stmt, 7, world.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 8, obj_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 9, obj_name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 10, time);
            sqlite3_bind_text(stmt, 11, type.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 12, data.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 13, status.c_str(), -1, SQLITE_STATIC);
            
            // 执行 SQL 语句
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                std::cerr << "SQL 插入失败: " << sqlite3_errmsg(db) << std::endl;
                sqlite3_finalize(stmt);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_close(db);
                return rc;
            }
            
            // 重置语句以供下次使用
            sqlite3_reset(stmt);
        }
        
        // 提交事务
        rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "无法提交事务: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        }

        // 清理资源
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return rc;
    }

    int getAllLog(std::vector<std::map<std::string, std::string>> &result) const {
        // 获取所有数据
        const std::string sql = "SELECT * FROM LOGDATA;";

        // 调用 querySQL_many 函数执行查询，并将结果存储到 result 中
        return querySQL_many(sql, result);
    }

    int searchLog(std::vector<std::map<std::string, std::string>> &result, 
                  const std::pair<std::string, double>& searchCriteria) const {
        // 获取当前时间戳（秒）
        const long long currentTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // 计算时间范围（秒），支持小数小时（如0.5表示半小时）
        const long long timeThreshold = currentTime - static_cast<long long>(searchCriteria.second * 3600);
        
        // 构建SQL查询语句，搜索关键词匹配任意字段并在时间范围内
        const std::string sql = "SELECT * FROM LOGDATA WHERE (name LIKE '%" + searchCriteria.first +
                         "%' OR type LIKE '%" + searchCriteria.first + 
                         "%' OR data LIKE '%" + searchCriteria.first + 
                         "%') AND time >= " + std::to_string(timeThreshold) + 
                         " LIMIT 5001;";

        // 调用 querySQL_many 函数执行查询，并将结果存储到 result 中
        return querySQL_many(sql, result);
    }

    // 新增带坐标和世界过滤的搜索函数
    int searchLog(std::vector<std::map<std::string, std::string>> &result,
                  const std::pair<std::string, double>& searchCriteria,
                  const double x, const double y, const double z, const double r, const std::string& world) const {
        // 获取当前时间戳（秒）
        const long long currentTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // 计算时间范围（秒），支持小数小时（如0.5表示半小时）
        const long long timeThreshold = currentTime - static_cast<long long>(searchCriteria.second * 3600);
        
        // 构建SQL查询语句，包含关键词匹配、时间范围以及坐标范围过滤
        const std::string sql = "SELECT * FROM LOGDATA WHERE "
                               "(name LIKE '%" + searchCriteria.first +
                               "%' OR type LIKE '%" + searchCriteria.first + 
                               "%' OR data LIKE '%" + searchCriteria.first + 
                               "%') AND time >= " + std::to_string(timeThreshold) +
                               " AND world = '" + world + "'" +
                               " AND ((pos_x - " + std::to_string(x) + ")*(pos_x - " + std::to_string(x) + ")" +
                               " + (pos_y - " + std::to_string(y) + ")*(pos_y - " + std::to_string(y) + ")" +
                               " + (pos_z - " + std::to_string(z) + ")*(pos_z - " + std::to_string(z) + ")" +
                               ") <= " + std::to_string(r*r) + 
                               " LIMIT 5001;";
                               
        // 调用 querySQL_many 函数执行查询，并将结果存储到 result 中
        return querySQL_many(sql, result);
    }

    //数据库工具

    //将逗号字符串分割为vector
    static std::vector<std::string> splitString(const std::string& input) {
        std::vector<std::string> result; // 用于存储分割后的结果
        std::string current;             // 当前正在构建的子字符串
        bool inBraces = false;           // 标记是否在 {} 内部
        bool inBrackets = false;         // 标记是否在 [] 内部

        for (char ch : input) {
            if (ch == '{') {
                // 遇到左大括号，标记进入 {} 内部
                inBraces = true;
                current += ch; // 将 { 添加到当前字符串
            } else if (ch == '}') {
                // 遇到右大括号，标记离开 {} 内部
                inBraces = false;
                current += ch; // 将 } 添加到当前字符串
            } else if (ch == '[') {
                // 遇到左方括号，标记进入 [] 内部
                inBrackets = true;
                current += ch; // 将 [ 添加到当前字符串
            } else if (ch == ']') {
                // 遇到右方括号，标记离开 [] 内部
                inBrackets = false;
                current += ch; // 将 ] 添加到当前字符串
            } else if (ch == ',' && !inBraces && !inBrackets) {
                // 遇到逗号且不在 {} 和 [] 内部时，分割字符串
                if (!current.empty()) {
                    result.push_back(current);
                    current.clear();
                }
            } else {
                // 其他情况，将字符添加到当前字符串
                current += ch;
            }
        }

        // 处理最后一个子字符串（如果非空）
        if (!current.empty()) {
            result.push_back(current);
        }

        return result;
    }

    //将数字字符逗号分割为vector
    static std::vector<int> splitStringInt(const std::string& input) {
        std::vector<int> result; // 用于存储分割后的结果
        std::stringstream ss(input);     // 使用 string stream 处理输入字符串
        std::string item;

        // 按逗号分割字符串
        while (std::getline(ss, item, ',')) {
            if (!item.empty()) {         // 如果分割出的元素非空，则添加到结果中
                std::stringstream sss(item);
                int groupid = 0;
                if (!(sss >> groupid)) {
                    return {}; // 如果解析失败，返回空
                }
                result.push_back(groupid);
            }
        }

        // 如果没有逗号且结果为空则返回空
        if (result.empty()) {
            return {};
        }

        return result;
    }

    //将vector(string)通过逗号分隔改为字符串
    static std::string vectorToString(const std::vector<std::string>& vec) {
        if (vec.empty()) {
            return ""; // 如果向量为空，返回空字符串
        }

        std::ostringstream oss; // 使用字符串流拼接结果
        for (size_t i = 0; i < vec.size(); ++i) {
            oss << vec[i]; // 添加当前元素
            if (i != vec.size() - 1 && !(vec[i].empty())) { // 如果不是最后一个元素且不为空，添加逗号
                oss << ",";
            }
        }

        return oss.str(); // 返回拼接后的字符串
    }

    //将vector(int)通过逗号分隔改为字符串
    static std::string IntVectorToString(const std::vector<int>& vec) {
        if (vec.empty()) {
            return ""; // 如果向量为空，返回空字符串
        }

        std::ostringstream oss; // 使用字符串流拼接结果
        for (size_t i = 0; i < vec.size(); ++i) {
            oss << vec[i]; // 添加当前元素
            if (i != vec.size() - 1) { // 如果不是最后一个元素，添加逗号
                oss << ",";
            }
        }

        return oss.str(); // 返回拼接后的字符串
    }

    //字符串改整数
    static int stringToInt(const std::string& str) {
        try {
            // 尝试将字符串转换为整数
            return std::stoi(str);
        } catch (const std::invalid_argument&) {
            // 捕获无效参数异常（例如无法解析为整数）
            return 0;
        } catch (const std::out_of_range&) {
            // 捕获超出范围异常（例如数值过大或过小）
            return 0;
        }
    }

    // 生成一个符合 RFC 4122 标准的 UUID v4
    static std::string generate_uuid_v4() {
        static thread_local std::mt19937 gen{std::random_device{}()};

        std::uniform_int_distribution<int> dis(0, 15);

        std::stringstream ss;
        ss << std::hex; // 设置为十六进制输出

        for (int i = 0; i < 8; ++i) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; ++i) ss << dis(gen);
        ss << "-";

        ss << "4"; // 版本号为 4
        for (int i = 0; i < 3; ++i) ss << dis(gen);
        ss << "-";

        ss << (dis(gen) & 0x3 | 0x8);
        for (int i = 0; i < 3; ++i) ss << dis(gen);
        ss << "-";

        for (int i = 0; i < 12; ++i) ss << dis(gen);

        return ss.str();
    }

private:
    std::string db_filename;
};

#endif // TIANYAN_DATABASE_H