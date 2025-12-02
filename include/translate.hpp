//
// Created by yuhang on 2025/10/27.
//

#ifndef TIANYAN_TRANSLATE_H
#define TIANYAN_TRANSLATE_H

#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>
#include <fmt/format.h>
using namespace std;
inline string language_file = "plugins/tianyan_data/language/lang.json";
class translate {
public:
    using json = nlohmann::json;
    json languageResource; // 存储从 lang.json 加载的语言资源

    // 构造函数中加载语言资源文件
    explicit translate(string lang_file = language_file) : lang_file_(std::move(lang_file)) { loadLanguage(); };

    // 加载语言资源文件
    pair<bool,string> loadLanguage() {
        const string lang_file = lang_file_;
        if (std::ifstream f(lang_file); f.is_open()) {
            languageResource = json::parse(f);
            f.close();
            return {true,"language file is normal"};
        }
        return {false,"you can download language file from github to change tianyan plugin language"};
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

    static int checkLanguageCommon(const std::string& lang_path, const std::string& default_lang_path) {
        namespace fs = std::filesystem;
        // 检查确定的语言文件是否存在
        if (!fs::exists(lang_path)) {
            return 0; // 源语言文件不存在，无法操作
        }

        // 情况1：默认语言文件不存在 → 直接复制
        if (!fs::exists(default_lang_path)) {
            try {
                fs::copy_file(lang_path, default_lang_path);
                return 1;
            } catch (...) {
                return 0; // 复制失败
            }
        }

        // 情况2：默认语言文件存在 → 比较内容是否相同
        try {
            std::ifstream src_file(lang_path, std::ios::binary);
            std::ifstream dst_file(default_lang_path, std::ios::binary);

            if (!src_file || !dst_file) {
                return 0;
            }

            // 逐字节比较文件内容
            bool identical = true;
            char src_byte, dst_byte;
            while (src_file.get(src_byte) && dst_file.get(dst_byte)) {
                if (src_byte != dst_byte) {
                    identical = false;
                    break;
                }
            }

            // 检查是否同时到达文件末尾
            if (identical && (src_file.eof() != dst_file.eof())) {
                identical = false;
            }

            if (!identical) {
                // 内容不同，覆盖默认语言文件
                dst_file.close();
                src_file.close();
                fs::copy_file(lang_path, default_lang_path, fs::copy_options::overwrite_existing);
                return 1;
            }

            // 内容相同，无需操作
            return 0;

        } catch (...) {
            return 0; // 比较或覆盖过程中出错
        }
    }
private:
    string lang_file_;
};
#endif //TIANYAN_TRANSLATE_H