#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace tianyan::webui {

/// 国际化语言服务抽象接口
/// WebUI 模块通过此接口获取多语言数据
class LanguageService {
public:
    virtual ~LanguageService() = default;

    /// 获取可用语言列表
    /// 返回形状: {"languages": ["en_US","zh_CN",...], "default": "en_US"}
    [[nodiscard]] virtual nlohmann::json getLanguages() = 0;

    /// 获取指定语言文件的键值对
    /// 返回即语言 JSON 文件解析后的内容
    [[nodiscard]] virtual nlohmann::json getLanguage(const std::string& langCode) = 0;
};

} // namespace tianyan::webui
