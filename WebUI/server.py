import importlib.util
import json
import logging
import math
import os
import subprocess
import sys
import time
from typing import Any, Dict, List

logger = logging.getLogger("tianyan_plugin")


def install_dependencies():
    """自动安装必要的Python依赖"""
    required_packages = ["fastapi", "uvicorn", "pydantic", "pymysql"]

    missing_packages = []
    for package in required_packages:
        spec = importlib.util.find_spec(package)
        if spec is None:
            missing_packages.append(package)

    if not missing_packages:
        return True

    print(f"Missing dependencies found: {missing_packages}")
    print("Attempting automatic installation...")

    try:
        import pip
        print("pip installed, version:", pip.__version__)
    except ImportError:
        print("pip not found, trying to install pip...")
        try:
            import ensurepip
            subprocess.check_call([sys.executable, "-m", "ensurepip", "--upgrade"])
            print("pip installed successfully")
        except Exception as error:
            print(f"Failed to install pip: {error}")
            print("Please install pip manually: https://pip.pypa.io/en/stable/installation/")
            return False

    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "--upgrade", "pip"])

        requirements_file = os.path.join(os.path.dirname(__file__), "requirements.txt")
        if os.path.exists(requirements_file):
            print(f"Using requirements.txt to install dependencies: {requirements_file}")
            subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", requirements_file])
        else:
            print("requirements.txt not found, installing missing packages...")
            for package in missing_packages:
                print(f"Installing {package}...")
                subprocess.check_call([sys.executable, "-m", "pip", "install", package])

        print("Dependencies installed successfully!")
        return True

    except subprocess.CalledProcessError as error:
        print(f"Failed to install dependencies: {error}")
        return False
    except Exception as error:
        print(f"Unknown error during installation: {error}")
        return False


def verify_dependencies():
    """验证所有必需依赖是否已安装"""
    required_packages = ["fastapi", "uvicorn", "pydantic", "pymysql"]

    missing_packages = []
    for package in required_packages:
        try:
            importlib.import_module(package)
        except ImportError as error:
            missing_packages.append(package)
            print(f"✗ {package} not installed: {error}")

    if missing_packages:
        print(f"\nMissing required dependencies: {missing_packages}")
        print("Attempting automatic installation...")

        if install_dependencies():
            print("\nRe-verifying dependencies...")
            final_missing = []
            for package in required_packages:
                try:
                    importlib.import_module(package)
                except ImportError:
                    final_missing.append(package)

            if final_missing:
                print(f"\nError: Failed to install the following dependencies: {final_missing}")
                print("Please install manually: pip install " + " ".join(final_missing))
                return False
            print("\nAll dependencies installed successfully!")
            return True

        print("\nAutomatic installation failed, please install dependencies manually.")
        print("Please run: pip install " + " ".join(missing_packages))
        return False

    return True


if __name__ == "__main__":
    if not verify_dependencies():
        print("Dependency check failed, program exiting.")
        sys.exit(1)

try:
    import uvicorn
    from fastapi import FastAPI, HTTPException, Query
    from fastapi.middleware.cors import CORSMiddleware
    from fastapi.responses import FileResponse
    from pydantic import BaseModel
    import pymysql
    from pymysql.cursors import DictCursor
except ImportError as e:
    print(f"Failed to import third-party libraries: {e}")
    print("Please ensure all dependencies are installed:")
    print("pip install fastapi uvicorn pydantic pymysql")
    sys.exit(1)


BASE_DIR = os.path.dirname(os.path.abspath(__file__))
os.chdir(BASE_DIR)

PLUGIN_CONFIG_PATH = "../config.json"
LEGACY_WEB_CONFIG_PATH = "../web_config.json"
LOG_PATH = "../logs/webui.log"
LANGUAGES_DIR = "languages"
READY_FILE = "ready"

os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)

