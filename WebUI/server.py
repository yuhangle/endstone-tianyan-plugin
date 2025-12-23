import importlib.util
import json
import logging
import math
import os
import sqlite3
import subprocess
import sys
import time
from typing import List, Dict, Any

logger = logging.getLogger("tianyan_plugin")

# 自动安装依赖
# noinspection PyUnusedImports
def install_dependencies():
    """自动安装必要的Python依赖"""

    # 要检查的必需包列表
    required_packages = ['fastapi', 'uvicorn', 'pydantic']

    # 检查是否已安装
    missing_packages = []
    for package in required_packages:
        spec = importlib.util.find_spec(package)
        if spec is None:
            missing_packages.append(package)

    if not missing_packages:
        # print("All required dependencies are installed.")
        return True

    print(f"Missing dependencies found: {missing_packages}")
    print("Attempting automatic installation...")

    # 检查pip是否可用
    try:
        # 尝试导入pip
        import pip
        print("pip installed, version:", pip.__version__)
    except ImportError:
        print("pip not found, trying to install pip...")
        try:
            # 使用ensurepip安装pip
            import ensurepip
            subprocess.check_call([sys.executable, "-m", "ensurepip", "--upgrade"])
            print("pip installed successfully")
        except Exception as error:
            print(f"Failed to install pip: {error}")
            print("Please install pip manually: https://pip.pypa.io/en/stable/installation/")
            return False

    # 安装缺失的包
    try:
        # 首先升级pip
        subprocess.check_call([sys.executable, "-m", "pip", "install", "--upgrade", "pip"])

        requirements_file = os.path.join(os.path.dirname(__file__), "requirements.txt")
        if os.path.exists(requirements_file):
            print(f"Using requirements.txt to install dependencies: {requirements_file}")
            subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", requirements_file])
        else:
            # 否则只安装缺失的包
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
    required_packages = ['fastapi', 'uvicorn', 'pydantic']

    # print("Verifying dependencies...")
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
            # 重新验证
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
            else:
                print("\nAll dependencies installed successfully!")
                return True
        else:
            print("\nAutomatic installation failed, please install dependencies manually.")
            print("Please run: pip install " + " ".join(missing_packages))
            return False
    else:
        return True


# 在导入第三方库之前验证依赖
if __name__ != "__main__":
    # 如果是被导入的，跳过依赖检查
    pass
else:
    # 主程序运行时验证依赖
    if not verify_dependencies():
        print("Dependency check failed, program exiting.")
        sys.exit(1)

try:
    # noinspection PyUnusedImports
    import uvicorn
    # noinspection PyUnusedImports
    from fastapi import FastAPI, Header, HTTPException, Query
    # noinspection PyUnusedImports
    from fastapi.middleware.cors import CORSMiddleware
    # noinspection PyUnusedImports
    from pydantic import BaseModel
    # noinspection PyUnusedImports
    from fastapi.staticfiles import StaticFiles
    # noinspection PyUnusedImports
    from fastapi.responses import FileResponse
except ImportError as e:
    print(f"Failed to import third-party libraries: {e}")
    print("Please ensure all dependencies are installed:")
    print("pip install fastapi uvicorn pydantic")
    sys.exit(1)

# 获取当前脚本所在的绝对路径
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
# print(f"Base directory: {BASE_DIR}")
# 强制切换工作目录到 WebUI 文件夹
os.chdir(BASE_DIR)

# 重新定位路径（相对于 WebUI 文件夹）
DB_PATH = "../ty_data.db"
CONFIG_PATH = "../web_config.json"
LOG_PATH = "../logs/webui.log"
LANGUAGES_DIR = "languages"

# 确保日志目录存在
os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)

# 全局调试标志
DEBUG_MODE = "--debug" in sys.argv


def load_config() -> dict:
    """安全加载配置文件，带默认值和错误处理"""
    default_config = {
        "secret": "",
        "backend_port": 8098,
        # 可根据需要添加其他默认项
    }
    if not os.path.exists(CONFIG_PATH):
        logging.warning(f"Config file not found. Generating template at {CONFIG_PATH}")
        template = {
            "secret": "your_secret",
            "backend_port": 8098
        }
        with open(CONFIG_PATH, 'w', encoding='utf-8') as cf:
            json.dump(template, cf, indent=4, ensure_ascii=False)
        return template

    try:
        with open(CONFIG_PATH, 'r', encoding='utf-8') as cf:
            the_config = json.load(cf)
            # 合并默认值（只补充缺失字段）
            for key, value in default_config.items():
                if key not in the_config:
                    the_config[key] = value
            return the_config
    except json.JSONDecodeError as error:
        logging.error(f"Invalid JSON in config file {CONFIG_PATH}: {error}")
        raise RuntimeError(f"Configuration file is not valid JSON: {error}")
    except Exception as error:
        logging.error(f"Failed to load config file {CONFIG_PATH}: {error}")
        raise RuntimeError(f"Cannot load configuration: {error}")


