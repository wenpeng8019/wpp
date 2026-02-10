# SQTP/1.0 Protocol Specification
> Structured Query Transfer Protocol Version 1.0

## 概述
SQTP (Structured Query Transfer Protocol) 是一种基于 HTTP 的结构化查询传输协议，将 SQL 数据操作命令映射为 HTTP 方法，使数据库查询和操作能够通过标准 HTTP 协议进行传输。

本规范定义了 SQTP/1.0 协议的完整语法、语义和行为，包括请求/响应格式、HTTP 头字段、数据编码规则和错误处理机制。

## 协议目标与边界

### 设计目标
SQTP 专注于**数据的访问和操作**，提供简洁、高效的数据查询和修改能力。协议设计遵循以下原则：

1. **数据为中心**：所有操作围绕数据的 CRUD（创建、读取、更新、删除）展开
2. **HTTP 原生**：充分利用 HTTP 协议的语义和基础设施
3. **开发者友好**：简单直观的 API，易于理解和使用
4. **类型安全**：明确的数据类型和编码规范
5. **可扩展性**：支持多种数据格式和视图方式

### 协议边界

**协议包含**（Data Access & Manipulation）：
- ✅ 数据查询：SELECT
- ✅ 数据插入：INSERT
- ✅ 数据更新：UPDATE, UPSERT, RESET
- ✅ 数据删除：DELETE
- ✅ 事务控制：BEGIN, COMMIT, ROLLBACK, SAVEPOINT
- ✅ 基本查询条件：WHERE, ORDER BY, LIMIT, OFFSET, GROUP BY
- ✅ 表连接：JOIN (LEFT, RIGHT, INNER, etc.)
- ✅ 数据基础设施：CREATE/DROP TABLE, INDEX, TRIGGER（数据操作必需的基础对象）

**协议不包含**（Database Management & Administration）：
- ❌ 数据库创建/删除：CREATE DATABASE, DROP DATABASE
- ❌ 用户管理：CREATE USER, GRANT, REVOKE
- ❌ 权限控制：通过应用层或中间件实现
- ❌ 数据库配置：PRAGMA, SET 等管理命令
- ❌ 性能分析：EXPLAIN, ANALYZE 等诊断工具
- ❌ 数据库维护：VACUUM, REINDEX, OPTIMIZE 等
- ❌ 备份恢复：由数据库管理工具负责
- ❌ Schema 迁移：建议使用专门的迁移工具
- ❌ 视图管理：CREATE VIEW（查询抽象应由应用层实现）

### 设计哲学

SQTP 将**数据操作**与**数据库管理**分离：

- **数据操作**（SQTP 负责）：应用程序日常的数据读写需求及其必需的基础设施
- **数据库管理**（其他工具负责）：DBA 或运维工具执行的管理任务

**关于 Schema 操作的边界**：
- ✅ **TABLE**：数据存储容器，数据操作的基础
- ✅ **INDEX**：查询性能优化，直接服务于数据访问
- ✅ **TRIGGER**：数据操作自动化，业务逻辑的一部分
- ❌ **VIEW**：查询抽象层，建议由应用层实现（API 封装）
- ❌ **DATABASE**：数据库级管理，属于运维范畴

这种分离带来的好处：
1. 协议更简洁、更专注
2. 安全边界更清晰（应用不能执行管理命令）
3. 易于实现和维护
4. 符合最小权限原则

## 协议特性
- **SQL 命令即 HTTP 方法**：SELECT、INSERT、UPDATE、UPSERT、RESET、DELETE 等 SQL 命令直接作为 HTTP 请求方法
- **SQL 子句即 HTTP 头**：WHERE、ORDER-BY、LIMIT 等 SQL 子句通过 HTTP 头字段传递
- **标准 HTTP 语义**：完全兼容 HTTP/1.1 协议，可使用标准 HTTP 客户端和服务器
- **多种数据格式**：支持 JSON、表单编码、多部分表单等多种数据编码方式
- **灵活的视图格式**：支持 object、row、column 三种数据视图格式，满足不同场景需求
- **安全机制**：UPDATE 和 DELETE 必需 WHERE 条件，防止误操作

## 文档目录

### 数据查询（Data Query）
- [01-SELECT.md](./01-SELECT.md) - SELECT 查询操作

### 数据操作（Data Manipulation）
- [02-INSERT.md](./02-INSERT.md) - INSERT 插入操作
- [03-UPDATE.md](./03-UPDATE.md) - UPDATE 更新操作
- [04-UPSERT.md](./04-UPSERT.md) - UPSERT 插入或更新操作
- [05-RESET.md](./05-RESET.md) - RESET 重置操作（DELETE + INSERT）
- [06-DELETE.md](./06-DELETE.md) - DELETE 删除操作

### 数据定义（Data Definition）
- [07-SCHEMA.md](./07-SCHEMA.md) - CREATE TABLE/INDEX/TRIGGER, DROP, ALTER 操作

### 事务控制（Transaction Control）
- [08-TRANSACTIONS.md](./08-TRANSACTIONS.md) - BEGIN/COMMIT/ROLLBACK/SAVEPOINT 操作

## 核心概念

### HTTP 请求行（Request Line）

SQTP 协议使用标准的 HTTP/1.1 协议，通过添加 `SQTP-` 前缀来扩展 HTTP 方法：

```
SQTP-<METHOD> <URI>[#<OBJECT-TYPE>] HTTP/1.1
```

