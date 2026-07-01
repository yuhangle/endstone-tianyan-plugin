![header](https://capsule-render.vercel.app/api?type=waving&height=300&color=gradient&text=Tianyan%20Protect&animation=fadeIn&textBg=false&descAlignY=78&desc=Lightweight%2C%20Fast%2C%20and%20Secure&section=footer&reversal=true)


# Tianyan Protect Plugin

[![中文](https://img.shields.io/badge/中文-README_chs.md-blue)](README_chs.md) 
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
![Build Status](https://github.com/yuhangle/endstone-tianyan-plugin/actions/workflows/workflow.yml/badge.svg)

Tianyan Protect Plugin is a log data recording and anti-malicious behavior plugin for the [Endstone](https://github.com/EndstoneMC/endstone) plugin loader, supporting a degree of rollback operations. Out-of-the-box, simple and efficient. Plays a role in security protection on Endstone servers.

## 📋 Feature Introduction

### 🔍 Behavior Logging

The Tianyan plugin uses Endstone's event API to log behavioral events, and can record the following behavioral events:

#### Player Interaction Events with Blocks and Entities
Interaction events between players and blocks or certain entities will be recorded.

#### Player Direct Block Breaking and Entity Explosion Block Destruction Events
Both players direct block breaking and entity explosion block destruction events will be recorded. Indirect block breaking by players, such as trampling farmland, cannot trigger events; operations on blocks by non-player entities will not trigger events unless they explode themselves.

#### Entity Damage Events
Except for entities excluded in the configuration file, all entity damage events will be recorded. Some special entities, such as paintings that are directly removed when attacked, cannot trigger damage events.

#### Player Block Placement Events
The plugin will record all direct block placement behaviors implemented by players.

#### Piston Events
All piston self-behaviors will be recorded, but the blocks being pushed cannot be recorded.

#### Liquid Flow Events
Liquid flow events will be recorded, capturing block information on the liquid's flow path, such as torches washed away by flowing water. However, secondary reactions cannot be recorded, such as campfires being extinguished by water or stone forming when water meets lava.

### 👤 Player Information Display and Ban Functions

#### Player Join Information Display
The Tianyan plugin supports displaying player join information. When a player joins the server, the player's system name and device ID will be displayed.

#### Ban Function & Anti-Spam Function
The plugin can ban devices by banning device IDs, adding devices using these device IDs to the ban list, preventing any player using that device (unchanged device ID) from entering the server. Players who have used banned devices will automatically have their new devices added to the ban list if they switch devices.

Automatic ban rules:
- When players send a large number of messages in a short time (6 messages within 10 seconds) in the server, they will be automatically banned for 24 hours
- When players send a large number of commands in a short time (12 commands within 10 seconds) in the server, they will be automatically banned for 24 hours

### 🛠️ Other Features

#### Restore Player-Caused Block Destruction, Placement, and Explosion Damage Functions
The Tianyan plugin supports restoring player-directly caused block destruction, block placement, and explosion damage. The principle is simply using the setblock command to restore blocks from the database, but it does not currently support restoring details such as chest contents.

#### View Player Inventory Function
The Tianyan plugin supports viewing item information in both online and offline players' inventories. Use `/tyo` to view a player's inventory through a built-in visual chest GUI — click any item to copy it to your own inventory. For offline players, inventory data is read directly from the world save files. The player must have joined the server at least once (to cache their UUID) to be queried by name.

#### 🌐 WebUI Panel

The Tianyan plugin provides a WebUI panel that can be accessed through a browser to view behavior records. To enable WebUI, set `"enable_web_ui": true` in `config.json`. WebUI configuration is also part of the main `config.json` — see the configuration section below.

You can access the WebUI panel via `server_IP:WebUI_port`. For example, with the default port 8098, the access URL is `http://127.0.0.1:8098`.

## 🚀 Installation, Configuration & Usage

### Install Endstone
Please refer to the [Endstone official documentation](https://github.com/EndstoneMC/endstone) for this step.

### Install Tianyan Plugin
1. Download the latest version of the plugin from the Releases page
2. Extract the plugin files and data folder
3. Place them in the `plugins` directory of the server directory
4. Run the server to use it

### Configure Tianyan Plugin

After running the Tianyan plugin, the `tianyan_data` folder will be automatically generated in the `plugins` directory of the server directory, and the `config.json` file will be automatically generated. Language files will not be automatically generated, please download them from the GitHub repository.

Default configuration:

```json
{
    "language": "zh_CN",
    "database_type": "sqlite",
    "mysql_host": "127.0.0.1",
    "mysql_port": 3306,
    "mysql_user": "root",
    "mysql_password": "",
    "mysql_database": "endstone",
    "enable_web_ui": false,
    "web_secret": "your_secret",
    "web_port": 8098,
    "10s_message_max": 6,
    "10s_command_max": 12,
    "no_log_mobs": [
        "minecraft:zombie_pigman",
        "minecraft:zombie",
        "minecraft:skeleton",
        "minecraft:bogged",
        "minecraft:slime"
    ],
    "enforce_no_log_mobs": false,
    "log_piston": true,
    "log_block_bomb": true,
    "log_entity_bomb": true,
    "log_block_break": true,
    "log_block_place": true,
    "log_entity_damage": true,
    "log_player_right_click_block": true,
    "log_player_right_click_entity": true,
    "log_entity_die": true,
    "log_player_pickup_item": true,
    "log_player_drop_item": true,
    "log_liquid_flow": true
}
```

Configuration item descriptions:
- `language`: Plugin language
- `database_type`: Database type, `sqlite` (default) or `mysql`
- `mysql_host`: MySQL server address (used when `database_type` is `mysql`)
- `mysql_port`: MySQL server port
- `mysql_user`: MySQL login user
- `mysql_password`: MySQL login password
- `mysql_database`: MySQL database name
- `enable_web_ui`: Enable WebUI
- `web_secret`: WebUI access key, used for authentication
- `web_port`: WebUI listen port (default 8098)
- `10s_message_max`: Maximum number of messages players can send within 10 seconds
- `10s_command_max`: Maximum number of commands players can use within 10 seconds
- `no_log_mobs`: List of entities not to be logged
- `enforce_no_log_mobs`: When set to `true`, entities in `no_log_mobs` are never logged even if named (default `false`)
- `log_piston`: Whether to log piston extend/retract events
- `log_block_bomb`: Whether to log block explosion events
- `log_entity_bomb`: Whether to log entity explosion events
- `log_block_break`: Whether to log block break events
- `log_block_place`: Whether to log block place events
- `log_entity_damage`: Whether to log entity damage events
- `log_player_right_click_block`: Whether to log player right-click block events
- `log_player_right_click_entity`: Whether to log player right-click entity events
- `log_entity_die`: Whether to log entity/player death events
- `log_player_pickup_item`: Whether to log player pickup item events
- `log_player_drop_item`: Whether to log player drop item events
- `log_liquid_flow`: Whether to log liquid flow events

> In entity-related events, entities in `no_log_mobs` will not be recorded unless the entity has been named. If `enforce_no_log_mobs` is set to `true`, entities in `no_log_mobs` will never be recorded, even if named.

The plugin configuration defaults to Chinese, and the language can be changed by modifying the `language` item in `config.json`. Supported languages can be viewed in the [language](language/zh_CN.json) folder.

Example: To change to English, modify the `language` item to `"en_US"`.

### MySQL Database Support

The Tianyan plugin supports using MySQL as the log storage backend. The MySQL client is built into the plugin using a Rust implementation (no additional dependencies required).

To enable MySQL, modify `plugins/tianyan_data/config.json`:

```json
{
    "database_type": "mysql",
    "mysql_host": "127.0.0.1",
    "mysql_port": 3306,
    "mysql_user": "root",
    "mysql_password": "",
    "mysql_database": "endstone"
}
```

Configuration notes:
- The MySQL connection parameters use the defaults shown above; only change what you need
- If MySQL connection fails, the plugin will automatically fall back to SQLite
- Use the `/tymigrate` command to migrate data between databases

## 📜 Plugin Command Usage

### `/ty` - Query player and some entity behavior logs (cannot be used from console, regular members can use this command)
```
/ty <radius> <time (unit: hours)>
```
Without parameters, a quick menu will pop up where players can query. The maximum searchable radius is 100 (blocks), and the maximum searchable time is 672 (hours).

### `/tys` - Search keywords (cannot be used from console)
```
/tys <time (unit: hours)> [search object type] [search object keyword]
```

Without parameters, a quick menu will pop up where players can query. Maximum searchable radius and time are unlimited.

Search object types:
- `source_id`: Behavior source ID
- `source_name`: Behavior source name
- `target_id`: Behavior target ID
- `target_name`: Behavior target name

Example: Search for behaviors implemented by a player named "ZhangSan" within 2 hours
```
/tys 2 source_name "ZhangSan"
```

Search specific behavior types:
```
/tys <time (unit: hours)> <action> <search behavior type>
```

Search behavior types:
- `block_break`: Block breaking
- `block_place`: Block placement
- `entity_damage`: Entity damage
- `player_right_click_block`: Player right-clicking block
- `player_right_click_entity`: Player right-clicking entity
- `entity_bomb`: Entity explosion
- `block_break_bomb`: Block explosion
- `piston_extend`: Piston extending
- `piston_retract`: Piston retracting
- `entity_die`: Entity death
- `player_pickup_item`: Player picking up item
- `player_drop_item`: Player dropping item
- `block_bomb`: Block explosion
- `liquid_flow`: Liquid flow

Example: Search for player block placement behaviors within 2 hours
```
/tys 2 action block_place
```

### `/tyo` - View player inventory (cannot be used from console)
```
/tyo <player name>
```
Opens a visual chest GUI to view the player's inventory. Click an item to copy it to your own inventory.

### `/ban-id` - Add device to blacklist
```
/ban-id <device ID> [reason]
```

### `/unban-id` - Remove device from blacklist
```
/unban-id <device ID>
```

### `/banlist-id` - List all banned device IDs
```
/banlist-id
```

### `/tyclean` - Clean database
```
/tyclean <time (unit: hours)>
```

This command can clean logs in the database that exceed the specified time.

### `/density` - Detect areas with the highest entity density
```
/density [area size (unit: blocks)]
```

This command can find areas with the highest entity density in the server and support teleporting to that area. Area size represents the edge length of a single search area (cube).

### `/tyback` - Restore player or entity partial behaviors (experimental; cannot be used from console)
```
/tyback <radius> <time (unit: hours)> <behavior type> <keyword>
```

This command can restore partial behaviors of players or entities. Including: player direct block destruction and placement, block destruction caused by entity explosions, entity deaths, and block changes caused by player interactions. Entity deaths will not restore attributes such as fur color and age; water-containing blocks created by players cannot be restored to their original state. The principle is to use setblock and summon commands to restore from the log database.
Without parameters, a quick menu will pop up where players can query. The maximum restorable radius is 100 (blocks), and time is unlimited. The actual restorable radius is affected by chunks, and restoration will not be possible if beyond the limit distance of setblock and summon commands, requiring you to get closer and try again)

### `/tymigrate` - Migrate database between SQLite and MySQL (operator only)
```
/tymigrate <sqlite|mysql> <sqlite|mysql>
```

Migrates all log data from the source database backend to the target backend. The active backend is automatically switched upon successful completion, no server restart required.

Examples:
- `/tymigrate sqlite mysql` — migrate from SQLite to MySQL
- `/tymigrate mysql sqlite` — migrate from MySQL to SQLite

## 🔧 Modification & Building

### Cloud Building
1. Fork the project
2. In your forked branch, run builds using Actions
3. Build results can be seen after a few minutes

### Local Building

Configure the development environment according to Endstone documentation:

- Linux needs to install sqlite3 development library
- Windows needs to use vcpkg or other package managers to install sqlite3 static development library

Clone the project code:
```shell
git clone https://github.com/yuhangle/endstone-tianyan-plugin.git
```

Open the code directory with your preferred development tool. Taking CLion as an example, click build project in the build to build the plugin.

When using vcpkg, Windows needs to configure the following parameters for CMake:

```
-DCMAKE_TOOLCHAIN_FILE="{Your vcpkg path}\scripts\buildsystems\vcpkg.cmake" \
-DVCPKG_TARGET_TRIPLET=x64-windows-static
```
