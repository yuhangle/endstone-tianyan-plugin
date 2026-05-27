//
// Created by yuhang on 2025/12/2.
//

#include "menu.h"
#include "tianyan_protect.h"
#include <tianyan_plugin.h>
#include <translate.hpp>
#include "database_util.h"
#include "world_inspector.h"
#include <inventoryui_init.h>

Menu::Menu(TianyanPlugin* tianyan, translate* tran)
    :plugin_(*tianyan), tran_(tran)
{}


//日志展示菜单
void Menu::showLogMenu(endstone::Player &player, const std::vector<TianyanCore::LogData>& logDatas, const int page) {
    // 计算分页信息
    constexpr int logsPerPage = 30;
    const int totalPages = (static_cast<int>(logDatas.size()) + logsPerPage - 1) / logsPerPage;
    const int currentPage = std::min(page, std::max(0, totalPages - 1)); // 确保当前页在有效范围内

    endstone::ActionForm logMenu;
    logMenu.setTitle(fmt::format("{} - {} {}/{} {}", tran_->getLocal("Logs"), tran_->getLocal("The"),currentPage + 1, std::max(1, totalPages),tran_->getLocal("Page")));

    string showLogs;

    // 如果有日志数据，则显示当前页的日志
    if (!logDatas.empty() && currentPage < totalPages) {
        const int startIndex = currentPage * logsPerPage;
        const int endIndex = std::min(startIndex + logsPerPage, static_cast<int>(logDatas.size()));

        // 只显示当前页的日志（倒序显示）
        std::vector<TianyanCore::LogData> pageLogs;
        for (int i = endIndex - 1; i >= startIndex; --i) {
            pageLogs.push_back(logDatas[i]);
        }

        for (const auto& logData : pageLogs) {
            // 构建要显示的日志信息
            std::vector<std::pair<std::string, std::string>> logFields;

            // 名字不为空时添加源名称
            if (!logData.name.empty()) {
                logFields.emplace_back(tran_->getLocal("Source Name"), logData.name);
            }

            // 根据是否是玩家添加不同类型的信息
            if (logData.id == "minecraft:player") {
                logFields.emplace_back(tran_->getLocal("Source Type"), tran_->getLocal("Player"));
            }
            else if (!logData.id.empty()){
                logFields.emplace_back(tran_->getLocal("Source ID"), logData.id);
            }

            // 添加行为类型
            logFields.emplace_back(tran_->getLocal("Action"), tran_->getLocal(logData.type));

            // 添加位置信息
            logFields.emplace_back(tran_->getLocal("Position"),
                                  fmt::format("{:.2f},{:.2f},{:.2f}", logData.pos_x, logData.pos_y, logData.pos_z));

            //添加维度信息
            logFields.emplace_back(tran_->getLocal("Dimension"), logData.world);

            // 添加时间信息
            logFields.emplace_back(tran_->getLocal("Time"), TianyanCore::timestampToString(logData.time));

            // 根据目标名称和ID是否存在添加相应信息
            if (!logData.obj_name.empty()) {
                logFields.emplace_back(tran_->getLocal("Target Name"), tran_->getLocal(logData.obj_name));
            }

            if (!logData.obj_id.empty()) {
                logFields.emplace_back(tran_->getLocal("Target ID"), logData.obj_id);
            }

            // 添加数据信息
            if (!logData.data.empty()){
                //对手持物品交互进行处理
                if (logData.type == "player_right_click_block") {
                    auto hand_block = db_util::splitString(logData.data);
                    logFields.emplace_back(tran_->getLocal("Item in Hand"), tran_->getLocal(hand_block[0]));
                    if (hand_block[1] != "[]") {
                        logFields.emplace_back(tran_->getLocal("Data"), hand_block[1]);
                    }
                } else {
                    if (logData.data != "[]") {
                        logFields.emplace_back(tran_->getLocal("Data"), logData.data);
                    }
                }
            }

            //事件是否取消或被恢复
            if (logData.status == "canceled") {
                logFields.emplace_back(tran_->getLocal("Status"), tran_->getLocal("This event has been canceled"));
            } else if (logData.status == "reverted") {
                logFields.emplace_back(tran_->getLocal("Status"), tran_->getLocal("This event has been reverted"));
            }

            // 格式化输出
            for (const auto&[fst, snd] : logFields) {
                showLogs += fmt::format("{}: {}\n", fst, snd);
            }

            showLogs += "--------------------------------\n\n";
        }
    } else {
        // 没有日志数据
        showLogs = tran_->getLocal("No log found");
    }

    logMenu.setContent(endstone::ColorFormat::Yellow + showLogs);

    // 设置翻页按钮
    std::vector<std::variant<endstone::Button, endstone::Divider, endstone::Header, endstone::Label>> controls;

    // 添加上一页按钮（如果有多页）
    if (totalPages > 1) {
        endstone::Button pageUp;
        pageUp.setText(tran_->getLocal("Page Up"));
        pageUp.setOnClick([this, &player, currentPage, logDatas, totalPages](endstone::Player*) {
            // 如果是第一页，则跳转到最后一页；否则上一页
            const int prevPage = (currentPage == 0) ? (totalPages - 1) : (currentPage - 1);
            showLogMenu(player, logDatas, prevPage);
        });
        controls.emplace_back(pageUp);
    }

    // 添加下一页按钮（如果有多页）
    if (totalPages > 1) {
        endstone::Button pageDown;
        pageDown.setText(tran_->getLocal("Page Down"));
        pageDown.setOnClick([this, &player,currentPage, totalPages, logDatas](endstone::Player*) {
            // 如果是最后一页，则回到第一页；否则下一页
            const int nextPage = (currentPage + 1) % totalPages;
            showLogMenu(player, logDatas, nextPage);
        });
        controls.emplace_back(pageDown);
    }

    logMenu.setControls(controls);
    player.sendForm(logMenu);
}

