#include "webui/server.h"

#include <algorithm>
#include <atomic>
#include <format>
#include <fstream>
#include <iostream>
#include <thread>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "webui_resources.h"  // 由 cmake/embed_webui_resources.cmake 生成

namespace tianyan::webui {

// ============================================================
// WebUIServer::Impl — PIMPL 实现，隐藏 httplib 细节
// ============================================================

class WebUIServer::Impl {
public:
    explicit Impl(WebUIConfig config)
        : config_(std::move(config)) {}

    ~Impl() {
        stop();
    }

    // === 工具方法 ===

    /// 根据文件扩展名返回 MIME 类型
    static std::string mimeType(const std::string& path) {
        if (path.ends_with(".html"))  return "text/html; charset=utf-8";
        if (path.ends_with(".js"))    return "text/javascript; charset=utf-8";
        if (path.ends_with(".css"))   return "text/css; charset=utf-8";
        if (path.ends_with(".svg"))   return "image/svg+xml";
        if (path.ends_with(".png"))   return "image/png";
        if (path.ends_with(".webp"))  return "image/webp";
        if (path.ends_with(".woff2")) return "font/woff2";
        if (path.ends_with(".woff"))  return "font/woff";
        if (path.ends_with(".ttf"))   return "font/ttf";
        if (path.ends_with(".json"))  return "application/json; charset=utf-8";
        return "application/octet-stream";
    }

    // === 服务注入 ===

    void setLogQueryService(std::unique_ptr<LogQueryService> svc) {
        log_query_svc_ = std::move(svc);
    }

    void setLanguageService(std::unique_ptr<LanguageService> svc) {
        lang_svc_ = std::move(svc);
    }

    void setAdminService(std::unique_ptr<AdminService> svc) {
        admin_svc_ = std::move(svc);
    }

    void setMainThreadDispatcher(MainThreadDispatcher dispatcher) {
        dispatcher_ = std::move(dispatcher);
    }

    // === 生命周期 ===

    bool start() {
        if (running_) {
            std::cerr << "[WebUIServer] Already running.\n";
            return false;
        }

        // 检查必要服务是否注入
        if (!log_query_svc_) {
            std::cerr << "[WebUIServer] LogQueryService not set.\n";
            return false;
        }
        if (!lang_svc_) {
            std::cerr << "[WebUIServer] LanguageService not set.\n";
            return false;
        }

        svr_ = std::make_unique<httplib::Server>();

        // === 配置线程池 ===
        int pool_size = config_.thread_pool_size;
        if (pool_size <= 0) {
            pool_size = std::max(2, static_cast<int>(std::thread::hardware_concurrency()));
        }
        svr_->new_task_queue = [pool_size]() -> httplib::TaskQueue* {
            return new httplib::ThreadPool(pool_size);
        };

        // === 注册路由 ===
        registerPreRouting();
        registerAPIRoutes();
        registerStaticFileRoutes();
        registerCORSPreflight();

        // === 启动监听 ===
        try {
            if (!svr_->bind_to_port(config_.host, config_.port)) {
                std::cerr << std::format("[WebUIServer] Failed to bind to {}:{}\n",
                    config_.host, config_.port);
                svr_.reset();
                return false;
            }
            actual_port_ = config_.port;

            // 在独立线程启动 listen
            listen_thread_ = std::thread([this]() {
                svr_->listen_after_bind();
            });

            running_ = true;
            std::cout << std::format("[WebUIServer] Started on {}:{} (thread_pool={})\n",
                config_.host, actual_port_, pool_size);
            return true;

        } catch (const std::exception& e) {
            std::cerr << std::format("[WebUIServer] Start failed: {}\n", e.what());
            svr_.reset();
            return false;
        }
    }

    void stop() {
        if (!running_ || !svr_) return;

        std::cout << "[WebUIServer] Stopping...\n";

        // 1. 停止接受新连接
        svr_->stop();

        // 2. 等待 listen 线程退出
        if (listen_thread_.joinable()) {
            listen_thread_.join();
        }

        running_ = false;
        actual_port_ = 0;
        svr_.reset();

        std::cout << "[WebUIServer] Stopped.\n";
    }

    bool isRunning() const noexcept {
        return running_;
    }

