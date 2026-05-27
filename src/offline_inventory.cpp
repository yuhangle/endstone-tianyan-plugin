#include "offline_inventory.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>

// ── server.properties 解析 ──

static std::optional<std::string> readLevelName(const std::string& server_root) {
    std::string prop_path = server_root + "/server.properties";
    std::ifstream file(prop_path);
    if (!file.is_open()) return std::nullopt;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("level-name=", 0) == 0) {
            auto value = line.substr(11);
            if (auto trim_pos = value.find_last_not_of(" \t\r\n"); trim_pos != std::string::npos)
                value = value.substr(0, trim_pos + 1);
            return value;
        }
    }
    return std::nullopt;
}

std::string OfflineInventoryReader::resolveWorldPath(const std::string& server_root) {
    auto level_name = readLevelName(server_root);
    return server_root + "/worlds/" + (level_name ? *level_name : "Bedrock level");
}

// ── 玩家缓存 ──

json OfflineInventoryReader::loadPlayerCache() {
    const auto cache_path = std::string(PLAYER_CACHE_FILE);
    if (!std::filesystem::exists(cache_path)) return json::object();
    std::ifstream file(cache_path);
    if (!file.is_open()) return json::object();
    try { json cache; file >> cache; return cache; } catch (...) { return json::object(); }
}

void OfflineInventoryReader::savePlayerCache(const json& cache) {
    if (std::ofstream file{std::string(PLAYER_CACHE_FILE)}; file.is_open()) file << cache.dump(2);
}

void OfflineInventoryReader::updatePlayerCache(const std::string& player_name, const std::string& uuid) {
    auto cache = loadPlayerCache();
    cache[player_name] = uuid;
    savePlayerCache(cache);
}

std::optional<std::string> OfflineInventoryReader::findUUIDByName(const std::string& player_name) {
    if (auto cache = loadPlayerCache(); cache.contains(player_name) && cache[player_name].is_string())
        return cache[player_name].get<std::string>();
    return std::nullopt;
}
