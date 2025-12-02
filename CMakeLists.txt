cmake_minimum_required(VERSION 3.15)

project(tianyan CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /utf-8")
endif()


include(FetchContent)
FetchContent_Declare(
        endstone
        GIT_REPOSITORY https://github.com/EndstoneMC/endstone.git
        GIT_TAG v0.10.10
)
FetchContent_MakeAvailable(endstone)

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

# SQLite3
find_package(SQLite3 REQUIRED)

# fmt (header-only or static)
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.2.0
)
FetchContent_MakeAvailable(fmt)

file(GLOB PLUGIN_SOURCES
        CONFIGURE_DEPENDS
        "src/*.cpp"
)

endstone_add_plugin(${PROJECT_NAME} ${PLUGIN_SOURCES})

target_include_directories(${PROJECT_NAME}
        PRIVATE ${CMAKE_SOURCE_DIR}/include
        )

target_link_libraries(${PROJECT_NAME} PRIVATE
        nlohmann_json::nlohmann_json
        SQLite::SQLite3
        fmt::fmt-header-only
)

find_package(Git QUIET)
if(GIT_FOUND)
    # 获取最近的 tag
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE LAST_TAG
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(LAST_TAG)
        # 计算自该 tag 以来的提交数
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --count "${LAST_TAG}..HEAD"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE GIT_COMMIT_COUNT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else()
        # 没有 tag，回退到总提交数
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

# 基础版本（主版本号）
set(BASE_VERSION "1.2.0")

# 检查是否为正式发布版本
if(RELEASE_VERSION)
    # 正式发布版本
    set(PROJECT_VERSION "${BASE_VERSION}")
else()
    # 开发版本
    set(PROJECT_VERSION "${BASE_VERSION}-alpha.${GIT_COMMIT_COUNT}+${GIT_SHORT_HASH}")
endif()

# 生成 version.h
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h"
)

# 将生成目录加入包含路径
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")