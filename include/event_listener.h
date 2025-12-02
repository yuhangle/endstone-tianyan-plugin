//
// Created by yuhang on 2025/11/6.
//

// ReSharper disable CppMemberFunctionMayBeConst
#ifndef TIANYAN_EVENTLISTENER_H
#define TIANYAN_EVENTLISTENER_H
#include "global.h"
#include <endstone/endstone.hpp>


class EventListener {
public:
    explicit EventListener(endstone::Plugin& plugin) : plugin_(plugin) {}

    // 检查是否允许触发事件
    static bool canTriggerEvent(const std::string& playername);

    static void onBlockBreak(const endstone::BlockBreakEvent& event);

    static void onBlockPlace(const endstone::BlockPlaceEvent& event);

    static void onActorDamage(const endstone::ActorDamageEvent& event);

    static void onPlayerRightClickBlock(const endstone::PlayerInteractEvent& event);

    static void onPlayerRightClickActor(const endstone::PlayerInteractActorEvent& event);

    static void onActorBomb(const endstone::ActorExplodeEvent& event);

    static void onPistonExtend(const endstone::BlockPistonExtendEvent&event);

    static void onPistonRetract(const endstone::BlockPistonRetractEvent&event);

    static void onActorDie(const endstone::ActorDeathEvent&event);

    static void onPlayerDie(const endstone::PlayerDeathEvent&event);

    static void onPlayPickup(const endstone::PlayerPickupItemEvent&event);

    //玩家加入事件
    void onPlayerJoin(const endstone::PlayerJoinEvent &event);

    //刷屏检测
    void onPlayerSendMSG(const endstone::PlayerChatEvent &event);

    //大量命令刷屏检测
    void onPlayerSendCMD(const endstone::PlayerChatEvent &event);

    //拒绝封禁玩家加入
    void onPlayerTryJoin(const endstone::PlayerLoginEvent &event);

private:
    endstone::Plugin &plugin_;
};

#endif //TIANYAN_EVENTLISTENER_H