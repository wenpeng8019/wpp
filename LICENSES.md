# 第三方库许可证信息

## 许可证概览

### 1. SQLite - Public Domain（公有领域）
- **许可类型**: 公有领域（Public Domain）
- **权限**: 完全自由使用、修改、分发、商业使用
- **要求**: 无任何限制
- **适合**: ✅ 可以自由修改和整合到任何项目中
- **来源**: `/third_party/sqlite/`

SQLite 的作者已经将其放入公有领域，可以在任何项目中自由使用、修改和重新分发，无需遵守任何许可条款。

### 2. TinyCC - LGPL 2.1
- **许可类型**: GNU Lesser General Public License v2.1
- **权限**: 可以使用、修改、分发
- **要求**: 
  - 如果修改 TinyCC 代码并分发，必须：
    - 公开修改后的源代码
    - 保持 LGPL 2.1 许可证
    - 标注修改内容
  - 动态链接 TinyCC 库的程序可以使用任何许可证
  - 静态链接或直接整合代码需要遵守 LGPL
- **适合**: ⚠️ 需要注意整合方式
- **来源**: `/third_party/tinycc/`

### 3. SQTP Protocol & Libraries - MIT License
- **许可类型**: MIT License
- **权限**: 可以在商业和开源项目中自由使用、修改、分发
- **要求**: 
  - 在软件副本中包含版权声明和许可证声明
  - 无需公开源代码
  - 无需使用相同许可证
- **适合**: ✅ 最宽松的许可证之一，可用于任何项目
- **包含内容**:
  - SQTP/1.0 协议规范（`docs/sqtp/`）
  - JavaScript 客户端库（`lib/sqtp/*.js`）
- **许可证文件**: `LICENSES/SQTP-LICENSE`

SQTP 协议和客户端库采用 MIT License，允许在任何项目中自由使用，包括商业闭源项目。这与 WPP 主项目的 GPL 3.0 许可证独立。

### 4. yyjson - MIT License
- **许可类型**: MIT License
- **权限**: 可以在商业和开源项目中自由使用、修改、分发
- **要求**: 
  - 在软件副本中包含版权声明和许可证声明
  - 无需公开源代码
  - 无需使用相同许可证
- **适合**: ✅ 最宽松的许可证之一，可用于任何项目
- **来源**: `/third_party/yyjson/`
- **许可证文件**: `third_party/yyjson/LICENSE`

yyjson 是一个高性能的 JSON 解析库，采用 MIT License，允许在任何项目中自由使用。

### 5. zlib - zlib License
- **许可类型**: zlib License（类似 MIT/BSD，非常宽松）
- **权限**: 可以在商业和开源项目中自由使用、修改、分发
- **要求**: 
  - 如果分发修改后的源代码，必须标注修改内容
  - 不得声称修改后的版本是原始版本
  - 版权声明不能被移除或修改
- **适合**: ✅ 非常宽松，可用于任何项目
- **来源**: `/third_party/zlib/`
- **版本**: 1.3.1
- **许可证文件**: `LICENSES/ZLIB-LICENSE`

zlib 是业界标准的压缩库，使用非常宽松的 zlib License，可以自由用于商业和开源项目。

### 6. uthash - BSD License
- **许可类型**: BSD License（修改版 BSD，非常宽松）
- **权限**: 可以在商业和开源项目中自由使用、修改、分发
- **要求**: 
  - 在源代码分发时保留版权声明
  - 在二进制分发时在文档中保留版权声明
  - 不得使用作者名字为衍生产品背书
- **适合**: ✅ 非常宽松，可用于任何项目
- **来源**: `/third_party/uthash/`
- **项目地址**: https://github.com/troydhanson/uthash
- **许可证文件**: `LICENSES/UTHASH-LICENSE`

uthash 是单头文件哈希表库，提供哈希表、动态数组、链表等数据结构实现。采用宽松的 BSD License，可自由用于商业和开源项目。

### 7. althttpd - 需要确认
- **状态**: 源码获取中
- **预期**: Public Domain（SQLite 项目系列通常是公有领域）
- **来源**: `/third_party/althttpd/`

## 项目许可建议