    uint16_t getPort() const noexcept {
        return actual_port_;
    }

private:
    // ============================================================
    // 配置与依赖
    // ============================================================
    WebUIConfig config_;
    std::unique_ptr<LogQueryService> log_query_svc_;
    std::unique_ptr<LanguageService> lang_svc_;
    std::unique_ptr<AdminService> admin_svc_;
    MainThreadDispatcher dispatcher_;

    std::unique_ptr<httplib::Server> svr_;
    std::atomic<bool> running_{false};
    uint16_t actual_port_{0};
    std::thread listen_thread_;

    // ============================================================
    // Helper: CORS 头
    // ============================================================
    static void setCorsHeaders(httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "X-Secret, X-Lang, Content-Type");
        res.set_header("Access-Control-Max-Age", "86400");
    }

    // ============================================================
    // Helper: JSON 响应
    // ============================================================
    static void jsonResponse(httplib::Response& res, const nlohmann::json& data, int status = 200) {
        res.status = status;
        res.set_content(data.dump(), "application/json; charset=utf-8");
        setCorsHeaders(res);
    }

    static void errorResponse(httplib::Response& res, const std::string& detail, int status = 400) {
        nlohmann::json err;
        err["detail"] = detail;
        jsonResponse(res, err, status);
    }

    // ============================================================
    // 预路由处理器（认证 + CORS）
    // ============================================================
    void registerPreRouting() const
    {
        svr_->set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
            // CORS 头（所有响应都设置）
            setCorsHeaders(res);

            // 对 /api/* 进行认证（语言接口除外：/api/languages, /api/language/*）
            if (req.path.starts_with("/api/") &&
                req.path != "/api/languages" &&
                !req.path.starts_with("/api/language/")) {
                if (const auto secret = req.get_header_value("X-Secret"); secret != config_.secret) {
                    res.status = 403;
                    res.set_content(R"({"detail":"Secret Invalid"})", "application/json");
                    return httplib::Server::HandlerResponse::Handled;
                }
            }

            return httplib::Server::HandlerResponse::Unhandled;
        });
    }

    // ============================================================
    // CORS 预检请求
    // ============================================================
    void registerCORSPreflight() const
    {
        svr_->Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
            setCorsHeaders(res);
            res.status = 204;
        });
    }

    // ============================================================
    // API 路由注册
    // ============================================================
    void registerAPIRoutes() const
    {
        // ---- GET /api/stats ----
        svr_->Get("/api/stats", [this](const httplib::Request&, httplib::Response& res) {
            try {
                const auto result = log_query_svc_->getStats();
                jsonResponse(res, result);
            } catch (const std::exception& e) {
                errorResponse(res, std::format("Server error: {}", e.what()), 500);
            }
        });

        // ---- GET /api/logs ----
        svr_->Get("/api/logs", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                LogQueryParams params;
                auto page_str = req.get_param_value("page");
                params.page = page_str.empty() ? 1 : std::max(1, std::stoi(page_str));
                auto ps_str = req.get_param_value("page_size");
                params.page_size = ps_str.empty() ? 100 : std::clamp(std::stoi(ps_str), 1, 500);

                if (auto start_time_str = req.get_param_value("start_time"); !start_time_str.empty()) {
                    params.start_time = std::stoll(start_time_str);
                }
                if (auto end_time_str = req.get_param_value("end_time"); !end_time_str.empty()) {
                    params.end_time = std::stoll(end_time_str);
                }

                if (auto filter_type = req.get_param_value("filter_type"); !filter_type.empty()) {
                    params.filter_type = filter_type;
                }
                if (auto filter_value = req.get_param_value("filter_value"); !filter_value.empty()) {
                    params.filter_value = filter_value;
                }

                // 坐标参数：要么全有，要么全无
                auto cx = req.get_param_value("center_x");
                auto cy = req.get_param_value("center_y");
                auto cz = req.get_param_value("center_z");
                if (auto cr = req.get_param_value("radius"); !cx.empty() && !cy.empty() && !cz.empty() && !cr.empty()) {
                    params.center_x = std::stod(cx);
                    params.center_y = std::stod(cy);
                    params.center_z = std::stod(cz);
                    params.radius = std::stod(cr);
                    if (*params.radius < 0) {
                        errorResponse(res, "Radius cannot be negative", 400);
                        return;
                    }
                } else if (!cx.empty() || !cy.empty() || !cz.empty() || !cr.empty()) {
                    errorResponse(res, "Coord query needs all: center_x, center_y, center_z, radius", 400);
                    return;
                }

                if (auto dim = req.get_param_value("dimension"); !dim.empty() && dim != "all") {
                    params.dimension = dim;
                }

                auto result = log_query_svc_->queryLogs(params);
                jsonResponse(res, result);

            } catch (const std::exception& e) {
                errorResponse(res, std::format("Server error: {}", e.what()), 500);
            }
        });

        // ---- GET /api/export ----
        svr_->Get("/api/export", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                LogExportParams params;
                auto sp_str = req.get_param_value("start_page");
                params.start_page = sp_str.empty() ? 1 : std::max(1, std::stoi(sp_str));
                auto ep_str = req.get_param_value("end_page");
                params.end_page = ep_str.empty() ? 1 : std::max(1, std::stoi(ep_str));
                auto ps_str = req.get_param_value("page_size");
                params.page_size = ps_str.empty() ? 100 : std::clamp(std::stoi(ps_str), 1, 500);

                if (params.start_page > params.end_page) {
                    errorResponse(res, "start_page cannot be greater than end_page", 400);
                    return;
                }

                // 计算最大导出条数限制
                if (static_cast<long long>(params.end_page - params.start_page + 1) * params.page_size > 50000) {
                    errorResponse(res, "Export too large, max 50000 records", 400);
                    return;
                }

                // 解析过滤参数
                if (auto start_time_str = req.get_param_value("start_time"); !start_time_str.empty()) params.start_time = std::stoll(start_time_str);
                if (auto end_time_str = req.get_param_value("end_time"); !end_time_str.empty()) params.end_time = std::stoll(end_time_str);
                if (auto filter_type = req.get_param_value("filter_type"); !filter_type.empty()) params.filter_type = filter_type;
                if (auto filter_value = req.get_param_value("filter_value"); !filter_value.empty()) params.filter_value = filter_value;

                auto cx = req.get_param_value("center_x");
                auto cy = req.get_param_value("center_y");
                auto cz = req.get_param_value("center_z");
                if (auto cr = req.get_param_value("radius"); !cx.empty() && !cy.empty() && !cz.empty() && !cr.empty()) {
                    params.center_x = std::stod(cx);
                    params.center_y = std::stod(cy);
                    params.center_z = std::stod(cz);
                    params.radius = std::stod(cr);
                }
                if (auto dim = req.get_param_value("dimension"); !dim.empty() && dim != "all") params.dimension = dim;

                auto result = log_query_svc_->exportLogs(params);
                jsonResponse(res, result);

            } catch (const std::exception& e) {
                errorResponse(res, std::format("Server error: {}", e.what()), 500);
            }
        });

        // ---- GET /api/db_info ----
        svr_->Get("/api/db_info", [this](const httplib::Request&, httplib::Response& res) {
            try {
                const auto result = log_query_svc_->getDbInfo();
                jsonResponse(res, result);
            } catch (const std::exception& e) {
                errorResponse(res, std::format("Server error: {}", e.what()), 500);
            }
        });

        // ---- GET /api/debug/query — 仅 DEBUG 模式 ----
        svr_->Get("/api/debug/query", [this](const httplib::Request& req, httplib::Response& res) {
            if (!config_.debug) {
                errorResponse(res, "This endpoint is only available in debug mode", 403);
                return;
            }

            const auto sql = req.get_param_value("sql");
            if (sql.empty()) {
                errorResponse(res, "SQL query required", 400);
                return;
            }

            // 安全检查：只允许 SELECT
            std::string trimmed = sql;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
            if (trimmed.size() < 6 ||
                (trimmed[0] != 'S' && trimmed[0] != 's') ||
                (trimmed[1] != 'E' && trimmed[1] != 'e') ||
                (trimmed[2] != 'L' && trimmed[2] != 'l') ||
                (trimmed[3] != 'E' && trimmed[3] != 'e') ||
                (trimmed[4] != 'C' && trimmed[4] != 'c') ||
                (trimmed[5] != 'T' && trimmed[5] != 't')) {
                errorResponse(res, "Only SELECT queries are allowed", 400);
                return;
            }

            try {
                const auto result = log_query_svc_->debugQuery(sql);
                jsonResponse(res, result);
            } catch (const std::exception& e) {
                errorResponse(res, std::format("Query error: {}", e.what()), 500);
            }
        });

        // ---- GET /api/languages ----
        svr_->Get("/api/languages", [this](const httplib::Request&, httplib::Response& res) {
            try {
                const auto result = lang_svc_->getLanguages();
                jsonResponse(res, result);
            } catch (const std::exception& e) {
                errorResponse(res, std::format("Server error: {}", e.what()), 500);
            }
        });

        // ---- GET /api/language/{lang_code} ----
        svr_->Get(R"(/api/language/([a-zA-Z_]+))",
            [this](const httplib::Request& req, httplib::Response& res) {
                try {
                    const auto lang_code = req.matches[1];
                    const auto result = lang_svc_->getLanguage(lang_code);
                    jsonResponse(res, result);
                } catch (const std::exception& e) {
                    errorResponse(res, std::format("Server error: {}", e.what()), 500);
                }
            }
        );

        // ---- 错误处理 ----
        svr_->set_error_handler([](const httplib::Request&, httplib::Response& res) {
            if (res.status == 404) {
                nlohmann::json err;
                err["detail"] = "Not Found";
                res.set_content(err.dump(), "application/json; charset=utf-8");
            }
        });
    }

    // ============================================================
    // 静态文件路由
    // ============================================================
    void registerStaticFileRoutes() {
        if (!config_.static_dir.empty()) {
            // 文件系统模式 — 开发调试用
            if (const bool ok = svr_->set_mount_point("/", config_.static_dir); !ok) {
                std::cerr << std::format("[WebUIServer] Failed to mount static dir: {}\n",
                    config_.static_dir);
            } else {
                std::cout << std::format("[WebUIServer] Serving static files from: {}\n",
                    config_.static_dir);
            }
        } else {
            // 嵌入式内存模式 — 生产构建
            registerEmbeddedResources();
            std::cout << "[WebUIServer] Serving embedded static resources from memory.\n";
        }
    }

    void registerEmbeddedResources() const
    {
        using namespace embedded;

        for (const auto & kResource : kResources) {
            const auto& [path_, data] = kResource;
            std::string path(path_);

            // 确定 Content-Type（扩展常见类型，防止特殊资源加载异常）
            std::string content_type = mimeType(path);

            // 注意：按值捕获 data (string_view)，不可捕获循环变量的引用
            svr_->Get(path, [data, content_type](const httplib::Request&, httplib::Response& res) {
                // 必须传 size，否则 std::string(data.data()) 用 strlen 越界读
                res.set_content(data.data(), data.size(), content_type);
                setCorsHeaders(res);
            });
        }

        // 根路径 "/" → 在 kResources 中按名查找 index.html
        // 不依赖 kResources[0] 的顺序假设（生成脚本排序不可靠）
        svr_->Get("/", [](const httplib::Request&, httplib::Response& res) {
            for (const auto& [path, data] : embedded::kResources) {
                if (path == "/index.html") {
                    res.set_content(data.data(), data.size(), "text/html; charset=utf-8");
                    setCorsHeaders(res);
                    return;
                }
            }
            res.status = 404;
        });
    }
};

// ============================================================
// WebUIServer — 委托给 Impl
// ============================================================

WebUIServer::WebUIServer(WebUIConfig config)
    : pimpl_(std::make_unique<Impl>(std::move(config))) {}

WebUIServer::~WebUIServer() = default;

void WebUIServer::setLogQueryService(std::unique_ptr<LogQueryService> svc) const
{
    pimpl_->setLogQueryService(std::move(svc));
}

void WebUIServer::setLanguageService(std::unique_ptr<LanguageService> svc) const
{
    pimpl_->setLanguageService(std::move(svc));
}

void WebUIServer::setAdminService(std::unique_ptr<AdminService> svc) const
{
    pimpl_->setAdminService(std::move(svc));
}

void WebUIServer::setMainThreadDispatcher(MainThreadDispatcher dispatcher) const
{
    pimpl_->setMainThreadDispatcher(std::move(dispatcher));
}

bool WebUIServer::start() const
{
    return pimpl_->start();
}

void WebUIServer::stop() const
{
    pimpl_->stop();
}

bool WebUIServer::isRunning() const noexcept {
    return pimpl_->isRunning();
}

uint16_t WebUIServer::getPort() const noexcept {
    return pimpl_->getPort();
}

} // namespace tianyan::webui
