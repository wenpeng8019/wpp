# WPP 项目快速指南

## 项目许可证策略

### 为什么选择 GPL 3.0？

WPP 项目选择 GPL 3.0 作为整体许可证，原因如下：

1. **兼容性**: GPL 3.0 与所有集成的第三方库兼容
   - Public Domain (SQLite, althttpd): 无限制
   - LGPL 2.1 (TinyCC): GPL 兼容 LGPL

2. **开放性**: 确保所有修改和改进都回馈社区

3. **保护性**: 防止闭源衍生品，保持项目的开源本质

### 如果需要闭源商业化怎么办？

如果将来需要闭源商业化，有以下选项：

1. **双许可证**: 为商业用户提供不同的许可证
2. **重新设计 TinyCC 集成**: 
   - 改为动态链接 libtcc（LGPL 允许）
   - 或替换为其他编译器（如 libclang）
3. **移除 TinyCC**: 如果不需要动态编译功能

## 修改第三方代码的规范

### SQLite（Public Domain）

**完全自由**，但建议：

```c
/*
 * Original: SQLite - Public Domain
 * https://sqlite.org/
 * 
 * Modified by WPP Project (2026-02-07)
 * Changes:
 *   - Added custom extension for X
 *   - Optimized Y for embedded use
 * 
 * This modified version is part of WPP (GPL 3.0)
 */
```

### althttpd（Public Domain）

**完全自由**，建议类似 SQLite 的标注方式。

### TinyCC（LGPL 2.1）

**需要谨慎**：

1. **最佳方案**: 不修改源码，只通过 libtcc API 调用
2. **如果必须修改**:
   ```c
   /*
    * TinyCC - Tiny C Compiler
    * Copyright (C) 2001-2004 Fabrice Bellard
    * LGPL 2.1 License
    * 
    * Modified by WPP Project (2026-02-07)
    * This file remains under LGPL 2.1
    * 
    * Changes:
    *   - Added feature X
    *   - Fixed bug Y
    * 
    * Modified source available at: [your-repo-url]
    */
   ```
3. **合规要求**:
   - 必须公开修改后的源码
   - 保持 LGPL 2.1 许可证
   - 提供获取源码的方式

## 代码组织最佳实践

### 1. 保持模块独立

```
modules/wpp-sqlite/
├── CMakeLists.txt
├── src/
│   ├── sqlite3.c          # SQLite 合并源文件（可修改）
│   ├── sqlite3.h
│   └── wpp_db.c           # WPP 封装层
└── include/
    └── wpp_db_internal.h  # 内部头文件
```

### 2. 清晰的 API 边界

```c
// include/wpp/database.h - 公共 API
int wpp_db_open(const char *filename, wpp_db_t **db);

// modules/wpp-sqlite/include/wpp_db_internal.h - 内部 API
struct wpp_db {
    sqlite3 *sqlite_handle;
    // ... 其他内部字段
};
```

### 3. 文档化所有修改

在 `docs/design/` 目录维护修改日志：

```markdown
# SQLite 修改日志

## 2026-02-07
- 添加了自定义函数 wpp_custom_func()
- 优化了查询缓存机制
- 文件: sqlite3.c, 行 1234-1256
```

## 构建和测试

### 开发构建

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### 发布构建

```bash
mkdir build-release && cd build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### 运行测试

```bash
cd build
make test
# 或
ctest --verbose
```

## 发布检查清单

在发布前确保：

- [ ] 所有修改的文件都有正确的许可证声明
- [ ] NOTICE 文件列出了所有使用的第三方组件
- [ ] 包含所有必要的许可证文本（LICENSES/ 目录）
- [ ] README 中明确说明了许可证信息
- [ ] 如果修改了 LGPL 代码，提供了获取源码的链接
- [ ] 文档中解释了各组件的集成方式
- [ ] 测试通过
- [ ] 更新了 CHANGELOG

## 常见问题

### Q: 我可以在闭源项目中使用 WPP 吗？

**A**: 不可以，WPP 使用 GPL 3.0。但您可以：
- 联系我们获取商业许可
- Fork 项目并移除 GPL 组件（仅保留 Public Domain 部分）
- 将 WPP 作为独立服务运行（网络隔离不触发 GPL）

### Q: 我修改了 TinyCC 的代码，是否必须开源？

**A**: 如果您分发修改后的 WPP（包含修改的 TinyCC），则必须：
- 开源您对 TinyCC 的修改
- 保持 LGPL 2.1 许可证
- 提供获取源码的途径

如果您只是内部使用，不分发，则不需要。

### Q: 我可以用不同的许可证发布基于 WPP 的衍生品吗？

**A**: 不可以。GPL 3.0 是 copyleft 许可证，所有衍生品必须使用 GPL 3.0 或兼容许可证。

### Q: Public Domain 的代码（SQLite、althttpd）我可以怎么用？

**A**: 完全自由！您可以：
- 复制这些代码到任何项目
- 修改并用于商业用途
- 以任何许可证发布
- 无需归属（虽然建议这样做）

## 资源

- [GPL 3.0 完整文本](https://www.gnu.org/licenses/gpl-3.0.html)
- [LGPL 2.1 完整文本](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html)
- [GPL 兼容许可证列表](https://www.gnu.org/licenses/license-list.html)
- [SQLite 许可证](https://sqlite.org/copyright.html)
- [TinyCC 文档](https://bellard.org/tcc/)

## 获取帮助

如有许可证相关问题，请：
1. 查看 [LICENSES.md](../LICENSES.md)
2. 阅读 [ARCHITECTURE.md](ARCHITECTURE.md)
3. 提交 Issue 或联系维护者
