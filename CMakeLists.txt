cmake_minimum_required(VERSION 3.10)
project(WPP VERSION 0.2.0 LANGUAGES C)

# 设置默认构建类型为 Debug（可断点调试）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 统一构建目录为 build/（兼容 CLion 和命令行）
if(NOT CMAKE_BINARY_DIR MATCHES "build$")
    message(STATUS "当前构建目录: ${CMAKE_BINARY_DIR}")
endif()

# C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项 - 参照 Makefile 的配置
set(CMAKE_C_FLAGS_DEBUG "-Wall -Wextra -g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-Wall -Wextra -O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-Wall -Wextra -O2 -g -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL "-Wall -Wextra -O2 -DNDEBUG")

# 包含目录（参照 Makefile 的 CFLAGS）
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/third_party/tinycc/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party/tinycc)
include_directories(${CMAKE_SOURCE_DIR}/third_party/uthash/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party/sqlite)
include_directories(${CMAKE_SOURCE_DIR}/third_party/yyjson/src)
include_directories(${CMAKE_SOURCE_DIR}/third_party/zlib)

# 选项
option(WPP_BUILD_TESTS "Build tests" OFF)
option(WPP_BUILD_EXAMPLES "Build examples" OFF)
option(WPP_BUILD_DOCS "Build documentation" OFF)

# =============================================================================
# SQLite 静态库
# =============================================================================

# SQLite 源文件路径
set(SQLITE_DIR ${CMAKE_SOURCE_DIR}/third_party/sqlite)
set(SQLITE_AMALGAMATION ${SQLITE_DIR}/sqlite3.c)

# 检查 sqlite3.c 是否存在，如果不存在则提示
if(NOT EXISTS ${SQLITE_AMALGAMATION})
    message(FATAL_ERROR 
        "SQLite amalgamation file not found: ${SQLITE_AMALGAMATION}\n"
        "Please run the following commands to generate it:\n"
        "  cd ${SQLITE_DIR}\n"
        "  ./configure\n"
        "  make sqlite3.c"
    )
endif()

# 创建 SQLite 静态库
add_library(sqlite3 STATIC ${SQLITE_AMALGAMATION})

# SQLite 编译选项
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_ENABLE_JSON1           # 启用 JSON 函数
    SQLITE_ENABLE_RTREE           # 启用 R-Tree 索引
    SQLITE_ENABLE_FTS5            # 启用全文搜索
    SQLITE_ENABLE_MATH_FUNCTIONS  # 启用数学函数
    SQLITE_THREADSAFE=1           # 线程安全
    SQLITE_DQS=0                  # 禁用双引号字符串
    SQLITE_DEFAULT_MEMSTATUS=0    # 禁用内存统计（提升性能）
    SQLITE_OMIT_DEPRECATED        # 省略已弃用的功能
    SQLITE_OMIT_PROGRESS_CALLBACK # 省略进度回调
)

# SQLite 根据构建类型调整编译优化
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(sqlite3 PRIVATE -g -O0)
else()
    target_compile_options(sqlite3 PRIVATE -O2)
endif()

# 包含 SQLite 头文件目录
target_include_directories(sqlite3 PUBLIC ${SQLITE_DIR})

# 平台特定的系统库链接
if(APPLE)
    # macOS 需要链接的系统库
    target_link_libraries(sqlite3 PRIVATE "-framework CoreFoundation")
elseif(UNIX)
    # Linux 需要链接的库
    target_link_libraries(sqlite3 PRIVATE pthread dl m)
endif()

message(STATUS "SQLite Configuration:")
message(STATUS "  Source: ${SQLITE_AMALGAMATION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")

# =============================================================================
# TinyCC 静态库
# =============================================================================

# TinyCC 源文件路径
set(TCC_DIR ${CMAKE_SOURCE_DIR}/third_party/tinycc)

# TinyCC 核心源文件（基于 macOS arm64）
set(TCC_SOURCES
    ${TCC_DIR}/libtcc.c
    ${TCC_DIR}/tccpp.c
    ${TCC_DIR}/tccgen.c
    ${TCC_DIR}/tccdbg.c
    ${TCC_DIR}/tccelf.c
    ${TCC_DIR}/tccasm.c
    ${TCC_DIR}/tccrun.c
    ${TCC_DIR}/tccmacho.c
)

