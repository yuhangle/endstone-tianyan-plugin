![header](https://capsule-render.vercel.app/api?type=waving&height=300&color=gradient&text=Tianyan%20Plugin&animation=fadeIn&textBg=false&descAlignY=78&desc=Behavior%20logging%20for%20Endstone&section=footer&reversal=true)


# Tianyan Plugin (天眼插件) 

[![English](https://img.shields.io/badge/English-README_eng.md-blue)](README_eng.md)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
![Build Status](https://github.com/yuhangle/endstone-tianyan-plugin/actions/workflows/workflow.yml/badge.svg)

一款适用于 [Endstone](https://github.com/EndstoneMC/endstone) 插件加载器的玩家行为记录查询插件。

## 📋 功能介绍

### 🔍 行为记录

天眼插件使用 Endstone 的事件 API 进行行为事件的记录，可以对以下行为事件进行记录：

#### 玩家对方块和实体交互行为事件
玩家对方块和部分实体进行的交互事件均会被记录。

#### 玩家直接破坏方块和实体爆炸破坏方块行为事件
玩家直接破坏方块和实体爆炸破坏方块行为事件均会被记录。玩家间接破坏方块，如踩坏耕地等，无法触发事件；非玩家实体对方块的操作不会触发事件，除非自己爆炸。

#### 实体受到伤害事件
除了配置文件中被排除的实体，所有实体受到伤害时均会被记录。部分特殊实体，比如画被攻击是直接被移除，无法触发伤害事件。

#### 玩家放置方块行为事件
插件将对全部玩家直接实行的方块放置行为事件进行记录。

#### 活塞行为事件
所有活塞自身行为会被记录，但是被推动的方块无法记录。

### 👤 玩家信息显示和封禁功能

#### 玩家加服信息显示
天眼插件支持玩家加服信息显示功能。当玩家加入服务器时，将显示玩家的系统名称和设备ID。

#### 封禁功能 & 防刷屏功能
插件可以通过封禁设备ID将使用此设备ID的设备拉入封禁名单，使得使用该设备（设备ID不变）的任何玩家都无法进入服务器。使用过封禁设备的玩家，若更换设备，会自动追加新设备到封禁名单中。

自动封禁规则：
- 当玩家在服务器内短时间发送大量消息（10秒内6条消息）时，将被自动封禁24小时
- 当玩家在服务器内短时间发送大量命令（10秒内12条命令）时将被自动封禁24小时

### 🛠️ 其它功能

#### 复原玩家直接造成的方块破坏与方块放置以及实体爆炸造成的破坏的功能
天眼插件支持复原玩家直接造成的方块破坏与方块放置以及实体爆炸造成的破坏，其原理是简单的使用了 setblock 命令从数据库中还原了方块，暂不支持还原箱子内容物之类的细节。

#### 查看玩家物品栏功能
天眼插件支持查看玩家的物品栏内的物品信息。

## 🚀 安装 & 配置 & 使用方法

### 安装 Endstone
这一步请参考 [Endstone 官方文档](https://github.com/EndstoneMC/endstone)。

### 安装天眼插件
1. 在 Release 页面下载最新版本的插件
2. 解压出插件文件与数据文件夹
3. 放在服务端目录的 `plugins` 目录下
4. 运行服务器即可使用

### 配置天眼插件

运行天眼插件后，将自动在服务端目录的 `plugins` 目录下生成 `tianyan_data` 文件夹，并自动生成 `config.json` 文件。语言文件不会自动生成，请从github仓库下载。

默认配置如下：

```json
{
    "10s_command_max": 12,
    "10s_message_max": 6,
    "language": "zh_CN",
    "no_log_mobs": [
        "minecraft:zombie_pigman",
        "minecraft:zombie",
        "minecraft:skeleton",
        "minecraft:bogged",
        "minecraft:slime"
    ]
}
```

配置项说明：
- `10s_command_max`: 10秒内玩家可使用命令的最大次数
- `10s_message_max`: 10秒内玩家可发送消息的最大次数
- `language`: 插件语言
- `no_log_mobs`: 不被记录的实体列表

> 在涉及实体事件中，除非使用命名牌，否则不会记录 `no_log_mobs` 中的实体。

插件配置默认使用中文，可以通过修改 `config.json` 中的 `language` 项更改语言。支持的语言可在 [language](language/zh_CN.json) 文件夹查看。

示例：要修改为英文时，修改 `language` 项为 `"en_US"`。

## 📜 插件命令使用方法

### `/ty` - 查询玩家及部分实体行为记录(控制台不可使用此命令,普通成员可使用此命令)
```
/ty <半径> <时间（单位：小时）>
```
无参数则将弹出快捷菜单，玩家可在菜单中查询。最大可查询半径为100(格),最大可查询时间为672(小时)。(查询数据超过25000条时，将截断查询，只显示25000条数据)

### `/tys` - 搜索关键词(控制台不可使用此命令)
```
/tys <时间（单位：小时）> [搜索对象类型] [搜索对象关键词]
```

无参数则将弹出快捷菜单，玩家可在菜单中查询。最大可查询半径和时间无限。(查询数据超过25000条时，将截断查询，只显示25000条数据)

搜索对象类型：
- `source_id`: 行为源ID
- `source_name`: 行为源名称
- `target_id`: 行为目标ID
- `target_name`: 行为目标名称

示例：搜索2小时内名为"ZhangSan"的玩家实施的行为
```
/tys 2 source_name "ZhangSan"
```

搜索特定行为类型：
```
/tys <时间（单位：小时）> <action> <搜索行为类型>
```

搜索行为类型：
- `block_break`: 方块破坏
- `block_place`: 方块放置
- `entity_damage`: 实体伤害
- `player_right_click_block`: 玩家右键方块
- `player_right_click_entity`: 玩家右键实体
- `entity_bomb`: 实体爆炸
- `block_break_bomb`: 方块爆炸
- `piston_extend`: 活塞弹出
- `piston_retract`: 活塞收回
- `entity_die`: 实体死亡
- `player_pickup_item`: 玩家拾取物品

示例：搜索2小时内的玩家方块放置行为
```
/tys 2 action block_place
```

### `/tyo` - 查看在线玩家物品栏(控制台不可使用此命令)
```
/tyo 玩家名
```

### `/ban-id` - 将设备加入黑名单
```
/ban-id <设备ID> [理由]
```

### `/unban-id` - 将设备移出黑名单
```
/unban-id <设备ID>
```

### `/banlist-id` - 列出所有被封禁的设备ID
```
/banlist-id
```

### `/tyclean` - 清理数据库
```
/tyclean <时间(单位:小时)>
```

此命令可清理数据库中超过指定时间的日志。

### `/density` - 检测实体密度最高的区域
```
/density [区域大小(单位:格)]
```

此命令可以在服务器内查找实体密度最高的区域，并支持传送到该区域。区域大小代表单个搜索区域(正方体)的边长。

### `/tyback` - 还原玩家或实体部分行为(实验性;控制台不可使用此命令)
```
/tyback <半径> <时间（单位：小时）> <行为类型> <关键词>
```

此命令能够恢复玩家或实体部分行为。包括：玩家对方块的直接破坏与放置、实体爆炸产生的方块破坏、实体的死亡、玩家交互造成方块的变化。其中实体死亡不会还原毛色、年龄等属性；玩家制造的含水方块无法被还原为原始状态。其原理为使用setblock和summon命令从日志数据库中还原。
无参数则将弹出快捷菜单，玩家可在菜单中查询。最大可恢复半径为100(格),时间无限。(查询数据超过25000条时，将截断查询，只恢复25000条数据。实际可恢复半径受区块影响，超出setblock命令和summon命令的极限距离将无法恢复，需要靠近再来一次)

## 🔧 修改 & 构建

### 云构建
1. Fork 项目
2. 在自己 fork 的分支中，使用 Action 运行构建
3. 几分钟后即可看到构建结果
4. Release 需要在自己的 Action 中配置一个名为 `RELEASE_TOKEN` 的密钥

### 本地构建

按照 Endstone 文档配置好开发环境：

- Linux 需要安装 sqlite3 开发库
- Windows 需要使用 vcpkg 或其它包管理工具 安装 sqlite3 静态开发库

克隆项目代码：
```shell
git clone https://github.com/yuhangle/endstone-tianyan-plugin.git
```

使用你喜欢的开发工具打开代码目录，以 CLion 为例，在构建中点击构建项目，即可构建插件。

其中，如果使用 vcpkg ，Windows需要为 CMake 配置以下参数：

```
-DCMAKE_TOOLCHAIN_FILE="{Your vcpkg path}\scripts\buildsystems\vcpkg.cmake" \
-DVCPKG_TARGET_TRIPLET=x64-windows-static
```
