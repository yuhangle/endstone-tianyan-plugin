#pragma once

#include <string>

#include "webui/service/language_service.h"

/// LanguageService 的插件侧实现
/// 读取语言 JSON 文件并提供给 WebUI 模块
class LanguageServiceImpl : public tianyan::webui::LanguageService {
public:
    explicit LanguageServiceImpl(std::string language_dir);
    ~LanguageServiceImpl() override = default;

    [[nodiscard]] nlohmann::json getLanguages() override;
    [[nodiscard]] nlohmann::json getLanguage(const std::string& langCode) override;

private:
    std::string language_dir_;
};
