//
// Created by yuhang on 2025/12/2.
//
// ReSharper disable CppMemberFunctionMayBeConst
#include "event_listener.h"
#include "tianyan_protect.h"
#include <tianyan_plugin.h>
#include <translate.hpp>
#include "database_util.h"

EventListener::EventListener(TianyanPlugin* tianyan, translate* tran)
    :plugin_(*tianyan), tran_(tran)
{}


void EventListener::initOnlinePlayers()
{
    online_players_.clear();
    for (const auto* p : plugin_.getServer().getOnlinePlayers()) {
        online_players_.insert(p);
    }
}

// 检查是否允许触发事件
bool EventListener::canTriggerEvent(const string& playername) {
    const auto now = std::chrono::steady_clock::now();

    // 查找玩家的上次触发时间
    if (TianyanCore::lastTriggerTime.contains(playername)) {
        const auto lastTime = TianyanCore::lastTriggerTime[playername];

        // 如果时间差小于 0.1 秒，不允许触发
        if (const auto elapsedTime = std::chrono::duration<double>(now - lastTime).count(); elapsedTime < 0.1) {
            return false;
        }
    }

    // 更新玩家的上次触发时间为当前时间
    TianyanCore::lastTriggerTime[playername] = now;
    return true;
}

void EventListener::onBlockBreak(const endstone::BlockBreakEvent& event){
    TianyanCore::LogData logData;
    logData.uuid = db_util::generate_uuid_v4(); // 生成UUID
    logData.id = event.getPlayer().getType();
    logData.name = event.getPlayer().getName();
    logData.pos_x = event.getBlock().getX();
    logData.pos_y = event.getBlock().getY();
    logData.pos_z = event.getBlock().getZ();
    logData.world = event.getBlock().getLocation().getDimension().getName();
    logData.obj_id = event.getBlock().getType();
    logData.time = std::time(nullptr);
    logData.type = "block_break";
    if (event.isCancelled()) {
        logData.status = "canceled";
    }
    const auto block_data = event.getBlock().getData();
    auto block_states = block_data->getBlockStates();
    logData.data = fmt::format("{}", block_states);
    {
        std::lock_guard lock(cacheMutex);
        logDataCache.push_back(logData);
    }
}

void EventListener::onBlockPlace(const endstone::BlockPlaceEvent& event){
    TianyanCore::LogData logData;
    logData.uuid = db_util::generate_uuid_v4(); // 生成UUID
    logData.id = event.getPlayer().getType();
    logData.name = event.getPlayer().getName();
    logData.pos_x = event.getBlockPlacedState().getX();
    logData.pos_y = event.getBlockPlacedState().getY();
    logData.pos_z = event.getBlockPlacedState().getZ();
    logData.world = event.getBlockPlacedState().getLocation().getDimension().getName();
    logData.obj_id = event.getBlockPlacedState().getType();
    logData.time = std::time(nullptr);
    logData.type = "block_place";
    if (event.isCancelled()) {
        logData.status = "canceled";
    }
    const auto block_data = event.getBlockPlacedState().getData();
    auto block_states = block_data->getBlockStates();
    logData.data = fmt::format("{}", block_states);
    {
        std::lock_guard lock(cacheMutex);
        logDataCache.push_back(logData);
    }
}

void EventListener::onActorDamage(const endstone::ActorDamageEvent& event){
    //无名的烂大街生物无需在意
    if (event.getActor().getNameTag().empty() && ranges::find(TianyanCore::no_log_mobs, event.getActor().getType()) != TianyanCore::no_log_mobs.end()) {
        return;
    }
    TianyanCore::LogData logData;
    logData.uuid = db_util::generate_uuid_v4(); // 生成UUID

    //实体造成伤害
    if (event.getDamageSource().getActor()) {
        logData.id = event.getDamageSource().getActor()->getType();
        logData.name = event.getDamageSource().getActor()->getName();
        logData.type = "entity_damage";
    }
    //其余伤害
    else {
        logData.id = event.getDamageSource().getType();
        logData.type = "damage";
    }
    logData.pos_x = event.getActor().getLocation().getX();
    logData.pos_y = event.getActor().getLocation().getY();
    logData.pos_z = event.getActor().getLocation().getZ();
    logData.world = event.getActor().getLocation().getDimension().getName();
    logData.obj_id = event.getActor().getType();
    logData.obj_name = event.getActor().getName();
    logData.time = std::time(nullptr);
    if (event.isCancelled()) {
        logData.status = "canceled";
    }
    auto damage = event.getDamage();
    logData.data = fmt::format("{}", damage);
    {
        std::lock_guard lock(cacheMutex);
        logDataCache.push_back(logData);
    }
}

