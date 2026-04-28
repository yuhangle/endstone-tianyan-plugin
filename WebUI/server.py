import importlib.util
import json
import logging
import math
import os
import subprocess
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

logger = logging.getLogger("tianyan_plugin")

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
VENV_DIR = os.path.join(BASE_DIR, ".venv")
VENV_PYTHON = (
    os.path.join(VENV_DIR, "Scripts", "python.exe")
    if os.name == "nt"
    else os.path.join(VENV_DIR, "bin", "python")
)
REQUIRED_PACKAGES = ["fastapi", "uvicorn", "pymysql"]


def in_virtualenv() -> bool:
    return sys.prefix != getattr(sys, "base_prefix", sys.prefix) or hasattr(sys, "real_prefix")


def install_with_python(python_executable: str, missing_packages: List[str]) -> None:
    requirements_file = os.path.join(BASE_DIR, "requirements.txt")
    subprocess.check_call([python_executable, "-m", "pip", "install", "--upgrade", "pip"])
    if os.path.exists(requirements_file):
        subprocess.check_call([python_executable, "-m", "pip", "install", "-r", requirements_file])
    else:
        subprocess.check_call([python_executable, "-m", "pip", "install", *missing_packages])


def install_dependencies() -> bool:
    missing_packages = [package for package in REQUIRED_PACKAGES if importlib.util.find_spec(package) is None]
    if not missing_packages:
        return True

    print(f"Missing dependencies found: {missing_packages}")
    print("Attempting automatic installation...")

    if not in_virtualenv():
        try:
            if not os.path.exists(VENV_PYTHON):
                print(f"Creating isolated Python environment: {VENV_DIR}")
                subprocess.check_call([sys.executable, "-m", "venv", VENV_DIR])
            install_with_python(VENV_PYTHON, missing_packages)
            print("Dependencies installed in isolated environment. Restarting Web API...")
            os.execv(VENV_PYTHON, [VENV_PYTHON, *sys.argv])
        except Exception as error:
            print(f"Failed to prepare isolated dependencies: {error}")
            return False

    try:
        import pip

        print("pip installed, version:", pip.__version__)
    except ImportError:
        print("pip not found, trying to install pip...")
        try:
            import ensurepip

            subprocess.check_call([sys.executable, "-m", "ensurepip", "--upgrade"])
        except Exception as error:
            print(f"Failed to install pip: {error}")
            return False

    try:
        install_with_python(sys.executable, missing_packages)
        print("Dependencies installed successfully!")
        return True
    except Exception as error:
        print(f"Failed to install dependencies: {error}")
        return False


def verify_dependencies() -> bool:
    missing_packages = []
    for package in REQUIRED_PACKAGES:
        try:
            importlib.import_module(package)
        except ImportError as error:
            missing_packages.append(package)
            print(f"Missing {package}: {error}")

    if not missing_packages:
        return True

    return install_dependencies()


if __name__ == "__main__" and not verify_dependencies():
    print("Dependency check failed, program exiting.")
    sys.exit(1)

try:
    import uvicorn
    from fastapi import FastAPI, HTTPException, Query
    from fastapi.middleware.cors import CORSMiddleware
    import pymysql
    from pymysql.cursors import DictCursor
except ImportError as error:
    print(f"Failed to import third-party libraries: {error}")
    print("Please ensure all dependencies are installed:")
    print("pip install fastapi uvicorn pymysql")
    sys.exit(1)


os.chdir(BASE_DIR)

PLUGIN_CONFIG_PATH = "../config.json"
LEGACY_WEB_CONFIG_PATH = "../web_config.json"
LOG_PATH = "../logs/webui.log"
READY_FILE = "ready"

DEFAULT_WINDOW_HOURS = 12
DEFAULT_QUERY_LIMIT = 100
MAX_QUERY_LIMIT = 200
MAX_EXPORT_RECORDS = 20000
PLAYER_SUGGESTION_LIMIT = 20
STATS_CACHE_TTL = 60
METADATA_CACHE_TTL = 300