//tyback菜单
void Menu::tybackMenu(const endstone::Player &sender) const
{
    endstone::ModalForm tyMenu;
    tyMenu.setTitle(tran_->getLocal("Revert Menu"));
    endstone::Slider radius;
    endstone::Slider Time;
    endstone::Dropdown keyType;
    endstone::TextInput keyWords;

    radius.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+tran_->getLocal("Radius"));radius.setDefaultValue(15);radius.setMin(1);radius.setMax(50);radius.setStep(1);
    Time.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+tran_->getLocal("Lookback Time (hours)"));Time.setDefaultValue(12);Time.setMin(1);Time.setMax(168);Time.setStep(1);
    keyType.setLabel(tran_->getLocal("Search By"));
    keyType.setOptions({tran_->getLocal("Source ID"),tran_->getLocal("Source Name"),tran_->getLocal("Target ID"),tran_->getLocal("Target Name"),tran_->getLocal("Action")});
    keyWords.setLabel(tran_->getLocal("Keywords"));
    keyWords.setPlaceholder(tran_->getLocal("Please enter keywords"));
    tyMenu.setControls({radius,Time,keyType,keyWords});
    tyMenu.setOnSubmit([=](const endstone::Player *p,const string& response) {
        json response_json = json::parse(response);
        const int r = response_json[0];
        const int time = response_json[1];
        const int search_key_type = response_json[2];
        const string search_key = response_json[3];
        std::ostringstream cmd;
        if (!search_key.empty()) {
            string key_type;
            if (search_key_type == 0) {key_type  = "source_id";} else if (search_key_type == 1) {key_type  = "source_name";} else if (search_key_type == 2) {key_type  = "target_id";}
            else if (search_key_type == 3) {key_type  = "target_name";} else if (search_key_type == 4) {key_type  = "action";}
            if (key_type == "action") {
                cmd << "tyback " << r << " " << time << " " <<key_type << " " << search_key ;
            } else {
                cmd << "tyback " << r << " " << time << " " <<key_type << " " << "\"" << search_key << "\"" ;
            }
            (void)p->performCommand(cmd.str());
        } else {
            cmd << "tyback " << r << " " << time;
            (void)p->performCommand(cmd.str());
        }
    });
    sender.asPlayer()->sendForm(tyMenu);
}

