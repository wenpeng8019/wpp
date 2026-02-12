# WPP 快速开始指南

> **当前版本**: v0.2.0 | **更新日期**: 2026-02-12

WPP (Web Programming Platform) 是一个零配置的Web服务器，支持SQLite数据库查询、动态C脚本编译执行、内置虚拟文件系统等功能。

## 🚀 5分钟快速体验

### 1. 获取项目

```bash
# 克隆项目
git clone https://github.com/your-username/wpp.git
cd wpp
```

### 2. 一键构建

**方式1: Make (推荐)**
```bash
make                     # Debug版本(可调试)
# 或者
make release            # 优化版本
make stripped           # 最小化版本
```

**方式2: CMake**
```bash
mkdir cmake_build && cd cmake_build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

### 3. 立即启动

```bash
./build/wpp             # 当前目录作为Web根目录
# 服务器启动: http://localhost:8080
# 浏览器自动打开! 🎉
```

### 4. 体验核心功能

**访问内置示例:**
- `http://localhost:8080/hello.html` - 项目介绍页面
- `http://localhost:8080/hello.c` - C脚本CGI示例

**SQTP数据库查询:**
```bash
# 创建表并插入数据
curl -X POST "http://localhost:8080/CREATE%20TABLE%20users(name%20TEXT,age%20INT)"
curl -X POST "http://localhost:8080/INSERT%20INTO%20users%20VALUES('Alice',25)"

# 查询数据 (JSON响应)
curl "http://localhost:8080/SELECT%20*%20FROM%20users"
```

🎯 **恭喜！** 你已经成功运行了WPP服务器！

---

## 📚 核心功能详解

### 🗄️ SQTP 协议 - RESTful数据库查询

通过HTTP直接执行SQL查询，无需额外的API层：

```bash
# 基本查询
curl "http://localhost:8080/SELECT%20*%20FROM%20table_name"

# 带条件查询
curl "http://localhost:8080/SELECT%20*%20FROM%20users%20WHERE%20age>18"

# 数据操作
curl -X POST "http://localhost:8080/INSERT%20INTO%20users%20VALUES('Bob',30)"
curl -X PUT "http://localhost:8080/UPDATE%20users%20SET%20age=26%20WHERE%20name='Alice'"
curl -X DELETE "http://localhost:8080/DELETE%20FROM%20users%20WHERE%20age<18"
```

**JavaScript客户端示例:**
```javascript
// 使用内置的SQTP客户端库
const data = await sqtp.query("SELECT * FROM users WHERE status='active'");
data.forEach(row => console.log(row.name, row.age));
```

### ⚡ C脚本 CGI - 动态编译执行

创建 `script.c` 文件:
```c
#include <stdio.h>
#include <sqlite3.h>

int main() {
    printf("Content-Type: text/html\n\n");
    printf("<h1>Hello from C!</h1>");
    
    // 可以直接使用SQLite
    sqlite3 *db;
    sqlite3_open(":memory:", &db);
    // ... 数据库操作
    
    return 0;
}
```

访问 `http://localhost:8080/script.c` 即可看到实时编译执行的结果！

### 📦 Buildins 虚拟文件系统

WPP内置了一个虚拟文件系统，提供零依赖启动：

```
buildins/                # 虚拟根目录
├── hello.html          # 内置欢迎页面
├── hello.c             # C脚本示例
├── include/            # C标准库头文件
│   ├── stdio.h
│   ├── stdlib.h
│   └── ...
└── lib/                # 库文件
    └── sqtp/           # SQTP客户端库
        ├── sqtp.fetch.js
        └── sqtp.test.html
```

**特性:**
- **自动压缩**: gzip压缩减少内存占用
- **智能缓存**: ETag支持浏览器缓存
- **哈希优化**: 大量文件时自动启用哈希查找
- **目录支持**: 完整的目录结构和导航

---

## 🔧 开发指南

### 构建系统

WPP提供双构建系统，选择你喜欢的方式：

**Make 构建 (原生)**
```bash
make help               # 查看所有选项
make debug              # 调试版本 (-g -O0)
make release            # 发布版本 (-O2)  
make stripped           # 最小版本 (-O2 + strip)
make clean              # 清理构建
```

**CMake 构建 (IDE友好)**
```bash
# 配置构建类型
cmake .. -DCMAKE_BUILD_TYPE=Debug      # 或 Release
cmake --build .

# 自定义目标
cmake --build . --target debug
cmake --build . --target release
cmake --build . --target stripped
cmake --build . --target build_help   # 显示帮助
```

### 项目结构

