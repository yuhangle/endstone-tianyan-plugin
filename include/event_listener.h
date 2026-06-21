//
// Created by yuhang on 2025/11/6.
//

// ReSharper disable CppMemberFunctionMayBeConst
#ifndef TIANYAN_EVENTLISTENER_H
#define TIANYAN_EVENTLISTENER_H
#include <endstone/endstone.hpp>

#include "translate.hpp"

class TianyanPlugin;
class translate;


class EventListener {
public:
    explicit EventListener(TianyanPlugin* tianyan, translate* tran);

    void initOnlinePlayers();

    // 检查是否允许触发事件
    static bool canTriggerEvent(const std::string& playername);

    //方块被破坏
    static void onBlockBreak(const endstone::BlockBreakEvent& event);

    //方块被放置
    static void onBlockPlace(const endstone::BlockPlaceEvent& event);

    //实体受伤
    static void onActorDamage(const endstone::ActorDamageEvent& event);

    //玩家右键方块
    static void onPlayerRightClickBlock(const endstone::PlayerInteractEvent& event);

    //玩家右键实体
    static void onPlayerRightClickActor(const endstone::PlayerInteractActorEvent& event);

    //实体爆炸
    static void onActorBomb(const endstone::ActorExplodeEvent& event);

    //方块爆炸
    static void onBlockBomb(const endstone::BlockExplodeEvent& event);

    //活塞伸出
    static void onPistonExtend(const endstone::BlockPistonExtendEvent&event);

    //活塞收回
    static void onPistonRetract(const endstone::BlockPistonRetractEvent&event);

    //实体死了
    static void onActorDie(const endstone::ActorDeathEvent&event);

    //玩家死了
    static void onPlayerDie(const endstone::PlayerDeathEvent&event);

    //监听液体流动
    static void onBlockFromTo(const endstone::BlockFromToEvent& event);

    //监听玩家拾取
    void onPlayerPickup(const endstone::PlayerPickupItemEvent&event);

    //监听玩家丢出
    void onPlayerDropItem(const endstone::PlayerDropItemEvent& event);

    //玩家加入事件
    void onPlayerJoin(const endstone::PlayerJoinEvent &event);

    //玩家离开事件
    void onPlayerLeave(const endstone::PlayerQuitEvent &event);

    //刷屏检测
    void onPlayerSendMSG(const endstone::PlayerChatEvent &event);

    //大量命令刷屏检测
    void onPlayerSendCMD(const endstone::PlayerChatEvent &event);

    //拒绝封禁玩家加入
    void onPlayerTryJoin(const endstone::PlayerLoginEvent &event);

private:
    endstone::Plugin &plugin_;
    translate* tran_;
    std::unordered_set<const endstone::Player*> online_players_;
};

#endif //TIANYAN_EVENTLISTENER_H