//tys 菜单
void Menu::tysMenu(const endstone::Player &sender) const
{
    endstone::ModalForm tyMenu;
    tyMenu.setTitle(tran_->getLocal("Log Browser"));
    endstone::Slider Time;
    endstone::Dropdown keyType;
    endstone::TextInput keyWords;

    Time.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+tran_->getLocal("Lookback Time (hours)"));Time.setDefaultValue(12);Time.setMin(1);Time.setMax(168);Time.setStep(1);
    keyType.setLabel(tran_->getLocal("Search By"));
    keyType.setOptions({tran_->getLocal("Source ID"),tran_->getLocal("Source Name"),tran_->getLocal("Target ID"),tran_->getLocal("Target Name"),tran_->getLocal("Action")});
    keyWords.setLabel(tran_->getLocal("Keywords"));
    keyWords.setPlaceholder(tran_->getLocal("Please enter keywords"));
    tyMenu.setControls({Time,keyType,keyWords});
    tyMenu.setOnSubmit([=](const endstone::Player *p,const string& response) {
        json response_json = json::parse(response);
        const int time = response_json[0];
        const int search_key_type = response_json[1];
        const string search_key = response_json[2];
        std::ostringstream cmd;
        if (!search_key.empty()) {
            string key_type;
            if (search_key_type == 0) {key_type  = "source_id";} else if (search_key_type == 1) {key_type  = "source_name";} else if (search_key_type == 2) {key_type  = "target_id";}
            else if (search_key_type == 3) {key_type  = "target_name";} else if (search_key_type == 4) {key_type  = "action";}
            cmd << "tys " << time << " " <<key_type << " " << "\"" << search_key << "\"" ;
            (void)p->performCommand(cmd.str());
        } else {
            cmd << "tys " << time;
            (void)p->performCommand(cmd.str());
        }
    });
    sender.asPlayer()->sendForm(tyMenu);
}

//ty菜单
void Menu::tyMenu(const endstone::Player &sender) const
{
    endstone::ModalForm tyMenu;
    tyMenu.setTitle(tran_->getLocal("Log Browser"));
    endstone::Slider radius;
    endstone::Slider Time;
    endstone::Dropdown keyType;
    endstone::TextInput keyWords;

    radius.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+tran_->getLocal("Radius"));radius.setDefaultValue(15);radius.setMin(1);radius.setMax(50);radius.setStep(1);
    Time.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+tran_->getLocal("Lookback Time (hours)"));Time.setDefaultValue(12);Time.setMin(1);Time.setMax(168);Time.setStep(1);
    keyType.setLabel(tran_->getLocal("Search By"));
    keyType.setOptions({tran_->getLocal("Source ID"),tran_->getLocal("Source Name"),tran_->getLocal("Target ID"),tran_->getLocal("Target Name"),tran_->getLocal("Action")});
    keyWords.setLabel(tran_->getLocal("Keywords"));
    keyWords.setPlaceholder(tran_->getLocal("Please enter keywords"));
    tyMenu.setControls({radius,Time,keyType,keyWords});
    tyMenu.setOnSubmit([=](const endstone::Player *p,const string& response) {
        json response_json = json::parse(response);
        const int r = response_json[0];
        const int time = response_json[1];
        const int search_key_type = response_json[2];
        const string search_key = response_json[3];
        std::ostringstream cmd;
        if (!search_key.empty()) {
            string key_type;
            if (search_key_type == 0) {key_type  = "source_id";} else if (search_key_type == 1) {key_type  = "source_name";} else if (search_key_type == 2) {key_type  = "target_id";}
            else if (search_key_type == 3) {key_type  = "target_name";} else if (search_key_type == 4) {key_type  = "action";}
            cmd << "ty " << r << " " << time << " " <<key_type << " " << "\"" << search_key << "\"" ;
            (void)p->performCommand(cmd.str());
        } else {
            cmd << "ty " << r << " " << time;
            (void)p->performCommand(cmd.str());
        }
    });
    sender.asPlayer()->sendForm(tyMenu);
}

