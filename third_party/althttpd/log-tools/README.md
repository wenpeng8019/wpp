# Althttpd 日志工具集

本目录包含用于管理、分析和查看 althttpd 日志的工具集。

## 工具概览

| 工具 | 类型 | 用途 |
|------|------|------|
| `extract-error-numbers.tcl` | TCL 脚本 | 提取错误代码交叉引用 |
| `logrotate.tcl` | TCL 脚本 | 日志文件轮转和归档 |
| `logtodb.c` | C 程序 | 日志转换为 SQLite 数据库 |
| `logview` | WAPP 脚本 | Web 日志查看器 |

---

## 1. extract-error-numbers.tcl

### 功能说明
从 `althttpd.c` 源代码中提取所有错误日志编号和对应的描述信息，生成 SQL 交叉引用表。

### 工作原理
扫描源代码中形如 `数字 /* LOG: 描述 */` 的注释，例如：
```c
NotFound(210); /* LOG: Empty request URI */
Malfunction(380 /* LOG: pipe() failed */, "Cannot create a pipe");
```

### 使用方法
```bash
cd /path/to/althttpd
./extract-error-numbers.tcl > error-codes.sql

# 导入到 SQLite 数据库
sqlite3 errors.db < error-codes.sql
```

### 输出示例
```sql
INSERT INTO xref VALUES(210,'Empty request URI');
INSERT INTO xref VALUES(380,'pipe() failed');
```

### 应用场景
- 生成错误代码文档
- 根据日志中的错误编号快速查找问题原因
- 建立日志分析系统的错误代码索引

---

## 2. logrotate.tcl

### 功能说明
自动轮转和压缩 althttpd 日志文件，防止日志文件无限增长占满磁盘空间。

### 工作原理
1. 检查日志文件大小是否超过 1GB
2. 如果超过，将日志重命名为 `YYYYMMDD.log`（如 `20260208.log`）
3. 使用 `xz` 压缩归档文件

### 配置
修改脚本中的日志文件路径：
```tcl
set LOGFILENAME /home/www/logs/http.log
```

### 使用方法

#### 手动运行
```bash
./logrotate.tcl
```

#### 定时任务（推荐）
添加到 crontab，每天凌晨 2 点执行：
```bash
crontab -e
# 添加以下行：
0 2 * * * /path/to/logrotate.tcl
```

#### 更频繁的检查
每小时检查一次：
```bash
0 * * * * /path/to/logrotate.tcl
```

### 输出文件
- `20260208.log.xz` - 压缩后的归档日志
- `http.log` - 当前活动日志（自动重新创建）

### 压缩比
xz 压缩通常可达到 10:1 以上的压缩比，1GB 日志压缩后约 100MB。

---

## 3. logtodb.c

### 功能说明
将 althttpd 的 CSV 格式文本日志转换为 SQLite 数据库，便于高效查询和分析。

### 编译
```bash
# 标准编译
gcc -o logtodb logtodb.c sqlite3.c -lpthread -ldl -lm

# 静态编译（用于 chroot 环境）
gcc -static -o logtodb logtodb.c sqlite3.c -lpthread -ldl -lm
```

### 使用方法

#### 基本用法
```bash
# 从标准输入读取
cat http.log | ./logtodb --db logs.db

# 从文件读取
./logtodb --db logs.db --logfile /var/log/http.log
```

#### 高级选项

**实时监控模式**（类似 `tail -f`）
```bash
./logtodb --db logs.db --logfile /var/log/http.log -f
```

**只处理最后 N 字节**
```bash
# 只处理最后 5MB 日志
./logtodb --db logs.db --logfile http.log --tail 5MB
```

**保留时间窗口**
```bash
# 只保留最近 300 秒（5分钟）的数据
./logtodb --db logs.db --logfile http.log --keep 300
```

**重置数据库**
```bash
# 清空现有数据重新导入
./logtodb --db logs.db --logfile http.log --reset
```

**组合使用**
```bash
# 实时监控，只保留最近 1 小时数据
./logtodb --db logs.db --logfile http.log -f --keep 3600
```

### 数据库结构

