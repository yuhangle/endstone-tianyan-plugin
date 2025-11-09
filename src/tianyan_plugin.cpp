//
// Created by yuhang on 2025/5/17.
//

#include "tianyan_plugin.h"

ENDSTONE_PLUGIN("tianyan_plugin", "1.2.0-alpha.4", TianyanPlugin)
{
    description = "A plugin for endstone to record behavior";

        command("ty")
            .description(Tran.getLocal("Check behavior logs at your current location"))
            .usages("/ty",
                    "/ty <r: float> <time: float>",
                    "/ty <r: float> <time: float> <source_id | source_name | target_id | target_name> <keywords: str>",
                    "/ty <r: float> <time: float> <action> <block_break | block_place | entity_damage | player_right_click_block | player_right_click_entity | entity_bomb | block_break_bomb | piston_extend | piston_retract | entity_die | player_pickup_item>"
                    )
            .permissions("ty.command.member");

        command("tyback")
            .description(Tran.getLocal("Revert behaviors by time"))
            .usages("/tyback",
                    "/tyback <r: float> <time: float>",
                    "/tyback <r: float> <time: float> <source_id | source_name | target_id | target_name> <keywords: str>",
                    "/tyback <r: float> <time: float> <action> <block_break | block_place | player_right_click_block | block_break_bomb | entity_die>"
                    )
            .permissions("ty.command.op");

        command("tys")
            .description(Tran.getLocal("Check behavior logs at server"))
            .usages("/tys",
                    "/tys <time: float>",
                    "/tys <time: float> <source_id | source_name | target_id | target_name> <keywords: str>",
                    "/tys <time: float> <action> <block_break | block_place | entity_damage | player_right_click_block | player_right_click_entity | entity_bomb | block_break_bomb | piston_extend | piston_retract | entity_die | player_pickup_item>"
                    )
            .permissions("ty.command.op");

        command("ban-id")
            .description(Tran.getLocal("Ban player by device id"))
            .usages("/ban-id <device-id: str> [reason: str]"
                    )
            .permissions("ty.command.op");

        command("unban-id")
            .description(Tran.getLocal("Unban player by device id"))
            .usages("/unban-id <device-id: str>"
                    )
            .permissions("ty.command.op");

        command("banlist-id")
            .description(Tran.getLocal("List baned player by device id"))
            .usages("/banlist-id"
                    )
            .permissions("ty.command.op");

        command("tyclean")
            .description(Tran.getLocal("Clean database"))
            .usages("/tyclean <time: int>"
                    )
            .permissions("ty.command.op");

    permission("ty.command.member")
            .description("Allow users to use the /ty command.")
            .default_(endstone::PermissionDefault::True);
    permission("ty.command.op")
        .description("OP command.")
        .default_(endstone::PermissionDefault::Operator);
}