DEBUG_MODE = "--debug" in sys.argv

ACTION_OPTIONS = [
    {"value": "block_break", "label": "方块破坏"},
    {"value": "block_place", "label": "方块放置"},
    {"value": "entity_damage", "label": "实体伤害"},
    {"value": "damage", "label": "环境伤害"},
    {"value": "player_right_click_block", "label": "方块触发"},
    {"value": "player_right_click_entity", "label": "实体交互"},
    {"value": "entity_bomb", "label": "爆炸源"},
    {"value": "block_break_bomb", "label": "爆炸破坏"},
    {"value": "piston_extend", "label": "活塞伸出"},
    {"value": "piston_retract", "label": "活塞收回"},
    {"value": "entity_die", "label": "实体死亡"},
    {"value": "player_pickup_item", "label": "拾取物品"},
]

PRESET_DEFINITIONS: Dict[str, Dict[str, Any]] = {
    "recent_activity": {
        "label": "最近活动",
        "description": "按时间倒序查看最近日志",
        "types": [],
    },
    "block_changes": {
        "label": "方块改动",
        "description": "放置、破坏、爆炸造成的方块变化",
        "types": ["block_break", "block_place", "block_break_bomb"],
    },
    "combat": {
        "label": "战斗与死亡",
        "description": "实体伤害、环境伤害、实体死亡",
        "types": ["entity_damage", "damage", "entity_die"],
    },
    "interactions": {
        "label": "交互记录",
        "description": "方块触发、实体交互、拾取物品",
        "types": ["player_right_click_block", "player_right_click_entity", "player_pickup_item"],
    },
    "redstone": {
        "label": "红石触发",
        "description": "活塞伸出与收回",
        "types": ["piston_extend", "piston_retract"],
    },
    "explosions": {
        "label": "爆炸相关",
        "description": "爆炸源与爆炸破坏记录",
        "types": ["entity_bomb", "block_break_bomb"],
    },
}

_stats_cache: Dict[str, Any] = {"expires_at": 0.0, "value": None}
_metadata_cache: Dict[str, Any] = {"expires_at": 0.0, "value": None}


def load_plugin_config() -> dict:
    default_config = {
        "mysql_host": "127.0.0.1",
        "mysql_port": 3306,
        "mysql_user": "root",
        "mysql_password": "root",
        "mysql_database": "tianyan",
        "web_backend_port": 8098,
    }

    if not os.path.exists(PLUGIN_CONFIG_PATH):
        logging.warning(f"Plugin config not found. Generating template at {PLUGIN_CONFIG_PATH}")
        with open(PLUGIN_CONFIG_PATH, "w", encoding="utf-8") as config_file:
            json.dump(default_config, config_file, indent=4, ensure_ascii=False)
        return default_config

    try:
        with open(PLUGIN_CONFIG_PATH, "r", encoding="utf-8") as config_file:
            config = json.load(config_file)
            for key, value in default_config.items():
                config.setdefault(key, value)
            return config
    except json.JSONDecodeError as error:
        raise RuntimeError(f"Configuration file is not valid JSON: {error}") from error


def load_legacy_web_config() -> dict:
    if not os.path.exists(LEGACY_WEB_CONFIG_PATH):
        return {}
    try:
        with open(LEGACY_WEB_CONFIG_PATH, "r", encoding="utf-8") as config_file:
            return json.load(config_file)
    except Exception:
        return {}


def get_backend_port() -> int:
    config = load_plugin_config()
    if "web_backend_port" in config:
        return int(config.get("web_backend_port", 8098))

    legacy = load_legacy_web_config()
    if "backend_port" in legacy:
        return int(legacy.get("backend_port", 8098))
    return 8098