def load_language(lang_code="en_US"):
    """加载语言文件"""
    lang_file = os.path.join(BASE_DIR, LANGUAGES_DIR, f"{lang_code}.json")

    # 如果请求的语言文件不存在，回退到英文
    if not os.path.exists(lang_file):
        lang_file = os.path.join(BASE_DIR, LANGUAGES_DIR, "en_US.json")

    try:
        with open(lang_file, 'r', encoding='utf-8') as lf:
            return json.load(lf)
    except Exception as error:
        logging.error(f"Failed to load language file {lang_file}: {error}")
        # 返回空字典
        return {}


# 配置日志系统
def setup_logging():
    """配置日志，输出到文件和控制台"""
    global logger
    # 根据调试模式设置日志级别
    if DEBUG_MODE:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)

    # 移除所有现有的处理器
    for handler in logger.handlers[:]:
        logger.removeHandler(handler)

    # 确保日志目录存在
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)

    # 👇 每次启动时清空日志文件内容
    with open(LOG_PATH, 'w', encoding='utf-8'):
        pass

    # 创建文件处理器
    file_handler = logging.FileHandler(LOG_PATH, encoding='utf-8')
    if DEBUG_MODE:
        file_handler.setLevel(logging.DEBUG)
    else:
        file_handler.setLevel(logging.INFO)

    # 创建控制台处理器（仅在调试模式或总是输出）
    console_handler = logging.StreamHandler()
    if DEBUG_MODE:
        console_handler.setLevel(logging.DEBUG)
    else:
        console_handler.setLevel(logging.INFO)

    # 创建格式化器
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    file_handler.setFormatter(formatter)
    console_handler.setFormatter(formatter)

    # 添加到 logger
    logger.addHandler(file_handler)
    logger.addHandler(console_handler)

    # 设置uvicorn日志
    if DEBUG_MODE:
        # 调试模式下保留更多日志
        logging.getLogger("uvicorn").setLevel(logging.DEBUG)
        logging.getLogger("uvicorn.access").setLevel(logging.DEBUG)
        logging.getLogger("uvicorn.error").setLevel(logging.DEBUG)
    else:
        # 生产模式下减少日志
        logging.getLogger("uvicorn").handlers = []
        logging.getLogger("uvicorn.access").handlers = []
        logging.getLogger("uvicorn.error").handlers = []


# 设置日志
setup_logging()

app = FastAPI()
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])


# 获取可用语言列表
@app.get("/api/languages")
async def get_languages():
    """获取可用的语言列表"""
    languages = []
    if os.path.exists(LANGUAGES_DIR):
        for file in os.listdir(LANGUAGES_DIR):
            if file.endswith(".json"):
                lang_code = file.replace(".json", "")
                languages.append(lang_code)

    # 确保至少返回英文
    if "en_US" not in languages:
        languages.append("en_US")

    return {"languages": sorted(languages), "default": "en_US"}


# 获取语言文件内容
@app.get("/api/language/{lang_code}")
async def get_language(lang_code: str):
    """获取指定语言文件内容"""
    return load_language(lang_code)


# 辅助函数：获取文件大小
def get_db_size():
    if os.path.exists(DB_PATH):
        size_bytes = os.path.getsize(DB_PATH)
        return f"{size_bytes / (1024 * 1024):.2f} MB"
    return "0 MB"


# 数据模型
class LogsResponse(BaseModel):
    data: List[Dict]
    total: int
    page: int
    page_size: int
    total_pages: int
    query_time_ms: float