DEBUG_MODE = "--debug" in sys.argv


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
        with open(PLUGIN_CONFIG_PATH, "w", encoding="utf-8") as cf:
            json.dump(default_config, cf, indent=4, ensure_ascii=False)
        return default_config

    try:
        with open(PLUGIN_CONFIG_PATH, "r", encoding="utf-8") as cf:
            config = json.load(cf)
            for key, value in default_config.items():
                if key not in config:
                    config[key] = value
            return config
    except json.JSONDecodeError as error:
        logging.error(f"Invalid JSON in config file {PLUGIN_CONFIG_PATH}: {error}")
        raise RuntimeError(f"Configuration file is not valid JSON: {error}")
    except Exception as error:
        logging.error(f"Failed to load config file {PLUGIN_CONFIG_PATH}: {error}")
        raise RuntimeError(f"Cannot load configuration: {error}")


def load_legacy_web_config() -> dict:
    if not os.path.exists(LEGACY_WEB_CONFIG_PATH):
        return {}
    try:
        with open(LEGACY_WEB_CONFIG_PATH, "r", encoding="utf-8") as cf:
            return json.load(cf)
    except Exception:
        return {}


def get_backend_port() -> int:
    conf = load_plugin_config()
    if "web_backend_port" in conf:
        return int(conf.get("web_backend_port", 8098))

    legacy = load_legacy_web_config()
    if "backend_port" in legacy:
        return int(legacy.get("backend_port", 8098))

    return 8098


def mysql_settings() -> dict:
    conf = load_plugin_config()
    return {
        "host": conf.get("mysql_host", "127.0.0.1"),
        "port": int(conf.get("mysql_port", 3306)),
        "user": conf.get("mysql_user", "root"),
        "password": conf.get("mysql_password", "root"),
        "database": conf.get("mysql_database", "tianyan"),
    }


def get_db_connection(dict_cursor: bool = False):
    cfg = mysql_settings()
    cursor_class = DictCursor if dict_cursor else pymysql.cursors.Cursor

    return pymysql.connect(
        host=cfg["host"],
        port=cfg["port"],
        user=cfg["user"],
        password=cfg["password"],
        database=cfg["database"],
        charset="utf8mb4",
        autocommit=True,
        connect_timeout=8,
        read_timeout=30,
        write_timeout=30,
        cursorclass=cursor_class,
    )


def load_language(lang_code="en_US"):
    lang_file = os.path.join(BASE_DIR, LANGUAGES_DIR, f"{lang_code}.json")

    if not os.path.exists(lang_file):
        lang_file = os.path.join(BASE_DIR, LANGUAGES_DIR, "en_US.json")

    try:
        with open(lang_file, "r", encoding="utf-8") as lf:
            return json.load(lf)
    except Exception as error:
        logging.error(f"Failed to load language file {lang_file}: {error}")
        return {}


def setup_logging():
    global logger
    logger.setLevel(logging.DEBUG if DEBUG_MODE else logging.INFO)

    for handler in logger.handlers[:]:
        logger.removeHandler(handler)

    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    with open(LOG_PATH, "w", encoding="utf-8"):
        pass

    file_handler = logging.FileHandler(LOG_PATH, encoding="utf-8")
    file_handler.setLevel(logging.DEBUG if DEBUG_MODE else logging.INFO)

    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.DEBUG if DEBUG_MODE else logging.INFO)

    formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
    file_handler.setFormatter(formatter)
    console_handler.setFormatter(formatter)

    logger.addHandler(file_handler)
    logger.addHandler(console_handler)

    if DEBUG_MODE:
        logging.getLogger("uvicorn").setLevel(logging.DEBUG)
        logging.getLogger("uvicorn.access").setLevel(logging.DEBUG)
        logging.getLogger("uvicorn.error").setLevel(logging.DEBUG)
    else:
        logging.getLogger("uvicorn").handlers = []
        logging.getLogger("uvicorn.access").handlers = []
        logging.getLogger("uvicorn.error").handlers = []


setup_logging()

app = FastAPI()
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])


def get_db_size():
    try:
        cfg = mysql_settings()
        conn = get_db_connection(dict_cursor=False)
        try:
            with conn.cursor() as cursor:
                cursor.execute(
                    "SELECT COALESCE(SUM(data_length + index_length), 0) "
                    "FROM information_schema.tables WHERE table_schema = %s",
                    (cfg["database"],),
                )
                size_bytes = cursor.fetchone()[0] or 0
                return f"{float(size_bytes) / (1024 * 1024):.2f} MB"
        finally:
            conn.close()
    except Exception as error:
        logging.error(f"Failed to get database size: {error}")
        return "Unknown"