def mysql_settings() -> dict:
    config = load_plugin_config()
    return {
        "host": config.get("mysql_host", "127.0.0.1"),
        "port": int(config.get("mysql_port", 3306)),
        "user": config.get("mysql_user", "root"),
        "password": config.get("mysql_password", "root"),
        "database": config.get("mysql_database", "tianyan"),
    }


def get_db_connection(dict_cursor: bool = False):
    config = mysql_settings()
    cursor_class = DictCursor if dict_cursor else pymysql.cursors.Cursor
    return pymysql.connect(
        host=config["host"],
        port=config["port"],
        user=config["user"],
        password=config["password"],
        database=config["database"],
        charset="utf8mb4",
        autocommit=True,
        connect_timeout=5,
        read_timeout=15,
        write_timeout=15,
        cursorclass=cursor_class,
    )


def setup_logging() -> None:
    logger.setLevel(logging.DEBUG if DEBUG_MODE else logging.INFO)
    for handler in logger.handlers[:]:
        logger.removeHandler(handler)

    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    with open(LOG_PATH, "w", encoding="utf-8"):
        pass

    formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")

    file_handler = logging.FileHandler(LOG_PATH, encoding="utf-8")
    file_handler.setFormatter(formatter)
    file_handler.setLevel(logging.DEBUG if DEBUG_MODE else logging.INFO)
    logger.addHandler(file_handler)

    console_handler = logging.StreamHandler()
    console_handler.setFormatter(formatter)
    console_handler.setLevel(logging.DEBUG if DEBUG_MODE else logging.INFO)
    logger.addHandler(console_handler)


def format_size(size_bytes: int) -> str:
    return f"{float(size_bytes) / (1024 * 1024):.2f} MB"


def normalize_text(value: Optional[str]) -> Optional[str]:
    if value is None:
        return None
    stripped = value.strip()
    return stripped or None


def normalize_time_window(start_time: Optional[int], end_time: Optional[int]) -> Tuple[int, int]:
    now = int(time.time())
    effective_end = end_time if end_time is not None else now
    effective_start = start_time if start_time is not None else effective_end - DEFAULT_WINDOW_HOURS * 3600

    if effective_start > effective_end:
        raise HTTPException(status_code=400, detail="开始时间不能晚于结束时间")
    return effective_start, effective_end


def build_coord_filters(center_x: Optional[float], center_y: Optional[float], center_z: Optional[float],
                        radius: Optional[float], conditions: List[str], params: List[Any]) -> bool:
    coord_values = [center_x, center_y, center_z, radius]
    if any(value is not None for value in coord_values):
        if not all(value is not None for value in coord_values):
            raise HTTPException(status_code=400, detail="坐标查询需要完整参数: center_x, center_y, center_z, radius")
        if radius is None or radius <= 0:
            raise HTTPException(status_code=400, detail="半径必须大于 0")

        min_x = center_x - radius
        max_x = center_x + radius
        min_y = center_y - radius
        max_y = center_y + radius
        min_z = center_z - radius
        max_z = center_z + radius

        conditions.extend(
            [
                "pos_x BETWEEN %s AND %s",
                "pos_y BETWEEN %s AND %s",
                "pos_z BETWEEN %s AND %s",
                "((pos_x - %s) * (pos_x - %s) + (pos_y - %s) * (pos_y - %s) + (pos_z - %s) * (pos_z - %s)) <= (%s * %s)",
            ]
        )
        params.extend(
            [
                min_x,
                max_x,
                min_y,
                max_y,
                min_z,
                max_z,
                center_x,
                center_x,
                center_y,
                center_y,
                center_z,
                center_z,
                radius,
                radius,
            ]
        )
        return True
    return False


def resolve_action_types(preset: Optional[str], action_type: Optional[str]) -> List[str]:
    action_value = normalize_text(action_type)
    if action_value:
        if action_value not in {item["value"] for item in ACTION_OPTIONS}:
            raise HTTPException(status_code=400, detail=f"未知行为类型: {action_value}")
        return [action_value]

    preset_value = normalize_text(preset)
    if not preset_value:
        return []
    if preset_value not in PRESET_DEFINITIONS:
        raise HTTPException(status_code=400, detail=f"未知预设: {preset_value}")
    return list(PRESET_DEFINITIONS[preset_value]["types"])