@app.get("/api/stats")
async def get_stats(x_secret: str = Header(None)):
    web_config = load_config()
    if x_secret != web_config.get("secret"):
        raise HTTPException(status_code=403, detail="Secret Invalid")

    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM LOGDATA")
    total_count = cursor.fetchone()[0]
    conn.close()

    return {
        "total_logs": total_count,
        "db_size": get_db_size(),
        "server_time": int(time.time())
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
        x_secret: str = Header(None),
        x_lang: str = Header("en_US")
):
    start_time_query = time.time()  # 记录查询开始时间

    web_config = load_config()
    if x_secret != web_config.get("secret"):
        raise HTTPException(status_code=403, detail="Secret Invalid")

    # 检查坐标查询参数
    has_coord_query = all([center_x is not None, center_y is not None, center_z is not None, radius is not None])
    if any([center_x is not None, center_y is not None, center_z is not None, radius is not None]):
        if not has_coord_query:
            raise HTTPException(
                status_code=400,
                detail="坐标查询需要完整参数: center_x, center_y, center_z, radius"
            )
        if radius < 0:
            raise HTTPException(status_code=400, detail="半径不能为负数")

    # 记录请求参数（调试模式）
    if DEBUG_MODE:
        logging.debug(f"API /logs 请求参数:")
        logging.debug(f"  page={page}, page_size={page_size}")
        logging.debug(f"  start_time={start_time}, end_time={end_time}")
        logging.debug(f"  filter_type={filter_type}, filter_value={filter_value}")
        logging.debug(f"  center_x={center_x}, center_y={center_y}, center_z={center_z}, radius={radius}")
        logging.debug(f"  dimension={dimension}")
        logging.debug(f"  has_coord_query={has_coord_query}")

    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row

    try:
        cursor = conn.cursor()
        # 构建计数查询 - 用于获取总记录数
        count_query = "SELECT COUNT(*) FROM LOGDATA WHERE 1=1"
        data_query = "SELECT * FROM LOGDATA WHERE 1=1"
        count_params = []
        data_params = []

        # 时间范围过滤
        if start_time:
            count_query += " AND time >= ?"
            data_query += " AND time >= ?"
            count_params.append(start_time)
            data_params.append(start_time)
        if end_time:
            count_query += " AND time <= ?"
            data_query += " AND time <= ?"
            count_params.append(end_time)
            data_params.append(end_time)

        # 字段过滤（使用LIKE进行模糊匹配）
        if filter_type and filter_value:
            # 安全检查：防止 SQL 注入
            allowed_fields = ["id", "name", "type", "obj_id", "obj_name", "world", "status", "data"]
            if filter_type in allowed_fields:
                # 关键修复：先确保字段不为NULL且不为空字符串，再进行LIKE匹配
                # 这样可以避免NULL值导致的意外匹配
                count_query += f" AND ({filter_type} IS NOT NULL AND {filter_type} != '' AND {filter_type} LIKE ?)"
                data_query += f" AND ({filter_type} IS NOT NULL AND {filter_type} != '' AND {filter_type} LIKE ?)"
                count_params.append(f"%{filter_value}%")
                data_params.append(f"%{filter_value}%")

        # 维度过滤 - 关键修复：正确处理维度参数
        if dimension:
            if dimension.lower() == "all" or dimension == "" or dimension is None:
                # 全部维度，不做过滤
                pass
            else:
                count_query += " AND world = ?"
                data_query += " AND world = ?"
                count_params.append(dimension)
                data_params.append(dimension)

        # 坐标范围过滤（独立于其他过滤器）
        if has_coord_query:
            # 先确保坐标值有效，避免NULL值导致的计算问题
            valid_coord_condition = " AND pos_x IS NOT NULL AND pos_y IS NOT NULL AND pos_z IS NOT NULL"
            count_query += valid_coord_condition
            data_query += valid_coord_condition

            # 使用欧几里得距离公式 (x-x0)² + (y-y0)² + (z-z0)² <= radius²
            distance_condition = f"""
                AND ((pos_x - ?) * (pos_x - ?) + 
                    (pos_y - ?) * (pos_y - ?) + 
                    (pos_z - ?) * (pos_z - ?)) <= (? * ?)
            """
            count_query += distance_condition
            data_query += distance_condition
            # 添加参数
            params = [center_x, center_x, center_y, center_y, center_z, center_z, radius, radius]
            count_params.extend(params)
            data_params.extend(params)

        # 记录SQL语句（调试模式）
        if DEBUG_MODE:
            logging.debug(f"计数查询SQL: {count_query}")
            logging.debug(f"计数查询参数: {count_params}")
            logging.debug(f"数据查询SQL: {data_query}")
            logging.debug(f"数据查询参数: {data_params}")

        # 执行计数查询
        cursor.execute(count_query, count_params)
        total_records = cursor.fetchone()[0]

        # 计算分页
        offset = (page - 1) * page_size
        total_pages = (total_records + page_size - 1) // page_size  # 向上取整

        # 构建数据查询（带排序和分页）
        data_query += " ORDER BY time DESC LIMIT ? OFFSET ?"
        data_params.extend([page_size, offset])

        # 记录完整的SQL语句（调试模式）
        if DEBUG_MODE:
            logging.debug(f"最终数据查询SQL: {data_query}")
            logging.debug(f"最终数据查询参数: {data_params}")

        # 执行数据查询
        cursor.execute(data_query, data_params)
        rows = cursor.fetchall()

        # 转换为字典列表
        data = []
        for row in rows:
            record: Dict[str, Any] = dict(row)
            # 为每条记录计算距离（如果进行了坐标查询）
            if has_coord_query:
                dx = float(record["pos_x"]) - center_x
                dy = float(record["pos_y"]) - center_y
                dz = float(record["pos_z"]) - center_z
                distance = math.sqrt(dx * dx + dy * dy + dz * dz)
                record["distance"] = round(distance, 2)
            data.append(record)

        query_time = (time.time() - start_time_query) * 1000  # 转换为毫秒

        # 记录查询结果（调试模式）
        if DEBUG_MODE:
            logging.debug(f"查询结果: 找到 {len(data)} 条记录，总计 {total_records} 条")
            logging.debug(f"查询耗时: {query_time:.2f} 毫秒")
            if len(data) > 0:
                # 只记录前几条记录的信息
                for i, record in enumerate(data[:3]):
                    logging.debug(
                        f"记录 {i + 1}: id={record.get('id')}, name={record.get('name')}, type={record.get('type')}, obj_id={record.get('obj_id')}")

        return LogsResponse(
            data=data,
            total=total_records,
            page=page,
            page_size=page_size,
            total_pages=total_pages,
            query_time_ms=round(query_time, 2)
        )

    except sqlite3.Error as error:
        current_data_query = locals().get('data_query', '未知查询')
        current_data_params = locals().get('data_params', [])
        logging.error(f"数据库查询错误: {str(error)}")
        logging.error(f"出错的SQL语句: {current_data_query}")
        logging.error(f"SQL参数: {current_data_params}")
        raise HTTPException(status_code=500, detail=f"数据库查询错误: {str(error)}")
    finally:
        conn.close()