**组成部分**：
- `SQTP-<METHOD>`：SQTP 方法，标准 SQL 操作加 `SQTP-` 前缀
  * 数据操作：`SQTP-SELECT`, `SQTP-INSERT`, `SQTP-UPDATE`, `SQTP-DELETE`, `SQTP-UPSERT`, `SQTP-RESET`
  * Schema 操作：`SQTP-CREATE`, `SQTP-DROP`, `SQTP-ALTER`
  * 事务控制：`SQTP-BEGIN`, `SQTP-COMMIT`, `SQTP-ROLLBACK`, `SQTP-SAVEPOINT`
- `<URI>`：资源路径，标识目标数据库（如 `/mydb` 或 `/path/to/db.sqlite`）
- `#<OBJECT-TYPE>`：对象类型标识（可选），用于 Schema 操作（CREATE, DROP, ALTER）
  * `#table` - 表对象
  * `#index` - 索引对象
  * `#trigger` - 触发器对象
- `HTTP/1.1`：协议版本，使用标准 HTTP/1.1

**数据操作示例**（DML）：
```http
SQTP-SELECT /mydb HTTP/1.1
SQTP-INSERT /users.db HTTP/1.1
SQTP-UPDATE /data/store.sqlite HTTP/1.1
SQTP-DELETE /mydb HTTP/1.1
```

**Schema 操作示例**（DDL）：
```http
SQTP-CREATE /mydb#table HTTP/1.1
SQTP-CREATE /mydb#index HTTP/1.1
SQTP-CREATE /mydb#trigger HTTP/1.1
SQTP-DROP /mydb#table HTTP/1.1
SQTP-ALTER /mydb#table HTTP/1.1
```

**事务控制示例**：
```http
SQTP-BEGIN /mydb HTTP/1.1
SQTP-COMMIT /mydb HTTP/1.1
SQTP-ROLLBACK /mydb HTTP/1.1
SQTP-SAVEPOINT /mydb HTTP/1.1
```

**设计说明**：
- **为什么用 HTTP/1.1 而不是 SQTP/1.0**：浏览器标准 API（Fetch, XMLHttpRequest）只支持 HTTP 协议
- **SQTP- 前缀的作用**：
  * 明确标识 SQTP 协议请求
  * 避免与标准 HTTP 方法冲突
  * 保持协议扩展性（如 SQTP2-SELECT）
- **URI 标识目标数据库**：具体格式由服务器实现决定
- **数据操作（DML）**：目标表通过 `FROM` header 指定
- **Schema 操作（DDL）**：对象类型通过 URI Fragment (`#object-type`) 指定，对象名通过 `NAME` header 指定
- **查询条件**：WHERE 子句等通过 HTTP Headers 传递，保持 URI 简洁

### 通用 Header 字段

**数据操作（DML）**:
- `TABLE`: 目标表名
- `WHERE`: WHERE 条件（可多个，AND 关系）
- `WHERE-IN`: IN 条件（可多个，AND 关系）
- `COLUMNS`: 列名列表
- `Content-Type`: 请求 body 编码格式
- `X-SQTP-Protocol`: 协议版本（SQTP/1.0）

**Schema 操作（DDL）**:
- URI Fragment `#`: 对象类型（table, index, trigger）
- `NAME`: 对象名称
- `IF-NOT-EXISTS`: 创建时如果已存在则忽略
- `IF-EXISTS`: 删除时如果不存在则忽略
- `ACTION`: ALTER 操作类型（RENAME, ADD-COLUMN, DROP-COLUMN, RENAME-COLUMN）

### WHERE 和 WHERE-IN 规则
1. **多个 WHERE**：彼此为 AND 关系
   ```http
   WHERE: age >= 18\n
   WHERE: created_at > '2025-01-01'\n
   ```
   等价于：`WHERE age >= 18 AND created_at > '2025-01-01'`

2. **多个 WHERE-IN**：彼此为 AND 关系
   ```http
   WHERE-IN: status\n
   WHERE-IN: role\n
   ```
   Body: `{"status": ["active"], "role": ["admin"]}`
   
   等价于：`WHERE status IN ('active') AND role IN ('admin')`

3. **混用**：WHERE 和 WHERE-IN 也是 AND 关系

### 数据编码格式
- **application/json**: JSON 格式（推荐）
- **application/x-www-form-urlencoded**: URL 编码表单
- **multipart/form-data**: 多部分表单（适合长文本和二进制）

### 视图格式（仅 SELECT）
- **object**: `[{col: val}, ...]` - 对象数组（默认）
- **row**: `[[val, val, ...], ...]` - 行数组
- **column**: `[[col_vals], [col_vals], ...]` - 列数组

## 快速开始

查看 [QUICKSTART.md](../QUICKSTART.md) 获取快速入门指南。

## 安全特性

1. **防误删除/更新**：UPDATE 和 DELETE 必需 WHERE 条件
   - 全表操作使用 `WHERE: *` 明确表示
   
2. **LIMIT 支持**：限制单次操作行数

3. **外键约束检查**：保护数据完整性

4. **事务支持**：确保数据一致性

## 版本历史

- **v1.0** (2026-02-09)
  - 初始版本发布
  - 支持 SELECT, INSERT, UPDATE, UPSERT, RESET, DELETE
  - 统一 WHERE 和 WHERE-IN 支持
  - 统一使用 TABLE header（替代 INTO）