void EventListener::onPlayerRightClickBlock(const endstone::PlayerInteractEvent& event) {
    TianyanCore::LogData logData;
    if (!event.getBlock()) {
        return;
    }
    //对特定物品的交互进行记录
    bool danger_item = false;
    if (event.hasItem())
    {
        static constexpr std::array<std::string_view, 3> dangerKeywords = {
            "end_crystal", "flint_and_steel", "fire_charge"
        };
        const auto item_id = event.getItem()->getType().getId().getKey();

        auto containsKeyword = [&item_id](const std::string_view kw) {
            return item_id.find(kw) != std::string::npos;
        };

        if (ranges::any_of(dangerKeywords, containsKeyword)) {
            danger_item = true;
        }
    }
    if (event.getBlock()->getData()->getBlockStates().empty() && !danger_item)
    {
        return;
    }

    if (!canTriggerEvent(event.getPlayer().getName())) {
        return;
    }
    logData.uuid = db_util::generate_uuid_v4(); // 生成UUID
    logData.id = event.getPlayer().getType();
    logData.name = event.getPlayer().getName();
    logData.pos_x = event.getBlock()->getX();
    logData.pos_y = event.getBlock()->getY();
    logData.pos_z = event.getBlock()->getZ();
    logData.world = event.getPlayer().getLocation().getDimension().getName();
    logData.obj_id = event.getBlock()->getType();
    logData.time = std::time(nullptr);
    logData.type = "player_right_click_block";
    if (event.isCancelled()) {
        logData.status = "canceled";
    }
    string hand_item;
    if (event.hasItem()) {
        hand_item = event.getItem()->getType().getId();
    } else {
        hand_item = "hand";
    }
    const auto block_data = event.getBlock()->getData();
    auto block_states = block_data->getBlockStates();
    logData.data = fmt::format("{},{}", hand_item,block_states);
    {
        std::lock_guard lock(cacheMutex);
        logDataCache.push_back(logData);
    }
}

void EventListener::onPlayerRightClickActor(const endstone::PlayerInteractActorEvent& event){
    //无名的烂大街生物无需在意
    if (event.getActor().getNameTag().empty() && ranges::find(TianyanCore::no_log_mobs, event.getActor().getType()) != TianyanCore::no_log_mobs.end()) {
        return;
    }
    TianyanCore::LogData logData;
    logData.uuid = db_util::generate_uuid_v4(); // 生成UUID
    logData.id = event.getPlayer().getType();
    logData.name = event.getPlayer().getName();
    logData.pos_x = event.getActor().getLocation().getX();
    logData.pos_y = event.getActor().getLocation().getY();
    logData.pos_z = event.getActor().getLocation().getZ();
    logData.world = event.getActor().getLocation().getDimension().getName();
    logData.obj_id = event.getActor().getType();
    logData.obj_name = event.getActor().getName();
    logData.time = std::time(nullptr);
    logData.type = "player_right_click_entity";
    if (event.isCancelled()) {
        logData.status = "canceled";
    }
    string hand_item;
    if (event.getPlayer().getInventory().getItemInMainHand()) {
        hand_item = event.getPlayer().getInventory().getItemInMainHand()->getType().getId();
    } else {
        hand_item = "hand";
    }
    logData.data = fmt::format("{}", hand_item);
    {
        std::lock_guard lock(cacheMutex);
        logDataCache.push_back(logData);
    }
}

void EventListener::onActorBomb(const endstone::ActorExplodeEvent& event) {
    TianyanCore::LogData logData;
    logData.uuid = db_util::generate_uuid_v4(); // 生成UUID
    logData.id = event.getActor().getType();
    logData.name = event.getActor().getName();
    logData.pos_x = event.getLocation().getX();
    logData.pos_y = event.getLocation().getY();
    logData.pos_z = event.getLocation().getZ();
    logData.world = event.getLocation().getDimension().getName();
    if (!event.getBlockList().empty()) {
        logData.obj_name = "Block";
    }
    logData.time = std::time(nullptr);
    logData.type = "entity_bomb";
    auto block_num = event.getBlockList().size();
    if (event.isCancelled()) {
        logData.status = "canceled";
        {
            std::lock_guard lock(cacheMutex);
            logDataCache.push_back(logData);
        }
    } else {
        logData.data = fmt::format("{}", block_num);
        {
            std::lock_guard lock(cacheMutex);
            logDataCache.push_back(logData);
        }
        for (const auto& block : event.getBlockList()) {
            // 方块类型
            TianyanCore::LogData bomb_data;
            bomb_data.uuid = db_util::generate_uuid_v4(); // 生成UUID
            bomb_data.id = event.getActor().getType();
            bomb_data.name = event.getActor().getName();
            bomb_data.pos_x = block->getX();
            bomb_data.pos_y = block->getY();
            bomb_data.pos_z = block->getZ();
            bomb_data.world = block->getLocation().getDimension().getName();
            bomb_data.obj_id = block->getType();
            bomb_data.time = std::time(nullptr);
            bomb_data.type = "block_break_bomb";
            bomb_data.data = fmt::format("{}", block->getData()->getBlockStates());
            {
                std::lock_guard lock(cacheMutex);
                logDataCache.push_back(bomb_data);
            }
        }
    }
}

