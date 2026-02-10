# WPP 项目架构设计

## 概述

WPP 是一个集成了 HTTP 服务器、数据库引擎和 C 编译器的现代化系统。通过模块化设计，各组件既可以独立使用，也可以协同工作。

## 设计原则

1. **模块化**: 每个功能都是独立的模块，清晰的接口边界
2. **可扩展**: 易于添加新功能和模块
3. **开源兼容**: 遵守所有第三方库的许可证要求
4. **高性能**: 最小化开销，充分利用底层库的性能优势
5. **易用性**: 提供统一、简洁的 API 接口

## 项目结构

```
wpp/
├── LICENSE                      # GPL 3.0 主许可证
├── NOTICE                       # 第三方组件归属说明
├── LICENSES.md                  # 许可证指南
├── LICENSES/                    # 各组件许可证
│   ├── TINYCC-LICENSE          # LGPL 2.1
│   ├── SQLITE-LICENSE          # Public Domain
│   └── ALTHTTPD-LICENSE        # Public Domain
│
├── include/                     # 公共头文件
│   └── wpp/
│       ├── wpp.h               # 主头文件
│       ├── server.h            # HTTP 服务器 API
│       ├── database.h          # 数据库 API
│       ├── compiler.h          # 编译器 API
│       └── core.h              # 核心功能 API
│
├── modules/                     # 功能模块
│   ├── wpp-core/               # 核心模块
│   │   ├── src/
│   │   ├── include/
│   │   └── CMakeLists.txt
│   │
│   ├── wpp-sqlite/             # 数据库模块（基于 SQLite）
│   │   ├── src/
│   │   │   ├── sqlite3.c      # SQLite 合并源文件
│   │   │   └── wpp_db.c       # WPP 数据库封装
│   │   ├── include/
│   │   └── CMakeLists.txt
│   │
│   ├── wpp-compiler/           # 编译器模块（基于 TinyCC）
│   │   ├── src/
│   │   │   └── wpp_compiler.c # WPP 编译器封装
│   │   ├── include/
│   │   └── CMakeLists.txt
│   │
│   └── wpp-server/             # HTTP 服务器模块（基于 althttpd）
│       ├── src/
│       │   ├── althttpd.c     # althttpd 源码
│       │   └── wpp_server.c   # WPP 服务器封装
│       ├── include/
│       └── CMakeLists.txt
│
├── src/                         # 主程序源码
│   └── main.c
│
├── third_party/                 # 原始第三方库（只读参考）
│   ├── sqlite/
│   ├── tinycc/
│   └── althttpd/
│
├── build/                       # 编译输出
├── docs/                        # 文档
│   ├── design/                 # 设计文档
│   └── api/                    # API 文档
│
├── examples/                    # 示例代码
├── tests/                       # 测试代码
├── CMakeLists.txt              # 主构建文件
└── Makefile                    # 兼容性构建
```

## 模块详解

### 1. wpp-core（核心模块）
- **职责**: 初始化、日志、配置、工具函数
- **许可证**: GPL 3.0
- **依赖**: 无
- **提供**:
  - 统一的初始化/清理接口
  - 日志系统
  - 配置管理
  - 内存管理辅助

### 2. wpp-sqlite（数据库模块）
- **职责**: 数据持久化和查询
- **许可证**: Public Domain（继承 SQLite）
- **依赖**: wpp-core
- **提供**:
  - 简化的数据库 API
  - 连接池管理
  - 事务支持
  - 高级查询构建器

### 3. wpp-compiler（编译器模块）
- **职责**: 动态编译 C 代码
- **许可证**: LGPL 2.1（继承 TinyCC）
- **依赖**: wpp-core, libtcc
- **提供**:
  - JIT 编译接口
  - 代码动态加载
  - 符号查找和调用

### 4. wpp-server（HTTP 服务器模块）
- **职责**: HTTP 请求处理
- **许可证**: Public Domain（继承 althttpd）
- **依赖**: wpp-core
- **提供**:
  - 现代化的 HTTP API
  - 路由管理
  - 中间件支持
  - WebSocket 支持（扩展）

