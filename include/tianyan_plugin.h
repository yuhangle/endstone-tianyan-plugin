//
// Created by yuhang on 2025/5/17.
//
#pragma once

#include <endstone/plugin/plugin.h>
#include <string>
#include <fstream>
#include "global.h"
#include "database.hpp"
#include "tianyan_core.h"
#include "tianyan_protect.h"
#include "event_listener.h"
#include "menu.h"
#include <mutex>
#include <unordered_set>

class TianyanPlugin : public endstone::Plugin {
public:
    //数据目录和配置文件检查
    void datafile_check() const;

    //python版本封禁数据迁移
    static void migrateOldBanData();

    // 读取配置文件
    [[nodiscard]] json read_config() const;

    void onLoad() override;

    void onEnable() override;

    void onDisable() override;

    bool onCommand(endstone::CommandSender &sender, const endstone::Command &command, const std::vector<std::string> &args) override;

        //缓存写入机制
    static void logsCacheWrite();

    //检查异步的数据库清理状态
    void checkDatabaseCleanStatus() const;

    //检查tyback后台
    void checkTybackSearchThread();

    //处理后台日志查询结果
    void flushPendingLogQueryResults();

    // 批量更新回溯状态
    static void updateRevertStatus();

private:
    struct PendingLogQueryResult {
        string player_name;
        vector<TianyanCore::LogData> search_data;
        string search_key;
        bool has_keyword_filter = false;
        string error_message;
    };

    bool beginPlayerLogQuery(const string &player_name);

    void finishPlayerLogQuery(const string &player_name);

    std::unique_ptr<TianyanProtect> protect_;
    std::unique_ptr<EventListener> eventListener_;
    std::unique_ptr<Menu> menu_;
    string clean_data_sender_name;
    struct TybackCache {
        bool status = false;
        bool is_running = false;
        string player_name;
        vector<TianyanCore::LogData> logDatas;
        double r,time;
    };
    TybackCache tyback_cache = {false};
    std::mutex pending_log_query_mutex_;
    std::unordered_set<string> pending_log_queries_;
    std::mutex completed_log_query_mutex_;
    vector<PendingLogQueryResult> completed_log_query_results_;
    shared_ptr<endstone::Task> windows_print_webui_log;
#ifdef _WIN32
    void dump_webui_log_once() const;
#endif
};