void EventListener::onBlockBomb(const endstone::BlockExplodeEvent& event)
{
    TianyanCore::LogData logData;
    logData.uuid = db_util::generate_uuid_v4();
    logData.id = event.getBlock().getType();
    logData.pos_x = event.getBlock().getX();
    logData.pos_y = event.getBlock().getY();
    logData.pos_z = event.getBlock().getZ();
    logData.world = event.getBlock().getLocation().getDimension().getName();
    if (!event.getBlockList().empty()) {
        logData.obj_name = "Block";
    }
    logData.time = std::time(nullptr);
    logData.type = "block_bomb";
    auto block_num = event.getBlockList().size();
    if (event.isCancelled()) {
        logData.status = "canceled";
        {
            std::lock_guard lock(cacheMutex);
            logDataCache.push_back(logData);
        }
    } else {
        logData.data = fmt::format("{}", block_num);
        {
            std::lock_guard lock(cacheMutex);
            logDataCache.push_back(logData);
        }
        for (const auto& block : event.getBlockList()) {
            TianyanCore::LogData bomb_data;
            if (block->getType() == "minecraft:air") {continue;}
            bomb_data.uuid = db_util::generate_uuid_v4();
            bomb_data.id = event.getBlock().getType();
            bomb_data.pos_x = block->getX();
            bomb_data.pos_y = block->getY();
            bomb_data.pos_z = block->getZ();
            bomb_data.world = block->getLocation().getDimension().getName();
            bomb_data.obj_id = block->getType();
            bomb_data.time = std::time(nullptr);
            bomb_data.type = "block_break_bomb";
            bomb_data.data = fmt::format("{}", block->getData()->getBlockStates());
            {
                std::lock_guard lock(cacheMutex);
                logDataCache.push_back(bomb_data);
            }
        }
    }
}

void EventListener::onPistonExtend(const endstone::BlockPistonExtendEvent&event) {
    TianyanCore::LogData logData;
    logData.uuid = db_util::generate_uuid_v4(); // 生成UUID
    logData.id = event.getBlock().getType();
    logData.pos_x = event.getBlock().getX();
    logData.pos_y = event.getBlock().getY();
    logData.pos_z = event.getBlock().getZ();
    logData.world = event.getBlock().getLocation().getDimension().getName();
    logData.time = std::time(nullptr);
    logData.type = "piston_extend";
    const auto Face = event.getDirection();
    string direct;
    if (Face == endstone::BlockFace::Down) {
        direct = "down";
    } else if (Face == endstone::BlockFace::Up) {
        direct = "up";
    } else if (Face == endstone::BlockFace::North) {
        direct = "north";
    } else if (Face == endstone::BlockFace::South) {
        direct = "south";
    } else if (Face == endstone::BlockFace::West) {
        direct = "west";
    } else if (Face == endstone::BlockFace::East) {
        direct = "east";
    }
    if (event.isCancelled()) {
        logData.status = "canceled";
    }
    logData.data = fmt::format("{},{}", direct, event.getBlock().getData()->getBlockStates());


    {
        std::lock_guard lock(cacheMutex);
        logDataCache.push_back(logData);
    }
}

