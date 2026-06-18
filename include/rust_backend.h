#pragma once

#include "database_backend.h"

#include <cstdint>
#include <string>

struct RustMySQLConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 3306;
    std::string user = "root";
    std::string password;
    std::string database = "endstone";
};

class RustBackend : public IDatabaseBackend {
public:
    explicit RustBackend(const RustMySQLConfig& config);
    ~RustBackend() override;

    RustBackend(const RustBackend&) = delete;
    RustBackend& operator=(const RustBackend&) = delete;

    [[nodiscard]] bool connected() const { return connected_; }

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
                  std::atomic<bool>* cancel) override;

    int searchLog(std::vector<std::map<std::string, std::string>>& result,
                  const std::pair<std::string, double>& key,
                  double x, double y, double z, double r,
                  const std::string& world,
                  std::atomic<bool>* cancel) override;

    bool updateStatusesByUUIDs(
        const std::vector<std::pair<std::string, std::string>>& pairs) override;

    int64_t getCleanCount(long long timestamp) override;

    int deleteBatch(long long timestamp, int limit) override;

    int64_t cleanupByRebuild(long long threshold) override;

    [[nodiscard]] bool isSqlite() const override { return false; }

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
    void* handle_ = nullptr;
    bool connected_ = false;

    // Convert a query result (opaque pointer) to vector-of-maps
    static int resultToMaps(void* result,
                     std::vector<std::map<std::string, std::string>>& out) ;

    // Run a parameterized query and return result as maps
    int queryToMaps(const std::string& sql,
                    std::vector<std::map<std::string, std::string>>& result,
                    const std::vector<std::string>& params = {}) const;

    // Run a parameterized query with cancel support
    int queryToMapsWithCancel(const std::string& sql,
                              std::vector<std::map<std::string, std::string>>& result,
                              const std::vector<std::string>& params,
                              std::atomic<bool>* cancel) const;

    // Get the last error string from Rust
    [[nodiscard]] std::string lastError() const;
};
