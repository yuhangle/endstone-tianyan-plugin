//
// Created by yuhang on 2025/5/17.
//
#pragma once

#include <endstone/plugin/plugin.h>
#include <string>
#include <fstream>
#include <mutex>
#include "database_backend.h"
#include "tianyan_protect.h"
#include "event_listener.h"
#include "menu.h"
#include "translate.hpp"
#include <tianyan/api.h>

class StaticTranslate
{
public:
    static std::string get(const std::string& key);
};

class TianyanPlugin : public endstone::Plugin, public tianyan::ITianyanAPI{
protected:
    // 实现受保护的底层查询
    std::vector<tianyan::LogData> getLogDataSyncImpl(double seconds, int limit) override;
public:
    //语言
    static inline std::unique_ptr<translate> Tran;
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
    void logsCacheWrite() const;

    //检查异步的数据库清理状态
    void checkDatabaseCleanStatus() const;

    //检查后台查询任务
    void checkAsyncTasks();

    // 批量更新回溯状态
    void updateRevertStatus() const;

    //api
    int getApiVersion() const override {
        return tianyan::TIANYAN_API_VERSION;
    }

    std::future<std::vector<tianyan::LogData>> getLogDataAsync(double hours) override;
    static std::vector<tianyan::LogData> processLogConversion(const std::vector<TianyanCore::LogData>& source, int limit);
    [[nodiscard]] std::vector<tianyan::LogData> getLogDataSync(double hours, int limit) const;

private:
    struct AsyncQueryTask {
        enum class Type { Ty, Tys, Tyback };
        Type type;
        uint64_t id = 0;
        std::string player_name;
        bool is_running = false;
        bool is_complete = false;
        bool cancelled = false;
        std::vector<TianyanCore::LogData> results;

        double r = 0;
        double hours = 0;
        std::string world;
        double x = 0, y = 0, z = 0;
        std::string key_type;
        std::string key;
    };

    //初始化其它实例
    std::unique_ptr<IDatabaseBackend> db_backend_;
    std::unique_ptr<TianyanCore> tyCore;
    std::unique_ptr<TianyanProtect> protect_;
    std::unique_ptr<EventListener> eventListener_;
    std::unique_ptr<Menu> menu_;
    string clean_data_sender_name;
    std::vector<AsyncQueryTask> async_tasks_;
    std::mutex async_tasks_mutex_;
    uint64_t next_task_id_ = 1;
    shared_ptr<endstone::Task> windows_print_webui_log;
#ifdef _WIN32
    void dump_webui_log_once() const;
#endif
};