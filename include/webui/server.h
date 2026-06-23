#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "webui/service/log_query_service.h"
#include "webui/service/language_service.h"
#include "webui/service/admin_service.h"

namespace tianyan::webui {

/// 主线程派发器
/// 将任务投递到 BDS 游戏主线程执行。
/// HTTP 工作线程不可直接调用游戏 API，必须通过此派发器中转。
/// 插件侧实现通过 endstone::Server::getScheduler().runTask() 包装。
using MainThreadDispatcher = std::function<void(std::function<void()>)>;

/// WebUI 服务器配置
struct WebUIConfig {
    std::string secret = "your_secret";
    uint16_t port = 8098;
    bool debug = false;
    std::string host = "0.0.0.0";

    /// 静态资源策略：
    ///   空字符串 → 从 CMake 编译的嵌入式字节数组提供（默认，单文件分发）
    ///   非空     → 从文件系统目录提供（开发调试时热修改前端）
    std::string static_dir;

    /// HTTP 线程池大小
    /// 建议 2-4。长耗时查询（如 /api/export）独占一线程，
    /// 其他请求由剩余线程处理，不被阻塞。
    /// 0 表示使用 cpp-httplib 默认值（硬件并发数）。
    int thread_pool_size = 2;
};

/// WebUI 服务器
/// 内嵌 cpp-httplib HTTP 服务，提供与原有 Python FastAPI 后端兼容的 REST API。
/// 生命周期由插件控制：start() / stop()
///
/// 设计原则：
/// - PIMPL 模式隐藏 httplib 实现细节
/// - 通过纯虚接口注入服务实现，与插件核心解耦
/// - 未来写操作通过 MainThreadDispatcher 投递到 BDS 主线程
class WebUIServer {
public:
    explicit WebUIServer(WebUIConfig config);
    ~WebUIServer();

    WebUIServer(const WebUIServer&) = delete;
    WebUIServer& operator=(const WebUIServer&) = delete;

    // === 服务注入（start 前调用） ===

    void setLogQueryService(std::unique_ptr<LogQueryService> svc) const;
    void setLanguageService(std::unique_ptr<LanguageService> svc) const;
    void setAdminService(std::unique_ptr<AdminService> svc) const;  // 预留

    /// 设置主线程派发器（start 前调用）
    /// 插件侧实现：
    ///   server.setMainThreadDispatcher([this](auto task) {
    ///       getServer().getScheduler().runTask(*this, std::move(task));
    ///   });
    void setMainThreadDispatcher(MainThreadDispatcher dispatcher) const;

    // === 生命周期 ===

    /// 启动 HTTP 服务。返回 true 表示启动成功。
    [[nodiscard]] bool start() const;

    /// 停止 HTTP 服务。
    /// - 关闭 listen socket，停止接受新连接
    /// - 等待活跃请求完成（graceful timeout ~3s）
    /// - 看门狗机制：超时后强制关闭残留连接
    void stop() const;

    /// 检查服务是否正在运行
    [[nodiscard]] bool isRunning() const noexcept;

    /// 获取实际监听端口（当 port=0 由 OS 分配时）
    [[nodiscard]] uint16_t getPort() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace tianyan::webui
