//
// Created by yuhang on 2025/3/25.
//

#ifndef TIANYAN_DATABASE_H
#define TIANYAN_DATABASE_H

#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if __has_include(<mysql/mysql.h>)
#include <mysql/mysql.h>
#elif __has_include(<mariadb/mysql.h>)
#include <mariadb/mysql.h>
#elif __has_include(<mysql.h>)
#include <mysql.h>
#else
#error "MySQL client headers not found. Install libmysqlclient or compatible MariaDB client headers."
#endif

// 数据库清理输出语句缓存
inline std::vector<std::string> clean_data_message;
inline int clean_data_status;  // 0:未开始 1:成功 -1:失败 2:进行中

namespace yuhangle {
    constexpr int DB_OK = 0;
    constexpr int DB_ERROR = 1;

    struct DatabaseConfig {
        std::string host{"127.0.0.1"};
        unsigned int port{3306};
        std::string user{"root"};
        std::string password{"root"};
        std::string database{"tianyan"};

        [[nodiscard]] std::string key() const {
            return host + ":" + std::to_string(port) + ":" + user + ":" + database;
        }
    };

    class DatabaseConnection {
    public:
        explicit DatabaseConnection(DatabaseConfig config)
            : config_(std::move(config)), db_(nullptr) {
        }

        ~DatabaseConnection() {
            if (db_ != nullptr) {
                mysql_close(db_);
                db_ = nullptr;
            }
        }

        bool open() {
            if (db_ != nullptr) {
                return true;
            }

            db_ = mysql_init(nullptr);
            if (db_ == nullptr) {
                return false;
            }

            bool reconnect = true;
            mysql_options(db_, MYSQL_OPT_RECONNECT, &reconnect);

            if (mysql_real_connect(
                db_,
                config_.host.c_str(),
                config_.user.c_str(),
                config_.password.c_str(),
                config_.database.c_str(),
                config_.port,
                nullptr,
                0
            ) == nullptr) {
                std::cerr << "MySQL connect failed: " << mysql_error(db_) << std::endl;
                mysql_close(db_);
                db_ = nullptr;
                return false;
            }

            if (mysql_set_character_set(db_, "utf8mb4") != 0) {
                std::cerr << "MySQL set charset failed: " << mysql_error(db_) << std::endl;
            }

            return true;
        }

        [[nodiscard]] MYSQL* get() const { return db_; }

    private:
        DatabaseConfig config_;
        MYSQL* db_;
    };

    class ConnectionPool {
    public:
        static ConnectionPool& getInstance(const DatabaseConfig& config) {
            static std::map<std::string, std::shared_ptr<ConnectionPool>> instances;
            static std::mutex instances_mutex;

            std::lock_guard<std::mutex> lock(instances_mutex);
            const auto key = config.key();
            const auto it = instances.find(key);
            if (it == instances.end()) {
                const auto pool = std::make_shared<ConnectionPool>(config);
                instances[key] = pool;
                return *pool;
            }
            return *(it->second);
        }

        explicit ConnectionPool(DatabaseConfig config, const size_t pool_size = 5)
            : config_(std::move(config)), pool_size_(pool_size) {
            initializePool();
        }

        std::shared_ptr<DatabaseConnection> getConnection() {
            std::unique_lock<std::mutex> lock(pool_mutex_);

            while (true) {
                if (!connections_.empty()) {
                    auto conn = connections_.front();
                    connections_.pop();
                    return conn;
                }

                if (current_size_ < pool_size_) {
                    if (auto conn = createConnection()) {
                        current_size_++;
                        return conn;
                    }
                    throw std::runtime_error("Failed to create MySQL connection.");
                }

                condition_.wait(lock);
            }
        }

        void returnConnection(const std::shared_ptr<DatabaseConnection>& conn) {
            if (conn == nullptr) {
                return;
            }
            std::lock_guard<std::mutex> lock(pool_mutex_);
            connections_.push(conn);
            condition_.notify_one();
        }

    private:
        void initializePool() {
            for (size_t i = 0; i < pool_size_; ++i) {
                if (auto conn = createConnection()) {
                    connections_.push(conn);
                    current_size_++;
                }
            }
        }

