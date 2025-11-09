//
// Created by yuhang on 2025/10/27.
//

#ifndef TIANYAN_TRANSLATE_H
#define TIANYAN_TRANSLATE_H

#include <fstream>
#include <nlohmann/json.hpp>
using namespace std;

class translate {
public:
    using json = nlohmann::json;
    json languageResource; // 存储从 lang.json 加载的语言资源

    // 构造函数中加载语言资源文件
    explicit translate(const string& lang);

    // 加载语言资源文件
    pair<bool,string> loadLanguage(const string& lang) {
        lang_file = lang;
        if (std::ifstream f(lang_file); f.is_open()) {
            languageResource = json::parse(f);
            f.close();
            return {true,"language file is normal"};
        } else {
            return {false,"you can download language file from github to change tianyan plugin language"};
        }
    }

    // 获取本地化字符串
    std::string getLocal(const std::string &key) {
        if (languageResource.find(key) != languageResource.end()) {
            return languageResource[key].get<std::string>();
        }
        return key; // 如果找不到，返回原始 key
    }

    template<typename... Args>
    std::string tr(const std::string& key, Args&&... args) {
        const std::string pattern = getLocal(key);
        return fmt::vformat(pattern, fmt::make_format_args(args...));
    }
private:
    string lang_file = "plugins/tianyan_data/language/en_US.json";
};

inline translate::translate(const string& lang) {
    loadLanguage(lang);
}

#endif //TIANYAN_TRANSLATE_H