void EventListener::onPistonRetract(const endstone::BlockPistonRetractEvent&event) {
    TianyanCore::LogData logData;
    logData.uuid = db_util::generate_uuid_v4(); // 生成UUID
    logData.id = event.getBlock().getType();
    logData.pos_x = event.getBlock().getX();
    logData.pos_y = event.getBlock().getY();
    logData.pos_z = event.getBlock().getZ();
    logData.world = event.getBlock().getLocation().getDimension().getName();
    logData.time = std::time(nullptr);
    logData.type = "piston_retract";
    const auto Face = event.getDirection();
    string direct;
    if (Face == endstone::BlockFace::Down) {
        direct = "down";
    } else if (Face == endstone::BlockFace::Up) {
        direct = "up";
    } else if (Face == endstone::BlockFace::North) {
        direct = "north";
    } else if (Face == endstone::BlockFace::South) {
        direct = "south";
    } else if (Face == endstone::BlockFace::West) {
        direct = "west";
    } else if (Face == endstone::BlockFace::East) {
        direct = "east";
    }
    if (event.isCancelled()) {
        logData.status = "canceled";
    }
    logData.data = fmt::format("{},{}", direct, event.getBlock().getData()->getBlockStates());

    {
        std::lock_guard lock(cacheMutex);
        logDataCache.push_back(logData);
    }
}

void EventListener::onActorDie(const endstone::ActorDeathEvent&event) {
    //无名的烂大街生物无需在意
    if (event.getActor().getNameTag().empty() && ranges::find(TianyanCore::no_log_mobs, event.getActor().getType()) != TianyanCore::no_log_mobs.end()) {
        return;
    }
    TianyanCore::LogData logData;
    logData.uuid = db_util::generate_uuid_v4(); // 生成UUID
    logData.pos_x = event.getActor().getLocation().getX();
    logData.pos_y = event.getActor().getLocation().getY();
    logData.pos_z = event.getActor().getLocation().getZ();
    logData.world = event.getActor().getLocation().getDimension().getName();
    logData.obj_id = event.getActor().getType();
    logData.obj_name = event.getActor().getName();
    logData.time = std::time(nullptr);
    logData.type = "entity_die";
    {
        std::lock_guard lock(cacheMutex);
        logDataCache.push_back(logData);
    }
}

void EventListener::onPlayerDie(const endstone::PlayerDeathEvent&event) {
    TianyanCore::LogData logData;
    logData.uuid = db_util::generate_uuid_v4(); // 生成UUID
    logData.pos_x = event.getActor().getLocation().getX();
    logData.pos_y = event.getActor().getLocation().getY();
    logData.pos_z = event.getActor().getLocation().getZ();
    logData.world = event.getActor().getLocation().getDimension().getName();
    logData.obj_id = event.getActor().getType();
    logData.obj_name = event.getActor().getName();
    logData.time = std::time(nullptr);
    logData.type = "entity_die";
    {
        std::lock_guard lock(cacheMutex);
        logDataCache.push_back(logData);
    }
}

void EventListener::onPlayerPickup(const endstone::PlayerPickupItemEvent&event) {
    TianyanCore::LogData logData;
    const endstone::Player& player = event.getPlayer();
    if (!online_players_.contains(&player)) {
        return;
    }
    logData.uuid = db_util::generate_uuid_v4();
    logData.id = player.getType();
    logData.name = player.getName();
    logData.pos_x = player.getLocation().getX();
    logData.pos_y = player.getLocation().getY();
    logData.pos_z = player.getLocation().getZ();
    logData.world = player.getLocation().getDimension().getName();
    logData.obj_id = event.getItem().getItemStack().getType().getId();
    logData.obj_name = event.getItem().getName();
    logData.time = std::time(nullptr);
    logData.type = "player_pickup_item";
    if (event.isCancelled()) {
        logData.status = "canceled";
    }
    {
        std::lock_guard lock(cacheMutex);
        logDataCache.push_back(logData);
    }
}

void EventListener::onPlayerDropItem(const endstone::PlayerDropItemEvent& event) {
    TianyanCore::LogData logData;

    const endstone::Player& player = event.getPlayer();
    if (!online_players_.contains(&player)) {
        return;
    }
    logData.uuid = db_util::generate_uuid_v4();
    logData.id = player.getType();
    logData.name = player.getName();
    logData.pos_x = player.getLocation().getX();
    logData.pos_y = player.getLocation().getY();
    logData.pos_z = player.getLocation().getZ();
    logData.world = player.getLocation().getDimension().getName();
    logData.obj_id = event.getItem().getType().getId();
    logData.time = std::time(nullptr);
    logData.type = "player_drop_item";
    if (event.isCancelled()) {
        logData.status = "canceled";
    }
    {
        std::lock_guard lock(cacheMutex);
        logDataCache.push_back(logData);
    }
}