def build_log_query(
    *,
    limit: int,
    enforce_limit: bool = True,
    preset: Optional[str],
    start_time: Optional[int],
    end_time: Optional[int],
    player_name: Optional[str],
    action_type: Optional[str],
    target_name: Optional[str],
    target_id: Optional[str],
    world: Optional[str],
    center_x: Optional[float],
    center_y: Optional[float],
    center_z: Optional[float],
    radius: Optional[float],
    cursor_time: Optional[int],
    cursor_uuid: Optional[str],
) -> Tuple[str, List[Any], bool, int, int]:
    if limit < 1:
        raise HTTPException(status_code=400, detail="limit 必须大于 0")
    if enforce_limit and limit > MAX_QUERY_LIMIT:
        raise HTTPException(status_code=400, detail=f"limit 必须在 1 到 {MAX_QUERY_LIMIT} 之间")

    effective_start, effective_end = normalize_time_window(start_time, end_time)
    conditions = ["time >= %s", "time <= %s"]
    params: List[Any] = [effective_start, effective_end]

    action_types = resolve_action_types(preset, action_type)
    if action_types:
        placeholders = ", ".join(["%s"] * len(action_types))
        conditions.append(f"type IN ({placeholders})")
        params.extend(action_types)

    player_name_value = normalize_text(player_name)
    target_name_value = normalize_text(target_name)
    target_id_value = normalize_text(target_id)
    world_value = normalize_text(world)

    if player_name_value:
        conditions.append("name = %s")
        params.append(player_name_value)
    if target_name_value:
        conditions.append("obj_name = %s")
        params.append(target_name_value)
    if target_id_value:
        conditions.append("obj_id = %s")
        params.append(target_id_value)
    if world_value and world_value.lower() != "all":
        conditions.append("world = %s")
        params.append(world_value)

    has_coord_query = build_coord_filters(center_x, center_y, center_z, radius, conditions, params)

    if cursor_time is not None:
        cursor_uuid_value = normalize_text(cursor_uuid)
        if not cursor_uuid_value:
            raise HTTPException(status_code=400, detail="cursor_uuid 不能为空")
        conditions.append("(time < %s OR (time = %s AND uuid < %s))")
        params.extend([cursor_time, cursor_time, cursor_uuid_value])

    where_clause = " AND ".join(conditions)
    sql = (
        "SELECT uuid, id, name, pos_x, pos_y, pos_z, world, obj_id, obj_name, time, type, data, status "
        f"FROM LOGDATA WHERE {where_clause} "
        "ORDER BY time DESC, uuid DESC LIMIT %s"
    )
    params.append(limit + 1)
    return sql, params, has_coord_query, effective_start, effective_end


def serialize_log_row(row: Dict[str, Any], has_coord_query: bool, center_x: Optional[float],
                      center_y: Optional[float], center_z: Optional[float]) -> Dict[str, Any]:
    record = dict(row)
    if has_coord_query and center_x is not None and center_y is not None and center_z is not None:
        dx = float(record.get("pos_x", 0.0)) - center_x
        dy = float(record.get("pos_y", 0.0)) - center_y
        dz = float(record.get("pos_z", 0.0)) - center_z
        record["distance"] = round(math.sqrt(dx * dx + dy * dy + dz * dz), 2)
    return record