//查看在线玩家物品栏函数
void Menu::showOnlinePlayerBag(const endstone::CommandSender &sender, const endstone::Player& player) const
{
    try {
        std::vector<std::map<std::string, std::string>> all_item;
        
        // 获取玩家全部物品
        for (auto items = player.getInventory().getContents(); auto &item : items) {
            // 如果槽位不为空
            if (item) {
                std::map<std::string, std::string> item_info;
                item_info["item"] = item->getType().getId();
                item_info["amount"] = std::to_string(item->getAmount());
                if (const auto meta = item->getItemMeta()) {
                    if (auto display_name = meta->getDisplayName();!display_name.empty()) {
                        item_info["name"] = display_name;
                    }
                    if (auto enhance = meta->getEnchants();!enhance.empty()) {
                        string enhances;
                        for (const auto &[fst, snd] : enhance) {
                            enhances += string(fst->getId().getKey()) + ":"+to_string(snd)+"\n";
                        }
                        item_info["enhance"] = enhances;
                    }
                }
                all_item.push_back(item_info);
            }
        }
        //盔甲
        auto helmet = player.getInventory().getHelmet();
        auto chestplate = player.getInventory().getChestplate();
        auto leggings = player.getInventory().getLeggings();
        auto boots = player.getInventory().getBoots();
        
        // 检查并添加头盔信息
        if (helmet) {
            std::map<std::string, std::string> item_info;
            item_info["item"] = helmet->getType().getId();
            item_info["amount"] = std::to_string(helmet->getAmount());
            if (const auto meta = helmet->getItemMeta()) {
                if (auto display_name = meta->getDisplayName();!display_name.empty()) {
                    item_info["name"] = display_name;
                }
                if (auto enhance = meta->getEnchants();!enhance.empty()) {
                    string enhances;
                    for (const auto &[fst, snd] : enhance) {
                        enhances += string(fst->getId()) + ":"+to_string(snd)+"\n";
                    }
                    item_info["enhance"] = enhances;
                }
            }
            all_item.push_back(item_info);
        }
        
        // 检查并添加胸甲信息
        if (chestplate) {
            std::map<std::string, std::string> item_info;
            item_info["item"] = chestplate->getType().getId();
            item_info["amount"] = std::to_string(chestplate->getAmount());
            if (const auto meta = chestplate->getItemMeta()) {
                if (auto display_name = meta->getDisplayName();!display_name.empty()) {
                    item_info["name"] = display_name;
                }
                if (auto enhance = meta->getEnchants();!enhance.empty()) {
                    string enhances;
                    for (const auto &[fst, snd] : enhance) {
                        enhances += string(fst->getId()) + ":"+to_string(snd)+"\n";
                    }
                    item_info["enhance"] = enhances;
                }
            }
            all_item.push_back(item_info);
        }
        
        // 检查并添加护腿信息
        if (leggings) {
            std::map<std::string, std::string> item_info;
            item_info["item"] = leggings->getType().getId();
            item_info["amount"] = std::to_string(leggings->getAmount());
            if (const auto meta = leggings->getItemMeta()) {
                if (auto display_name = meta->getDisplayName();!display_name.empty()) {
                    item_info["name"] = display_name;
                }
                if (auto enhance = meta->getEnchants();!enhance.empty()) {
                    string enhances;
                    for (const auto &[fst, snd] : enhance) {
                        enhances += string(fst->getId()) + ":"+to_string(snd)+"\n";
                    }
                    item_info["enhance"] = enhances;
                }
            }
            all_item.push_back(item_info);
        }
        
        // 检查并添加靴子信息
        if (boots) {
            std::map<std::string, std::string> item_info;
            item_info["item"] = boots->getType().getId();
            item_info["amount"] = std::to_string(boots->getAmount());
            if (const auto meta = boots->getItemMeta()) {
                if (auto display_name = meta->getDisplayName();!display_name.empty()) {
                    item_info["name"] = display_name;
                }
                if (auto enhance = meta->getEnchants();!enhance.empty()) {
                    string enhances;
                    for (const auto &[fst, snd] : enhance) {
                        enhances += string(fst->getId()) + ":"+to_string(snd)+"\n";
                    }
                    item_info["enhance"] = enhances;
                }
            }
            all_item.push_back(item_info);
        }

        //副手
        if (auto offhand = player.getInventory().getItemInOffHand()) {
            std::map<std::string, std::string> item_info;
            item_info["item"] = offhand->getType().getId();
            item_info["amount"] = std::to_string(offhand->getAmount());
            if (const auto meta = offhand->getItemMeta()) {
                if (auto display_name = meta->getDisplayName();!display_name.empty()) {
                    item_info["name"] = display_name;
                }
                if (auto enhance = meta->getEnchants();!enhance.empty()) {
                    string enhances;
                    for (const auto &[fst, snd] : enhance) {
                        enhances += string(fst->getId()) + ":"+to_string(snd)+"\n";
                    }
                    item_info["enhance"] = enhances;
                }
            }
            all_item.push_back(item_info);
        }

        // 构建物品信息输出
        std::string output_item;
        for (const auto& item_info : all_item) {
            std::string message = fmt::format("{}{}: {} \n {}: {} \n",
                endstone::ColorFormat::Yellow,
                tran_->getLocal("Item ID"), item_info.at("item"),
                tran_->getLocal("Amount"), item_info.at("amount"));
            if (item_info.contains("name")) {
                message += fmt::format("{}: {}\n",tran_->getLocal("Custom name"),item_info.at("name"));
            }
            if (item_info.contains("enhance")) {
                message += fmt::format("{}: {}\n",tran_->getLocal("Enhance"),item_info.at("enhance"));
            }
            output_item += message + std::string(20, '-') + "\n";
        }

        if (output_item.empty()) {
            sender.sendMessage(fmt::format("{}{}", endstone::ColorFormat::Red, tran_->getLocal("This player's inventory is empty")));
        } else {
            endstone::ActionForm form;
            form.setTitle(tran_->tr("{0}'s Inventory", player.getName()));
            form.setContent(output_item);
            sender.asPlayer()->sendForm(form);
        }
    } catch (...) {
        sender.sendMessage(fmt::format("{}{}", endstone::ColorFormat::Red, tran_->getLocal("Unknow Error")));
    }
}

