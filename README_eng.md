![header](https://capsule-render.vercel.app/api?type=waving&height=300&color=gradient&text=Tianyan%20Plugin&animation=fadeIn&textBg=false&descAlignY=78&desc=Behavior%20logging%20for%20Endstone&section=footer&reversal=true)


# Tianyan Plugin

[![中文](https://img.shields.io/badge/Chinese-README_chsinese.md-blue)](README.md) 
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
![Build Status](https://github.com/yuhangle/endstone-tianyan-plugin/actions/workflows/workflow.yml/badge.svg)

A player behavior logging and querying plugin for the [Endstone](https://github.com/EndstoneMC/endstone) plugin loader.

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

#### View Online Player Inventory Function
The Tianyan plugin supports viewing item information in online players' inventories.

#### 🌐 WebUI Panel

The Tianyan plugin provides a WebUI panel that can be accessed through a browser to view behavior records. To enable the WebUI feature, set "enable_web_ui" to true in the configuration file.

After the WebUI is started, a WebUI configuration file web_config.json will be generated in the plugin data directory:
```json
{
"secret": "your_secret",
"backend_port": 8098
}
```
- `secret`: Access key for the WebUI, used for authentication

- `backend_port`: WebUI port

Running the WebUI requires the following pip packages: `fastapi`, `uvicorn`. These packages will be automatically installed when the WebUI runs.

You can access the WebUI panel via `server_IP:WebUI_port`. For example, with the default port 8098, the access URL is `http://127.0.0.1:8098`.

You can also specify the backend IP and port within the WebUI after connecting.

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
    "10s_command_max": 12,
    "10s_message_max": 6,
    "enable_web_ui": false,
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

Configuration item descriptions:
- `10s_command_max`: Maximum number of commands players can use within 10 seconds
- `10s_message_max`: Maximum number of messages players can send within 10 seconds
- `enable_web_ui`: Enable WebUI
- `language`: Plugin language
- `no_log_mobs`: List of entities not to be logged

> In entity-related events, entities in `no_log_mobs` will not be recorded unless named tags are used.

The plugin configuration defaults to Chinese, and the language can be changed by modifying the `language` item in `config.json`. Supported languages can be viewed in the [language](language/zh_CN.json) folder.

Example: To change to English, modify the `language` item to `"en_US"`.

## 📜 Plugin Command Usage

### `/ty` - Query player and some entity behavior logs (cannot be used from console, regular members can use this command)
```
/ty <radius> <time (unit: hours)>
```
Without parameters, a quick menu will pop up where players can query. The maximum searchable radius is 100 (blocks), and the maximum searchable time is 672 (hours). (When query data exceeds 25,000 entries, the query will be truncated and only 25,000 data entries will be displayed)

### `/tys` - Search keywords (cannot be used from console)
```
/tys <time (unit: hours)> [search object type] [search object keyword]
```

Without parameters, a quick menu will pop up where players can query. Maximum searchable radius and time are unlimited. (When query data exceeds 25,000 entries, the query will be truncated and only 25,000 data entries will be displayed)

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

Example: Search for player block placement behaviors within 2 hours
```
/tys 2 action block_place
```

### `/tyo` - View online player inventory (cannot be used from console)
```
/tyo <player name>
```

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
Without parameters, a quick menu will pop up where players can query. The maximum restorable radius is 100 (blocks), and time is unlimited. (When query data exceeds 25,000 entries, the query will be truncated and only 25,000 data entries will be restored. The actual restorable radius is affected by chunks, and restoration will not be possible if beyond the limit distance of setblock and summon commands, requiring you to get closer and try again)

## 🔧 Modification & Building

### Cloud Building
1. Fork the project
2. In your forked branch, run builds using Actions
3. Build results can be seen after a few minutes
4. Releases need to configure a secret named `RELEASE_TOKEN` in your own Actions

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