# 新增：批量查询接口（用于导出）
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
        x_secret: str = Header(None),
        x_lang: str = Header("en_US")
):
    web_config = load_config()
    if x_secret != web_config.get("secret"):
        raise HTTPException(status_code=403, detail="Secret Invalid")

    if start_page > end_page:
        raise HTTPException(status_code=400, detail="开始页码不能大于结束页码")

    if (end_page - start_page + 1) * page_size > 50000:
        raise HTTPException(status_code=400, detail="导出数据量过大，最多50000条")

    # 记录请求参数（调试模式）
    if DEBUG_MODE:
        logging.debug(f"API /export 请求参数:")
        logging.debug(f"  start_page={start_page}, end_page={end_page}, page_size={page_size}")
        logging.debug(f"  start_time={start_time}, end_time={end_time}")
        logging.debug(f"  filter_type={filter_type}, filter_value={filter_value}")
        logging.debug(f"  center_x={center_x}, center_y={center_y}, center_z={center_z}, radius={radius}")
        logging.debug(f"  dimension={dimension}")

    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row

    try:
        cursor = conn.cursor()

        # 先获取满足条件的数据总数
        count_query = "SELECT COUNT(*) FROM LOGDATA WHERE 1=1"
        count_params = []

        # 时间范围过滤
        if start_time:
            count_query += " AND time >= ?"
            count_params.append(start_time)
        if end_time:
            count_query += " AND time <= ?"
            count_params.append(end_time)

        # 字段过滤（关键修复：需要与get_logs函数保持一致）
        if filter_type and filter_value:
            allowed_fields = ["id", "name", "type", "obj_id", "obj_name", "world", "status", "data"]
            if filter_type in allowed_fields:
                # 关键修复：先确保字段不为NULL且不为空字符串，再进行LIKE匹配
                count_query += f" AND ({filter_type} IS NOT NULL AND {filter_type} != '' AND {filter_type} LIKE ?)"
                count_params.append(f"%{filter_value}%")

        # 维度过滤 - 关键修复
        if dimension and dimension.lower() != "all" and dimension != "" and dimension is not None:
            count_query += " AND world = ?"
            count_params.append(dimension)

        # 坐标范围过滤
        has_coord_query = all([center_x is not None, center_y is not None, center_z is not None, radius is not None])
        if has_coord_query:
            # 先确保坐标值有效，避免NULL值导致的计算问题
            valid_coord_condition = " AND pos_x IS NOT NULL AND pos_y IS NOT NULL AND pos_z IS NOT NULL"
            count_query += valid_coord_condition

            distance_condition = f"""
                AND ((pos_x - ?) * (pos_x - ?) + 
                    (pos_y - ?) * (pos_y - ?) + 
                    (pos_z - ?) * (pos_z - ?)) <= (? * ?)
            """
            count_query += distance_condition
            params = [center_x, center_x, center_y, center_y, center_z, center_z, radius, radius]
            count_params.extend(params)

        # 记录计数查询SQL（调试模式）
        if DEBUG_MODE:
            logging.debug(f"导出计数查询SQL: {count_query}")
            logging.debug(f"导出计数查询参数: {count_params}")

        cursor.execute(count_query, count_params)
        total_records = cursor.fetchone()[0]

        # 计算实际的结束页码
        total_pages = (total_records + page_size - 1) // page_size
        actual_end_page = min(end_page, total_pages)

        if start_page > total_pages:
            logging.warning(f"导出请求开始页码 {start_page} 超出总页数 {total_pages}")
            return {
                "data": [],
                "total_records": 0,
                "exported_records": 0,
                "pages_exported": 0,
                "message": f"开始页码 {start_page} 超出总页数 {total_pages}"
            }

        # 循环获取所有页的数据
        all_data = []
        for page_num in range(start_page, actual_end_page + 1):
            offset = (page_num - 1) * page_size

            data_query = "SELECT * FROM LOGDATA WHERE 1=1"
            data_params = []

            # 时间范围过滤
            if start_time:
                data_query += " AND time >= ?"
                data_params.append(start_time)
            if end_time:
                data_query += " AND time <= ?"
                data_params.append(end_time)

            # 字段过滤（关键修复：需要与计数查询保持一致）
            if filter_type and filter_value:
                allowed_fields = ["id", "name", "type", "obj_id", "obj_name", "world", "status", "data"]
                if filter_type in allowed_fields:
                    # 关键修复：先确保字段不为NULL且不为空字符串，再进行LIKE匹配
                    data_query += f" AND ({filter_type} IS NOT NULL AND {filter_type} != '' AND {filter_type} LIKE ?)"
                    data_params.append(f"%{filter_value}%")

            # 维度过滤 - 关键修复
            if dimension and dimension.lower() != "all" and dimension != "" and dimension is not None:
                data_query += " AND world = ?"
                data_params.append(dimension)

            # 坐标范围过滤
            if has_coord_query:
                distance_condition = f"""
                    AND ((pos_x - ?) * (pos_x - ?) + 
                         (pos_y - ?) * (pos_y - ?) + 
                         (pos_z - ?) * (pos_z - ?)) <= (? * ?)
                """
                data_query += distance_condition
                params = [center_x, center_x, center_y, center_y, center_z, center_z, radius, radius]
                data_params.extend(params)

            data_query += " ORDER BY time DESC LIMIT ? OFFSET ?"
            data_params.extend([page_size, offset])

            # 记录每页的SQL（调试模式）
            if DEBUG_MODE:
                logging.debug(f"导出第 {page_num} 页SQL: {data_query}")
                logging.debug(f"导出第 {page_num} 页参数: {data_params}")

            cursor.execute(data_query, data_params)
            rows = cursor.fetchall()
            for row in rows:
                record: Dict[str, Any] = dict(row)
                # 为每条记录计算距离（如果进行了坐标查询）
                if has_coord_query:
                    dx = float(record["pos_x"]) - center_x
                    dy = float(record["pos_y"]) - center_y
                    dz = float(record["pos_z"]) - center_z
                    distance = math.sqrt(dx * dx + dy * dy + dz * dz)
                    record["distance"] = round(distance, 2)
                all_data.append(record)

        # 记录导出结果（调试模式）
        if DEBUG_MODE:
            logging.debug(f"导出结果: 共导出 {len(all_data)} 条记录")
            logging.debug(f"导出页数: 从 {start_page} 页到 {actual_end_page} 页")
            if len(all_data) > 0:
                # 只记录前几条记录的信息
                for i, record in enumerate(all_data[:3]):
                    logging.debug(
                        f"导出记录 {i + 1}: id={record.get('id')}, name={record.get('name')}, type={record.get('type')}, obj_id={record.get('obj_id')}")

        return {
            "data": all_data,
            "total_records": total_records,
            "exported_records": len(all_data),
            "pages_exported": actual_end_page - start_page + 1,
            "start_page": start_page,
            "end_page": actual_end_page,
            "total_pages": total_pages
        }

    except sqlite3.Error as error:
        current_data_query = locals().get('data_query', '未知查询')
        current_data_params = locals().get('data_params', [])
        logging.error(f"导出数据错误: {str(error)}")
        logging.error(f"出错的SQL语句: {current_data_query}")
        logging.error(f"SQL参数: {current_data_params}")
        raise HTTPException(status_code=500, detail=f"导出数据错误: {str(error)}")
    finally:
        conn.close()


