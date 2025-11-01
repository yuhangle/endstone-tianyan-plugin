//
// Created by yuhang on 2025/5/17.
//

#include "tianyan_plugin.h"

ENDSTONE_PLUGIN("tianyan_plugin", "1.2.0dev2", TianyanPlugin)
{
    description = "A plugin for endstone to record behavior";

        command("ty")
            .description(Tran.getLocal("Check behavior logs at your current location"))
            .usages("/ty",
                    "/ty <r: float> <time: float>"
                    )
            .permissions("ty.command.member");

        command("tyback")
            .description(Tran.getLocal("Revert behaviors by time"))
            .usages("/tyback",
                    "/tyback <r: float> <time: float>"
                    )
            .permissions("ty.command.op");

    permission("ty.command.member")
            .description("Allow users to use the /ty command.")
            .default_(endstone::PermissionDefault::True);
    permission("ty.command.op")
        .description("OP command.")
        .default_(endstone::PermissionDefault::Operator);
}