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

class TianyanPlugin : public endstone::Plugin {
public:
        //数据目录和配置文件检查
    void datafile_check() const;

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

    // 批量更新回溯状态
    static void updateRevertStatus();

private:
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
};