        std::shared_ptr<DatabaseConnection> createConnection() {
            auto conn = std::make_shared<DatabaseConnection>(config_);
            if (conn->open()) {
                return conn;
            }
            return nullptr;
        }

        DatabaseConfig config_;
        size_t pool_size_;
        size_t current_size_ = 0;
        std::queue<std::shared_ptr<DatabaseConnection>> connections_;
        std::mutex pool_mutex_;
        std::condition_variable condition_;
    };

    class Database {
    public:
        explicit Database(DatabaseConfig config)
            : config_(std::move(config)) {
        }

        static bool fileExists(const std::string& filename) {
            const std::ifstream f(filename.c_str());
            return f.good();
        }

        [[nodiscard]] int init_database() const {
            if (!ensureDatabaseExists()) {
                return DB_ERROR;
            }

            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                std::cerr << "Get MySQL connection failed: " << e.what() << std::endl;
                return DB_ERROR;
            }
            MYSQL* db = conn->get();

            const std::string create_logdata_table =
                "CREATE TABLE IF NOT EXISTS LOGDATA ("
                "uuid VARCHAR(36) PRIMARY KEY, "
                "id TEXT, "
                "name TEXT, "
                "pos_x DOUBLE, pos_y DOUBLE, pos_z DOUBLE, "
                "world VARCHAR(64), "
                "obj_id TEXT, "
                "obj_name TEXT, "
                "time BIGINT, "
                "type VARCHAR(64), "
                "data LONGTEXT, "
                "status VARCHAR(32)"
                ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

            if (mysql_query(db, create_logdata_table.c_str()) != 0) {
                std::cerr << "创建 logdata 表失败: " << mysql_error(db) << std::endl;
                pool.returnConnection(conn);
                return DB_ERROR;
            }

            createIndexIfNeeded(db, "idx_logdata_time", "time");
            createIndexIfNeeded(db, "idx_logdata_world", "world");
            createIndexIfNeeded(db, "idx_logdata_type", "type");
            createIndexIfNeeded(db, "idx_logdata_name", "name");

            pool.returnConnection(conn);
            return DB_OK;
        }

        [[nodiscard]] int executeSQL(const std::string& sql) const {
            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                std::cerr << "Get MySQL connection failed: " << e.what() << std::endl;
                return DB_ERROR;
            }

            const int rc = mysql_query(conn->get(), sql.c_str()) == 0 ? DB_OK : DB_ERROR;
            if (rc != DB_OK) {
                std::cerr << "SQL 执行失败: " << mysql_error(conn->get()) << std::endl;
            }