def fetch_coordinate_page(
    cursor,
    *,
    limit: int,
    preset: Optional[str],
    start_time: Optional[int],
    end_time: Optional[int],
    player_name: Optional[str],
    action_type: Optional[str],
    target_name: Optional[str],
    target_id: Optional[str],
    world: Optional[str],
    center_x: float,
    center_y: float,
    center_z: float,
    radius: float,
    cursor_time: Optional[int],
    cursor_uuid: Optional[str],
) -> Dict[str, Any]:
    started_at = time.time()
    collected: List[Dict[str, Any]] = []
    scan_cursor_time = cursor_time
    scan_cursor_uuid = cursor_uuid
    scanned_rows = 0
    scan_limit = 10000
    batch_size = min(max(limit * 10, 1000), 2000)

    def matches_coordinate(row: Dict[str, Any]) -> bool:
        row_x = row.get("pos_x")
        row_y = row.get("pos_y")
        row_z = row.get("pos_z")
        if row_x is None or row_y is None or row_z is None:
            return False

        min_x = center_x - radius
        max_x = center_x + radius
        min_y = center_y - radius
        max_y = center_y + radius
        min_z = center_z - radius
        max_z = center_z + radius

        row_x = float(row_x)
        row_y = float(row_y)
        row_z = float(row_z)
        if row_x < min_x or row_x > max_x or row_y < min_y or row_y > max_y or row_z < min_z or row_z > max_z:
            return False

        dx = row_x - center_x
        dy = row_y - center_y
        dz = row_z - center_z
        return (dx * dx + dy * dy + dz * dz) <= (radius * radius)

    effective_start = 0
    effective_end = 0

    while len(collected) <= limit and scanned_rows < scan_limit:
        sql, params, _, effective_start, effective_end = build_log_query(
            limit=batch_size,
            enforce_limit=False,
            preset=preset,
            start_time=start_time,
            end_time=end_time,
            player_name=player_name,
            action_type=action_type,
            target_name=target_name,
            target_id=target_id,
            world=world,
            center_x=None,
            center_y=None,
            center_z=None,
            radius=None,
            cursor_time=scan_cursor_time,
            cursor_uuid=scan_cursor_uuid,
        )

        cursor.execute(sql, tuple(params))
        rows = cursor.fetchall()
        if not rows:
            break

        scanned_rows += len(rows)
        for row in rows:
            if matches_coordinate(row):
                collected.append(serialize_log_row(row, True, center_x, center_y, center_z))
                if len(collected) > limit:
                    break

        scan_cursor_time = rows[-1]["time"]
        scan_cursor_uuid = rows[-1]["uuid"]
        if len(rows) < batch_size:
            break

    has_more = len(collected) > limit
    data = collected[:limit]
    next_cursor = None
    if has_more and data:
        last_row = data[-1]
        next_cursor = {"time": last_row["time"], "uuid": last_row["uuid"]}

    return {
        "data": data,
        "count": len(data),
        "limit": limit,
        "has_more": has_more,
        "next_cursor": next_cursor,
        "query_time_ms": round((time.time() - started_at) * 1000, 2),
        "start_time": effective_start,
        "end_time": effective_end,
    }


