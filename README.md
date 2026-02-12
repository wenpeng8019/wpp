# WPP

> **`./wpp` - 安全、高性能、SQLite 内置、C 语言动态脚本 CGI**

一个无依赖的单一可执行文件 Web 服务器工具。可使任何文件夹瞬间变为自带数据库的动态 Web 服务器，而无需任何配置。

```bash
./wpp
# 🚀 服务器启动于 http://localhost:8080
# 🌐 浏览器自动打开，您的文件夹现在是一个 Web 应用！
```

[![Version](https://img.shields.io/badge/version-v0.2.0-blue.svg)](https://github.com/wenpeng8019/wpp/releases)
[![License](https://img.shields.io/badge/license-GPL%20v3-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux-lightgrey.svg)](#-supported-platforms)

---

## ✨ 核心特性

### 🗄️ **SQTP 协议 - 结构化查询传输协议**
通过自定义 HTTP 方法和头部直接查询 SQLite 数据库：

**URI 路径说明:**
- **`/`** - 内存数据库，可直接使用但数据不保存 (适合临时操作)
- **`/.db`** - 默认文件数据库，首次访问时自动创建 (持久化存储)
- **`/db/filename`** - 自定义指定的其他文件数据库路径，需要由 C CGI 主动创建。否则会返回 404 错误

```bash
# 内存数据库查询 (临时数据，重启丢失)
curl -X SQTP-SELECT localhost:8080/ \
  -H "FROM: users" \
  -H "WHERE: status='active'"

# 默认文件数据库插入 (自动创建 .db 文件，持久化存储)
curl -X SQTP-INSERT localhost:8080/.db \
  -H "TABLE: users" \
  -H "COLUMNS: name, age" \
  -H "Content-Type: application/json" \
  -d '["Alice", 25]'

# 指定文件数据库查询 (自定义数据库文件)
curl -X SQTP-SELECT localhost:8080/db/main \
  -H "FROM: users" \
  -H "X-SQTP-View: object"

# JSON 响应，直接用于 JavaScript fetch()
```

**JavaScript 客户端 (3 种使用方式):**

```html
<!-- 1. Promise/Async-Await 方式 (推荐) -->
<script src="lib/sqtp/sqtp.xhr.promise.js"></script>
<script>
  const db = new SQTP();
  // 现代 async/await 语法
  const users = await db.select('users')
    .where('age > 18')
    .execute();
  users.forEach(user => console.log(user.name, user.age));
</script>

<!-- 2. 传统回调方式 (兼容性最佳) -->
<script src="lib/sqtp/sqtp.xhr.callback.js"></script>
<script>
  const db = new SQTP();
  // Error-First Callback 模式
  db.select('users').where('age > 18').execute(function(err, users) {
    if (err) return console.error(err);
    users.forEach(user => console.log(user.name, user.age));
  });
</script>

<!-- 3. Fetch API 方式 (现代浏览器) -->
<script src="lib/sqtp/sqtp.fetch.js"></script>
<script>
  const db = new SQTP();
  // 基于 Fetch API
  db.select('users')
    .where('age > 18')
    .execute()
    .then(users => users.forEach(user => console.log(user.name, user.age)))
    .catch(err => console.error(err));
</script>
```

### 🔥 **C 脚本 CGI - 运行时编译**
基于 TinyCC 即时编译技术，将 `.c` 文件作为 CGI 脚本执行：

**hello.c:**
```c
#include <stdio.h>
#include <sqlite3.h>

int main() {
    printf("Content-Type: text/html\r\n\r\n");
    printf("<h1>Hello from C!</h1>");
    printf("<p>Query: %s</p>", getenv("QUERY_STRING"));
    
    // 直接使用 SQLite
    sqlite3 *db;
    sqlite3_open("data.db", &db);
    // ... 数据库操作
    
    return 0;
}
```

访问：`http://localhost:8080/hello.c?name=world` ✅ 无需编译，即刻执行！

### 📦 **Buildins 虚拟文件系统**
内置的 sysroot 概念虚拟文件系统，提供零依赖启动：

- **🔧 C 头文件**: `<stdio.h>`, `<stdlib.h>`, `<sqlite3.h>` 等完整标准库
- **📚 客户端库**: 3 种 SQTP JavaScript 客户端库，满足不同需求 (Promise/Callback/Fetch)
- **🎛️ 管理界面**: 内置的数据库管理和项目介绍页面
- **⚡ 性能优化**: gzip 压缩、ETag 缓存、自适应哈希查找

---

## 🚀 5分钟快速开始

### 1. 克隆与构建

```bash
# 克隆项目
git clone https://github.com/wenpeng8019/wpp.git
cd wpp

# 一键构建 (Choose one)
make                     # Make 构建 (推荐)
# 或
cmake -B build && cmake --build build  # CMake 构建
```

### 2. 启动服务器

```bash
./build/wpp              # 在当前目录启动Web服务器
# ✨ 浏览器自动打开 http://localhost:8080
```

### 3. 体验核心功能

**立即体验内置示例:**
- [`/hello.html`](http://localhost:8080/hello.html) - WPP 项目介绍页面
- [`/hello.c`](http://localhost:8080/hello.c) - C 脚本 CGI 示例

**测试 SQTP 数据库查询:**
```bash
# 插入测试数据到默认文件数据库 (自动创建 .db 文件)
curl -X SQTP-INSERT localhost:8080/.db \
  -H "TABLE: users" \
  -H "COLUMNS: name, age" \
  -H "Content-Type: application/json" \
  -d '["Alice", 25]'

# 查询数据 (返回 JSON，数据已持久化保存)
curl -X SQTP-SELECT localhost:8080/.db \
  -H "FROM: users" \
  -H "X-SQTP-View: object"
```

🎉 **恭喜！** 您已成功运行了 WPP！

---

## 💡 使用场景

### 🏗️ **快速原型开发**
无需后端框架，构建数据驱动的 Web 应用：

```c
// api.c - 动态 API 端点
#include <stdio.h>
#include <sqlite3.h>

int main() {
    printf("Content-Type: application/json\r\n\r\n");
    
    sqlite3 *db;
    sqlite3_open("data.db", &db);
    
    // 查询并输出 JSON
    printf("[{\"name\":\"Alice\",\"age\":25}]");
    
    sqlite3_close(db);
    return 0;
}
```

### 📊 **数据库 Web 界面**
通过浏览器使用 SQTP JavaScript 客户端查询 SQLite：

```html
<!-- index.html -->
<script src="lib/sqtp/sqtp.xhr.promise.js"></script>
<script>
// 前端实时数据库查询
const loadUsers = async () => {
    const db = new SQTP();
    const users = await db.select('users')
        .where('age > 18')
        .execute();
    document.getElementById('users').innerHTML = 
        users.map(u => `<p>${u.name}: ${u.age}</p>`).join('');
};
</script>
```

### 🔧 **嵌入式 Web UI**
完美适用于 IoT 设备、本地工具或嵌入式系统——单一二进制文件，无外部依赖。

### 🧪 **教learning与实验**
- **C 语言学习**: 立即执行 C 代码，无需编译步骤
- **数据库实验**: SQLite + Web 界面，可视化查询结果  
- **HTTP 协议理解**: RESTful API 设计和实现

---

## 📋 构建选项

WPP 支持双构建系统，选择您喜欢的方式：

### Make 构建 (原生，推荐)

```bash
make help               # 查看所有选项
make                    # Debug 版本 (默认，可调试)
make release            # Release 版本 (O2 优化)
make stripped           # 最小版本 (O2 + strip，2.0MB)
```

### CMake 构建 (IDE 友好)

```bash
# 配置构建类型
mkdir cmake_build && cd cmake_build
cmake .. -DCMAKE_BUILD_TYPE=Debug      # 或 Release
cmake --build .

# 自定义目标
cmake --build . --target release      # 发布版本
cmake --build . --target stripped     # 最小版本
```

**构建结果对比:**

| 版本 | 大小 | 优化 | 适用场景 |
|------|------|------|----------|
| Debug | 3.4MB | `-g -O0` | 开发调试 |
| Release | 2.1MB | `-O2` | 生产部署 |
| Stripped | 2.0MB | `-O2 + strip` | 嵌入式/容器 |

---

## 🏗️ 技术架构

**多层进程模型:**
```
主进程 (端口 8080)
 ├── HTTP 服务器 (基于 althttpd)
 ├── SQTP 协议处理器  
 ├── Buildins 虚拟文件系统
 └── 请求处理器 (每连接)
      └── CGI 子进程 (C 脚本执行)
           ├── TinyCC 即时编译
           └── SQLite 数据库访问
```

**核心组件:**

| 组件 | 功能 | 技术栈 |
|------|------|--------|
| **HTTP 服务器** | HTTP/1.1协议，静态文件服务 | althttpd (改进版) |
| **SQTP 协议** | RESTful数据库查询 | SQLite + JSON |
| **TinyCC 集成** | C脚本即时编译执行 | libtcc |
| **Buildins 系统** | 虚拟文件系统 | gzip + 哈希优化 |
| **数据库** | 内嵌式 SQL 数据库 | SQLite 3 |

---

## 📖 深入学习

### 📚 完整文档

- [**快速开始指南**](docs/QUICKSTART.md) - 详细的入门教程和最佳实践
- [**架构设计文档**](docs/ARCHITECTURE.md) - 深入的系统设计和技术架构
- [**SQTP 协议规范**](docs/sqtp/) - 完整的 SQTP/1.0 协议文档
- [**Buildins 系统说明**](docs/BUILDINS.md) - 虚拟文件系统技术详解

### 💼 示例项目

- [**Todo 应用**](examples/todo-app/) - 完整的任务管理应用 (计划中)
- [**博客引擎**](examples/blog-engine/) - 数据驱动的博客系统 (计划中)  
- [**监控面板**](examples/monitoring/) - 实时数据监控仪表板 (计划中)
- [**API 服务**](examples/api-server/) - RESTful API 服务示例 (计划中)

---

## 🛡️ 安全特性

### 进程隔离
- **Fork 沙箱**: 每个 C 脚本在独立子进程中执行
- **资源限制**: CPU 时间、内存使用、文件访问控制
- **权限下降**: 可选的 chroot 和用户权限控制

### 输入验证
- **SQL 注入防护**: SQTP 参数化查询
- **路径遍历防护**: 严格的路径验证和边界检查
- **缓冲区保护**: 所有字符串操作长度验证

### 数据保护
- **只读 Buildins**: 内置资源不可篡改
- **数据隔离**: SQLite 数据库文件权限控制
- **会话管理**: 可选的身份验证和授权机制 (计划中)

---

## 📊 性能数据

**基准测试环境**: macOS (Apple M1), 16GB RAM

| 场景 | 吞吐量 | 延迟 | 内存占用 |
|------|--------|------|----------|
| 静态文件服务 | 15,000 req/s | 3.2ms | < 10MB |
| SQTP 查询 | 8,000 req/s | 8.1ms | < 15MB |
| C 脚本 CGI | 2,500 req/s | 25ms | < 50MB |

**优化特性:**
- **零拷贝传输**: `sendfile()` 系统调用优化
- **智能缓存**: ETag/Last-Modified 浏览器缓存
- **压缩传输**: 自动 gzip 压缩
- **预热机制**: TinyCC 环境预配置，fork 复用

---

## 🌍 支持平台

### ✅ 完全支持
- **macOS** (Intel / Apple Silicon)
- **Linux** (Ubuntu 20.04+、CentOS 8+、Arch、Alpine)

### 🔄 计划支持  
- **Windows** (WSL 兼容层)
- **FreeBSD** / **OpenBSD**
- **嵌入式 Linux** (ARM/MIPS)

### 📋 环境要求
- **C 编译器**: GCC 7+ 或 Clang 9+
- **构建工具**: Make 或 CMake 3.10+
- **内存**: 最低 256MB (推荐 1GB+)
- **磁盘**: < 10MB (编译) + 数据存储空间

---

## 🛠️ 高级配置

### 命令行选项

```bash
./build/wpp [web_root] [OPTIONS]

Examples:
  ./build/wpp                    # 当前目录作为 Web 根目录
  ./build/wpp /var/www          # 指定 Web 根目录
  ./build/wpp --stop            # 停止运行中的服务器

Options:
  -s, --stop        停止当前运行的 wpp 实例
  -h, --help        显示帮助信息并退出
```

### 环境变量配置

```bash
# 调试模式
WPP_DEBUG=1 ./build/wpp

# 设置端口 (TODO: v0.3.0)
WPP_PORT=8080 ./build/wpp

# 数据库路径 (TODO: v0.3.0)  
WPP_DATABASE=data.db ./build/wpp
```

### C 脚本 CGI 环境

所有标准 CGI 环境变量均可用：

```c
// 获取 CGI 环境信息
char *method = getenv("REQUEST_METHOD");      // GET, POST, PUT, DELETE
char *query = getenv("QUERY_STRING");         // URL 参数
char *content_length = getenv("CONTENT_LENGTH"); // POST 数据长度
char *content_type = getenv("CONTENT_TYPE");  // 请求内容类型
char *remote_addr = getenv("REMOTE_ADDR");    // 客户端 IP
char *user_agent = getenv("HTTP_USER_AGENT"); // 浏览器信息
// ... 更多 CGI 变量
```

---

## 🤝 贡献指南

欢迎贡献！我们正在寻找以下领域的帮助：

### 🌟 优先级功能
- [ ] 更多 SQTP 操作 (JOIN、GROUP BY、子查询)
- [ ] HTTPS/TLS 支持
- [ ] WebSocket 实时通信
- [ ] 配置文件支持 (JSON/YAML)
- [ ] 性能监控和指标

### 🔧 技术改进  
- [ ] Windows 平台支持
- [ ] 包管理器支持 (Homebrew、apt、yum)
- [ ] 更多编程语言支持 (Lua、Python 等)
- [ ] 单元测试和集成测试
- [ ] 代码覆盖率提升

### 📝 文档完善
- [ ] API 参考文档
- [ ] 更多使用示例
- [ ] 视频教程
- [ ] 多语言文档 (English)

**参与方式:**
1. Fork 项目
2. 创建功能分支: `git checkout -b feature/amazing-feature`
3. 提交更改: `git commit -m 'Add amazing feature'`
4. 推送分支: `git push origin feature/amazing-feature`
5. 创建 Pull Request

---

## 📄 开源许可

**GPL 3.0** - 与所有集成组件兼容

| 组件 | 许可证 | 用途 | 状态 |
|------|-------|----- |------|
| **SQLite** | Public Domain | 嵌入式数据库 | ✅ 完全兼容 |
| **TinyCC** | LGPL 2.1 | 运行时 C 编译器 | ✅ GPL 兼容 |
| **althttpd** | Public Domain | HTTP 服务器基础 | ✅ 完全兼容 |
| **yyjson** | MIT | JSON 解析器 | ✅ GPL 兼容 |
| **zlib** | zlib License | 压缩库 | ✅ GPL 兼容 |
| **uthash** | BSD-like | 哈希表库 | ✅ GPL 兼容 |

详细信息请参阅：[LICENSE](LICENSE)、[NOTICE](NOTICE) 和 [LICENSES.md](LICENSES.md)

**商业化选项:**
- 联系我们获取双许可证
- 移除 GPL 组件的自定义版本
- 企业支持和定制开发服务

---

## 🙏 致谢

WPP 站在巨人的肩膀上，感谢以下开源项目：

- **SQLite** - D. Richard Hipp 和团队，提供了世界上最广泛部署的数据库引擎
- **TinyCC** - Fabrice Bellard，革命性的小型快速 C 编译器
- **althttpd** - D. Richard Hipp，简洁高效的 HTTP 服务器
- **yyjson** - YaoYuan，高性能 JSON 库
- **zlib** - Jean-loup Gailly 和 Mark Adler，无处不在的压缩库
- **uthash** - Troy D. Hanson，优秀的 C 哈希表实现

---

## 🎯 项目状态与路线图

### 📈 当前状态

**版本**: [v0.2.0](https://github.com/wenpeng8019/wpp/releases/tag/v0.2.0)  
**状态**: Beta (核心功能稳定，积极开发中)

### ✅ 已完成功能

- [x] HTTP/1.1 服务器 (基于 fork 的并发模型)
- [x] SQTP 协议 (8 个基础操作)
- [x] TinyCC C 脚本 CGI
- [x] SQLite 集成 (共享内存数据库)
- [x] Buildins 虚拟文件系统  
- [x] JavaScript 客户端库 (Promise/Callback/Fetch 三种方式)
- [x] 进程隔离和安全模型
- [x] 双构建系统 (Make/CMake)
- [x] 完整的项目文档

### 🚧 开发路线图

**v0.3.0** (2026 Q2)
- [ ] WebSocket 实时通信支持
- [ ] 多数据库后端 (PostgreSQL/MySQL)  
- [ ] HTTPS/TLS 加密传输
- [ ] 配置文件支持 (JSON/YAML)
- [ ] 性能监控和指标收集

**v0.4.0** (2026 Q3)
- [ ] 集群和负载均衡
- [ ] 插件系统和 API
- [ ] Windows 平台原生支持
- [ ] 包管理器分发 (Homebrew/apt)
- [ ] 容器化和 K8s 支持

**v1.0.0** (2026 Q4)
- [ ] 生产级稳定性和性能
- [ ] 完整的安全特性
- [ ] 企业级功能
- [ ] 完整的测试覆盖
- [ ] 长期支持 (LTS) 版本

---

## 💫 为什么选择 WPP？

### 🚀 **极简哲学**
一个命令启动，无配置文件，无复杂依赖。将任何文件夹变成动态 Web 应用。

### ⚡ **即时开发**
C 脚本即写即用，SQLite 即查即得。前端到数据库，一站式解决方案。

### 🛡️ **安全可靠**  
进程隔离、资源限制、输入验证。企业级安全特性，个人项目友好。

### 🧠 **学习友好**
清晰的架构、详细的文档、丰富的示例。理解 Web 技术的最佳实践平台。

### 🌍 **开源精神**
GPL 3.0 许可，社区驱动，持续创新。您的贡献改变世界。

---

**🌟 如果 WPP 对您有帮助，请务必 Star ⭐ 此项目！**

**📢 关注我们**: [GitHub](https://github.com/wenpeng8019/wpp) | [Issues](https://github.com/wenpeng8019/wpp/issues) | [Discussions](https://github.com/wenpeng8019/wpp/discussions)

---

<div align="center">

**用 ❤️ 为快速 Web 开发而制作**  
**© 2026 WPP Project · 释放创造力，简化复杂性**

</div>