class LogsResponse(BaseModel):
    data: List[Dict]
    total: int
    page: int
    page_size: int
    total_pages: int
    query_time_ms: float


@app.get("/api/languages")
async def get_languages():
    languages = []
    if os.path.exists(LANGUAGES_DIR):
        for file in os.listdir(LANGUAGES_DIR):
            if file.endswith(".json"):
                languages.append(file.replace(".json", ""))

    if "en_US" not in languages:
        languages.append("en_US")

    return {"languages": sorted(languages), "default": "en_US"}


@app.get("/api/language/{lang_code}")
async def get_language(lang_code: str):
    return load_language(lang_code)


@app.get("/api/stats")
async def get_stats():
    try:
        conn = get_db_connection(dict_cursor=False)
    except Exception as error:
        raise HTTPException(status_code=500, detail=f"MySQL连接失败: {error}")

    try:
        with conn.cursor() as cursor:
            cursor.execute("SELECT COUNT(*) FROM LOGDATA")
            total_count = cursor.fetchone()[0]
    except pymysql.MySQLError as error:
        raise HTTPException(status_code=500, detail=f"数据库查询错误: {error}")
    finally:
        conn.close()

    return {
        "total_logs": total_count,
        "db_size": get_db_size(),
        "server_time": int(time.time()),
    }