# 新增：获取数据库索引状态和建议
@app.get("/api/db_info")
async def get_db_info(x_secret: str = Header(None)):
    web_config = load_config()
    if x_secret != web_config.get("secret"):
        raise HTTPException(status_code=403, detail="Secret Invalid")

    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()

    try:
        # 获取表信息
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
        tables = [row[0] for row in cursor.fetchall()]

        # 获取索引信息
        cursor.execute("SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='LOGDATA'")
        indexes = [row[0] for row in cursor.fetchall()]

        # 获取列信息
        cursor.execute("PRAGMA table_info(LOGDATA)")
        columns = [{"name": row[1], "type": row[2], "notnull": row[3], "pk": row[5]} for row in cursor.fetchall()]

        # 分析查询性能
        cursor.execute("SELECT COUNT(*) FROM LOGDATA")
        total_rows = cursor.fetchone()[0]

        return {
            "tables": tables,
            "indexes": indexes,
            "columns": columns,
            "total_rows": total_rows
        }
    finally:
        conn.close()


# 用于调试
@app.get("/api/debug/query")
async def debug_query(
        sql: str = Query(None, description="SQL查询语句"),
        x_secret: str = Header(None)
):
    """调试接口：直接执行SQL查询（仅调试模式可用）"""
    if not DEBUG_MODE:
        raise HTTPException(status_code=403, detail="此接口仅在调试模式下可用")

    web_config = load_config()
    if x_secret != web_config.get("secret"):
        raise HTTPException(status_code=403, detail="Secret Invalid")

    if not sql or not sql.strip():
        raise HTTPException(status_code=400, detail="需要提供SQL查询语句")

    # 安全检查：限制为SELECT查询
    if not sql.strip().upper().startswith("SELECT"):
        raise HTTPException(status_code=400, detail="只允许SELECT查询")

    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row

    try:
        cursor = conn.cursor()
        logging.info(f"调试查询执行SQL: {sql}")

        cursor.execute(sql)
        rows = cursor.fetchall()

        # 转换为字典列表
        data = []
        for row in rows:
            data.append(dict(row))

        return {
            "success": True,
            "row_count": len(data),
            "data": data[:100],  # 限制返回前100条
            "total": len(data)
        }
    except sqlite3.Error as error:
        logging.error(f"调试查询错误: {str(error)}")
        return {
            "success": False,
            "error": str(error)
        }
    finally:
        conn.close()


@app.get("/")
async def get_index():
    return FileResponse(os.path.join(BASE_DIR, "index.html"))


if __name__ == "__main__":
    # 检查是否以调试模式启动
    if DEBUG_MODE:
        print(f"Boot mode: DEBUG")
        print(f"Database Path: {DB_PATH}")
        print(f"Config file: {CONFIG_PATH}")
        print(f"Log file: {LOG_PATH}")
        print(f"Languages Path: {LANGUAGES_DIR}")

    conf = load_config()
    # 配置uvicorn运行参数
    config = {
        "host": "0.0.0.0",
        "port": conf.get("backend_port", 8098),
        "log_config": None,  # 禁用uvicorn的默认日志配置
        "access_log": DEBUG_MODE,  # 仅在调试模式下启用访问日志
        "log_level": "debug" if DEBUG_MODE else "warning"  # 设置日志级别
    }

    logger.info(f"Start WebUI Service，127.0.0.1:{config['port']}")
    if conf.get("secret", "your_secret") == "your_secret":
        logger.warning("Using the default secret 'your_secret' — please update it for better security.")

    uvicorn.run(app, **config)