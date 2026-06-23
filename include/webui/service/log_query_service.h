#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace tianyan::webui {

/// 日志查询参数
struct LogQueryParams {
    int page = 1;
    int page_size = 100;
    std::optional<std::int64_t> start_time;   // Unix sec
    std::optional<std::int64_t> end_time;     // Unix sec
    std::optional<std::string> filter_type;   // name / id / type / obj_name / obj_id
    std::optional<std::string> filter_value;  // LIKE 匹配值
    std::optional<double> center_x;
    std::optional<double> center_y;
    std::optional<double> center_z;
    std::optional<double> radius;
    std::optional<std::string> dimension;     // world name
};

/// 日志导出参数
struct LogExportParams {
    int start_page = 1;
    int end_page = 1;
    int page_size = 100;
    std::optional<std::int64_t> start_time;
    std::optional<std::int64_t> end_time;
    std::optional<std::string> filter_type;
    std::optional<std::string> filter_value;
    std::optional<double> center_x;
    std::optional<double> center_y;
    std::optional<double> center_z;
    std::optional<double> radius;
    std::optional<std::string> dimension;
};

/// 日志查询服务抽象接口
/// WebUI 模块通过此接口读取日志数据，不直接依赖数据库实现
class LogQueryService {
public:
    virtual ~LogQueryService() = default;

    /// 获取统计信息
    /// 返回形状: {"total_logs": int, "db_size": str, "server_time": int}
    [[nodiscard]] virtual nlohmann::json getStats() = 0;

    /// 分页查询日志
    /// 返回形状匹配现有 Python 后端 /api/logs 响应
    [[nodiscard]] virtual nlohmann::json queryLogs(const LogQueryParams& params) = 0;

    /// 批量导出
    /// 返回形状匹配现有 Python 后端 /api/export 响应
    [[nodiscard]] virtual nlohmann::json exportLogs(const LogExportParams& params) = 0;

    /// 获取数据库结构信息
    /// 返回形状: {"tables": [...], "indexes": [...], "columns": [...], "total_rows": int}
    [[nodiscard]] virtual nlohmann::json getDbInfo() = 0;

    /// 调试查询（仅 DEBUG 模式可用）
    /// 直接执行 SELECT SQL，返回前 100 行
    [[nodiscard]] virtual nlohmann::json debugQuery(const std::string& sql) = 0;
};

} // namespace tianyan::webui
