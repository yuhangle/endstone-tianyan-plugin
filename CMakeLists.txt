cmake_minimum_required(VERSION 3.15)

project(tianyan CXX)

set(BASE_VERSION "1.2.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /utf-8")
endif()


include(FetchContent)
FetchContent_Declare(
        endstone
        GIT_REPOSITORY https://github.com/EndstoneMC/endstone.git
        GIT_TAG v0.11.0
)
FetchContent_MakeAvailable(endstone)

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

# MySQL client
if(WIN32)
    find_package(unofficial-libmysql CONFIG REQUIRED)
else()
    if(APPLE)
        set(_TIANYAN_PREFER_STATIC_MYSQL_DEFAULT OFF)
    else()
        set(_TIANYAN_PREFER_STATIC_MYSQL_DEFAULT ON)
    endif()
    option(TIANYAN_PREFER_STATIC_MYSQL "Prefer static MySQL client link on non-Windows platforms." ${_TIANYAN_PREFER_STATIC_MYSQL_DEFAULT})

    # Allow explicit override from command line/cache.
    set(MYSQL_LINK_MODE "manual")
    if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
        set(MYSQL_LINK_MODE "dynamic")
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(MYSQL QUIET mysqlclient)
            if(NOT MYSQL_FOUND)
                pkg_check_modules(MYSQL QUIET libmysqlclient)
            endif()
            if(NOT MYSQL_FOUND)
                pkg_check_modules(MYSQL QUIET mariadb)
            endif()
        endif()

        find_path(MYSQL_INCLUDE_DIR
                NAMES mysql/mysql.h mariadb/mysql.h mysql.h
                HINTS ${MYSQL_INCLUDE_DIRS}
        )

        if(TIANYAN_PREFER_STATIC_MYSQL AND MYSQL_FOUND)
            set(_MYSQL_SAVED_SUFFIXES "${CMAKE_FIND_LIBRARY_SUFFIXES}")
            set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
            find_library(MYSQL_LIBRARY_STATIC
                    NAMES mysqlclient mariadb mariadbclient
                    HINTS ${MYSQL_STATIC_LIBRARY_DIRS} ${MYSQL_LIBRARY_DIRS}
            )
            set(CMAKE_FIND_LIBRARY_SUFFIXES "${_MYSQL_SAVED_SUFFIXES}")
            if(MYSQL_LIBRARY_STATIC)
                set(MYSQL_LIBRARY "${MYSQL_LIBRARY_STATIC}")
                set(MYSQL_LINK_MODE "static")
            endif()
        endif()

        if(NOT MYSQL_LIBRARY)
            find_library(MYSQL_LIBRARY
                    NAMES mysqlclient mariadb mariadbclient
                    HINTS ${MYSQL_LIBRARY_DIRS}
            )
        endif()
    endif()

    if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
        message(FATAL_ERROR "MySQL client library not found. Please install libmysqlclient-dev/default-libmysqlclient-dev (Linux) or MariaDB Connector C.")
    endif()
endif()

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
        fmt::fmt-header-only
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::libmysql::libmysql)
else()
    target_include_directories(${PROJECT_NAME} PRIVATE ${MYSQL_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${MYSQL_LIBRARY})
    if(MYSQL_LINK_MODE STREQUAL "static")
        message(STATUS "Using static MySQL client library: ${MYSQL_LIBRARY}")
        if(MYSQL_STATIC_LIBRARY_DIRS)
            target_link_directories(${PROJECT_NAME} PRIVATE ${MYSQL_STATIC_LIBRARY_DIRS})
        endif()
        if(MYSQL_STATIC_LIBRARIES)
            foreach(MYSQL_DEP IN LISTS MYSQL_STATIC_LIBRARIES)
                if(NOT MYSQL_DEP STREQUAL "mysqlclient" AND NOT MYSQL_DEP STREQUAL "mariadb" AND NOT MYSQL_DEP STREQUAL "mariadbclient")
                    target_link_libraries(${PROJECT_NAME} PRIVATE ${MYSQL_DEP})
                endif()
            endforeach()
        endif()
        if(MYSQL_STATIC_LDFLAGS_OTHER)
            target_link_options(${PROJECT_NAME} PRIVATE ${MYSQL_STATIC_LDFLAGS_OTHER})
        endif()

        # When building with clang/libc++, static libmysqlclient may still require
        # libstdc++ symbols from its internal C++ objects (e.g. iostream buffers).
        # Link libstdc++ explicitly to provide those symbols at runtime.
        string(FIND "${CMAKE_CXX_FLAGS}" "-stdlib=libc++" _TIANYAN_USES_LIBCXX_FLAG)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND _TIANYAN_USES_LIBCXX_FLAG GREATER -1)
            set(TIANYAN_STDCXX_LIBRARY "")
            execute_process(
                    COMMAND g++ -print-file-name=libstdc++.so
                    OUTPUT_VARIABLE TIANYAN_STDCXX_CANDIDATE
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                    RESULT_VARIABLE TIANYAN_STDCXX_GPP_RESULT
            )
            if(TIANYAN_STDCXX_GPP_RESULT EQUAL 0
                    AND NOT TIANYAN_STDCXX_CANDIDATE STREQUAL "libstdc++.so"
                    AND EXISTS "${TIANYAN_STDCXX_CANDIDATE}")
                set(TIANYAN_STDCXX_LIBRARY "${TIANYAN_STDCXX_CANDIDATE}")
            endif()

            if(NOT TIANYAN_STDCXX_LIBRARY)
                find_library(TIANYAN_STDCXX_LIBRARY NAMES stdc++)
            endif()
            if(TIANYAN_STDCXX_LIBRARY)
                target_link_libraries(${PROJECT_NAME} PRIVATE ${TIANYAN_STDCXX_LIBRARY})
                message(STATUS "Linking extra runtime dependency for static mysqlclient: ${TIANYAN_STDCXX_LIBRARY}")
            else()
                message(WARNING "libstdc++ not found while using static mysqlclient with clang/libc++; plugin may fail to load at runtime.")
            endif()
        endif()
    elseif(MYSQL_LINK_MODE STREQUAL "dynamic")
        message(STATUS "Using dynamic MySQL client library: ${MYSQL_LIBRARY}")
        if(MYSQL_LIBRARY_DIRS)
            target_link_directories(${PROJECT_NAME} PRIVATE ${MYSQL_LIBRARY_DIRS})
        endif()
        if(MYSQL_LDFLAGS_OTHER)
            target_link_options(${PROJECT_NAME} PRIVATE ${MYSQL_LDFLAGS_OTHER})
        endif()
    else()
        message(STATUS "Using manually configured MySQL client library: ${MYSQL_LIBRARY}")
    endif()
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