def fetch_log_page(
    cursor,
    *,
    limit: int,
    preset: Optional[str],
    start_time: Optional[int],
    end_time: Optional[int],
    player_name: Optional[str],
    action_type: Optional[str],
    target_name: Optional[str],
    target_id: Optional[str],
    world: Optional[str],
    center_x: Optional[float],
    center_y: Optional[float],
    center_z: Optional[float],
    radius: Optional[float],
    cursor_time: Optional[int],
    cursor_uuid: Optional[str],
) -> Dict[str, Any]:
    coord_values = [center_x, center_y, center_z, radius]
    coord_requested = any(value is not None for value in coord_values)
    if coord_requested and not all(value is not None for value in coord_values):
        raise HTTPException(status_code=400, detail="坐标查询需要完整参数: center_x, center_y, center_z, radius")
    if coord_requested and radius is not None and radius <= 0:
        raise HTTPException(status_code=400, detail="半径必须大于 0")
    if coord_requested:
        return fetch_coordinate_page(
            cursor,
            limit=limit,
            preset=preset,
            start_time=start_time,
            end_time=end_time,
            player_name=player_name,
            action_type=action_type,
            target_name=target_name,
            target_id=target_id,
            world=world,
            center_x=center_x,
            center_y=center_y,
            center_z=center_z,
            radius=radius,
            cursor_time=cursor_time,
            cursor_uuid=cursor_uuid,
        )

    started_at = time.time()
    sql, params, has_coord_query, effective_start, effective_end = build_log_query(
        limit=limit,
        enforce_limit=True,
        preset=preset,
        start_time=start_time,
        end_time=end_time,
        player_name=player_name,
        action_type=action_type,
        target_name=target_name,
        target_id=target_id,
        world=world,
        center_x=center_x,
        center_y=center_y,
        center_z=center_z,
        radius=radius,
        cursor_time=cursor_time,
        cursor_uuid=cursor_uuid,
    )

    if DEBUG_MODE:
        logger.debug(f"Executing log query: {sql}")
        logger.debug(f"Query params: {params}")

    cursor.execute(sql, tuple(params))
    rows = cursor.fetchall()
    has_more = len(rows) > limit
    page_rows = rows[:limit]
    data = [
        serialize_log_row(row, has_coord_query, center_x, center_y, center_z)
        for row in page_rows
    ]

    next_cursor = None
    if has_more and page_rows:
        last_row = page_rows[-1]
        next_cursor = {"time": last_row["time"], "uuid": last_row["uuid"]}

    return {
        "data": data,
        "count": len(data),
        "limit": limit,
        "has_more": has_more,
        "next_cursor": next_cursor,
        "query_time_ms": round((time.time() - started_at) * 1000, 2),
        "start_time": effective_start,
        "end_time": effective_end,
    }


def get_cached_stats(force_refresh: bool = False) -> Dict[str, Any]:
    now = time.time()
    if not force_refresh and _stats_cache["value"] is not None and _stats_cache["expires_at"] > now:
        return _stats_cache["value"]

    config = mysql_settings()
    conn = get_db_connection(dict_cursor=True)
    try:
        with conn.cursor() as cursor:
            cursor.execute(
                """
                SELECT
                    COALESCE(table_rows, 0) AS approx_rows,
                    COALESCE(data_length + index_length, 0) AS size_bytes
                FROM information_schema.tables
                WHERE table_schema = %s AND table_name = 'LOGDATA'
                """,
                (config["database"],),
            )
            row = cursor.fetchone() or {"approx_rows": 0, "size_bytes": 0}

        stats = {
            "total_logs": int(row.get("approx_rows") or 0),
            "db_size": format_size(int(row.get("size_bytes") or 0)),
            "server_time": int(time.time()),
        }
        _stats_cache["value"] = stats
        _stats_cache["expires_at"] = now + STATS_CACHE_TTL
        return stats
    finally:
        conn.close()


def get_cached_metadata(force_refresh: bool = False) -> Dict[str, Any]:
    now = time.time()
    if not force_refresh and _metadata_cache["value"] is not None and _metadata_cache["expires_at"] > now:
        return _metadata_cache["value"]

    conn = get_db_connection(dict_cursor=True)
    try:
        with conn.cursor() as cursor:
            cursor.execute("SELECT DISTINCT world FROM LOGDATA WHERE world IS NOT NULL AND world <> '' ORDER BY world LIMIT 32")
            worlds = [row["world"] for row in cursor.fetchall() if row.get("world")]

        metadata = {
            "worlds": worlds,
            "actions": ACTION_OPTIONS,
            "presets": [
                {"value": key, **value}
                for key, value in PRESET_DEFINITIONS.items()
            ],
        }
        _metadata_cache["value"] = metadata
        _metadata_cache["expires_at"] = now + METADATA_CACHE_TTL
        return metadata
    finally:
        conn.close()


setup_logging()

app = FastAPI(docs_url=None, redoc_url=None, openapi_url=None)
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])


