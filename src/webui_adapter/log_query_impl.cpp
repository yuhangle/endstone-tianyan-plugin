#include "log_query_impl.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <mutex>
#include <stdexcept>
#include <string_view>

#include "database_backend.h"

// 计数缓存有效期：翻页/重复查询命中缓存，避免每次请求都全表 COUNT
static constexpr auto kCountCacheTtl = std::chrono::seconds(30);

// 含非时间条件时的计数扫描上限，与游戏内 searchLog 的 10 万行上限一致
static constexpr int kCountScanLimit = 100000;

// ============================================================
// SQL 辅助函数
// ============================================================

/// 转义 SQL 字符串中的单引号（双写 ' → ''）
static std::string sqlEscape(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        if (c == '\'') result += "''";
        else result += c;
    }
    return result;
}

/// 将值嵌入 SQL（数值类型直接转字符串）
static std::string sqlVal(const std::string& s) {
    return "'" + sqlEscape(s) + "'";
}

static std::string sqlVal(double d) {
    return std::to_string(d);
}

static std::string sqlVal(int64_t t) {
    return std::to_string(t);
}

// ============================================================
// SQL 构建 — 直接拼接参数值（IDatabaseBackend::querySQL
// 不支持 ? 占位符，所有值必须嵌入 SQL 字符串）
// ============================================================

auto LogQueryImpl::buildQuery(
    std::optional<std::int64_t> start_time,
    std::optional<std::int64_t> end_time,
    const std::optional<std::string>& filter_type,
    const std::optional<std::string>& filter_value,
    const std::optional<double>& center_x,
    const std::optional<double>& center_y,
    const std::optional<double>& center_z,
    const std::optional<double>& radius,
    const std::optional<std::string>& dimension) -> QueryBuilder
{
    QueryBuilder qb;
    std::string where;
    bool has_extra_filter = false;

    // 时间范围
    if (start_time) {
        std::string clause = " AND time >= " + sqlVal(*start_time);
        where += clause;
    }
    if (end_time) {
        std::string clause = " AND time <= " + sqlVal(*end_time);
        where += clause;
    }

    // 关键词模糊匹配无法走索引：未指定起始时间时默认只回溯最近 7 天，
    // 避免大表全表扫描；需要更早的数据时请显式选择时间区段
    if (filter_value && !filter_value->empty() && !start_time) {
        constexpr std::int64_t kKeywordLookback = 7 * 24 * 3600;
        const std::int64_t anchor = end_time
            ? *end_time
            : std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
        const std::string clause = " AND time >= " + sqlVal(anchor - kKeywordLookback);
        where += clause;
    }

    // 字段模糊匹配
    static constexpr std::array<std::string_view, 8> kAllowedFields = {
        "id", "name", "type", "obj_id", "obj_name", "world", "status", "data"
    };

    // 关键词模糊匹配：
    // - 指定了字段(filter_type) → 仅在该字段上 LIKE
    // - 未指定字段（前端“全部字段/不筛选”只传 filter_value）→ 在所有可搜索字段上 OR 匹配
    if (filter_value && !filter_value->empty()) {
        const std::string escaped_pattern = sqlEscape("%" + *filter_value + "%");
        if (filter_type &&
            std::ranges::find(kAllowedFields, *filter_type) != kAllowedFields.end()) {
            std::string condition = std::format(
                " AND ({} IS NOT NULL AND {} != '' AND {} LIKE '{}')",
                *filter_type, *filter_type, *filter_type, escaped_pattern);
            where += condition;
            has_extra_filter = true;
        } else if (!filter_type) {
            // 未指定字段：全部字段关键词搜索
            std::string condition = " AND (";
            bool first = true;
            for (const auto field : kAllowedFields) {
                if (!first) condition += " OR ";
                first = false;
                condition += std::format(
                    "({} IS NOT NULL AND {} != '' AND {} LIKE '{}')",
                    field, field, field, escaped_pattern);
            }
            condition += ")";
            where += condition;
            has_extra_filter = true;
        }
    }

    // 维度
    if (dimension && !dimension->empty()) {
        std::string clause = " AND world = " + sqlVal(*dimension);
        where += clause;
        has_extra_filter = true;
    }

    // 坐标范围
    if (center_x && center_y && center_z && radius) {
        std::string coord_valid =
            " AND pos_x IS NOT NULL AND pos_y IS NOT NULL AND pos_z IS NOT NULL";
        where += coord_valid;
        has_extra_filter = true;

        // 矩形预过滤（利用索引）
        double x_min = *center_x - *radius, x_max = *center_x + *radius;
        double y_min = *center_y - *radius, y_max = *center_y + *radius;
        double z_min = *center_z - *radius, z_max = *center_z + *radius;

        std::string bbox = std::format(
            " AND pos_x >= {} AND pos_x <= {} AND pos_y >= {} AND pos_y <= {}"
            " AND pos_z >= {} AND pos_z <= {}",
            sqlVal(x_min), sqlVal(x_max),
            sqlVal(y_min), sqlVal(y_max),
            sqlVal(z_min), sqlVal(z_max));
        where += bbox;

        // 球面距离精确过滤
        double cx = *center_x, cy = *center_y, cz = *center_z, r = *radius;
        std::string distance = std::format(
            " AND ((pos_x - {0}) * (pos_x - {0})"
            " + (pos_y - {1}) * (pos_y - {1})"
            " + (pos_z - {2}) * (pos_z - {2})) <= ({3} * {3})",
            sqlVal(cx), sqlVal(cy), sqlVal(cz), sqlVal(r));
        where += distance;
    }

    // 非时间条件无法只走 time 索引：计数只扫描最近的 10 万条匹配（与游戏内 searchLog 上限一致），
    // 否则 COUNT 需要扫描全部匹配行，大数据量下等待数十秒直到前端超时
    if (has_extra_filter) {
        qb.count_sql =
            "SELECT COUNT(*) AS cnt FROM (SELECT 1 FROM LOGDATA WHERE 1=1" + where +
            " ORDER BY time DESC LIMIT " + std::to_string(kCountScanLimit) + ") AS capped_count";
    } else {
        qb.count_sql += where;
    }
    qb.data_sql += where;
    return qb;
}

