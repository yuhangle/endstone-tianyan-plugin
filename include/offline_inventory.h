#pragma once

#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct OfflineItem {
    int slot = 0;
    std::string name;
    int count = 0;
    int damage = 0;
    json tag;
};

struct OfflinePlayerInventory {
    std::string player_key;
    std::vector<OfflineItem> inventory;
    std::vector<OfflineItem> armor;
    std::optional<OfflineItem> offhand;
};

class OfflineInventoryReader {
public:
    static constexpr std::string_view PLAYER_CACHE_FILE = "plugins/tianyan_data/players_cache.json";

    static std::string resolveWorldPath(const std::string& server_root);
    static json loadPlayerCache();
    static void savePlayerCache(const json& cache);
    static void updatePlayerCache(const std::string& player_name, const std::string& uuid);
    static std::optional<std::string> findUUIDByName(const std::string& player_name);

    OfflineInventoryReader() = delete;
};
