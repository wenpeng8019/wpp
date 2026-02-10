# WPP Project

一个模块化的 C 语言项目，集成了 HTTP 服务器、数据库引擎和动态编译器。

## 特性

- 🌐 **HTTP 服务器**: 基于 althttpd，轻量级且安全
- 💾 **数据库引擎**: 集成 SQLite，提供强大的数据持久化能力
- ⚡ **动态编译**: 使用 TinyCC 实现运行时 C 代码编译
- 🧩 **模块化设计**: 清晰的模块边界，易于维护和扩展
- 📖 **开源合规**: 遵守所有第三方库的许可证要求

## 快速开始

### 依赖

- CMake 3.10+
- GCC 或 Clang
- Make

### 构建

```bash
# 使用 CMake（推荐）
mkdir build && cd build
cmake ..
make

# 或使用 Makefile
make
```

### 运行

```bash
./build/wpp
```

## 项目结构

```
wpp/
├── include/wpp/          # 公共 API 头文件
│   ├── wpp.h            # 主头文件
│   ├── core.h           # 核心功能
│   ├── server.h         # HTTP 服务器
│   ├── database.h       # 数据库
│   └── compiler.h       # 编译器
│
├── modules/             # 功能模块
│   ├── wpp-core/        # 核心模块（GPL 3.0）
│   ├── wpp-sqlite/      # 数据库模块（Public Domain）
│   ├── wpp-compiler/    # 编译器模块（LGPL 2.1）
│   └── wpp-server/      # HTTP 服务器模块（Public Domain）
│
├── src/                 # 主程序
├── third_party/         # 第三方源码（参考）
│   ├── sqlite/          # SQLite 源码
│   ├── tinycc/          # TinyCC 源码
│   └── althttpd/        # althttpd 源码
│
├── docs/                # 文档
│   ├── design/          # 设计文档
│   └── api/             # API 文档
│
├── LICENSES/            # 许可证文件
├── LICENSE              # 主许可证（GPL 3.0）
├── NOTICE               # 第三方组件归属
└── LICENSES.md          # 许可证指南
```

## 架构

WPP 采用模块化架构，每个模块都有明确的职责：

- **wpp-core**: 提供日志、配置、内存管理等基础功能
- **wpp-sqlite**: 封装 SQLite，提供简化的数据库 API
- **wpp-compiler**: 封装 TinyCC，实现动态 C 代码编译
- **wpp-server**: 基于 althttpd，提供现代化的 HTTP 服务

详细架构设计请参考 [ARCHITECTURE.md](docs/design/ARCHITECTURE.md)

## 许可证

本项目使用 **GPL 3.0** 许可证，这与集成的第三方库兼容：

| 组件 | 原始许可证 | 使用方式 |
|------|-----------|---------|
| SQLite | Public Domain | 直接修改和集成 |
| TinyCC | LGPL 2.1 | 封装调用（符合 LGPL） |
| althttpd | Public Domain | 直接修改和集成 |

详细信息请参考：
- [LICENSE](LICENSE) - 主许可证
- [NOTICE](NOTICE) - 第三方组件归属
- [LICENSES.md](LICENSES.md) - 许可证合规指南

## 第三方库

### SQLite (Public Domain)
- **功能**: 嵌入式关系数据库
- **许可**: 公有领域，完全自由使用
- **来源**: https://sqlite.org/

### TinyCC (LGPL 2.1)
- **功能**: 快速的 C 编译器
- **许可**: LGPL 2.1
- **来源**: https://bellard.org/tcc/

### althttpd (Public Domain)
- **功能**: 轻量级 HTTP 服务器
- **许可**: 公有领域（预期）
- **来源**: https://sqlite.org/althttpd/

## 开发

### 添加新模块

1. 在 `modules/` 下创建新目录
2. 添加 `CMakeLists.txt`
3. 在 `include/wpp/` 添加公共 API
4. 更新主 `CMakeLists.txt`

### 编码规范

- 使用 C11 标准
- 遵循项目的代码风格
- 所有公共 API 都有文档注释
- 修改第三方代码时添加修改说明

### 测试

```bash
cd build
make test
```

## 贡献

欢迎贡献！请确保：

1. 遵守相关的开源许可证
2. 添加适当的测试
3. 更新文档
4. 在提交信息中清晰描述更改

## 联系

- 项目主页: [待添加]
- 问题反馈: [待添加]
- 邮件: [待添加]

## 致谢

感谢以下开源项目：

- SQLite 团队
- TinyCC (Fabrice Bellard 等)
- althttpd (D. Richard Hipp)

## 路线图

- [x] 项目结构搭建
- [x] 核心模块基础功能
- [ ] SQLite 集成和封装
- [ ] TinyCC 集成和封装
- [ ] althttpd 现代化改造
- [ ] 完整的示例应用
- [ ] 性能优化
- [ ] 文档完善

---

**License**: GPL 3.0 (compatible with LGPL 2.1 and Public Domain)  
**Version**: 0.1.0  
**Status**: Alpha / Development
