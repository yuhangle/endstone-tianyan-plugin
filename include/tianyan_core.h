//
// Created by yuhang on 2025/10/26.
//

#ifndef TIANYAN_TIANYANCORE_H
#define TIANYAN_TIANYANCORE_H
#include "database_backend.h"
#include <chrono>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace std;
using TimePoint = std::chrono::steady_clock::time_point;

class IDatabaseBackend;

class TianyanCore {
public:
    explicit TianyanCore(IDatabaseBackend& db);
    //文件目录
    static constexpr std::string_view dataPath = "plugins/tianyan_data";
    static inline const string language_path = "plugins/tianyan_data/language/";
    static inline const string dbPath = "plugins/tianyan_data/ty_data.db";
    static inline const string config_path = "plugins/tianyan_data/config.json";
    static inline const string ban_id_path = "plugins/tianyan_data/ban-id.json";
    static inline string language_file = language_path + "en_US";
    //配置变量
    static inline int max_message_in_10s;
    static inline int max_command_in_10s;
    static inline vector<string> no_log_mobs;
    static inline bool enable_web_ui = false;
    // ---- 记录配置 ----
    // 严格模式记录实体
    static inline bool config_enforce_no_log_mobs = false;
    // 是否记录活塞推拉事件
    static inline bool config_log_piston = true;
    // 是否记录实体爆炸事件
    static inline bool config_log_entity_bomb = true;
    // 是否记录方块爆炸事件
    static inline bool config_log_block_bomb = true;
    // 是否记录方块破坏事件
    static inline bool config_log_block_break = true;
    // 是否记录方块放置事件
    static inline bool config_log_block_place = true;
    // 是否记录实体受伤事件
    static inline bool config_log_entity_damage = true;
    // 是否记录玩家右键方块事件
    static inline bool config_log_player_right_click_block = true;
    // 是否记录玩家右键实体事件
    static inline bool config_log_player_right_click_entity = true;
    // 是否记录实体死亡事件
    static inline bool config_log_entity_die = true;
    // 是否记录玩家拾取物品事件
    static inline bool config_log_player_pickup_item = true;
    // 是否记录玩家丢弃物品事件
    static inline bool config_log_player_drop_item = true;
    // 是否记录液体流动事件
    static inline bool config_log_liquid_flow = true;

    // 存储每个玩家的上次触发时间
    static inline std::unordered_map<string, TimePoint> lastTriggerTime;
    // 全局缓存：每个玩家的消息时间戳列表（仅保留最近10秒内的）
    static inline std::unordered_map<string, std::vector<TimePoint>> playerMessageTimes;
    // 全局缓存：每个玩家的命令时间戳列表（仅保留最近10秒内的）
    static inline std::unordered_map<string, std::vector<TimePoint>> playerCommandTimes;
    // 回溯状态缓存 - 存储需要标记为"reverted"的日志UUID和状态
    static inline vector<pair<string, string>> revertStatusCache;

    //记录数据结构
    struct LogData {
        string uuid;
        string id;
        string name;
        double pos_x = -1.0;
        double pos_y = -320.0;
        double pos_z = -1.0;
        string world;
        string obj_id;
        string obj_name;
        long long time = 0;
        string type;
        string data;
        string status;
    };

    //封禁设备ID玩家数据
    struct BanIDPlayer {
        string player_name;
        string device_id;
        optional<string> reason;
        string time;
    };

    // 计算实体密度的结构体
    struct DensityResult {
        std::optional<std::string> dim;
        std::optional<double> mid_x, mid_y, mid_z;
        std::optional<int> count;
        std::optional<std::string> entity_type;
        std::optional<std::string> entity_pos;
        std::optional<double> entity_pos_x, entity_pos_y, entity_pos_z;
    };


    //将字符串形式的Unix时间戳转换为 2 long 类型
    static long long stringToTimestamp(const std::string& timestampStr) ;

    //将时间戳转化为人类时间
    static std::string timestampToString(long long timestamp);

    //记录日志
    [[nodiscard]] int recordLog(const LogData& logData) const;

    //批量记录日志
    [[nodiscard]] int recordLogs(const std::vector<LogData>& logDatas) const;

    //查询日志
    [[nodiscard]] vector<LogData> searchLog(const pair<string,double>& key, atomic<bool>* cancel = nullptr) const;

    //查询日志并在指定世界和坐标范围内筛选
    [[nodiscard]] vector<LogData> searchLog(const pair<string,double>& key, double x, double y, double z, double r, const string& world, atomic<bool>* cancel = nullptr) const;

    // 记录玩家发送了一条消息（自动清理过期记录）
    static int recordPlayerSendMSG(const string& player_name);

    // 检查玩家在10秒内是否发送消息超过6条（即 ≥7 条）
    static bool checkPlayerSendMSG(const string& player_name);

    // 记录玩家发送了一条消息（自动清理过期记录）
    static int recordPlayerSendCMD(const string& player_name);

    // 检查玩家在10秒内是否发送消息超过6条（即 ≥7 条）
    static bool checkPlayerSendCMD(const string& player_name);
private:
    IDatabaseBackend& db_backend_;
};


#endif //TIANYAN_TIANYANCORE_H