```
wpp/                              # 项目根目录
├── src/                          # 源代码
│   ├── main.c                    # 主入口
│   ├── httpd.{c,h}              # HTTP服务器
│   ├── buildins.{c,h}           # 虚拟文件系统
│   ├── http_sqtp.c              # SQTP协议
│   ├── http_cgi_c.c             # C脚本CGI
│   ├── tcc_evn.{c,h}            # TinyCC环境
│   └── buildins/                # 生成的内置资源
├── buildins/                    # 内置文件源码
├── third_party/                 # 第三方库
├── docs/                        # 文档
├── tools/                       # 构建工具
├── Makefile                     # Make构建
└── CMakeLists.txt               # CMake构建
```

### 开发环境设置

**调试模式 (推荐开发使用)**
```bash
make debug                       # 启用调试符号
gdb ./build/wpp                  # GDB调试
# 或使用IDE断点调试
```

**性能测试**
```bash
make release                     # 优化构建
./build/wpp &                    # 后台运行

# 压力测试
ab -n 1000 -c 100 http://localhost:8080/
```

**内存检查**
```bash
make debug
valgrind --leak-check=full ./build/wpp
```

---

## 💡 使用示例

### Web应用开发

**1. 创建数据库驱动的网页**

`index.html`:
```html
<!DOCTYPE html>
<html>
<head>
    <script src="lib/sqtp/sqtp.fetch.js"></script>
</head>
<body>
    <div id="users"></div>
    <script>
        sqtp.query("SELECT * FROM users").then(data => {
            document.getElementById('users').innerHTML = 
                data.map(u => `<p>${u.name}: ${u.age}</p>`).join('');
        });
    </script>
</body>
</html>
```

**2. API端点开发**

`api/users.c`:
```c
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>

int main() {
    printf("Content-Type: application/json\n\n");
    
    sqlite3 *db;
    sqlite3_open("data.db", &db);
    
    // 处理查询参数 
    char *query_string = getenv("QUERY_STRING");
    
    // 执行查询并输出JSON
    // ... 
    
    sqlite3_close(db);
    return 0;
}
```

### 微服务开发

**配置文件 `wpp.service`:**
```ini
[Unit]
Description=WPP Web Server
After=network.target

[Service]
Type=forking
ExecStart=/usr/local/bin/wpp /var/www/myapp
PIDFile=/var/www/myapp/.wpp.pid
User=www-data
Group=www-data

[Install]
WantedBy=multi-user.target
```

### 数据分析应用

`dashboard.c`:
```c
#include <stdio.h>
#include <sqlite3.h>
#include <yyjson.h>

int main() {
    printf("Content-Type: application/json\n\n");
    
    sqlite3 *db;
    sqlite3_open("analytics.db", &db);
    
    // 复杂查询和JSON生成
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    // ... 数据处理
    
    char *json = yyjson_mut_write(doc, 0, NULL);
    printf("%s", json);
    
    return 0;
}
```

---

## 📖 进阶主题

### 部署配置

**生产环境部署:**
```bash
# 编译优化版本
make stripped

# 部署到服务器  
sudo cp build/wpp /usr/local/bin/
sudo mkdir -p /var/www/myapp
sudo systemctl enable wpp.service
sudo systemctl start wpp.service
```

**Docker部署:**
```dockerfile
FROM alpine:latest
RUN apk add --no-cache gcc musl-dev make sqlite
COPY . /app
WORKDIR /app
RUN make stripped
EXPOSE 8080
CMD ["./build/wpp"]
```

### 性能优化

**1. buildins优化:**
- 将常用文件放入 `buildins/` 目录
- 使用gzip压缩减少内存占用
- 资源数量超过50时自动启用哈希查找

**2. 数据库优化:**
```sql
-- 为常用查询创建索引
CREATE INDEX idx_user_status ON users(status);
CREATE INDEX idx_log_timestamp ON logs(timestamp);

-- 使用事务提升写入性能
BEGIN;
INSERT INTO users VALUES (...);
INSERT INTO users VALUES (...);
COMMIT;
```

**3. 缓存策略:**
- 静态文件自动ETag缓存
- 动态内容通过HTTP头控制缓存
- 数据库连接池 (TODO: v0.3.0)

### 安全考虑

**1. SQTP安全:**
```c
// 总是使用参数化查询
sqlite3_prepare_v2(db, "SELECT * FROM users WHERE id = ?", -1, &stmt, NULL);
sqlite3_bind_int(stmt, 1, user_id);
```

**2. C脚本沙箱:**
- 进程隔离：每个脚本运行在独立子进程
- 时间限制：CPU时间和墙钟时间控制  
- 内存限制：虚拟内存大小限制
- 文件访问限制：chroot沙箱 (可选)

