//
// Created by yuhang on 2025/11/6.
//

#ifndef TIANYAN_COMMANDS_H
#define TIANYAN_COMMANDS_H
#include "global.h"
#include <endstone/endstone.hpp>

class Menu{
public:
    explicit Menu(endstone::Plugin &plugin) : plugin_(plugin) {}
    
    //日志展示菜单
    static void showLogMenu(endstone::Player &player, const std::vector<TianyanCore::LogData>& logDatas, int page = 0);

    //tyback菜单
    static void tybackMenu(const endstone::Player &sender);

    //tys 菜单
    static void tysMenu(const endstone::Player &sender);

    //ty菜单
    static void tyMenu(const endstone::Player &sender);

    //查看在线玩家物品栏函数
    static void showOnlinePlayerBag(const endstone::CommandSender &sender, const endstone::Player& player);

    //查找实体密度高区域
    void findHighDensityRegion(endstone::Player &player, int size = 20) const;


private:
    endstone::Plugin &plugin_;
};


#endif //TIANYAN_COMMANDS_H