@app.get("/api/logs", response_model=LogsResponse)
async def get_logs(
    page: int = Query(1, ge=1, description="页码"),
    page_size: int = Query(100, ge=1, le=500, description="每页条数"),
    start_time: int = Query(None, description="开始时间（Unix时间戳）"),
    end_time: int = Query(None, description="结束时间（Unix时间戳）"),
    filter_type: str = Query(None, description="过滤字段类型"),
    filter_value: str = Query(None, description="过滤值（支持模糊匹配）"),
    center_x: float = Query(None, description="中心点X坐标"),
    center_y: float = Query(None, description="中心点Y坐标"),
    center_z: float = Query(None, description="中心点Z坐标"),
    radius: float = Query(None, description="查询半径（格）"),
    dimension: str = Query(None, description="维度名称"),
):
    start_time_query = time.time()

    has_coord_query = all([center_x is not None, center_y is not None, center_z is not None, radius is not None])
    if any([center_x is not None, center_y is not None, center_z is not None, radius is not None]):
        if not has_coord_query:
            raise HTTPException(status_code=400, detail="坐标查询需要完整参数: center_x, center_y, center_z, radius")
        if radius < 0:
            raise HTTPException(status_code=400, detail="半径不能为负数")

    if DEBUG_MODE:
        logging.debug("API /logs 请求参数:")
        logging.debug(f"  page={page}, page_size={page_size}")
        logging.debug(f"  start_time={start_time}, end_time={end_time}")
        logging.debug(f"  filter_type={filter_type}, filter_value={filter_value}")
        logging.debug(f"  center_x={center_x}, center_y={center_y}, center_z={center_z}, radius={radius}")
        logging.debug(f"  dimension={dimension}")
        logging.debug(f"  has_coord_query={has_coord_query}")

    try:
        conn = get_db_connection(dict_cursor=True)
    except Exception as error:
        raise HTTPException(status_code=500, detail=f"MySQL连接失败: {error}")

    try:
        with conn.cursor() as cursor:
            conditions = ["1=1"]
            params = []

            if start_time:
                conditions.append("time >= %s")
                params.append(start_time)
            if end_time:
                conditions.append("time <= %s")
                params.append(end_time)

            if filter_type and filter_value:
                allowed_fields = ["id", "name", "type", "obj_id", "obj_name", "world", "status", "data"]
                if filter_type in allowed_fields:
                    conditions.append(f"({filter_type} IS NOT NULL AND {filter_type} != '' AND {filter_type} LIKE %s)")
                    params.append(f"%{filter_value}%")

            if dimension and dimension.lower() != "all":
                conditions.append("world = %s")
                params.append(dimension)

            if has_coord_query:
                conditions.append("pos_x IS NOT NULL")
                conditions.append("pos_y IS NOT NULL")
                conditions.append("pos_z IS NOT NULL")
                conditions.append(
                    "((pos_x - %s) * (pos_x - %s) + (pos_y - %s) * (pos_y - %s) + (pos_z - %s) * (pos_z - %s)) <= (%s * %s)"
                )
                params.extend([center_x, center_x, center_y, center_y, center_z, center_z, radius, radius])

            where_clause = " AND ".join(conditions)
            count_query = f"SELECT COUNT(*) AS cnt FROM LOGDATA WHERE {where_clause}"

            if DEBUG_MODE:
                logging.debug(f"计数查询SQL: {count_query}")
                logging.debug(f"计数查询参数: {params}")

            cursor.execute(count_query, tuple(params))
            total_records = cursor.fetchone()["cnt"]

            offset = (page - 1) * page_size
            total_pages = (total_records + page_size - 1) // page_size

            data_query = f"SELECT * FROM LOGDATA WHERE {where_clause} ORDER BY time DESC LIMIT %s OFFSET %s"
            data_params = params + [page_size, offset]

            if DEBUG_MODE:
                logging.debug(f"最终数据查询SQL: {data_query}")
                logging.debug(f"最终数据查询参数: {data_params}")

            cursor.execute(data_query, tuple(data_params))
            rows = cursor.fetchall()

        data = []
        for row in rows:
            record = dict(row)
            if has_coord_query:
                dx = float(record.get("pos_x", 0.0)) - center_x
                dy = float(record.get("pos_y", 0.0)) - center_y
                dz = float(record.get("pos_z", 0.0)) - center_z
                distance = math.sqrt(dx * dx + dy * dy + dz * dz)
                record["distance"] = round(distance, 2)
            data.append(record)

        query_time = (time.time() - start_time_query) * 1000

        if DEBUG_MODE:
            logging.debug(f"查询结果: 找到 {len(data)} 条记录，总计 {total_records} 条")
            logging.debug(f"查询耗时: {query_time:.2f} 毫秒")

        return LogsResponse(
            data=data,
            total=total_records,
            page=page,
            page_size=page_size,
            total_pages=total_pages,
            query_time_ms=round(query_time, 2),
        )

    except pymysql.MySQLError as error:
        current_data_query = locals().get("data_query", "未知查询")
        current_data_params = locals().get("data_params", [])
        logging.error(f"数据库查询错误: {str(error)}")
        logging.error(f"出错的SQL语句: {current_data_query}")
        logging.error(f"SQL参数: {current_data_params}")
        raise HTTPException(status_code=500, detail=f"数据库查询错误: {str(error)}")
    finally:
        conn.close()


