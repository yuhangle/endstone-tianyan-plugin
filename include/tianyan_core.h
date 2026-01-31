//
// Created by yuhang on 2025/10/26.
//

#ifndef TIANYAN_TIANYANCORE_H
#define TIANYAN_TIANYANCORE_H
#include "database.hpp"
#include <unordered_map>

using namespace std;
using TimePoint = std::chrono::steady_clock::time_point;
//文件目录
inline string dataPath = "plugins/tianyan_data";
inline string language_path = "plugins/tianyan_data/language/";
inline string dbPath = "plugins/tianyan_data/ty_data.db";
inline string config_path = "plugins/tianyan_data/config.json";
inline string ban_id_path = "plugins/tianyan_data/ban-id.json";
//配置变量
inline int max_message_in_10s;
inline int max_command_in_10s;
inline vector<string> no_log_mobs;
inline bool enable_web_ui = false;
// 存储每个玩家的上次触发时间
inline std::unordered_map<string, TimePoint> lastTriggerTime;
// 全局缓存：每个玩家的消息时间戳列表（仅保留最近10秒内的）
inline std::unordered_map<string, std::vector<TimePoint>> playerMessageTimes;
// 全局缓存：每个玩家的命令时间戳列表（仅保留最近10秒内的）
inline std::unordered_map<string, std::vector<TimePoint>> playerCommandTimes;
// 回溯状态缓存 - 存储需要标记为"reverted"的日志UUID和状态
inline vector<pair<string, string>> revertStatusCache;

class TianyanCore {
public:
    explicit TianyanCore(yuhangle::Database database);

    //记录数据结构
    struct LogData {
        string uuid;
        string id;
        string name;
        double pos_x;
        double pos_y;
        double pos_z;
        string world;
        string obj_id;
        string obj_name;
        long long time;
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
    [[nodiscard]] vector<LogData> searchLog(const pair<string,double>& key) const;
    
    //查询日志并在指定世界和坐标范围内筛选
    [[nodiscard]] vector<LogData> searchLog(const pair<string,double>& key, double x, double y, double z, double r, const string& world, bool if_max = false) const;

    // 记录玩家发送了一条消息（自动清理过期记录）
    static int recordPlayerSendMSG(const string& player_name);

    // 检查玩家在10秒内是否发送消息超过6条（即 ≥7 条）
    static bool checkPlayerSendMSG(const string& player_name);

    // 记录玩家发送了一条消息（自动清理过期记录）
    static int recordPlayerSendCMD(const string& player_name);

    // 检查玩家在10秒内是否发送消息超过6条（即 ≥7 条）
    static bool checkPlayerSendCMD(const string& player_name);
private:
    yuhangle::Database Database;
};


#endif //TIANYAN_TIANYANCORE_H