// ============================================================
// 构造函数
// ============================================================

LogQueryImpl::LogQueryImpl(IDatabaseBackend& db) : db_(db) {}

// ============================================================
// getStats
// ============================================================

nlohmann::json LogQueryImpl::getStats() {
    nlohmann::json result;

    try {
        result["total_logs"] = queryCount(QueryBuilder::kFullCountSql);
    } catch (const std::exception&) {
        // 统计失败不阻断登录/面板（查询接口会显式返回错误）
        result["total_logs"] = 0;
    }

    result["db_size"] = getDbSizeInfo();
    result["server_time"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return result;
}

// ============================================================
// getDbSizeInfo
// ============================================================

nlohmann::json LogQueryImpl::getDbSizeInfo() const {
    if (db_.isSqlite()) {
        std::vector<std::map<std::string, std::string>> page_count, page_size;
        db_.querySQL("PRAGMA page_count", page_count);
        db_.querySQL("PRAGMA page_size", page_size);

        if (!page_count.empty() && !page_size.empty()) {
            const double pages = std::stod(page_count[0]["page_count"]);
            const double size = std::stod(page_size[0]["page_size"]);
            double mb = (pages * size) / (1024.0 * 1024.0);
            return {std::format("{:.2f} MB", mb)};
        }
        return {"0 MB"};
    }
    // MySQL: 通过 information_schema 查询 data_length + index_length 估算
    std::vector<std::map<std::string, std::string>> size_result;
    db_.querySQL(
        "SELECT ROUND(SUM(data_length + index_length) / 1024 / 1024, 2) AS size_mb "
        "FROM information_schema.tables "
        "WHERE table_schema = DATABASE()", size_result);
    if (!size_result.empty() && !size_result[0]["size_mb"].empty()) {
        const std::string& mb_str = size_result[0]["size_mb"];
        try {
            double mb = std::stod(mb_str);
            return {std::format("{:.2f} MB", mb)};
        } catch (...) {
            // fallthrough
        }
    }
    return {"0 MB"};
}

// ============================================================
// 计数查询（带 TTL 缓存）
// ============================================================

std::int64_t LogQueryImpl::queryCount(const std::string& count_sql) {
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard lock(count_cache_mutex_);
        if (const auto it = count_cache_.find(count_sql);
            it != count_cache_.end() && now - it->second.queried_at < kCountCacheTtl) {
            return it->second.value;
        }
    }

    std::vector<std::map<std::string, std::string>> count_result;
    if (db_.querySQL(count_sql, count_result) != 0 || count_result.empty()) {
        throw std::runtime_error("Count query failed");
    }

    std::int64_t total;
    try {
        total = std::stoll(count_result[0].at("cnt"));
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid count result");
    }

    {
        std::lock_guard lock(count_cache_mutex_);
        for (auto it = count_cache_.begin(); it != count_cache_.end();) {
            if (now - it->second.queried_at >= kCountCacheTtl) {
                it = count_cache_.erase(it);
            } else {
                ++it;
            }
        }
        count_cache_[count_sql] = {total, now};
    }
    return total;
}