# 根据平台添加架构特定源文件
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    list(APPEND TCC_SOURCES
        ${TCC_DIR}/arm64-gen.c
        ${TCC_DIR}/arm64-link.c
        ${TCC_DIR}/arm64-asm.c
    )
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    list(APPEND TCC_SOURCES
        ${TCC_DIR}/x86_64-gen.c
        ${TCC_DIR}/x86_64-link.c
        ${TCC_DIR}/i386-asm.c
    )
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686")
    list(APPEND TCC_SOURCES
        ${TCC_DIR}/i386-gen.c
        ${TCC_DIR}/i386-link.c
        ${TCC_DIR}/i386-asm.c
    )
endif()

# 创建 TinyCC 静态库
add_library(tcc STATIC ${TCC_SOURCES})

# TinyCC 编译选项
target_compile_definitions(tcc PRIVATE
    ONE_SOURCE=0              # 多文件编译模式
    TCC_TARGET_MACHO          # macOS Mach-O 格式
)

# TinyCC 根据构建类型调整编译优化  
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(tcc PRIVATE 
        -g -O0
        -Wno-unused-parameter -Wno-sign-compare
    )
else()
    target_compile_options(tcc PRIVATE 
        -O2
        -Wno-unused-parameter -Wno-sign-compare
    )
endif()

# 包含 TinyCC 头文件目录
target_include_directories(tcc PUBLIC ${TCC_DIR})

# 平台特定的系统库链接
if(APPLE)
    target_link_libraries(tcc PRIVATE dl)
elseif(UNIX)
    target_link_libraries(tcc PRIVATE pthread dl m)
endif()

message(STATUS "TinyCC Configuration:")
message(STATUS "  Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")

# =============================================================================
# yyjson 静态库
# =============================================================================

# yyjson 源文件路径
set(YYJSON_DIR ${CMAKE_SOURCE_DIR}/third_party/yyjson)
set(YYJSON_SOURCE ${YYJSON_DIR}/src/yyjson.c)

# 检查 yyjson.c 是否存在
if(NOT EXISTS ${YYJSON_SOURCE})
    message(FATAL_ERROR 
        "yyjson source file not found: ${YYJSON_SOURCE}\n"
        "Please check if yyjson is properly cloned in third_party/"
    )
endif()

# 创建 yyjson 静态库
add_library(yyjson STATIC ${YYJSON_SOURCE})

# yyjson 根据构建类型调整编译优化
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(yyjson PRIVATE -g -O0)
else()
    target_compile_options(yyjson PRIVATE -O2)
endif()

# 包含 yyjson 头文件目录
target_include_directories(yyjson PUBLIC ${YYJSON_DIR}/src)

message(STATUS "yyjson Configuration:")
message(STATUS "  Source: ${YYJSON_SOURCE}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")

# =============================================================================
# zlib 静态库
# =============================================================================

# zlib 源文件路径
set(ZLIB_DIR ${CMAKE_SOURCE_DIR}/third_party/zlib)

# zlib 核心源文件
set(ZLIB_SOURCES
    ${ZLIB_DIR}/adler32.c
    ${ZLIB_DIR}/compress.c
    ${ZLIB_DIR}/crc32.c
    ${ZLIB_DIR}/deflate.c
    ${ZLIB_DIR}/infback.c
    ${ZLIB_DIR}/inffast.c
    ${ZLIB_DIR}/inflate.c
    ${ZLIB_DIR}/inftrees.c
    ${ZLIB_DIR}/trees.c
    ${ZLIB_DIR}/uncompr.c
    ${ZLIB_DIR}/zutil.c
)

# 创建 zlib 静态库
add_library(zlib STATIC ${ZLIB_SOURCES})

# zlib 根据构建类型调整编译优化
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(zlib PRIVATE 
        -g -O0
        -Wno-implicit-fallthrough
    )
else()
    target_compile_options(zlib PRIVATE 
        -O2
        -Wno-implicit-fallthrough
    )
endif()

# 包含 zlib 头文件目录
target_include_directories(zlib PUBLIC ${ZLIB_DIR})

message(STATUS "zlib Configuration:")
message(STATUS "  Source: ${ZLIB_DIR}/*.c")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")

# =============================================================================
# OpenSSL（用于 TLS 支持）
# =============================================================================

# 根据平台设置 OpenSSL 搜索路径
if(APPLE)
    # macOS: Homebrew OpenSSL 路径 
    if(EXISTS /opt/homebrew/opt/openssl@3)
        set(OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl@3)
    elseif(EXISTS /usr/local/opt/openssl@3)
        set(OPENSSL_ROOT_DIR /usr/local/opt/openssl@3)
    elseif(EXISTS /opt/homebrew/opt/openssl@1.1)
        set(OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl@1.1)
    elseif(EXISTS /usr/local/opt/openssl@1.1)
        set(OPENSSL_ROOT_DIR /usr/local/opt/openssl@1.1)
    endif()