def get_player_suggestions(q: str, limit: int) -> Dict[str, Any]:
    query_text = normalize_text(q)
    if not query_text or len(query_text) < 1:
        return {"players": []}
    suggestion_limit = min(max(limit, 1), PLAYER_SUGGESTION_LIMIT)

    conn = get_db_connection(dict_cursor=True)
    try:
        with conn.cursor() as cursor:
            cursor.execute(
                """
                SELECT name, MAX(time) AS last_seen
                FROM LOGDATA
                WHERE id = 'minecraft:player' AND name IS NOT NULL AND name <> '' AND name LIKE %s
                GROUP BY name
                ORDER BY last_seen DESC
                LIMIT %s
                """,
                (f"{query_text}%", suggestion_limit),
            )
            rows = cursor.fetchall()
        return {"players": [row["name"] for row in rows if row.get("name")]}
    except pymysql.MySQLError as error:
        raise HTTPException(status_code=500, detail=f"获取玩家建议失败: {error}") from error
    finally:
        conn.close()


def get_logs(
    *,
    limit: int,
    preset: str,
    start_time: Optional[int],
    end_time: Optional[int],
    player_name: Optional[str],
    action_type: Optional[str],
    target_name: Optional[str],
    target_id: Optional[str],
    world: Optional[str],
    center_x: Optional[float],
    center_y: Optional[float],
    center_z: Optional[float],
    radius: Optional[float],
    cursor_time: Optional[int],
    cursor_uuid: Optional[str],
) -> Dict[str, Any]:
    conn = get_db_connection(dict_cursor=True)
    try:
        with conn.cursor() as cursor:
            return fetch_log_page(
                cursor,
                limit=limit,
                preset=preset,
                start_time=start_time,
                end_time=end_time,
                player_name=player_name,
                action_type=action_type,
                target_name=target_name,
                target_id=target_id,
                world=world,
                center_x=center_x,
                center_y=center_y,
                center_z=center_z,
                radius=radius,
                cursor_time=cursor_time,
                cursor_uuid=cursor_uuid,
            )
    except pymysql.MySQLError as error:
        raise HTTPException(status_code=500, detail=f"数据库查询错误: {error}") from error
    finally:
        conn.close()


def export_logs(
    *,
    max_records: int,
    preset: str,
    start_time: Optional[int],
    end_time: Optional[int],
    player_name: Optional[str],
    action_type: Optional[str],
    target_name: Optional[str],
    target_id: Optional[str],
    world: Optional[str],
    center_x: Optional[float],
    center_y: Optional[float],
    center_z: Optional[float],
    radius: Optional[float],
) -> Dict[str, Any]:
    conn = get_db_connection(dict_cursor=True)
    try:
        all_rows: List[Dict[str, Any]] = []
        next_cursor_time: Optional[int] = None
        next_cursor_uuid: Optional[str] = None
        query_started = time.time()

        with conn.cursor() as cursor:
            while len(all_rows) < max_records:
                current_limit = min(MAX_QUERY_LIMIT, max_records - len(all_rows))
                page = fetch_log_page(
                    cursor,
                    limit=current_limit,
                    preset=preset,
                    start_time=start_time,
                    end_time=end_time,
                    player_name=player_name,
                    action_type=action_type,
                    target_name=target_name,
                    target_id=target_id,
                    world=world,
                    center_x=center_x,
                    center_y=center_y,
                    center_z=center_z,
                    radius=radius,
                    cursor_time=next_cursor_time,
                    cursor_uuid=next_cursor_uuid,
                )
                all_rows.extend(page["data"])
                if not page["has_more"] or not page["next_cursor"]:
                    break
                next_cursor_time = page["next_cursor"]["time"]
                next_cursor_uuid = page["next_cursor"]["uuid"]

        return {
            "data": all_rows,
            "exported_records": len(all_rows),
            "max_records": max_records,
            "query_time_ms": round((time.time() - query_started) * 1000, 2),
        }
    except pymysql.MySQLError as error:
        raise HTTPException(status_code=500, detail=f"导出失败: {error}") from error
    finally:
        conn.close()


