//
// Created by yuhang on 2025/10/26.
//

#ifndef TIANYAN_TIANYANCORE_H
#define TIANYAN_TIANYANCORE_H
#include "DataBase.hpp"
#include <iomanip>  // 👈 Windows需要
#include <chrono>  // 👈 Windows需要
#include <ctime>   // 👈 Windows需要
using namespace std;

class TianyanCore {
public:
    explicit TianyanCore(DataBase database);

    //记录数据结构
    struct LogData {
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
    [[nodiscard]] vector<LogData> searchLog(const pair<string,double>& key, double x, double y, double z, double r, const string& world) const;
private:
    DataBase Database;
};


#endif //TIANYAN_TIANYANCORE_H