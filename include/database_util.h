#pragma once

#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace db_util {

inline std::vector<std::string> splitString(const std::string& input) {
    std::vector<std::string> result;
    std::string current;
    bool inBraces = false;
    bool inBrackets = false;

    for (const char ch : input) {
        if (ch == '{') {
            inBraces = true;
            current += ch;
        } else if (ch == '}') {
            inBraces = false;
            current += ch;
        } else if (ch == '[') {
            inBrackets = true;
            current += ch;
        } else if (ch == ']') {
            inBrackets = false;
            current += ch;
        } else if (ch == ',' && !inBraces && !inBrackets) {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += ch;
        }
    }

    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

inline std::vector<int> splitStringInt(const std::string& input) {
    std::vector<int> result;
    std::stringstream ss(input);
    std::string item;

    while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
            std::stringstream sss(item);
            int groupid = 0;
            if (!(sss >> groupid)) {
                return {};
            }
            result.push_back(groupid);
        }
    }

    if (result.empty()) {
        return {};
    }

    return result;
}

inline std::string vectorToString(const std::vector<std::string>& vec) {
    if (vec.empty()) {
        return "";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        oss << vec[i];
        if (i != vec.size() - 1 && !(vec[i].empty())) {
            oss << ",";
        }
    }

    return oss.str();
}

inline std::string intVectorToString(const std::vector<int>& vec) {
    if (vec.empty()) {
        return "";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        oss << vec[i];
        if (i != vec.size() - 1) {
            oss << ",";
        }
    }

    return oss.str();
}

inline int stringToInt(const std::string& str) {
    try {
        return std::stoi(str);
    } catch (const std::invalid_argument&) {
        return 0;
    } catch (const std::out_of_range&) {
        return 0;
    }
}

inline std::string generate_uuid_v4() {
    static thread_local std::mt19937 gen{std::random_device{}()};

    std::uniform_int_distribution<int> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 8; ++i) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; ++i) ss << dis(gen);
    ss << "-";

    ss << "4";
    for (int i = 0; i < 3; ++i) ss << dis(gen);
    ss << "-";

    ss << (dis(gen) & 0x3 | 0x8);
    for (int i = 0; i < 3; ++i) ss << dis(gen);
    ss << "-";

    for (int i = 0; i < 12; ++i) ss << dis(gen);

    return ss.str();
}

} // namespace db_util