//玩家加入事件
void EventListener::onPlayerJoin(const endstone::PlayerJoinEvent &event) {
    if (!event.getPlayer().asPlayer()) return;
    online_players_.insert(&event.getPlayer());

    // 缓存玩家名→UUID 映射
    OfflineInventoryReader::updatePlayerCache(
        event.getPlayer().getName(),
        event.getPlayer().getUniqueId().str()
    );

    std::ostringstream out;
    out << endstone::ColorFormat::Yellow << tran_->getLocal("Player") << " " << event.getPlayer().getName() << " " << tran_->getLocal("joined server!")
    << " " <<tran_->getLocal("Device OS: ")<< event.getPlayer().getDeviceOS() << " " << tran_->getLocal("Device ID: ") << event.getPlayer().getDeviceId();
    plugin_.getServer().getLogger().info(out.str());
}

//玩家离开事件
void EventListener::onPlayerLeave(const endstone::PlayerQuitEvent& event)
{
    online_players_.erase(&event.getPlayer());
}


//刷屏检测
void EventListener::onPlayerSendMSG(const endstone::PlayerChatEvent &event) {
    TianyanCore::recordPlayerSendMSG(event.getPlayer().getName());
    if (TianyanCore::checkPlayerSendMSG(event.getPlayer().getName())) {
        string reason = tran_->getLocal("Too many messages sent in a short time");
        plugin_.getServer().getBanList().addBan(event.getPlayer().getName(),reason, std::chrono::hours(24), "Tianyan Plugin");
        event.getPlayer().kick(reason);
        plugin_.getServer().broadcastMessage(endstone::ColorFormat::Yellow + tran_->getLocal("Player") + ": " + event.getPlayer().getName() + " " + tran_->getLocal("has been banned for sending too many messages in a short time"));
    }
}

//大量命令刷屏检测
void EventListener::onPlayerSendCMD(const endstone::PlayerChatEvent &event) {
    TianyanCore::recordPlayerSendCMD(event.getPlayer().getName());
    if (TianyanCore::checkPlayerSendCMD(event.getPlayer().getName())) {
        string reason = tran_->getLocal("Too many commands sent in a short time");
        plugin_.getServer().getBanList().addBan(event.getPlayer().getName(),reason, std::chrono::hours(24), "Tianyan Plugin");
        event.getPlayer().kick(reason);
        plugin_.getServer().broadcastMessage(endstone::ColorFormat::Yellow + tran_->getLocal("Player") + ": " + event.getPlayer().getName() + " " + tran_->getLocal("has been banned for sending too many commands in a short time"));
    }
}

//拒绝封禁玩家加入
void EventListener::onPlayerTryJoin(const endstone::PlayerLoginEvent &event) {
    const auto player_name = event.getPlayer().getName();
    const auto device_id = event.getPlayer().getDeviceId();
    //检查玩家设备是否已封禁
    if (const auto banData = TianyanProtect::getBannedPlayerByDeviceId(device_id);banData.has_value()) {
        const auto reason = banData.value().reason.value_or("");
        if (banData->player_name == "Null") {
            if (TianyanProtect::updatePlayerNameForDeviceId(banData.value().device_id,player_name)) {
                plugin_.getLogger().info(endstone::ColorFormat::Yellow+tran_->getLocal("Player")+": "+player_name + " " + tran_->getLocal("has been banned for using a banned device")+": " + banData.value().device_id);
            } else {
                plugin_.getLogger().error(tran_->getLocal("Unknow Error"));
            }
            event.getPlayer().kick(tran_->getLocal("Your device has been baned")+": "+reason);
        } else {
            event.getPlayer().kick(tran_->getLocal("Your device has been baned")+": "+reason);
            plugin_.getLogger().info(endstone::ColorFormat::Yellow+tran_->getLocal("Baned player: ")+player_name+" ("+device_id+") "+tran_->getLocal("try to join server"));
        }
    }
    //通过设备ID检查，检查是否使用过封禁设备
    else {
        if (TianyanProtect::isPlayerBanned(player_name)) {
            plugin_.getLogger().info(endstone::ColorFormat::Yellow+tran_->getLocal("Baned player: ")+player_name+" ("+device_id+") "+tran_->getLocal("try to join server"));
            string reason;
            for (const auto &baned_player : BanIDPlayers) {
                if (baned_player.player_name == player_name) {
                    reason = baned_player.reason.value_or("");
                    break;
                }
            }
            if (reason.empty()) {
                (void)plugin_.getServer().dispatchCommand(plugin_.getServer().getCommandSender(),"ban-id " + device_id);
            } else {
                (void)plugin_.getServer().dispatchCommand(plugin_.getServer().getCommandSender(),"ban-id " + device_id+ " \"" + reason +"\"");
            }
            event.getPlayer().kick(tran_->getLocal("Your device has been baned")+": "+reason);
        }
    }
}