//可视化物品栏展示
bool Menu::showPlayerInventoryUI(endstone::Player &sender, endstone::Player &target) {
    // 如果玩家已有打开的菜单，直接刷新物品
    if (last_inventory_menu_) {
        for (auto viewers = last_inventory_menu_->get_viewers(); auto& viewer : viewers) {
            if (viewer->getName() == sender.getName()) {
                auto inv = last_inventory_menu_->get_inventory();
                inv->clear();
                auto contents = target.getInventory().getContents();
                for (int i = 0; i < 36 && i < static_cast<int>(contents.size()); i++) {
                    if (contents[i]) inv->set_item(i, *contents[i]);
                }
                if (auto helmet = target.getInventory().getHelmet()) inv->set_item(45, *helmet);
                if (auto chestplate = target.getInventory().getChestplate()) inv->set_item(46, *chestplate);
                if (auto leggings = target.getInventory().getLeggings()) inv->set_item(47, *leggings);
                if (auto boots = target.getInventory().getBoots()) inv->set_item(48, *boots);
                if (auto offhand = target.getInventory().getItemInOffHand()) inv->set_item(49, *offhand);
                return true;
            }
        }
        last_inventory_menu_->close_all();
    }

    // 没有现成的菜单，创建新的
    auto menu = inventoryui::create_menu(
        inventoryui::MenuTypeId::DOUBLE_CHEST,
        tran_->tr("{0}'s Inventory", target.getName())
    );
    if (!menu) return false;
    last_inventory_menu_ = menu;
    auto inv = menu->get_inventory();

    auto contents = target.getInventory().getContents();
    for (int i = 0; i < 36 && i < static_cast<int>(contents.size()); i++) {
        if (contents[i]) inv->set_item(i, *contents[i]);
    }

    if (auto helmet = target.getInventory().getHelmet()) inv->set_item(45, *helmet);
    if (auto chestplate = target.getInventory().getChestplate()) inv->set_item(46, *chestplate);
    if (auto leggings = target.getInventory().getLeggings()) inv->set_item(47, *leggings);
    if (auto boots = target.getInventory().getBoots()) inv->set_item(48, *boots);

    if (auto offhand = target.getInventory().getItemInOffHand()) {
        inv->set_item(49, *offhand);
    }

    // 注册点击回调：复制物品到点击者背包
    menu->set_listener([this](const endstone::Player &clicker, int,
                              const endstone::ItemStack &item,
                              inventoryui::UIInventory &) {
        if (item.getType().getId() == "minecraft:air") return;
        auto copy = item;
        if (const auto leftover =
                clicker.getInventory().addItem(std::move(copy));
            leftover.empty()) {
            clicker.sendMessage("§a" + tran_->tr("Copied to your inventory"));
        } else {
            clicker.sendMessage("§c" + tran_->tr("Inventory full, cannot copy item"));
        }
    });

    menu->send_to(sender);
    return true;
}