elseif(UNIX)
    # Linux: 通常在系统标准路径中
endif()

# 查找 OpenSSL
find_package(OpenSSL)

if(OPENSSL_FOUND)
    message(STATUS "OpenSSL Configuration:")
    message(STATUS "  Version: ${OPENSSL_VERSION}")
    message(STATUS "  Include: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "  Libraries: ${OPENSSL_LIBRARIES}")
else()
    message(WARNING "OpenSSL not found. TLS support will be disabled.")
endif()

# =============================================================================
# BUILDINS 资源自动生成
# =============================================================================

# 定义生成的文件
set(BUILDINS_SOURCES ${CMAKE_SOURCE_DIR}/src/buildins/sysroot.c)
set(BUILDINS_HEADERS ${CMAKE_SOURCE_DIR}/src/buildins/sysroot.h)
set(BUILDINS_SCRIPT ${CMAKE_SOURCE_DIR}/tools/make_buildins.sh)
set(BUILDINS_DIR ${CMAKE_SOURCE_DIR}/buildins)

# 查找 buildins 目录下的所有文件
file(GLOB_RECURSE BUILDINS_INPUT_FILES 
    ${BUILDINS_DIR}/*
)

# 添加自定义命令：生成 BUILDINS 资源
add_custom_command(
    OUTPUT ${BUILDINS_SOURCES} ${BUILDINS_HEADERS}
    COMMAND bash ${BUILDINS_SCRIPT}
    DEPENDS ${BUILDINS_INPUT_FILES} ${BUILDINS_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating embedded BUILDINS resources..."
    VERBATIM
)

# 创建自定义目标（确保在编译主程序前生成）
add_custom_target(generate_buildins
    DEPENDS ${BUILDINS_SOURCES} ${BUILDINS_HEADERS}
)

message(STATUS "BUILDINS Resource Configuration:")
message(STATUS "  Script: ${BUILDINS_SCRIPT}")
message(STATUS "  Output: ${BUILDINS_SOURCES}, ${BUILDINS_HEADERS}")

# =============================================================================
# 主程序
# =============================================================================

# 主程序源文件（参照 Makefile）
add_executable(wpp 
    src/main.c
    src/common.c
    src/httpd.c
    src/http_sqtp.c
    src/http_cgi_c.c
    src/buildins.c
    src/vfile.c
    src/tcc_evn.c
    ${BUILDINS_SOURCES}
)

# 确保在编译前生成 BUILDINS 资源
add_dependencies(wpp generate_buildins)

# 如果找到 OpenSSL，启用 TLS 支持
if(OPENSSL_FOUND)
    target_compile_definitions(wpp PRIVATE ENABLE_TLS)
    target_include_directories(wpp PRIVATE ${OPENSSL_INCLUDE_DIR})
endif()

# 链接库（参照 Makefile 的 LDFLAGS 顺序：先 TCC，再其他库，最后系统库）
target_link_libraries(wpp 
    tcc
    sqlite3
    yyjson
    zlib
    m
    dl
    pthread
)

# 链接 OpenSSL
if(OPENSSL_FOUND)
    target_link_libraries(wpp ${OPENSSL_LIBRARIES})
endif()

# 设置输出目录到 build/ （参照 Makefile）
set_target_properties(wpp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build
)

# =============================================================================
# 自定义构建目标（参照 Makefile）
# =============================================================================

# Debug 目标（默认）
add_custom_target(debug
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config Debug
    COMMENT "Building debug version (可断点调试)"
)

# Release 目标
add_custom_target(release
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config Release
    COMMENT "Building release version (O2 optimized)"
)

# Stripped 目标 (Release + strip)
add_custom_target(stripped
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config Release
    COMMAND strip ${CMAKE_SOURCE_DIR}/build/wpp
    COMMENT "Building stripped version (O2 + stripped)"
)

# Build Help 目标
add_custom_target(build_help
    COMMAND echo "Available build targets:"
    COMMAND echo "  debug     - Debug version with symbols (default)"
    COMMAND echo "  release   - Release version with O2 optimization"
    COMMAND echo "  stripped  - Release version with symbols stripped"
    COMMAND echo ""
    COMMAND echo "Build examples:"
    COMMAND echo "  cmake --build . --target debug"  
    COMMAND echo "  cmake --build . --target release"
    COMMAND echo "  cmake --build . --target stripped"
    VERBATIM
)

message(STATUS "WPP Project Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output: ${CMAKE_SOURCE_DIR}/build/wpp")
message(STATUS "  Available Targets: debug (default), release, stripped")

# 安装规则
install(TARGETS wpp DESTINATION bin)