### 方案 A：完全开源（推荐用于修改整合）
如果要修改并整合这些库的代码：

1. **整体项目许可**: LGPL 2.1 或 GPL 2.0+
   - 原因：TinyCC 使用 LGPL 2.1，如果直接整合其代码需要兼容的许可证

2. **代码组织**:
   ```
   wpp/
   ├── LICENSE                    # 项目整体许可证（LGPL 2.1 或 GPL）
   ├── LICENSES.md               # 详细的许可证说明
   ├── src/
   │   ├── core/                 # WPP 核心代码
   │   ├── sqlite-mod/           # 修改后的 SQLite 代码
   │   ├── tinycc-mod/           # 修改后的 TinyCC 代码
   │   └── althttpd-mod/         # 修改后的 althttpd 代码
   ├── include/
   │   ├── wpp/                  # WPP 头文件
   │   └── third-party/          # 修改的第三方头文件
   └── third_party/              # 原始源码（仅供参考）
   ```

3. **文件头部标注**:
   - 原始代码：保留原始许可证声明
   - 修改的代码：添加修改说明和日期
   - 新代码：使用项目许可证

### 方案 B：模块化（推荐用于灵活性）

1. **整体项目许可**: MIT 或 BSD（宽松）

2. **代码组织**:
   ```
   wpp/
   ├── LICENSE                    # 项目主许可证（MIT/BSD）
   ├── LICENSES/                  # 各模块许可证
   │   ├── WPP-LICENSE           # WPP 核心（MIT/BSD）
   │   ├── SQLITE-LICENSE        # SQLite（Public Domain）
   │   ├── TINYCC-LICENSE        # TinyCC（LGPL 2.1）
   │   └── ALTHTTPD-LICENSE      # althttpd（待确认）
   ├── modules/
   │   ├── wpp-core/             # WPP 核心（MIT/BSD）
   │   ├── wpp-sqlite/           # SQLite 包装/修改（Public Domain）
   │   ├── wpp-compiler/         # TinyCC 包装（LGPL 2.1）
   │   └── wpp-server/           # HTTP 服务器包装（Public Domain）
   ├── include/
   │   └── wpp/                  # 统一的 API 接口
   └── third_party/              # 原始库（只读参考）
   ```

3. **链接方式**:
   - TinyCC: 编译为动态库（.so/.dylib），通过 API 调用
   - SQLite: 直接整合到核心（Public Domain）
   - althttpd: 直接整合或修改（预期 Public Domain）

## 合规检查清单

- [ ] 保留所有原始许可证文件
- [ ] 在修改的文件中标注修改内容和日期
- [ ] 创建 NOTICE 文件列出所有使用的开源组件
- [ ] 在文档中说明各组件的许可证
- [ ] 如果使用 LGPL 代码，提供源码访问方式
- [ ] 在分发时包含完整的许可证文本

## 推荐做法

### 代码文件头部模板

**修改现有代码时**:
```c
/*
 * Original work: Copyright (C) XXXX Original Author
 * Modified work: Copyright (C) 2026 WPP Project
 * 
 * This file contains modifications to [原始项目名]
 * Original license: [原始许可证]
 * Modified under: [项目许可证]
 * 
 * Modifications:
 *   - 2026-02-07: [描述修改内容]
 */
```

**新创建的整合代码**:
```c
/*
 * Copyright (C) 2026 WPP Project
 * 
 * This file integrates code from:
 *   - SQLite (Public Domain)
 *   - TinyCC (LGPL 2.1)
 *   - althttpd (Public Domain)
 * 
 * Licensed under: [项目许可证]
 */
```

## 建议

**最佳方案**: 采用 **方案 B（模块化）** + **LGPL 2.1 或 GPL 3.0 整体许可证**

优点：
- ✅ 完全合规
- ✅ 可以自由修改所有组件
- ✅ 清晰的模块边界
- ✅ 便于后续维护和更新
- ✅ 开源社区友好

注意事项：
- 如果将来需要闭源商业化，需要重新评估 TinyCC 的使用方式（改为动态链接）
- Public Domain 的代码（SQLite、althttpd）可以随意使用，无任何限制
