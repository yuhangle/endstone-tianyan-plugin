#include "language_impl.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

LanguageServiceImpl::LanguageServiceImpl(std::string language_dir)
    : language_dir_(std::move(language_dir)) {}

nlohmann::json LanguageServiceImpl::getLanguages() {
    nlohmann::json result;
    result["languages"] = nlohmann::json::array();
    result["default"] = "en_US";

    if (!fs::exists(language_dir_)) {
        // 目录不存在，返回默认值
        result["languages"].push_back("en_US");
        result["languages"].push_back("zh_CN");
        return result;
    }

    std::vector<std::string> languages;
    for (const auto& entry : fs::directory_iterator(language_dir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string lang_code = entry.path().stem().string();
            languages.push_back(lang_code);
        }
    }

    std::ranges::sort(languages);

    for (const auto& lang : languages) {
        result["languages"].push_back(lang);
    }

    // 确保至少包含 en_US
    if (!languages.empty() && std::ranges::find(languages, "en_US") == languages.end()) {
        result["languages"].push_back("en_US");
    }

    return result;
}

nlohmann::json LanguageServiceImpl::getLanguage(const std::string& langCode) {
    // 构建语言文件路径
    fs::path lang_file = fs::path(language_dir_) / (langCode + ".json");

    // 文件不存在则回退到 en_US
    if (!fs::exists(lang_file)) {
        lang_file = fs::path(language_dir_) / "en_US.json";
    }

    // 如果仍然不存在，返回空 JSON
    if (!fs::exists(lang_file)) {
        return nlohmann::json::object();
    }

    try {
        std::ifstream file(lang_file);
        if (!file.is_open()) {
            return nlohmann::json::object();
        }
        return nlohmann::json::parse(file);
    } catch (const std::exception& e) {
        std::cerr << std::format("[LanguageService] Failed to parse {}: {}\n",
            lang_file.string(), e.what());
        return nlohmann::json::object();
    }
}