            pool.returnConnection(conn);
            return rc;
        }

        static int queryCallback(void* data, int argc, char** argv, char** azColName) {
            auto* result = static_cast<std::vector<std::map<std::string, std::string>>*>(data);
            std::map<std::string, std::string> row;
            for (int i = 0; i < argc; i++) {
                row[azColName[i]] = argv[i] ? argv[i] : "NULL";
            }
            result->push_back(row);
            return 0;
        }

        int querySQL(const std::string& sql, std::vector<std::map<std::string, std::string>>& result) const {
            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                std::cerr << "Get MySQL connection failed: " << e.what() << std::endl;
                return DB_ERROR;
            }

            MYSQL* db = conn->get();
            if (mysql_query(db, sql.c_str()) != 0) {
                std::cerr << "SQL 查询失败: " << mysql_error(db) << std::endl;
                pool.returnConnection(conn);
                return DB_ERROR;
            }

            const int fetch_rc = fetchResultToMaps(db, result);
            pool.returnConnection(conn);
            return fetch_rc;
        }

        [[nodiscard]] int updateSQL(const std::string& table, const std::string& set_clause,
                                    const std::string& where_clause) const {
            if (!isSafeIdentifier(table)) {
                std::cerr << "Invalid table identifier: " << table << std::endl;
                return DB_ERROR;
            }

            const std::string sql = "UPDATE " + table + " SET " + set_clause + " WHERE " + where_clause + ";";
            return executeSQL(sql);
        }

        [[nodiscard]] bool cleanDataBase(double hours) const {
            clean_data_status = 2;
            auto start_time = std::chrono::high_resolution_clock::now();

            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                clean_data_message.push_back(std::string("Get MySQL connection failed: ") + e.what());
                clean_data_status = -1;
                return false;
            }
            MYSQL* db = conn->get();

            const long long currentTime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const long long timeThreshold = currentTime - static_cast<long long>(hours * 3600);

            long long deletedCount = 0;
            const std::string countSql = "SELECT COUNT(*) FROM LOGDATA WHERE time < " + std::to_string(timeThreshold) + ";";
            if (!querySingleLongLong(db, countSql, deletedCount)) {
                clean_data_message.push_back(std::string("SQL 查询失败: ") + mysql_error(db));
                clean_data_status = -1;
                pool.returnConnection(conn);
                return false;
            }

            if (mysql_query(db, "START TRANSACTION;") != 0) {
                clean_data_message.push_back(std::string("Can not begin transaction: ") + mysql_error(db));
                clean_data_status = -1;
                pool.returnConnection(conn);
                return false;
            }

            const std::string deleteSql = "DELETE FROM LOGDATA WHERE time < " + std::to_string(timeThreshold) + ";";
            if (mysql_query(db, deleteSql.c_str()) != 0) {
                clean_data_message.push_back(std::string("SQL delete failed: ") + mysql_error(db));
                mysql_query(db, "ROLLBACK;");
                clean_data_status = -1;
                pool.returnConnection(conn);
                return false;
            }

            if (mysql_query(db, "COMMIT;") != 0) {
                clean_data_message.push_back(std::string("Can not commit: ") + mysql_error(db));
                mysql_query(db, "ROLLBACK;");
                clean_data_status = -1;
                pool.returnConnection(conn);
                return false;
            }

            if (mysql_query(db, "OPTIMIZE TABLE LOGDATA;") != 0) {
                clean_data_message.push_back(std::string("Database optimize failed: ") + mysql_error(db));
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();

            clean_data_message.emplace_back("Time elapsed: ");
            clean_data_message.emplace_back(std::to_string(seconds));
            clean_data_message.emplace_back("Number of cleaned logs: ");
            clean_data_message.emplace_back(std::to_string(deletedCount));

            clean_data_status = 1;
            pool.returnConnection(conn);
            return true;
        }

        [[nodiscard]] bool isValueExists(const std::string& tableName,
                                         const std::string& columnName,
                                         const std::string& value) const {
            if (!isSafeIdentifier(tableName) || !isSafeIdentifier(columnName)) {
                std::cerr << "Invalid SQL identifier." << std::endl;
                return false;
            }

            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                std::cerr << "Get MySQL connection failed: " << e.what() << std::endl;
                return false;
            }
            MYSQL* db = conn->get();

            const std::string sql = "SELECT COUNT(*) FROM " + tableName + " WHERE " + columnName + " = " +
                                    quoteString(db, value) + ";";

            long long count = 0;
            const bool ok = querySingleLongLong(db, sql, count);
            pool.returnConnection(conn);
            return ok && count > 0;
        }

        [[nodiscard]] bool updateValue(const std::string& tableName,
                                       const std::string& targetColumn,
                                       const std::string& newValue,
                                       const std::string& conditionColumn,
                                       const std::string& conditionValue) const {
            if (!isSafeIdentifier(tableName) || !isSafeIdentifier(targetColumn) || !isSafeIdentifier(conditionColumn)) {
                std::cerr << "Invalid SQL identifier." << std::endl;
                return false;
            }

            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                std::cerr << "Get MySQL connection failed: " << e.what() << std::endl;
                return false;
            }

            MYSQL* db = conn->get();
            const std::string sql = "UPDATE " + tableName +
                                    " SET " + targetColumn + " = " + quoteString(db, newValue) +
                                    " WHERE " + conditionColumn + " = " + quoteString(db, conditionValue) + ";";

            const bool ok = mysql_query(db, sql.c_str()) == 0;
            if (!ok) {
                std::cerr << "SQL 更新失败: " << mysql_error(db) << std::endl;
            }

            pool.returnConnection(conn);
            return ok;
        }

        [[nodiscard]] bool updateStatusByUUID(const std::string& uuid, const std::string& newStatus) const {
            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                std::cerr << "Get MySQL connection failed: " << e.what() << std::endl;
                return false;
            }

            MYSQL* db = conn->get();
            const std::string sql = "UPDATE LOGDATA SET status = " + quoteString(db, newStatus) +
                                    " WHERE uuid = " + quoteString(db, uuid) + ";";

            const bool ok = mysql_query(db, sql.c_str()) == 0;
            if (!ok) {
                std::cerr << "SQL 更新失败: " << mysql_error(db) << std::endl;
            }

            pool.returnConnection(conn);
            return ok;
        }

        [[nodiscard]] bool updateStatusesByUUIDs(
            const std::vector<std::pair<std::string, std::string>>& uuidStatusPairs) const {
            if (uuidStatusPairs.empty()) {
                return true;
            }

            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                std::cerr << "Get MySQL connection failed: " << e.what() << std::endl;
                return false;
            }
            MYSQL* db = conn->get();

            if (mysql_query(db, "START TRANSACTION;") != 0) {
                std::cerr << "无法开始事务: " << mysql_error(db) << std::endl;
                pool.returnConnection(conn);
                return false;
            }

            for (const auto& [uuid, status] : uuidStatusPairs) {
                const std::string sql = "UPDATE LOGDATA SET status = " + quoteString(db, status) +
                                        " WHERE uuid = " + quoteString(db, uuid) + ";";
                if (mysql_query(db, sql.c_str()) != 0) {
                    std::cerr << "SQL 更新失败: " << mysql_error(db) << std::endl;
                    mysql_query(db, "ROLLBACK;");
                    pool.returnConnection(conn);
                    return false;
                }
            }

            if (mysql_query(db, "COMMIT;") != 0) {
                std::cerr << "无法提交事务: " << mysql_error(db) << std::endl;
                mysql_query(db, "ROLLBACK;");
                pool.returnConnection(conn);
                return false;
            }

            pool.returnConnection(conn);
            return true;
        }

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
            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                std::cerr << "Get MySQL connection failed: " << e.what() << std::endl;
                return DB_ERROR;
            }
            MYSQL* db = conn->get();

            const std::string sql = buildInsertSQL(db, uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name,
                                                   time, type, data, status);
            const int rc = mysql_query(db, sql.c_str()) == 0 ? DB_OK : DB_ERROR;
            if (rc != DB_OK) {
                std::cerr << "SQL 插入失败: " << mysql_error(db) << std::endl;
            }

            pool.returnConnection(conn);
            return rc;
        }

        [[nodiscard]] int addLogs(const std::vector<std::tuple<std::string, std::string, std::string, double, double,
                                                                double, std::string, std::string, std::string,
                                                                long long, std::string, std::string,
                                                                std::string>>& logs) const {
            if (logs.empty()) {
                return DB_OK;
            }

            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                std::cerr << "Get MySQL connection failed: " << e.what() << std::endl;
                return DB_ERROR;
            }
            MYSQL* db = conn->get();

            if (mysql_query(db, "START TRANSACTION;") != 0) {
                std::cerr << "无法开始事务: " << mysql_error(db) << std::endl;
                pool.returnConnection(conn);
                return DB_ERROR;
            }

            for (const auto& log : logs) {
                const auto& [uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, time, type, data, status] = log;
                const std::string sql = buildInsertSQL(db, uuid, id, name, pos_x, pos_y, pos_z, world, obj_id,
                                                       obj_name, time, type, data, status);
                if (mysql_query(db, sql.c_str()) != 0) {
                    std::cerr << "SQL 插入失败: " << mysql_error(db) << std::endl;
                    mysql_query(db, "ROLLBACK;");
                    pool.returnConnection(conn);
                    return DB_ERROR;
                }
            }

            if (mysql_query(db, "COMMIT;") != 0) {
                std::cerr << "无法提交事务: " << mysql_error(db) << std::endl;
                mysql_query(db, "ROLLBACK;");
                pool.returnConnection(conn);
                return DB_ERROR;
            }

            pool.returnConnection(conn);
            return DB_OK;
        }

        int getAllLog(std::vector<std::map<std::string, std::string>>& result) const {
            const std::string sql = "SELECT * FROM LOGDATA;";
            return querySQL(sql, result);
        }

        int searchLog(std::vector<std::map<std::string, std::string>>& result,
                      const std::pair<std::string, double>& searchCriteria) const {
            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                std::cerr << "Get MySQL connection failed: " << e.what() << std::endl;
                return DB_ERROR;
            }
            MYSQL* db = conn->get();

            const long long currentTime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const long long timeThreshold = currentTime - static_cast<long long>(searchCriteria.second * 3600);

            const std::string searchPattern = "%" + escapeString(db, searchCriteria.first) + "%";
            const std::string sql =
                "SELECT * FROM LOGDATA WHERE "
                "(name LIKE " + quoteLiteral(searchPattern) +
                " OR type LIKE " + quoteLiteral(searchPattern) +
                " OR data LIKE " + quoteLiteral(searchPattern) +
                ") AND time >= " + std::to_string(timeThreshold) + " LIMIT 10000;";

            if (mysql_query(db, sql.c_str()) != 0) {
                std::cerr << "SQL 查询失败: " << mysql_error(db) << std::endl;
                pool.returnConnection(conn);
                return DB_ERROR;
            }

            const int rc = fetchResultToMaps(db, result);
            pool.returnConnection(conn);
            return rc;
        }

        int searchLog(std::vector<std::map<std::string, std::string>>& result,
                      const std::pair<std::string, double>& searchCriteria,
                      const double x, const double y, const double z, const double r,
                      const std::string& world,
                      bool if_max = false) const {
            auto& pool = ConnectionPool::getInstance(config_);
            std::shared_ptr<DatabaseConnection> conn;
            try {
                conn = pool.getConnection();
            } catch (const std::exception& e) {
                std::cerr << "Get MySQL connection failed: " << e.what() << std::endl;
                return DB_ERROR;
            }
            MYSQL* db = conn->get();

            const long long currentTime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const long long timeThreshold = currentTime - static_cast<long long>(searchCriteria.second * 3600);

            const std::string searchPattern = "%" + escapeString(db, searchCriteria.first) + "%";
            std::string sql =
                "SELECT * FROM LOGDATA WHERE "
                "(name LIKE " + quoteLiteral(searchPattern) +
                " OR type LIKE " + quoteLiteral(searchPattern) +
                " OR data LIKE " + quoteLiteral(searchPattern) +
                ") AND time >= " + std::to_string(timeThreshold) +
                " AND world = " + quoteString(db, world) +
                " AND ((pos_x - " + toSqlDouble(x) + ")*(pos_x - " + toSqlDouble(x) + ")"
                " + (pos_y - " + toSqlDouble(y) + ")*(pos_y - " + toSqlDouble(y) + ")"
                " + (pos_z - " + toSqlDouble(z) + ")*(pos_z - " + toSqlDouble(z) + ")) <= " +
                toSqlDouble(r * r);

            if (!if_max) {
                sql += " LIMIT 10000";
            }
            sql += ";";

            if (mysql_query(db, sql.c_str()) != 0) {
                std::cerr << "SQL 查询失败: " << mysql_error(db) << std::endl;
                pool.returnConnection(conn);
                return DB_ERROR;
            }

            const int rc = fetchResultToMaps(db, result);
            pool.returnConnection(conn);
            return rc;
        }

        static std::vector<std::string> splitString(const std::string& input) {
            std::vector<std::string> result;
            std::string current;
            bool inBraces = false;
            bool inBrackets = false;

            for (const char ch : input) {
                if (ch == '{') {
                    inBraces = true;
                    current += ch;
                } else if (ch == '}') {
                    inBraces = false;
                    current += ch;
                } else if (ch == '[') {
                    inBrackets = true;
                    current += ch;
                } else if (ch == ']') {
                    inBrackets = false;
                    current += ch;
                } else if (ch == ',' && !inBraces && !inBrackets) {
                    if (!current.empty()) {
                        result.push_back(current);
                        current.clear();
                    }
                } else {
                    current += ch;
                }
            }

            if (!current.empty()) {
                result.push_back(current);
            }

            return result;
        }

        static std::vector<int> splitStringInt(const std::string& input) {
            std::vector<int> result;
            std::stringstream ss(input);
            std::string item;

            while (std::getline(ss, item, ',')) {
                if (!item.empty()) {
                    std::stringstream sss(item);
                    int groupid = 0;
                    if (!(sss >> groupid)) {
                        return {};
                    }
                    result.push_back(groupid);
                }
            }

            if (result.empty()) {
                return {};
            }

            return result;
        }

        static std::string vectorToString(const std::vector<std::string>& vec) {
            if (vec.empty()) {
                return "";
            }

            std::ostringstream oss;
            for (size_t i = 0; i < vec.size(); ++i) {
                oss << vec[i];
                if (i != vec.size() - 1 && !vec[i].empty()) {
                    oss << ",";
                }
            }

            return oss.str();
        }

        static std::string IntVectorToString(const std::vector<int>& vec) {
            if (vec.empty()) {
                return "";
            }

            std::ostringstream oss;
            for (size_t i = 0; i < vec.size(); ++i) {
                oss << vec[i];
                if (i != vec.size() - 1) {
                    oss << ",";
                }
            }

            return oss.str();
        }

        static int stringToInt(const std::string& str) {
            try {
                return std::stoi(str);
            } catch (const std::invalid_argument&) {
                return 0;
            } catch (const std::out_of_range&) {
                return 0;
            }
        }

        static std::string generate_uuid_v4() {
            static thread_local std::mt19937 gen{std::random_device{}()};
            std::uniform_int_distribution<int> dis(0, 15);

            std::stringstream ss;
            ss << std::hex;

            for (int i = 0; i < 8; ++i) ss << dis(gen);
            ss << "-";
            for (int i = 0; i < 4; ++i) ss << dis(gen);
            ss << "-";
            ss << "4";
            for (int i = 0; i < 3; ++i) ss << dis(gen);
            ss << "-";
            ss << (dis(gen) & 0x3 | 0x8);
            for (int i = 0; i < 3; ++i) ss << dis(gen);
            ss << "-";
            for (int i = 0; i < 12; ++i) ss << dis(gen);

            return ss.str();
        }

    private:
        static bool isSafeIdentifier(const std::string& id) {
            if (id.empty()) {
                return false;
            }
            for (const unsigned char c : id) {
                if (!(std::isalnum(c) || c == '_')) {
                    return false;
                }
            }
            return true;
        }

        static std::string escapeString(MYSQL* db, const std::string& input) {
            if (db == nullptr) {
                return input;
            }

            std::string escaped;
            escaped.resize(input.size() * 2 + 1);
            const auto len = mysql_real_escape_string(
                db,
                escaped.data(),
                input.c_str(),
                static_cast<unsigned long>(input.size())
            );
            escaped.resize(len);
            return escaped;
        }

        static std::string quoteLiteral(const std::string& escaped) {
            return "'" + escaped + "'";
        }

        static std::string quoteString(MYSQL* db, const std::string& value) {
            return quoteLiteral(escapeString(db, value));
        }

        static std::string toSqlDouble(const double value) {
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
            oss << std::setprecision(15) << value;
            return oss.str();
        }

        static std::string toSqlStringIdentifier(const std::string& raw) {
            std::string out;
            out.reserve(raw.size());
            for (char c : raw) {
                if (c == '`') {
                    out += "``";
                } else {
                    out += c;
                }
            }
            return out;
        }

        static int fetchResultToMaps(MYSQL* db, std::vector<std::map<std::string, std::string>>& result) {
            MYSQL_RES* res = mysql_store_result(db);
            if (res == nullptr) {
                if (mysql_field_count(db) == 0) {
                    return DB_OK;
                }
                std::cerr << "SQL 查询失败: " << mysql_error(db) << std::endl;
                return DB_ERROR;
            }

            const auto field_count = mysql_num_fields(res);
            MYSQL_FIELD* fields = mysql_fetch_fields(res);

            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr) {
                const unsigned long* lengths = mysql_fetch_lengths(res);
                std::map<std::string, std::string> line;
                for (unsigned int i = 0; i < field_count; ++i) {
                    if (row[i] == nullptr) {
                        line[fields[i].name] = "NULL";
                    } else {
                        line[fields[i].name] = std::string(row[i], lengths[i]);
                    }
                }
                result.push_back(std::move(line));
            }

            mysql_free_result(res);
            return DB_OK;
        }

        static bool querySingleLongLong(MYSQL* db, const std::string& sql, long long& out) {
            if (mysql_query(db, sql.c_str()) != 0) {
                return false;
            }

            MYSQL_RES* res = mysql_store_result(db);
            if (res == nullptr) {
                return false;
            }

            MYSQL_ROW row = mysql_fetch_row(res);
            if (row != nullptr && row[0] != nullptr) {
                out = std::atoll(row[0]);
            } else {
                out = 0;
            }
            mysql_free_result(res);
            return true;
        }

        static std::string buildInsertSQL(
            MYSQL* db,
            const std::string& uuid,
            const std::string& id,
            const std::string& name,
            const double pos_x,
            const double pos_y,
            const double pos_z,
            const std::string& world,
            const std::string& obj_id,
            const std::string& obj_name,
            const long long time,
            const std::string& type,
            const std::string& data,
            const std::string& status
        ) {
            return "INSERT INTO LOGDATA (uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, time, type, data, status) VALUES (" +
                quoteString(db, uuid) + "," +
                quoteString(db, id) + "," +
                quoteString(db, name) + "," +
                toSqlDouble(pos_x) + "," +
                toSqlDouble(pos_y) + "," +
                toSqlDouble(pos_z) + "," +
                quoteString(db, world) + "," +
                quoteString(db, obj_id) + "," +
                quoteString(db, obj_name) + "," +
                std::to_string(time) + "," +
                quoteString(db, type) + "," +
                quoteString(db, data) + "," +
                quoteString(db, status) +
                ");";
        }

        bool ensureDatabaseExists() const {
            MYSQL* db = mysql_init(nullptr);
            if (db == nullptr) {
                return false;
            }

            if (mysql_real_connect(
                db,
                config_.host.c_str(),
                config_.user.c_str(),
                config_.password.c_str(),
                config_.database.c_str(),
                config_.port,
                nullptr,
                0
            ) != nullptr) {
                mysql_close(db);
                return true;
            }

            mysql_close(db);

            db = mysql_init(nullptr);
            if (db == nullptr) {
                return false;
            }

            if (mysql_real_connect(
                db,
                config_.host.c_str(),
                config_.user.c_str(),
                config_.password.c_str(),
                nullptr,
                config_.port,
                nullptr,
                0
            ) == nullptr) {
                std::cerr << "MySQL connect failed: " << mysql_error(db) << std::endl;
                mysql_close(db);
                return false;
            }

            const std::string db_name = toSqlStringIdentifier(config_.database);
            const std::string sql = "CREATE DATABASE IF NOT EXISTS `" + db_name + "` CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;";
            if (mysql_query(db, sql.c_str()) != 0) {
                std::cerr << "Create database failed: " << mysql_error(db) << std::endl;
                mysql_close(db);
                return false;
            }

            mysql_close(db);
            return true;
        }

        static void createIndexIfNeeded(MYSQL* db, const std::string& index_name, const std::string& column_name) {
            const std::string sql = "CREATE INDEX " + index_name + " ON LOGDATA(" + column_name + ");";
            if (mysql_query(db, sql.c_str()) != 0) {
                // 1061: Duplicate key name
                if (mysql_errno(db) != 1061) {
                    std::cerr << "Create index failed(" << index_name << "): " << mysql_error(db) << std::endl;
                }
            }
        }

        DatabaseConfig config_;
    };
}

#endif  // TIANYAN_DATABASE_H