@app.get("/api/export")
async def export_logs(
    start_page: int = Query(1, ge=1, description="开始页码"),
    end_page: int = Query(1, ge=1, description="结束页码"),
    page_size: int = Query(100, ge=1, le=500, description="每页条数"),
    start_time: int = Query(None, description="开始时间（Unix时间戳）"),
    end_time: int = Query(None, description="结束时间（Unix时间戳）"),
    filter_type: str = Query(None, description="过滤字段类型"),
    filter_value: str = Query(None, description="过滤值（支持模糊匹配）"),
    center_x: float = Query(None, description="中心点X坐标"),
    center_y: float = Query(None, description="中心点Y坐标"),
    center_z: float = Query(None, description="中心点Z坐标"),
    radius: float = Query(None, description="查询半径（格）"),
    dimension: str = Query(None, description="维度名称"),
):
    if start_page > end_page:
        raise HTTPException(status_code=400, detail="开始页码不能大于结束页码")

    if (end_page - start_page + 1) * page_size > 50000:
        raise HTTPException(status_code=400, detail="导出数据量过大，最多50000条")

    if DEBUG_MODE:
        logging.debug("API /export 请求参数:")
        logging.debug(f"  start_page={start_page}, end_page={end_page}, page_size={page_size}")
        logging.debug(f"  start_time={start_time}, end_time={end_time}")
        logging.debug(f"  filter_type={filter_type}, filter_value={filter_value}")
        logging.debug(f"  center_x={center_x}, center_y={center_y}, center_z={center_z}, radius={radius}")
        logging.debug(f"  dimension={dimension}")

    try:
        conn = get_db_connection(dict_cursor=True)
    except Exception as error:
        raise HTTPException(status_code=500, detail=f"MySQL连接失败: {error}")

    try:
        with conn.cursor() as cursor:
            conditions = ["1=1"]
            count_params = []

            if start_time:
                conditions.append("time >= %s")
                count_params.append(start_time)
            if end_time:
                conditions.append("time <= %s")
                count_params.append(end_time)

            if filter_type and filter_value:
                allowed_fields = ["id", "name", "type", "obj_id", "obj_name", "world", "status", "data"]
                if filter_type in allowed_fields:
                    conditions.append(f"({filter_type} IS NOT NULL AND {filter_type} != '' AND {filter_type} LIKE %s)")
                    count_params.append(f"%{filter_value}%")

            if dimension and dimension.lower() != "all":
                conditions.append("world = %s")
                count_params.append(dimension)

            has_coord_query = all([center_x is not None, center_y is not None, center_z is not None, radius is not None])
            if has_coord_query:
                conditions.append("pos_x IS NOT NULL")
                conditions.append("pos_y IS NOT NULL")
                conditions.append("pos_z IS NOT NULL")
                conditions.append(
                    "((pos_x - %s) * (pos_x - %s) + (pos_y - %s) * (pos_y - %s) + (pos_z - %s) * (pos_z - %s)) <= (%s * %s)"
                )
                count_params.extend([center_x, center_x, center_y, center_y, center_z, center_z, radius, radius])

            where_clause = " AND ".join(conditions)
            count_query = f"SELECT COUNT(*) AS cnt FROM LOGDATA WHERE {where_clause}"

            if DEBUG_MODE:
                logging.debug(f"导出计数查询SQL: {count_query}")
                logging.debug(f"导出计数查询参数: {count_params}")

            cursor.execute(count_query, tuple(count_params))
            total_records = cursor.fetchone()["cnt"]

            total_pages = (total_records + page_size - 1) // page_size
            actual_end_page = min(end_page, total_pages)

            if start_page > total_pages:
                logging.warning(f"导出请求开始页码 {start_page} 超出总页数 {total_pages}")
                return {
                    "data": [],
                    "total_records": 0,
                    "exported_records": 0,
                    "pages_exported": 0,
                    "message": f"开始页码 {start_page} 超出总页数 {total_pages}",
                }

            all_data = []
            for page_num in range(start_page, actual_end_page + 1):
                offset = (page_num - 1) * page_size
                data_query = f"SELECT * FROM LOGDATA WHERE {where_clause} ORDER BY time DESC LIMIT %s OFFSET %s"
                data_params = count_params + [page_size, offset]

                if DEBUG_MODE:
                    logging.debug(f"导出第 {page_num} 页SQL: {data_query}")
                    logging.debug(f"导出第 {page_num} 页参数: {data_params}")

                cursor.execute(data_query, tuple(data_params))
                rows = cursor.fetchall()

                for row in rows:
                    record = dict(row)
                    if has_coord_query:
                        dx = float(record.get("pos_x", 0.0)) - center_x
                        dy = float(record.get("pos_y", 0.0)) - center_y
                        dz = float(record.get("pos_z", 0.0)) - center_z
                        distance = math.sqrt(dx * dx + dy * dy + dz * dz)
                        record["distance"] = round(distance, 2)
                    all_data.append(record)

        if DEBUG_MODE:
            logging.debug(f"导出结果: 共导出 {len(all_data)} 条记录")
            logging.debug(f"导出页数: 从 {start_page} 页到 {actual_end_page} 页")

        return {
            "data": all_data,
            "total_records": total_records,
            "exported_records": len(all_data),
            "pages_exported": actual_end_page - start_page + 1,
            "start_page": start_page,
            "end_page": actual_end_page,
            "total_pages": total_pages,
        }

    except pymysql.MySQLError as error:
        current_data_query = locals().get("data_query", "未知查询")
        current_data_params = locals().get("data_params", [])
        logging.error(f"导出数据错误: {str(error)}")
        logging.error(f"出错的SQL语句: {current_data_query}")
        logging.error(f"SQL参数: {current_data_params}")
        raise HTTPException(status_code=500, detail=f"导出数据错误: {str(error)}")
    finally:
        conn.close()