**3. HTTP安全:**
- 输入验证：路径遍历防护
- 缓冲区保护：所有字符串操作长度检查
- 头文件注入防护：HTTP头验证

---

## 🔧 故障排除

### 常见问题

**Q: 编译失败，提示找不到头文件**
```bash
# 确保第三方库已正确初始化
git submodule update --init --recursive

# 检查SQLite是否已配置
cd third_party/sqlite && ./configure && make sqlite3.c
```

**Q: 运行时提示端口被占用**
```bash
# 停止现有实例
./build/wpp --stop

# 或强制杀死进程
pkill -9 wpp
```

**Q: C脚本编译失败**
```bash
# 检查TinyCC是否正确集成
./build/wpp -h   # 查看帮助确认功能

# 查看详细错误信息
tail -f /tmp/wpp.log
```

**Q: SQTP查询返回错误**
```bash
# 检查SQL语法
curl -v "http://localhost:8080/SELECT%20*%20FROM%20users"

# 查看数据库文件权限  
ls -la *.db
```

### 调试技巧

**1. 详细日志:**
```bash
# 启用详细日志模式
WPP_DEBUG=1 ./build/wpp

# 或查看系统日志
tail -f /var/log/syslog | grep wpp
```

**2. 网络调试:**
```bash
# 抓包分析
tcpdump -i lo port 8080

# 测试HTTP响应
curl -v http://localhost:8080/hello.html
```

**3. 内存调试:**
```bash
# 内存泄露检查
valgrind --leak-check=full ./build/wpp

# 性能分析
valgrind --tool=callgrind ./build/wpp
```

---

## 📋 许可证与合规

### 开源许可证策略

WPP 项目采用 **GPL 3.0** 许可证，原因：

1. **兼容性**: 与所有集成的第三方库兼容
   - Public Domain (SQLite, althttpd): 无限制
   - LGPL 2.1 (TinyCC): GPL兼容LGPL

2. **开放性**: 确保所有修改和改进回馈社区  
3. **保护性**: 防止闭源衍生品，保持开源本质

### 商业化选项

如需闭源商业化：

1. **双许可证**: 联系我们获取商业许可
2. **重新设计TinyCC集成**: 改用动态链接或替代方案
3. **移除TinyCC**: 如不需要动态编译功能

### 第三方库合规

**SQLite (Public Domain)**
- 完全自由使用和修改
- 建议保留原始版权声明

**TinyCC (LGPL 2.1)**
- 可以静态链接 (当前实现)
- 如有修改必须开源修改部分
- 保持LGPL许可证

详细许可证信息请参阅 [LICENSES.md](../LICENSES.md)

---

## 🛣️ 开发路线图

### v0.2.0 (当前版本) ✅
- [x] buildins虚拟文件系统
- [x] SQTP协议基础支持
- [x] TinyCC CGI集成  
- [x] 双构建系统 (Make/CMake)
- [x] gzip压缩和缓存优化

### v0.3.0 (进行中) 🔄
- [ ] WebSocket支持
- [ ] 多数据库后端 (PostgreSQL/MySQL)
- [ ] 配置文件支持 (JSON/YAML)
- [ ] 性能监控和指标

### v1.0.0 (目标) 🎯
- [ ] 生产级稳定性和性能
- [ ] 完整的安全特性
- [ ] 集群和负载均衡支持
- [ ] 插件系统

---

## 📚 相关资源

**文档:**
- [架构设计](ARCHITECTURE.md) - 深入了解技术架构
- [TinyCC集成](tcc-integration.md) - C脚本开发指南  
- [SQTP协议规范](sqtp/) - 数据库查询协议详解
- [buildins系统](BUILDINS.md) - 虚拟文件系统说明

**示例项目:**
- [WPP Todo App](examples/todo/) - 完整的待办应用
- [WPP Blog Engine](examples/blog/) - 博客引擎示例
- [WPP Analytics](examples/analytics/) - 数据分析仪表板

**社区:**
- [GitHub Issues](https://github.com/username/wpp/issues) - 问题反馈
- [Discussions](https://github.com/username/wpp/discussions) - 社区讨论  
- [Wiki](https://github.com/username/wpp/wiki) - 社区文档

**开发工具:**
- [WPP CLI](tools/wpp-cli/) - 命令行工具
- [VS Code Extension](tools/vscode-wpp/) - 编辑器支持
- [Docker Images](https://hub.docker.com/r/wpp/server) - 容器部署

---

**🎉 欢迎贡献代码！** 查看 [CONTRIBUTING.md](../CONTRIBUTING.md) 了解如何参与项目开发。