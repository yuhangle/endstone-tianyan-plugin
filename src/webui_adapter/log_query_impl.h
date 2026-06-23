#pragma once

#include "webui/service/log_query_service.h"

class IDatabaseBackend;

/// LogQueryService 的插件侧实现
/// 包装 IDatabaseBackend，执行 SQL 查询
class LogQueryImpl : public tianyan::webui::LogQueryService {
public:
    explicit LogQueryImpl(IDatabaseBackend& db);
    ~LogQueryImpl() override = default;

    [[nodiscard]] nlohmann::json getStats() override;
    [[nodiscard]] nlohmann::json queryLogs(const tianyan::webui::LogQueryParams& params) override;
    [[nodiscard]] nlohmann::json exportLogs(const tianyan::webui::LogExportParams& params) override;
    [[nodiscard]] nlohmann::json getDbInfo() override;
    [[nodiscard]] nlohmann::json debugQuery(const std::string& sql) override;

private:
    IDatabaseBackend& db_;

    // SQL 构建辅助
    // 注意：IDatabaseBackend::querySQL 不支持 ? 占位符，
    // 所有值必须直接嵌入 SQL 字符串。
    struct QueryBuilder {
        std::string count_sql = "SELECT COUNT(*) AS cnt FROM LOGDATA WHERE 1=1";
        std::string data_sql = "SELECT * FROM LOGDATA WHERE 1=1";
    };

    [[nodiscard]] static QueryBuilder buildQuery(
        std::optional<std::int64_t> start_time,
        std::optional<std::int64_t> end_time,
        const std::optional<std::string>& filter_type,
        const std::optional<std::string>& filter_value,
        const std::optional<double>& center_x,
        const std::optional<double>& center_y,
        const std::optional<double>& center_z,
        const std::optional<double>& radius,
        const std::optional<std::string>& dimension) ;

    // 获取数据库大小信息
    [[nodiscard]] nlohmann::json getDbSizeInfo() const;
};