@app.get("/api/db_info")
async def get_db_info():
    try:
        conn = get_db_connection(dict_cursor=True)
    except Exception as error:
        raise HTTPException(status_code=500, detail=f"MySQL连接失败: {error}")

    try:
        cfg = mysql_settings()
        with conn.cursor() as cursor:
            cursor.execute("SHOW TABLES")
            table_rows = cursor.fetchall()
            tables = [list(row.values())[0] for row in table_rows]

            cursor.execute("SHOW INDEX FROM LOGDATA")
            index_rows = cursor.fetchall()
            indexes = sorted({row.get("Key_name", "") for row in index_rows if row.get("Key_name")})

            cursor.execute("SHOW COLUMNS FROM LOGDATA")
            column_rows = cursor.fetchall()
            columns = []
            for row in column_rows:
                columns.append(
                    {
                        "name": row.get("Field"),
                        "type": row.get("Type"),
                        "notnull": 1 if row.get("Null") == "NO" else 0,
                        "pk": 1 if row.get("Key") == "PRI" else 0,
                    }
                )

            cursor.execute("SELECT COUNT(*) AS cnt FROM LOGDATA")
            total_rows = cursor.fetchone()["cnt"]

        return {
            "database": cfg["database"],
            "tables": tables,
            "indexes": indexes,
            "columns": columns,
            "total_rows": total_rows,
        }
    except pymysql.MySQLError as error:
        raise HTTPException(status_code=500, detail=f"获取数据库信息失败: {error}")
    finally:
        conn.close()


@app.get("/api/debug/query")
async def debug_query(sql: str = Query(None, description="SQL查询语句")):
    if not DEBUG_MODE:
        raise HTTPException(status_code=403, detail="此接口仅在调试模式下可用")

    if not sql or not sql.strip():
        raise HTTPException(status_code=400, detail="需要提供SQL查询语句")

    if not sql.strip().upper().startswith("SELECT"):
        raise HTTPException(status_code=400, detail="只允许SELECT查询")

    try:
        conn = get_db_connection(dict_cursor=True)
    except Exception as error:
        raise HTTPException(status_code=500, detail=f"MySQL连接失败: {error}")

    try:
        with conn.cursor() as cursor:
            logging.info(f"调试查询执行SQL: {sql}")
            cursor.execute(sql)
            rows = cursor.fetchall()
            data = [dict(row) for row in rows]

        return {
            "success": True,
            "row_count": len(data),
            "data": data[:100],
            "total": len(data),
        }
    except pymysql.MySQLError as error:
        logging.error(f"调试查询错误: {str(error)}")
        return {
            "success": False,
            "error": str(error),
        }
    finally:
        conn.close()


@app.get("/")
async def get_index():
    return FileResponse(os.path.join(BASE_DIR, "index.html"))


if __name__ == "__main__":
    if DEBUG_MODE:
        cfg = mysql_settings()
        print("Boot mode: DEBUG")
        print(f"MySQL Host: {cfg['host']}")
        print(f"MySQL Port: {cfg['port']}")
        print(f"MySQL Database: {cfg['database']}")
        print(f"Config file: {PLUGIN_CONFIG_PATH}")
        print(f"Log file: {LOG_PATH}")
        print(f"Languages Path: {LANGUAGES_DIR}")

    config = {
        "host": "0.0.0.0",
        "port": get_backend_port(),
        "log_config": None,
        "access_log": DEBUG_MODE,
        "log_level": "debug" if DEBUG_MODE else "warning",
    }

    logger.info(f"Start WebUI Service，127.0.0.1:{config['port']}")
    with open(READY_FILE, "w", encoding="utf-8"):
        pass

    uvicorn.run(app, **config)
