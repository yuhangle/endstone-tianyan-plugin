#pragma once

namespace tianyan::webui {

/// 预留：管理员操作接口
/// 未来通过此接口向 WebUI 暴露插件的内部操作能力（封禁、回滚、密度扫描等）
/// 写操作必须通过 MainThreadDispatcher 投递到 BDS 主线程执行
class AdminService {
public:
    virtual ~AdminService() = default;

    // 未来在此添加纯虚方法：
    // [[nodiscard]] virtual nlohmann::json banPlayer(const std::string& deviceId) = 0;
    // [[nodiscard]] virtual nlohmann::json unbanPlayer(const std::string& deviceId) = 0;
    // [[nodiscard]] virtual nlohmann::json rollback(const RollbackParams& params) = 0;
    // ...
};

} // namespace tianyan::webui