## 数据流

```
[HTTP Request] 
    ↓
[wpp-server] → 路由解析
    ↓
[Handler] → 可能使用：
    ├── [wpp-sqlite] → 查询数据
    ├── [wpp-compiler] → 动态编译执行代码
    └── [wpp-core] → 日志/配置
    ↓
[HTTP Response]
```

## 集成方式

### SQLite 集成
由于 SQLite 是 Public Domain，可以：
- 直接修改源代码
- 添加 WPP 特定的扩展
- 优化特定用例的性能

### TinyCC 集成
由于 TinyCC 是 LGPL 2.1，建议：
- 编译为动态库（libtcc.so）
- 通过 API 接口调用
- 不直接修改 TinyCC 源码，或将修改贡献回上游

### althttpd 集成
由于 althttpd 是 Public Domain，可以：
- 大幅重构和现代化
- 添加新特性（如 HTTP/2、WebSocket）
- 集成到 WPP 的事件循环

## 构建系统

使用 CMake 作为主要构建系统：

```cmake
# 主 CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(WPP VERSION 1.0.0)

# 选项
option(WPP_BUILD_TESTS "Build tests" ON)
option(WPP_BUILD_EXAMPLES "Build examples" ON)

# 子模块
add_subdirectory(modules/wpp-core)
add_subdirectory(modules/wpp-sqlite)
add_subdirectory(modules/wpp-compiler)
add_subdirectory(modules/wpp-server)

# 主程序
add_executable(wpp src/main.c)
target_link_libraries(wpp 
    wpp-core 
    wpp-sqlite 
    wpp-compiler 
    wpp-server
)
```

## API 设计示例

```c
// wpp.h - 主头文件
#ifndef WPP_H
#define WPP_H

#include <wpp/core.h>
#include <wpp/server.h>
#include <wpp/database.h>
#include <wpp/compiler.h>

// 初始化整个系统
int wpp_init(wpp_config_t *config);

// 清理资源
void wpp_cleanup(void);

#endif // WPP_H
```

## 开发工作流

1. **修改第三方代码**:
   - 保留原始代码在 `third_party/`
   - 复制需要修改的文件到对应 module
   - 在文件头部添加修改说明

2. **添加新功能**:
   - 在对应模块的 `src/` 下添加新文件
   - 更新模块的 CMakeLists.txt
   - 在 `include/wpp/` 添加公共 API

3. **测试**:
   - 每个模块都有自己的测试
   - 集成测试在顶层 `tests/` 目录

4. **文档**:
   - API 文档使用 Doxygen 从源码生成
   - 设计文档在 `docs/design/`

## 许可证合规

### 文件头部标注

**原始 SQLite 文件**:
```c
/*
** Original: SQLite (Public Domain)
** Modified: 2026-02-07 by WPP Project
** 
** Modifications:
**   - Added WPP-specific extensions
**   - Optimized for embedded use
*/
```

**原始 TinyCC 文件（如果必须修改）**:
```c
/*
 * Original: TinyCC - Copyright (C) 2001-2004 Fabrice Bellard
 * Modified: 2026-02-07 by WPP Project
 * License: LGPL 2.1
 * 
 * Modifications must comply with LGPL 2.1
 */
```

**新创建的文件**:
```c
/*
 * WPP Project - [Module Name]
 * Copyright (C) 2026 WPP Project Contributors
 * 
 * This file is part of WPP.
 * Licensed under GPL 3.0. See LICENSE file.
 */
```

## 下一步

1. [ ] 确认 althttpd 的确切许可证
2. [ ] 创建各模块的 CMakeLists.txt
3. [ ] 实现核心模块的基础功能
4. [ ] 集成 SQLite 并创建封装 API
5. [ ] 集成 TinyCC 并创建编译 API
6. [ ] 重构 althttpd 为现代 HTTP 服务器
7. [ ] 编写示例和测试
8. [ ] 完善文档