bool Menu::showOfflinePlayerInventoryUI(endstone::Player &sender, const std::string& player_name,
    const std::vector<std::optional<endstone::ItemStack>>& contents,
    const std::array<std::optional<endstone::ItemStack>, 4>& armor,
    const std::optional<endstone::ItemStack>& offhand)
{
    if (last_inventory_menu_) {
        last_inventory_menu_->close_all();
    }

    const auto menu = inventoryui::create_menu(
        inventoryui::MenuTypeId::DOUBLE_CHEST,
        tran_->tr("{0}'s Inventory (Offline)", player_name)
    );
    last_inventory_menu_ = menu;
    const auto inv = menu->get_inventory();

    for (int i = 0; i < 36 && i < static_cast<int>(contents.size()); i++) {
        if (contents[i]) inv->set_item(i, *contents[i]);
    }

    if (armor[0]) inv->set_item(45, *armor[0]);
    if (armor[1]) inv->set_item(46, *armor[1]);
    if (armor[2]) inv->set_item(47, *armor[2]);
    if (armor[3]) inv->set_item(48, *armor[3]);
    if (offhand) inv->set_item(49, *offhand);

    // 注册点击回调：复制物品到点击者背包
    menu->set_listener([this](const endstone::Player &clicker, int,
                              const endstone::ItemStack &item,
                              inventoryui::UIInventory &) {
        if (item.getType().getId() == "minecraft:air") return;
        auto copy = item;
        if (const auto leftover =
                clicker.getInventory().addItem(std::move(copy));
            leftover.empty()) {
            clicker.sendMessage("§a" + tran_->tr("Copied to your inventory"));
        } else {
            clicker.sendMessage("§c" + tran_->tr("Inventory full, cannot copy item"));
        }
    });

    menu->send_to(sender);
    return true;
}

bool Menu::showOfflinePlayerInventoryEncoded(endstone::Player &sender, const std::string& player_name,
    const std::string& world_path, const std::string& player_uuid)
{
    auto* encoded = wi_get_encoded_inventory(world_path.c_str(), player_uuid.c_str());
    if (!encoded) {
        sender.sendErrorMessage(tran_->tr(tran_->getLocal("Player {} not found (offline)"), player_name));
        return false;
    }

    // Try visual GUI with pre-encoded items
    {
        const auto menu = inventoryui::create_menu(
            inventoryui::MenuTypeId::DOUBLE_CHEST,
            tran_->tr("{0}'s Inventory (Offline)", player_name)
        );
        if (menu) {
            last_inventory_menu_ = menu;
            const auto inv = menu->get_inventory();

            for (int i = 0; i < encoded->count; i++) {
                auto& [slot, type_id, count, damage, nbt_bytes, nbt_len] = encoded->items[i];
                std::vector<uint8_t> nbt(nbt_bytes, nbt_bytes + nbt_len);
                inv->set_pre_encoded_item(slot, std::string(type_id), count, damage, nbt);
            }

            wi_free_encoded_inventory(encoded);

            // 注册点击回调：复制物品到点击者背包
            menu->set_listener([this](const endstone::Player &clicker, int,
                                      const endstone::ItemStack &item,
                                      inventoryui::UIInventory &) {
                if (item.getType().getId() == "minecraft:air") return;
                auto copy = item;
                if (const auto leftover =
                        clicker.getInventory().addItem(std::move(copy));
                    leftover.empty()) {
                    clicker.sendMessage("§a" + tran_->tr("Copied to your inventory"));
                } else {
                    clicker.sendMessage("§c" + tran_->tr("Inventory full, cannot copy item"));
                }
            });

            menu->send_to(sender);
            return true;
        }
    }

    // Fallback: ActionForm text
    plugin_.getLogger().info("[ty_debug] Falling back to ActionForm text for {}", player_name);
    endstone::ActionForm form;
    form.setTitle(tran_->tr("{0}'s Inventory (Offline)", player_name));
    std::string content;
    for (int i = 0; i < encoded->count; i++) {
        auto& item = encoded->items[i];
        content += fmt::format("Slot {}: {} x{} (damage: {})\n",
            item.slot, item.type_id ? item.type_id : "?", item.count, item.damage);
    }
    wi_free_encoded_inventory(encoded);

    if (content.empty()) {
        content = tran_->getLocal("Inventory is empty");
    }
    form.setContent(content);
    plugin_.getLogger().info("[ty_debug] Sending ActionForm to {}", player_name);
    sender.sendForm(form);
    return true;
}