创建的 `log` 表包含以下字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| date | TEXT | 时间戳（ISO8601 格式） |
| ip | TEXT | 客户端 IP 地址 |
| url | TEXT | 请求 URI |
| ref | TEXT | Referer（来源页面） |
| code | INT | HTTP 状态码（200、404 等） |
| nIn | INT | 请求字节数 |
| nOut | INT | 响应字节数 |
| t1, t2 | INT | 进程时间（用户态、内核态）毫秒 |
| t3, t4 | INT | CGI 脚本时间（用户态、内核态）毫秒 |
| t5 | INT | 墙钟时间（毫秒） |
| nreq | INT | 请求序号 |
| agent | TEXT | User-Agent 字符串 |
| user | TEXT | 认证用户名 |
| n | INT | SCRIPT_NAME 的字节数 |
| lineno | INT | 生成此日志的 althttpd 代码行号 |
| domain | TEXT | 域名（派生） |
| scriptname | TEXT | 脚本/文件名（派生） |
| uriofst | INT | REQUEST_URI 在 url 中的偏移 |
| cgi | INT | 是否为 CGI 请求（计算列） |

### 查询示例

```sql
-- 统计各状态码数量
SELECT code, COUNT(*) FROM log GROUP BY code;

-- 查找最慢的请求
SELECT date, url, t5 FROM log ORDER BY t5 DESC LIMIT 10;

-- 统计各域名访问量
SELECT domain, COUNT(*) FROM log GROUP BY domain;

-- CGI vs 静态文件性能对比
SELECT 
  CASE WHEN cgi THEN 'CGI' ELSE 'Static' END as type,
  AVG(t5) as avg_time,
  COUNT(*) as count
FROM log GROUP BY cgi;

-- 最近 1 小时的错误请求
SELECT * FROM log 
WHERE code >= 400 
  AND datetime(date) > datetime('now', '-1 hour')
ORDER BY date DESC;
```

---

## 4. logview

### 功能说明
基于 Web 的日志查看器，提供实时、可视化的日志分析界面。

