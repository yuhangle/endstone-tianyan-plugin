cmake_minimum_required(VERSION 3.15)

project(tianyan CXX)

set(BASE_VERSION "1.2.3")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /utf-8")
endif()


include(FetchContent)
include(ExternalProject)
FetchContent_Declare(
        endstone
        GIT_REPOSITORY https://github.com/EndstoneMC/endstone.git
        GIT_TAG v0.11.4
)
FetchContent_MakeAvailable(endstone)

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

# fmt (header-only or static)
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.2.0
)
FetchContent_MakeAvailable(fmt)

# endstone-inventoryui (static linking for embedded visual inventory UI)
set(INVENTORYUI_BUILD_AS_STATIC ON)
FetchContent_Declare(
    endstone_inventoryui
    GIT_REPOSITORY https://github.com/yuhangle/endstone-inventoryui-api.git
    GIT_TAG main
)
FetchContent_MakeAvailable(endstone_inventoryui)

# ---- cpp-httplib (WebUI 后端 HTTP 库) ----
FetchContent_Declare(
    cpp_httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.0
)
FetchContent_MakeAvailable(cpp_httplib)

# ---- 前端资源内置化（Vue 3 + Vite 单文件构建） ----
set(WEBUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/WebUI-Next/dist")
set(WEBUI_RESOURCE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/webui_resources.h")

if (NOT EXISTS "${WEBUI_SOURCE_DIR}/index.html")
    message(WARNING "WebUI-Next/dist/index.html not found. Generating a dummy index.html for build to succeed. Please build WebUI-Next to get the real UI.")
    file(MAKE_DIRECTORY "${WEBUI_SOURCE_DIR}")
    file(WRITE "${WEBUI_SOURCE_DIR}/index.html" "<html><body><h1>WebUI not built. Please run pnpm build in WebUI-Next.</h1></body></html>")
endif()

add_custom_command(
    OUTPUT ${WEBUI_RESOURCE_OUTPUT}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_DIR=${WEBUI_SOURCE_DIR}
        -DOUTPUT_FILE=${WEBUI_RESOURCE_OUTPUT}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_webui_resources.cmake"
    DEPENDS
        ${WEBUI_SOURCE_DIR}/index.html
    COMMENT "Embedding WebUI static resources into C++ byte arrays..."
)
add_custom_target(webui_resources DEPENDS ${WEBUI_RESOURCE_OUTPUT})

# ---- Rust MySQL static library (built via cargo) ----
if (WIN32)
    set(RUST_MYSQL_LIB_NAME "rust_mysql.lib")
    set(RUST_MYSQL_SYSTEM_LIBS ws2_32 ntdll userenv bcrypt)
else()
    set(RUST_MYSQL_LIB_NAME "librust_mysql.a")
    set(RUST_MYSQL_SYSTEM_LIBS dl pthread rt m)
endif()

set(RUST_MYSQL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rust_mysql")
set(RUST_MYSQL_LIB_PATH "${RUST_MYSQL_SRC_DIR}/target/release/${RUST_MYSQL_LIB_NAME}")

ExternalProject_Add(rust_mysql_build
    SOURCE_DIR "${RUST_MYSQL_SRC_DIR}"
    BUILD_IN_SOURCE TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cargo build --release
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "${RUST_MYSQL_LIB_PATH}"
    USES_TERMINAL_BUILD TRUE
)

add_library(rust_mysql STATIC IMPORTED GLOBAL)
add_dependencies(rust_mysql rust_mysql_build)
set_target_properties(rust_mysql PROPERTIES
    IMPORTED_LOCATION "${RUST_MYSQL_LIB_PATH}"
)

# ---- world-inspector Rust static library ----
set(WI_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/world_inspector/src/world_inspector_build")
if(WIN32 AND NOT MINGW)
    set(WI_LIB_PATH "${WI_SRC_DIR}/target/release/world_inspector.lib")
else()
    set(WI_LIB_PATH "${WI_SRC_DIR}/target/release/libworld_inspector.a")
endif()

ExternalProject_Add(world_inspector_build
    GIT_REPOSITORY https://github.com/yuhangle/world-inspector.git
    GIT_TAG main
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}/world_inspector"
    BUILD_IN_SOURCE TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cargo build --release --lib
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "${WI_LIB_PATH}"
    USES_TERMINAL_BUILD TRUE
)

add_library(world_inspector STATIC IMPORTED GLOBAL)
add_dependencies(world_inspector world_inspector_build)
set_target_properties(world_inspector PROPERTIES
    IMPORTED_LOCATION "${WI_LIB_PATH}"
)

# SQLite3
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    message(STATUS "SQLite3 not found by find_package, falling back to FetchContent...")
    FetchContent_Declare(
        sqlite3
        GIT_REPOSITORY https://github.com/azadkuh/sqlite-amalgamation.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(sqlite3)
    add_library(SQLite3::SQLite3 ALIAS sqlite3)
endif()

file(GLOB PLUGIN_SOURCES
        CONFIGURE_DEPENDS
        "src/*.cpp"
        "src/webui_adapter/*.cpp"
)

# ---- tianyan_webui 静态库 ----
file(GLOB WEBUI_SOURCES
        CONFIGURE_DEPENDS
        "src/webui/*.cpp"
        "src/webui/handlers/*.cpp"
)

add_library(tianyan_webui STATIC ${WEBUI_SOURCES})
target_include_directories(tianyan_webui PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${cpp_httplib_SOURCE_DIR}"
)
target_link_libraries(tianyan_webui PUBLIC
    nlohmann_json::nlohmann_json
)
add_dependencies(tianyan_webui webui_resources)
set_target_properties(tianyan_webui PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
# 抑制 cpp-httplib 的 operator"" _t 弃用警告（第三方库代码，不影响功能）
target_compile_options(tianyan_webui PRIVATE -Wno-deprecated-literal-operator)

endstone_add_plugin(${PROJECT_NAME} ${PLUGIN_SOURCES})

target_include_directories(${PROJECT_NAME}
        PRIVATE ${CMAKE_SOURCE_DIR}/include
        )

target_link_libraries(${PROJECT_NAME} PRIVATE
        nlohmann_json::nlohmann_json
        SQLite3::SQLite3
        fmt::fmt-header-only
        endstone_inventoryui
        rust_mysql
        world_inspector
        tianyan_webui
        ${RUST_MYSQL_SYSTEM_LIBS}
)

# 优化 + Rust 多定义兼容
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(MSVC)
        # MSVC: 允许重复符号（多个 Rust .a 都有 std 符号）
        target_link_options(${PROJECT_NAME} PRIVATE /FORCE:MULTIPLE)
    else()
        # GCC/Clang: 函数分段 + GC 死代码消除
        target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections)
        target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections -Wl,--allow-multiple-definition)
    endif()
endif()

# Linux Release 构建后自动 strip
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT MSVC)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND strip $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Stripping debug symbols from ${PROJECT_NAME}")
endif()

find_package(Git QUIET)
if(GIT_FOUND)
    # 1. 查找最近的【纯净】正式版本 Tag (排除带 - 的 Tag)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0 --match "v[0-9]*" --exclude "*-*" HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE LAST_STABLE_TAG
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(LAST_STABLE_TAG)
        # 有稳定版基准：计算距离该 Tag 的提交数
        execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-list --count "${LAST_STABLE_TAG}..HEAD"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_COMMIT_COUNT
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else()
        # 无稳定版基准：计算总提交数
        execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_COMMIT_COUNT
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    # 获取短哈希
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE GIT_SHORT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(GIT_COMMIT_COUNT "0")
    set(GIT_SHORT_HASH "unknown")
endif()

# 决定最终版本号
if(RELEASE_VERSION)
    set(PROJECT_VERSION "${BASE_VERSION}")
else()
    set(PROJECT_VERSION "${BASE_VERSION}-alpha.${GIT_COMMIT_COUNT}+${GIT_SHORT_HASH}")
endif()

# 生成 version.h
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h"
)

# 将生成目录加入包含路径
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")