//
// Created by yuhang on 2025/12/2.
//

#include "menu.h"
#include "tianyan_protect.h"

//日志展示菜单
void Menu::showLogMenu(endstone::Player &player, const std::vector<TianyanCore::LogData>& logDatas, const int page) {
    // 计算分页信息
    constexpr int logsPerPage = 30;
    const int totalPages = (static_cast<int>(logDatas.size()) + logsPerPage - 1) / logsPerPage;
    const int currentPage = std::min(page, std::max(0, totalPages - 1)); // 确保当前页在有效范围内

    endstone::ActionForm logMenu;
    logMenu.setTitle(fmt::format("{} - {} {}/{} {}", Tran.getLocal("Logs"), Tran.getLocal("The"),currentPage + 1, std::max(1, totalPages),Tran.getLocal("Page")));

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
                logFields.emplace_back(Tran.getLocal("Source Name"), logData.name);
            }

            // 根据是否是玩家添加不同类型的信息
            if (logData.id == "minecraft:player") {
                logFields.emplace_back(Tran.getLocal("Source Type"), Tran.getLocal("Player"));
            }
            else if (!logData.id.empty()){
                logFields.emplace_back(Tran.getLocal("Source ID"), logData.id);
            }

            // 添加行为类型
            logFields.emplace_back(Tran.getLocal("Action"), Tran.getLocal(logData.type));

            // 添加位置信息
            logFields.emplace_back(Tran.getLocal("Position"),
                                  fmt::format("{:.2f},{:.2f},{:.2f}", logData.pos_x, logData.pos_y, logData.pos_z));

            //添加维度信息
            logFields.emplace_back(Tran.getLocal("Dimension"), logData.world);

            // 添加时间信息
            logFields.emplace_back(Tran.getLocal("Time"), TianyanCore::timestampToString(logData.time));

            // 根据目标名称和ID是否存在添加相应信息
            if (!logData.obj_name.empty()) {
                logFields.emplace_back(Tran.getLocal("Target Name"), Tran.getLocal(logData.obj_name));
            }

            if (!logData.obj_id.empty()) {
                logFields.emplace_back(Tran.getLocal("Target ID"), logData.obj_id);
            }

            // 添加数据信息
            if (!logData.data.empty()){
                //对手持物品交互进行处理
                if (logData.type == "player_right_click_block") {
                    auto hand_block = yuhangle::Database::splitString(logData.data);
                    logFields.emplace_back(Tran.getLocal("Item in Hand"), Tran.getLocal(hand_block[0]));
                    if (hand_block[1] != "[]") {
                        logFields.emplace_back(Tran.getLocal("Data"), hand_block[1]);
                    }
                } else {
                    if (logData.data != "[]") {
                        logFields.emplace_back(Tran.getLocal("Data"), logData.data);
                    }
                }
            }

            //事件是否取消或被恢复
            if (logData.status == "canceled") {
                logFields.emplace_back(Tran.getLocal("Status"), Tran.getLocal("This event has been canceled"));
            } else if (logData.status == "reverted") {
                logFields.emplace_back(Tran.getLocal("Status"), Tran.getLocal("This event has been reverted"));
            }

            // 格式化输出
            for (const auto&[fst, snd] : logFields) {
                showLogs += fmt::format("{}: {}\n", fst, snd);
            }

            showLogs += "--------------------------------\n\n";
        }
    } else {
        // 没有日志数据
        showLogs = Tran.getLocal("No log found");
    }

    logMenu.setContent(endstone::ColorFormat::Yellow + showLogs);

    // 设置翻页按钮
    std::vector<std::variant<endstone::Button, endstone::Divider, endstone::Header, endstone::Label>> controls;

    // 添加上一页按钮（如果有多页）
    if (totalPages > 1) {
        endstone::Button pageUp;
        pageUp.setText(Tran.getLocal("Page Up"));
        pageUp.setOnClick([=, &player](endstone::Player*) {
            // 如果是第一页，则跳转到最后一页；否则上一页
            const int prevPage = (currentPage == 0) ? (totalPages - 1) : (currentPage - 1);
            showLogMenu(player, logDatas, prevPage);
        });
        controls.emplace_back(pageUp);
    }

    // 添加下一页按钮（如果有多页）
    if (totalPages > 1) {
        endstone::Button pageDown;
        pageDown.setText(Tran.getLocal("Page Down"));
        pageDown.setOnClick([=, &player](endstone::Player*) {
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
void Menu::tybackMenu(const endstone::Player &sender) {
    endstone::ModalForm tyMenu;
    tyMenu.setTitle(Tran.getLocal("Revert Menu"));
    endstone::Slider radius;
    endstone::Slider Time;
    endstone::Dropdown keyType;
    endstone::TextInput keyWords;

    radius.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+Tran.getLocal("Radius"));radius.setDefaultValue(15);radius.setMin(1);radius.setMax(50);radius.setStep(1);
    Time.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+Tran.getLocal("Lookback Time (hours)"));Time.setDefaultValue(12);Time.setMin(1);Time.setMax(168);Time.setStep(1);
    keyType.setLabel(Tran.getLocal("Search By"));
    keyType.setOptions({Tran.getLocal("Source ID"),Tran.getLocal("Source Name"),Tran.getLocal("Target ID"),Tran.getLocal("Target Name"),Tran.getLocal("Action")});
    keyWords.setLabel(Tran.getLocal("Keywords"));
    keyWords.setPlaceholder(Tran.getLocal("Please enter keywords"));
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
void Menu::tysMenu(const endstone::Player &sender) {
    endstone::ModalForm tyMenu;
    tyMenu.setTitle(Tran.getLocal("Log Browser"));
    endstone::Slider Time;
    endstone::Dropdown keyType;
    endstone::TextInput keyWords;

    Time.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+Tran.getLocal("Lookback Time (hours)"));Time.setDefaultValue(12);Time.setMin(1);Time.setMax(168);Time.setStep(1);
    keyType.setLabel(Tran.getLocal("Search By"));
    keyType.setOptions({Tran.getLocal("Source ID"),Tran.getLocal("Source Name"),Tran.getLocal("Target ID"),Tran.getLocal("Target Name"),Tran.getLocal("Action")});
    keyWords.setLabel(Tran.getLocal("Keywords"));
    keyWords.setPlaceholder(Tran.getLocal("Please enter keywords"));
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
void Menu::tyMenu(const endstone::Player &sender) {
    endstone::ModalForm tyMenu;
    tyMenu.setTitle(Tran.getLocal("Log Browser"));
    endstone::Slider radius;
    endstone::Slider Time;
    endstone::Dropdown keyType;
    endstone::TextInput keyWords;

    radius.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+Tran.getLocal("Radius"));radius.setDefaultValue(15);radius.setMin(1);radius.setMax(50);radius.setStep(1);
    Time.setLabel(endstone::ColorFormat::Red+"*"+endstone::ColorFormat::Reset+Tran.getLocal("Lookback Time (hours)"));Time.setDefaultValue(12);Time.setMin(1);Time.setMax(168);Time.setStep(1);
    keyType.setLabel(Tran.getLocal("Search By"));
    keyType.setOptions({Tran.getLocal("Source ID"),Tran.getLocal("Source Name"),Tran.getLocal("Target ID"),Tran.getLocal("Target Name"),Tran.getLocal("Action")});
    keyWords.setLabel(Tran.getLocal("Keywords"));
    keyWords.setPlaceholder(Tran.getLocal("Please enter keywords"));
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
void Menu::showOnlinePlayerBag(const endstone::CommandSender &sender, const endstone::Player& player) {
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
                Tran.getLocal("Item ID"), item_info.at("item"),
                Tran.getLocal("Amount"), item_info.at("amount"));
            if (item_info.contains("name")) {
                message += fmt::format("{}: {}\n",Tran.getLocal("Custom name"),item_info.at("name"));
            }
            if (item_info.contains("enhance")) {
                message += fmt::format("{}: {}\n",Tran.getLocal("Enhance"),item_info.at("enhance"));
            }
            output_item += message + std::string(20, '-') + "\n";
        }

        if (output_item.empty()) {
            sender.sendMessage(fmt::format("{}{}", endstone::ColorFormat::Red, Tran.getLocal("This player's inventory is empty")));
        } else {
            endstone::ActionForm form;
            form.setTitle(player.getName());
            form.setContent(output_item);
            sender.asPlayer()->sendForm(form);
        }
    } catch (...) {
        sender.sendMessage(fmt::format("{}{}", endstone::ColorFormat::Red, Tran.getLocal("Unknow Error")));
    }
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
            Tran.getLocal("Highest density region in dimension"), result.dim.value(),
            Tran.getLocal("Midpoint coordinates"), result.mid_x.value(), result.mid_y.value(), result.mid_z.value(),
            Tran.getLocal("Entity count"), result.count.value(),
            Tran.getLocal("Most common entity"), result.entity_type.value(),
            Tran.getLocal("Random entity position"), result.entity_pos.value()
        );
        
        // 创建表单
        endstone::ActionForm form;
        form.setTitle(Tran.getLocal("Entity Density Detection Result"));
        form.setContent(content);
        
        // 添加按钮
        std::vector<std::variant<endstone::Button, endstone::Divider, endstone::Header, endstone::Label>> controls;
        
        // 传送按钮
        endstone::Button tpButton;
        tpButton.setText(Tran.getLocal("Teleport to this area"));
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
        printButton.setText(Tran.getLocal("Print information to chat"));
        printButton.setOnClick([result](const endstone::Player* p) {
            std::string message = fmt::format(
                "{}{} {},\n"
                "{}:({:.1f}, {:.1f}, {:.1f}),\n"
                "{}:{}\n"
                "{}: {},\n"
                "{}: {}",
                endstone::ColorFormat::Yellow,
                Tran.getLocal("Highest density region in dimension"), result.dim.value(),
                Tran.getLocal("Midpoint coordinates"), result.mid_x.value(), result.mid_y.value(), result.mid_z.value(),
                Tran.getLocal("Entity count"), result.count.value(),
                Tran.getLocal("Most common entity"), result.entity_type.value(),
                Tran.getLocal("Random entity position"), result.entity_pos.value()
            );
            p->sendMessage(message);
        });
        controls.emplace_back(printButton);
        
        form.setControls(controls);
        player.sendForm(form);
    } else {
        player.sendMessage(endstone::ColorFormat::Yellow + Tran.getLocal("No entities detected currently"));
    }
}