@app.get("/api/query")
async def query(
    mode: str = Query("health", description="health/options/players/stats/logs/export"),
    limit: int = Query(DEFAULT_QUERY_LIMIT, ge=1, le=MAX_QUERY_LIMIT, description="每次返回条数"),
    max_records: int = Query(5000, ge=1, le=MAX_EXPORT_RECORDS, description="最多导出条数"),
    force_refresh: bool = Query(False, description="是否强制刷新统计缓存"),
    q: str = Query("", description="玩家名前缀"),
    preset: str = Query("recent_activity", description="预设检索"),
    start_time: Optional[int] = Query(None, description="开始时间（Unix 时间戳）"),
    end_time: Optional[int] = Query(None, description="结束时间（Unix 时间戳）"),
    player_name: Optional[str] = Query(None, description="玩家名"),
    action_type: Optional[str] = Query(None, description="行为类型"),
    target_name: Optional[str] = Query(None, description="目标名称"),
    target_id: Optional[str] = Query(None, description="目标 ID"),
    world: Optional[str] = Query(None, description="维度"),
    center_x: Optional[float] = Query(None, description="中心点 X"),
    center_y: Optional[float] = Query(None, description="中心点 Y"),
    center_z: Optional[float] = Query(None, description="中心点 Z"),
    radius: Optional[float] = Query(None, description="半径"),
    cursor_time: Optional[int] = Query(None, description="下一页时间游标"),
    cursor_uuid: Optional[str] = Query(None, description="下一页 UUID 游标"),
):
    mode_value = normalize_text(mode)
    if mode_value == "health":
        return {"ok": True, "server_time": int(time.time())}
    if mode_value == "options":
        try:
            return get_cached_metadata()
        except Exception as error:
            raise HTTPException(status_code=500, detail=f"获取选项失败: {error}") from error
    if mode_value == "players":
        return get_player_suggestions(q=q, limit=limit)
    if mode_value == "stats":
        try:
            return get_cached_stats(force_refresh=force_refresh)
        except Exception as error:
            raise HTTPException(status_code=500, detail=f"获取状态失败: {error}") from error
    if mode_value == "logs":
        return get_logs(
            limit=limit,
            preset=preset,
            start_time=start_time,
            end_time=end_time,
            player_name=player_name,
            action_type=action_type,
            target_name=target_name,
            target_id=target_id,
            world=world,
            center_x=center_x,
            center_y=center_y,
            center_z=center_z,
            radius=radius,
            cursor_time=cursor_time,
            cursor_uuid=cursor_uuid,
        )
    if mode_value == "export":
        return export_logs(
            max_records=max_records,
            preset=preset,
            start_time=start_time,
            end_time=end_time,
            player_name=player_name,
            action_type=action_type,
            target_name=target_name,
            target_id=target_id,
            world=world,
            center_x=center_x,
            center_y=center_y,
            center_z=center_z,
            radius=radius,
        )

    raise HTTPException(status_code=400, detail=f"未知查询模式: {mode_value}")


if __name__ == "__main__":
    if DEBUG_MODE:
        config = mysql_settings()
        print("Boot mode: DEBUG")
        print(f"MySQL Host: {config['host']}")
        print(f"MySQL Port: {config['port']}")
        print(f"MySQL Database: {config['database']}")
        print(f"Config file: {PLUGIN_CONFIG_PATH}")
        print(f"Log file: {LOG_PATH}")

    server_config = {
        "host": "0.0.0.0",
        "port": get_backend_port(),
        "log_config": None,
        "access_log": DEBUG_MODE,
        "log_level": "debug" if DEBUG_MODE else "warning",
    }

    logger.info(f"Start WebUI Service，127.0.0.1:{server_config['port']}")
    with open(READY_FILE, "w", encoding="utf-8"):
        pass

    uvicorn.run(app, **server_config)
