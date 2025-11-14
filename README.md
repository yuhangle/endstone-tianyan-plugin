[**English**](README_eng.md)

一款适用于Endstone插件加载器的玩家行为记录查询插件。

# 功能介绍

## 行为记录

天眼插件使用endstone的事件API进行行为事件的记录，可以对以下行为事件进行记录：

### 玩家对方块和实体交互行为事件

玩家对方块和部分实体进行的交互事件均会被记录。

### 玩家直接破坏方块和实体爆炸破坏方块行为事件

玩家直接破坏方块和实体爆炸破坏方块行为事件均会被记录。玩家间接破坏方块，如踩坏耕地等，无法触发事件；非玩家实体对方块的操作不会触发事件，除非自己爆炸。

### 实体受到伤害事件

除了配置文件中被排除的实体，所有实体受到伤害时均会被记录。部分特殊实体，如画被攻击是直接被移除，无法触发伤害事件。

### 玩家放置方块行为事件

插件将对全部玩家直接实行的方块放置行为事件进行记录。

### 活塞行为事件

所有活塞自身行为会被记录，但是被推动的方块无法记录。

## 玩家信息显示和封禁功能

### 玩家加服信息显示

天眼插件支持玩家加服信息显示功能。当玩家加入服务器时，将显示玩家的系统名称和设备ID。

### 封禁功能&防刷屏功能

插件可以通过封禁设备ID将使用此设备ID的设备拉入封禁名单，使得使用该设备（设备ID不变）的任何玩家都无法进入服务器。使用过封禁设备的玩家，若更换设备，会自动追加新设备到封禁名单中。
当玩家在服务器内短时间发送大量消息（10秒内6条消息）时，将被自动封禁24小时；当玩家在服务器内短时间发送大量命令（10秒内12条命令）时将被自动封禁24小时。

## 其它功能

### 复原玩家直接造成的方块破坏与方块放置以及实体爆炸造成的破坏的功能

天眼插件支持复原玩家直接造成的方块破坏与方块放置以及实体爆炸造成的破坏，其原理是简单的使用了setblock命令从数据库中还原了方块，暂不支持还原箱子内容物之类的细节。

### 查看玩家物品栏功能

天眼插件支持查看玩家的物品栏内的物品信息。

# 安装&配置&使用方法

## 安装endstone

这一步请参考endstone官方文档。

## 安装天眼插件

在release处下载最新版本的插件，解压出插件文件与数据文件夹放在服务端目录的plugins目录下，运行服务器即可使用。

## 配置天眼插件

运行天眼插件后，将自动在服务端目录的plugins目录下生成tianyan_data文件夹，并自动生成config.json文件，打开config.json，里面是默认配置

```
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

其从上到下分别代表：10秒内玩家可使用命令的最大次数、10秒内玩家可发送消息的最大次数、插件语言，不被记录的实体。

不被记录的实体，在涉及实体事件中，除非使用命名牌，否则不会被记录。

插件配置默认使用中文，可以通过修改config.json中的language项更改语言。支持的语言可在language文件夹查看。例：要修改为英文时，修改language项为"en_US"

## 插件命令使用方法

天眼命令使用方法

使用 /ty 命令查询玩家及部分实体行为记录 格式:

```
/ty <半径> <时间（单位：小时）>
```

无参数则将弹出快捷菜单，玩家可在菜单中查询。

使用 /tys 命令搜索关键词 格式:

```
/tys <时间（单位：小时）> [搜索对象类型] [搜索对象关键词]
```

搜索对象类型:<source_id | source_name | target_id | target_name> (行为源ID | 行为源名称 | 行为目标ID | 行为目标名称)  搜索对象关键词为要搜索的对象的关键词。

例：搜索2小时内名为"ZhangSan"的玩家实施的行为，使用命令`/tys 2 source_name "ZhangSan"`

```
/tys <时间（单位：小时）> <action> <搜索行为类型>
```

搜索行为类型: <block_break | block_place | entity_damage | player_right_click_block | player_right_click_entity | entity_bomb | block_break_bomb | piston_extend | piston_retract | entity_die | player_pickup_item>(方块破坏 | 方块放置 | 实体伤害 | 玩家右键方块 | 玩家右键实体 | 实体爆炸 | 方块爆炸 |  活塞弹出 | 活塞收回 | 实体死亡 | 玩家拾取物品)

例: 搜索2小时内的玩家方块放置行为，使用命令`/tys 2 action block_place`

使用 /tyo 命令查看在线玩家物品栏 格式:

```
/tyo 玩家名
```

使用/ban-id 命令将一名玩家的设备加入黑名单,格式:

```
/ban-id <设备ID> [理由]
```

使用/unban-id 命令将一名玩家的设备移出黑名单 格式:

```
/unban-id <设备ID>
```

使用/banlist-id 命令列出所有被加入黑名单的玩家的设备ID

```
/banlist-id
```

使用/tyclean 命令清理数据库

```
/tyclean <时间(单位:小时)>
```

使用/density 命令检测实体密度最高的区域

```
/density [区域大小(单位:格)]
```

使用/tyback 命令还原玩家直接方块放置与破坏行为和实体爆炸破坏

```
/tyback <半径> <时间（单位：小时）> <行为类型> <关键词>
```

## 修改&构建

### 云构建

fork 项目，在自己fork的分支中，使用action运行构建，几分钟后即可看到构建结果。Release需要在自己的action中配置一个名为`RELEASE_TOKEN`的密钥。

### 本地构建

按照endstone文档配置好开发环境，linux需要安装sqlite3开发库，windows需要使用vcpkg安装sqlite3静态开发库，并为cmake配置以下参数：

```
-DCMAKE_TOOLCHAIN_FILE="{Your vcpkg path}\scripts\buildsystems\vcpkg.cmake" `
-DVCPKG_TARGET_TRIPLET=x64-windows-static
```

使用git clone命令克隆项目代码

```shell
git clone https://github.com/yuhangle/endstone_TianyanPlugin.git
```

使用你喜欢的开发工具打开代码目录，以Clion为例，在构建中点击构建项目，即可构建插件。