// ============================================================
// queryLogs
// ============================================================

nlohmann::json LogQueryImpl::queryLogs(const tianyan::webui::LogQueryParams& params) {
    auto start_time = std::chrono::steady_clock::now();
    bool has_coord = params.center_x && params.center_y && params.center_z && params.radius;

    auto qb = buildQuery(params.start_time, params.end_time,
                         params.filter_type, params.filter_value,
                         params.center_x, params.center_y, params.center_z, params.radius,
                         params.dimension);

    // 计数（带缓存；失败时抛出错误，而不是静默返回空结果）
    const int64_t total_records = queryCount(qb.count_sql);

    // 分页
    auto total_pages = static_cast<int>((total_records + params.page_size - 1) / params.page_size);
    auto offset = static_cast<int>((params.page - 1) * params.page_size);

    std::string data_sql = std::format("{} ORDER BY time DESC LIMIT {} OFFSET {}",
        qb.data_sql, params.page_size, offset);

    std::vector<std::map<std::string, std::string>> data_result;
    if (db_.querySQL(data_sql, data_result) != 0) {
        throw std::runtime_error("Log query failed");
    }

    // 构建 JSON
    nlohmann::json data = nlohmann::json::array();
    for (const auto& row : data_result) {
        nlohmann::json entry;
        try {
            entry["uuid"] = row.at("uuid");
            entry["id"] = row.at("id");
            entry["name"] = row.at("name");
            entry["pos_x"] = std::stod(row.at("pos_x"));
            entry["pos_y"] = std::stod(row.at("pos_y"));
            entry["pos_z"] = std::stod(row.at("pos_z"));
            entry["world"] = row.at("world");
            entry["obj_id"] = row.at("obj_id");
            entry["obj_name"] = row.at("obj_name");
            entry["time"] = std::stoll(row.at("time"));
            entry["type"] = row.at("type");
            entry["data"] = row.at("data");
            entry["status"] = row.at("status");

            if (has_coord) {
                double dx = std::stod(row.at("pos_x")) - *params.center_x;
                double dy = std::stod(row.at("pos_y")) - *params.center_y;
                double dz = std::stod(row.at("pos_z")) - *params.center_z;
                double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                entry["distance"] = std::round(distance * 100.0) / 100.0;
            }
        } catch (const std::exception&) {
            // 跳过脏数据行（如 NULL 坐标 → 空串），与游戏内 searchLog 的容错行为一致
            continue;
        }

        data.push_back(std::move(entry));
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();

    nlohmann::json result;
    result["data"] = std::move(data);
    result["total"] = total_records;
    result["page"] = params.page;
    result["page_size"] = params.page_size;
    result["total_pages"] = total_pages;
    result["query_time_ms"] = static_cast<double>(elapsed);

    return result;
}

// ============================================================
// exportLogs
// ============================================================

nlohmann::json LogQueryImpl::exportLogs(const tianyan::webui::LogExportParams& params) {
    bool has_coord = params.center_x && params.center_y && params.center_z && params.radius;

    auto qb = buildQuery(params.start_time, params.end_time,
                         params.filter_type, params.filter_value,
                         params.center_x, params.center_y, params.center_z, params.radius,
                         params.dimension);

    // 计数（带缓存；失败时抛出错误，而不是静默返回空结果）
    const int64_t total_records = queryCount(qb.count_sql);

    auto total_pages = static_cast<int>((total_records + params.page_size - 1) / params.page_size);
    int actual_end = std::min(params.end_page, std::max(total_pages, 1));

    nlohmann::json all_data = nlohmann::json::array();

    for (int page = params.start_page; page <= actual_end; ++page) {
        int offset = (page - 1) * params.page_size;
        std::string data_sql = std::format("{} ORDER BY time DESC LIMIT {} OFFSET {}",
            qb.data_sql, params.page_size, offset);

        std::vector<std::map<std::string, std::string>> page_result;
        if (db_.querySQL(data_sql, page_result) != 0) {
            throw std::runtime_error("Export query failed");
        }

        for (const auto& row : page_result) {
            nlohmann::json entry;
            try {
                entry["uuid"] = row.at("uuid");
                entry["id"] = row.at("id");
                entry["name"] = row.at("name");
                entry["pos_x"] = std::stod(row.at("pos_x"));
                entry["pos_y"] = std::stod(row.at("pos_y"));
                entry["pos_z"] = std::stod(row.at("pos_z"));
                entry["world"] = row.at("world");
                entry["obj_id"] = row.at("obj_id");
                entry["obj_name"] = row.at("obj_name");
                entry["time"] = std::stoll(row.at("time"));
                entry["type"] = row.at("type");
                entry["data"] = row.at("data");
                entry["status"] = row.at("status");

                if (has_coord) {
                    double dx = std::stod(row.at("pos_x")) - *params.center_x;
                    double dy = std::stod(row.at("pos_y")) - *params.center_y;
                    double dz = std::stod(row.at("pos_z")) - *params.center_z;
                    double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                    entry["distance"] = std::round(distance * 100.0) / 100.0;
                }
            } catch (const std::exception&) {
                // 跳过脏数据行（如 NULL 坐标 → 空串），与游戏内 searchLog 的容错行为一致
                continue;
            }

            all_data.push_back(std::move(entry));
        }
    }

    nlohmann::json result;
    result["data"] = std::move(all_data);
    result["total_records"] = total_records;
    result["exported_records"] = static_cast<int>(result["data"].size());
    result["pages_exported"] = actual_end - params.start_page + 1;
    result["start_page"] = params.start_page;
    result["end_page"] = actual_end;
    result["total_pages"] = total_pages;

    return result;
}

// ============================================================
// getDbInfo
// ============================================================

nlohmann::json LogQueryImpl::getDbInfo() {
    nlohmann::json result;

    if (db_.isSqlite()) {
        std::vector<std::map<std::string, std::string>> tables;
        db_.querySQL("SELECT name FROM sqlite_master WHERE type='table'", tables);
        result["tables"] = nlohmann::json::array();
        for (const auto& t : tables) {
            result["tables"].push_back(t.at("name"));
        }

        std::vector<std::map<std::string, std::string>> indexes;
        db_.querySQL(
            "SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='LOGDATA'",
            indexes);
        result["indexes"] = nlohmann::json::array();
        for (const auto& idx : indexes) {
            result["indexes"].push_back(idx.at("name"));
        }

        std::vector<std::map<std::string, std::string>> columns;
        db_.querySQL("PRAGMA table_info(LOGDATA)", columns);
        result["columns"] = nlohmann::json::array();
        for (const auto& col : columns) {
            nlohmann::json c;
            c["name"] = col.at("name");
            c["type"] = col.at("type");
            c["notnull"] = std::stoi(col.at("notnull"));
            c["pk"] = std::stoi(col.at("pk"));
            result["columns"].push_back(std::move(c));
        }

        std::vector<std::map<std::string, std::string>> count_result;
        db_.querySQL("SELECT COUNT(*) AS cnt FROM LOGDATA", count_result);
        result["total_rows"] = count_result.empty() ? 0 : std::stoll(count_result[0]["cnt"]);

    } else {
        // 表名
        {
            std::vector<std::map<std::string, std::string>> tables;
            db_.querySQL(
                "SELECT table_name AS name FROM information_schema.tables"
                " WHERE table_schema = DATABASE()", tables);
            result["tables"] = nlohmann::json::array();
            for (const auto& t : tables) {
                result["tables"].push_back(t.at("name"));
            }
        }

        // 索引
        {
            std::vector<std::map<std::string, std::string>> indexes;
            db_.querySQL(
                "SELECT index_name AS name FROM information_schema.statistics"
                " WHERE table_schema = DATABASE() AND table_name = 'LOGDATA'"
                " GROUP BY index_name", indexes);
            result["indexes"] = nlohmann::json::array();
            for (const auto& idx : indexes) {
                result["indexes"].push_back(idx.at("name"));
            }
        }

        // 列信息
        {
            std::vector<std::map<std::string, std::string>> columns;
            db_.querySQL(
                "SELECT column_name AS name, column_type AS type,"
                " is_nullable AS nullable, column_key AS col_key"
                " FROM information_schema.columns"
                " WHERE table_schema = DATABASE() AND table_name = 'LOGDATA'"
                " ORDER BY ordinal_position", columns);
            result["columns"] = nlohmann::json::array();
            for (const auto& col : columns) {
                nlohmann::json c;
                c["name"] = col.at("name");
                c["type"] = col.at("type");
                c["notnull"] = (col.at("nullable") == "YES") ? 0 : 1;
                c["pk"] = (col.at("col_key") == "PRI") ? 1 : 0;
                result["columns"].push_back(std::move(c));
            }
        }

        // 行数
        {
            std::vector<std::map<std::string, std::string>> count_result;
            db_.querySQL("SELECT COUNT(*) AS cnt FROM LOGDATA", count_result);
            result["total_rows"] = count_result.empty() ? 0 : std::stoll(count_result[0]["cnt"]);
        }
    }

    return result;
}

// ============================================================
// debugQuery
// ============================================================

nlohmann::json LogQueryImpl::debugQuery(const std::string& sql) {
    std::vector<std::map<std::string, std::string>> result;
    int rc = db_.querySQL(sql, result);

    nlohmann::json response;
    if (rc == 0) {
        response["success"] = true;
        response["row_count"] = static_cast<int>(result.size());

        nlohmann::json data = nlohmann::json::array();
        int count = 0;
        for (const auto& row : result) {
            if (count >= 100) break;
            nlohmann::json entry;
            for (const auto& [key, value] : row) {
                entry[key] = value;
            }
            data.push_back(std::move(entry));
            ++count;
        }
        response["data"] = std::move(data);
        response["total"] = static_cast<int>(result.size());
    } else {
        response["success"] = false;
        response["error"] = std::format("Query failed with code {}", rc);
    }

    return response;
}
