cmake_minimum_required(VERSION 3.10)
project(WPP VERSION 0.1.0 LANGUAGES C)

# 统一构建目录为 build/（兼容 CLion 和命令行）
if(NOT CMAKE_BINARY_DIR MATCHES "build$")
    message(STATUS "当前构建目录: ${CMAKE_BINARY_DIR}")
endif()

# C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# 选项
option(WPP_BUILD_TESTS "Build tests" ON)
option(WPP_BUILD_EXAMPLES "Build examples" ON)
option(WPP_BUILD_DOCS "Build documentation" OFF)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)

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
    # SQLITE_OMIT_SHARED_CACHE    # 需要共享缓存支持共享内存数据库
)

# SQLite 调试选项（用于学习和断点调试）
target_compile_options(sqlite3 PRIVATE
    -g                            # 生成调试符号
    -O0                           # 禁用优化，便于单步调试
    # -O3                         # 最高优化级别（已禁用）
    # -DNDEBUG                    # 禁用断言（已禁用，保留断言用于学习）
)

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
message(STATUS "  JSON support: Enabled")
message(STATUS "  FTS5 support: Enabled")
message(STATUS "  R-Tree support: Enabled")

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

# TinyCC 调试选项（用于学习和断点调试）
target_compile_options(tcc PRIVATE
    -g                        # 生成调试符号
    -O0                       # 禁用优化，便于单步调试
    -Wno-unused-parameter     # 忽略未使用参数警告
    -Wno-sign-compare         # 忽略符号比较警告
)

# 包含 TinyCC 头文件目录
target_include_directories(tcc PUBLIC ${TCC_DIR})

# 平台特定的系统库链接
if(APPLE)
    target_link_libraries(tcc PRIVATE dl)
elseif(UNIX)
    target_link_libraries(tcc PRIVATE pthread dl m)
endif()

message(STATUS "TinyCC Configuration:")
message(STATUS "  Source files: ${TCC_DIR}/*.c")
message(STATUS "  Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Mode: Static library (source build)")

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

# yyjson 编译选项
target_compile_definitions(yyjson PRIVATE
    # 可以添加 yyjson 特定的编译选项
)

# yyjson 调试选项（用于学习和断点调试）
target_compile_options(yyjson PRIVATE
    -g                        # 生成调试符号
    -O0                       # 禁用优化，便于单步调试
)

# 包含 yyjson 头文件目录
target_include_directories(yyjson PUBLIC ${YYJSON_DIR}/src)

message(STATUS "yyjson Configuration:")
message(STATUS "  Source: ${YYJSON_SOURCE}")
message(STATUS "  Mode: Static library (source build)")

# =============================================================================
# OpenSSL（用于 TLS 支持）
# =============================================================================

# 根据平台设置 OpenSSL 搜索路径
if(APPLE)
    # macOS: Homebrew OpenSSL 路径
    # 支持 Apple Silicon (arm64) 和 Intel (x86_64)
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
    # Ubuntu/Debian: /usr/include/openssl, /usr/lib/x86_64-linux-gnu/
    # CentOS/RHEL: /usr/include/openssl, /usr/lib64/
    # find_package 会自动搜索这些标准路径
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
    if(APPLE)
        message(WARNING "To install OpenSSL on macOS:")
        message(WARNING "  brew install openssl@3")
    elseif(UNIX)
        message(WARNING "To install OpenSSL on Linux:")
        message(WARNING "  Ubuntu/Debian: sudo apt-get install libssl-dev")
        message(WARNING "  CentOS/RHEL:   sudo yum install openssl-devel")
        message(WARNING "  Arch Linux:    sudo pacman -S openssl")
    endif()
endif()

# =============================================================================
# 主程序
# =============================================================================

add_executable(wpp 
    src/main.c
    src/common.c
    src/httpd.c
    src/http_sqtp.c
    src/http_cgi_c.c
)

# 如果找到 OpenSSL，启用 TLS 支持
if(OPENSSL_FOUND)
    target_compile_definitions(wpp PRIVATE ENABLE_TLS)
    target_include_directories(wpp PRIVATE ${OPENSSL_INCLUDE_DIR})
endif()

# 链接 SQLite、TinyCC、yyjson 和 OpenSSL 库
target_link_libraries(wpp 
    sqlite3
    tcc
    yyjson
)

# 链接 OpenSSL
if(OPENSSL_FOUND)
    target_link_libraries(wpp ${OPENSSL_LIBRARIES})
endif()

# 注意：althttpd.c 包含 main 函数，暂不直接编译
# 后续可以将其编译为独立的可执行文件或修改为库

# 安装规则
install(TARGETS wpp DESTINATION bin)
install(DIRECTORY include/wpp DESTINATION include)

# 文档
if(WPP_BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating API documentation"
        )
    endif()
endif()