### 技术栈
- **语言**: TCL
- **框架**: WAPP (https://wapp.tcl.tk)
- **数据库**: SQLite（调用 logtodb 生成）
- **集成**: Fossil SCM sub-CGI

### 部署要求

#### 依赖
1. **wapptclsh** - 静态编译的 WAPP TCL shell
2. **logtodb** - 静态编译的日志转换工具
3. **Fossil** - 用于权限控制和 CGI 托管

#### 构建静态二进制
```bash
# 编译 wapptclsh（需要 TCL 源码）
# 参考：https://wapp.tcl.tk/

# 编译 logtodb
gcc -static -o logtodb logtodb.c sqlite3.c -lpthread -ldl -lm

# 安装到系统路径
sudo cp wapptclsh logtodb /usr/bin/
sudo chmod +x /usr/bin/wapptclsh /usr/bin/logtodb
```

#### 在 Fossil 中配置 sub-CGI
在 Fossil CGI 脚本中添加：
```bash
#!/usr/bin/fossil
repository: /path/to/repo.fossil
extroot: /path/to/extension-scripts
```

#### 安装 logview
```bash
# 复制脚本到扩展目录
cp logview /path/to/extension-scripts/
chmod +x /path/to/extension-scripts/logview

# 确保日志文件可访问
# 如果运行在 chroot 中，路径相对于 chroot 根目录
```

### 使用方法

#### 访问界面
1. 登录到 Fossil 仓库（需要 check-in 权限）
2. 访问 URL：`https://your-site.com/repository/logview`

#### 两种模式

**标准模式** - 查看最近 60 分钟数据
```
https://your-site.com/repository/logview
```

**快速模式** - 查看最近 5 分钟数据（响应更快）
```
https://your-site.com/repository/logview5
```

### 权限控制
只有具有 **check-in 权限**（Fossil 权限字母 `i`）的用户才能访问 logview。未授权用户会被重定向到登录页面。

### 配置选项

#### 修改日志文件路径
编辑 logview 脚本：
```tcl
# 默认路径（chroot 环境相对路径）
exec /usr/bin/logtodb --db /logs/recent.db --logfile /logs/http.log ...

# 修改为你的实际路径
exec /usr/bin/logtodb --db /var/db/logs.db --logfile /var/log/http.log ...
```

#### 调整时间窗口
```tcl
# 标准模式：最近 60 分钟
--tail 50MB --keep 3600

# 快速模式：最近 5 分钟
--tail 5MB --keep 300

# 自定义：最近 24 小时
--tail 500MB --keep 86400
```

### 故障排查

#### 权限问题
```bash
# 检查 wapptclsh 和 logtodb 是否可执行
ls -l /usr/bin/wapptclsh /usr/bin/logtodb

# 检查日志文件权限
ls -l /logs/http.log

# 检查数据库文件权限
ls -l /logs/recent.db
```

#### Chroot 环境
确保所有必需的文件在 chroot 内可访问：
```bash
# 静态编译可避免依赖共享库
ldd /usr/bin/logtodb  # 应显示 "not a dynamic executable"
ldd /usr/bin/wapptclsh
```

#### 调试模式
在 logview 脚本中添加调试输出：
```tcl
puts stderr "Debug: Opening database /logs/recent.db"
puts stderr "Debug: FOSSIL_CAPABILITIES = [wapp-param FOSSIL_CAPABILITIES]"
```

---

## 完整工作流程

### 场景 1：基本日志管理

```bash
# 1. Althttpd 运行，生成日志
althttpd -root /var/www -logfile /var/log/http.log

# 2. 定期轮转日志（crontab）
0 2 * * * /path/to/logrotate.tcl

# 3. 归档的日志文件
/var/log/20260208.log.xz
/var/log/20260207.log.xz
```

### 场景 2：日志分析

```bash
# 1. 转换历史日志到数据库
./logtodb --db analysis.db --logfile /var/log/http.log

# 2. 提取错误代码参考
./extract-error-numbers.tcl > errors.sql
sqlite3 analysis.db < errors.sql

# 3. SQL 查询分析
sqlite3 analysis.db "SELECT code, COUNT(*) FROM log GROUP BY code"
```

### 场景 3：实时监控

```bash
# 1. 启动实时监控（后台运行）
nohup ./logtodb --db /var/db/realtime.db \
                --logfile /var/log/http.log \
                -f --keep 3600 &

# 2. 通过 Web 界面查看
# 访问 https://your-site.com/logview

# 3. 或直接查询数据库
sqlite3 /var/db/realtime.db \
  "SELECT * FROM log WHERE date > datetime('now','-5 minutes')"
```

### 场景 4：问题诊断

```bash
# 1. 查看特定时间段的错误
sqlite3 logs.db <<EOF
SELECT date, ip, url, code, lineno 
FROM log 
WHERE code >= 400 
  AND date BETWEEN '2026-02-08 10:00' AND '2026-02-08 11:00'
ORDER BY date;
EOF

# 2. 根据错误编号查找原因
sqlite3 errors.db "SELECT * FROM xref WHERE rowid = 380"

# 3. 在源码中定位
grep -n "380.*LOG:" althttpd.c
```

---

## 性能建议

### logtodb 性能优化
- 使用 `--tail` 限制处理的数据量
- 使用 `--keep` 自动删除旧数据
- 定期运行 `VACUUM` 压缩数据库
- 对常用查询字段建立索引

### 索引建议
```sql
CREATE INDEX idx_date ON log(date);
CREATE INDEX idx_code ON log(code);
CREATE INDEX idx_domain ON log(domain);
CREATE INDEX idx_cgi ON log(cgi);
```

### 日志轮转建议
- 高流量站点：每天轮转或文件达到 500MB
- 中等流量：每周轮转或文件达到 1GB
- 低流量：每月轮转

---

## 安全注意事项

1. **logview 权限控制**：确保只有授权用户可访问
2. **数据库文件保护**：设置适当的文件权限
3. **日志文件保护**：防止未授权访问敏感信息（IP、User-Agent 等）
4. **定期清理**：不要无限期保留所有日志数据

```bash
# 推荐的文件权限
chmod 600 /var/log/http.log
chmod 600 /var/db/logs.db
chmod 700 /path/to/log-tools
```

---

## 常见问题

### Q: logtodb 显示 "database is locked"
**A**: 多个进程同时访问数据库。使用 WAL 模式：
```sql
PRAGMA journal_mode=WAL;
```

### Q: logview 显示 404
**A**: 检查：
1. logview 是否有执行权限
2. Fossil sub-CGI 配置是否正确
3. 路径是否正确（特别是 chroot 环境）

### Q: 日志轮转后 althttpd 无法写入
**A**: Althttpd 会自动重新打开日志文件，无需重启。如果问题持续，检查文件权限。

### Q: extract-error-numbers.tcl 没有输出
**A**: 确保：
1. 在 althttpd 源码目录运行
2. althttpd.c 文件存在
3. TCL 解释器已安装：`which tclsh`

---

## 版本信息

这些工具是 althttpd 项目的一部分：
- 项目主页：https://sqlite.org/althttpd
- 作者：D. Richard Hipp
- 许可证：Public Domain

---

## 相关资源

- [Althttpd 主文档](../althttpd.md)
- [部署指南](../deployment/)
- [WAPP 框架](https://wapp.tcl.tk)
- [SQLite 文档](https://sqlite.org/docs.html)
- [TCL 教程](https://www.tcl.tk/man/)