//查找实体密度高区域
void Menu::findHighDensityRegion(endstone::Player &player, const int size) const {

    // 调用TianyanProtect中的实现函数获取结果

    // 显示结果
    if (auto result = TianyanProtect::calculateEntityDensity(plugin_.getServer(),size); result.dim.has_value()) {
        // 构建表单内容
        std::string content = fmt::format(
            "{}{} {},\n"
            "{}:({:.1f}, {:.1f}, {:.1f}),\n"
            "{}:{}\n"
            "{}: {},\n"
            "{}: {}",
            endstone::ColorFormat::Yellow,
            tran_->getLocal("Highest density region in dimension"), result.dim.value(),
            tran_->getLocal("Midpoint coordinates"), result.mid_x.value(), result.mid_y.value(), result.mid_z.value(),
            tran_->getLocal("Entity count"), result.count.value(),
            tran_->getLocal("Most common entity"), result.entity_type.value(),
            tran_->getLocal("Random entity position"), result.entity_pos.value()
        );
        
        // 创建表单
        endstone::ActionForm form;
        form.setTitle(tran_->getLocal("Entity Density Detection Result"));
        form.setContent(content);
        
        // 添加按钮
        std::vector<std::variant<endstone::Button, endstone::Divider, endstone::Header, endstone::Label>> controls;
        
        // 传送按钮
        endstone::Button tpButton;
        tpButton.setText(tran_->getLocal("Teleport to this area"));
        tpButton.setOnClick([this, result](const endstone::Player* p) {
            const std::string dim = result.dim.value();
            // 转换维度名称
            const auto dimension = plugin_.getServer().getLevel()->getDimension(dim);
            const auto location = endstone::Location(*dimension,static_cast<float>(result.entity_pos_x.value()),static_cast<float>(result.entity_pos_y.value()),static_cast<float>(result.entity_pos_z.value()));
            p->asPlayer()->teleport(location);

        });
        controls.emplace_back(tpButton);
        
        // 打印信息按钮
        endstone::Button printButton;
        printButton.setText(tran_->getLocal("Print information to chat"));
        printButton.setOnClick([this, result](const endstone::Player* p) {
            std::string message = fmt::format(
                "{}{} {},\n"
                "{}:({:.1f}, {:.1f}, {:.1f}),\n"
                "{}:{}\n"
                "{}: {},\n"
                "{}: {}",
                endstone::ColorFormat::Yellow,
                tran_->getLocal("Highest density region in dimension"), result.dim.value(),
                tran_->getLocal("Midpoint coordinates"), result.mid_x.value(), result.mid_y.value(), result.mid_z.value(),
                tran_->getLocal("Entity count"), result.count.value(),
                tran_->getLocal("Most common entity"), result.entity_type.value(),
                tran_->getLocal("Random entity position"), result.entity_pos.value()
            );
            p->sendMessage(message);
        });
        controls.emplace_back(printButton);
        
        form.setControls(controls);
        player.sendForm(form);
    } else {
        player.sendMessage(endstone::ColorFormat::Yellow + tran_->getLocal("No entities detected currently"));
    }
}