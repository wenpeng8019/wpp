## Schema Operations

数据定义语言（DDL）操作，用于创建和管理数据表、索引、触发器等数据库对象。

**注意**：根据 SQTP 协议边界定义，仅支持与数据操作直接相关的基础 Schema 操作：
- ✅ TABLE：数据容器
- ✅ INDEX：查询性能优化
- ✅ TRIGGER：数据操作自动化
- ❌ VIEW：查询抽象，建议使用应用层封装
- ❌ DATABASE：数据库管理，由 DBA 工具负责

---

## CREATE TABLE

创建新表

```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT UNIQUE NOT NULL,
    age INTEGER CHECK(age >= 0),
    status TEXT DEFAULT 'active',
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS users (...);
CREATE TEMPORARY TABLE temp_users (...);
CREATE TABLE config (key TEXT PRIMARY KEY, value TEXT) WITHOUT ROWID;
```

**HTTP 请求协议**:

基本示例：
```http
SQTP-CREATE /db/main#table HTTP/1.1\n
NAME: users\n
IF-NOT-EXISTS: true\n
COLUMN: id INTEGER\n
COLUMN: name TEXT\n
COLUMN: email TEXT\n
COLUMN: age INTEGER DEFAULT 0 CHECK age >= 0\n
COLUMN: status TEXT DEFAULT 'active'\n
COLUMN: created_at TEXT DEFAULT CURRENT_TIMESTAMP\n
PRIMARY-KEY: id\n
NOT-NULL: name, email\n
UNIQUE: email\n
AUTOINC: id\n
\n
```

临时表示例：
```http
SQTP-CREATE /db/main#table HTTP/1.1\n
NAME: temp_session\n
TYPE: temporary\n
COLUMN: session_id TEXT\n
COLUMN: user_id INTEGER\n
COLUMN: created_at TEXT DEFAULT CURRENT_TIMESTAMP\n
PRIMARY-KEY: session_id\n
NOT-NULL: session_id, user_id\n
\n
```

复杂示例（复合主键、外键、WITHOUT ROWID）：
```http
SQTP-CREATE /db/main#table HTTP/1.1\n
NAME: order_items\n
COLUMN: order_id INTEGER\n
COLUMN: product_id INTEGER\n
COLUMN: quantity INTEGER DEFAULT 1 CHECK quantity > 0\n
COLUMN: price REAL\n
PRIMARY-KEY: order_id, product_id\n
NOT-NULL: order_id, product_id, price\n
FOREIGN-KEY: order_id REFERENCES orders(id) ON DELETE CASCADE ON UPDATE CASCADE\n
FOREIGN-KEY: product_id REFERENCES products(id)\n
WITHOUT-ROWID: true\n
\n
```

**请求格式**:
- URI Fragment: `#table`（必需）
- Header `NAME`: 表名（必需）
- Header `IF-NOT-EXISTS`: 如果表已存在则忽略（可选，默认 false）
- Header `TYPE`: 表类型（可选）
  * `temporary`: 临时表，会话结束后自动删除
  * `memory`: 内存表，数据存储在内存中
- Header `WITHOUT-ROWID`: 是否使用无 ROWID 优化（可选，默认 false）
  * `true`: 不创建内部 rowid 列，要求必须有 PRIMARY KEY
  * 适用场景：主键不是整数、表数据量大、需要节省存储空间
- Header `COLUMN`: 列定义（可多个），格式：`name type [constraint...]`
  * `name`: 列名
  * `type`: 数据类型（INTEGER, TEXT, REAL, BLOB, NUMERIC）
  * 约束（可选）：`DEFAULT value`, `CHECK expression`
- Header `PRIMARY-KEY`: 主键列（逗号分隔，支持复合主键）
- Header `NOT-NULL`: 非空约束列（逗号分隔，可多列）
- Header `UNIQUE`: 唯一约束列（可多个 header，每个可包含逗号分隔的列名）
- Header `AUTOINC`: 自增列（仅支持单列 INTEGER PRIMARY KEY）
- Header `FOREIGN-KEY`: 外键约束（可多个），格式：`column REFERENCES table(column) [ON DELETE action] [ON UPDATE action]`
  * `ON DELETE`: CASCADE | SET NULL | RESTRICT | NO ACTION
  * `ON UPDATE`: CASCADE | SET NULL | RESTRICT | NO ACTION

**COLUMN 格式说明**:

基本格式：`COLUMN: name type`
```
COLUMN: id INTEGER
COLUMN: name TEXT
```

带约束：`COLUMN: name type constraint1 constraint2 ...`
```
COLUMN: age INTEGER DEFAULT 0        # 默认值
COLUMN: price REAL DEFAULT 0.0
COLUMN: status TEXT DEFAULT 'active' CHECK status IN ('active', 'inactive')
COLUMN: email TEXT                    # 非空约束用 NOT-NULL header
```

**约束类型**:
- `DEFAULT value`: 默认值（字符串需加引号）
- `CHECK expression`: 检查约束

**表级约束** (通过独立 header):
- `NOT-NULL: col1, col2`: 非空约束
- `PRIMARY-KEY: col1[, col2]`: 主键约束
- `UNIQUE: col1[, col2]`: 唯一约束（可多个独立 UNIQUE header）
- `AUTOINC: col`: 自增约束（仅限单列 INTEGER PRIMARY KEY）
- `FOREIGN-KEY: col REFERENCES table(col) [ON DELETE action] [ON UPDATE action]`: 外键约束
- `WITHOUT-ROWID: true`: 无 ROWID 表（要求必须有 PRIMARY KEY）

**注意事项**:
1. AUTOINC 只能用于单列 INTEGER PRIMARY KEY
2. WITHOUT-ROWID 表必须定义 PRIMARY KEY
3. CHECK 约束表达式不需要括号
4. DEFAULT 值中的字符串需要加引号

**HTTP 应答协议**:

成功应答：
```http
HTTP/1.1 201 Created\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Execution-Time: 0.015\n
Location: /db/main/tables/users\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

表已存在（IF-NOT-EXISTS: true）：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Action: SKIPPED\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

**HTTP 状态码**:
- `201 Created`: 表创建成功
- `200 OK`: 表已存在（IF-NOT-EXISTS: true）
- `400 Bad Request`: 请求格式错误、类型不支持
- `409 Conflict`: 表已存在（IF-NOT-EXISTS: false）
- `500 Internal Server Error`: 数据库内部错误

---

## CREATE INDEX

创建索引以优化查询性能

```sql
CREATE INDEX idx_users_email ON users(email);
CREATE UNIQUE INDEX idx_users_username ON users(username);
CREATE INDEX idx_users_status_created ON users(status, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_name ON table_name(column);
CREATE INDEX idx_users_lower_email ON users(lower(email));  -- 表达式索引
CREATE INDEX idx_active_users ON users(email) WHERE status = 'active';  -- 部分索引
```

**HTTP 请求协议**:

基本示例：
```http
SQTP-CREATE /db/main#index HTTP/1.1\n
NAME: idx_users_email\n
TABLE: users\n
COLUMN: email\n
\n
```

唯一索引示例：
```http
SQTP-CREATE /db/main#index HTTP/1.1\n
NAME: idx_users_username\n
TABLE: users\n
UNIQUE: true\n
COLUMN: username\n
\n
```

多列索引示例：
```http
SQTP-CREATE /db/main#index HTTP/1.1\n
NAME: idx_users_status_created\n
TABLE: users\n
COLUMN: status ASC\n
COLUMN: created_at DESC\n
\n
```

部分索引示例（SQLite 3.8.0+）：
```http
SQTP-CREATE /db/main#index HTTP/1.1\n
NAME: idx_active_users_email\n
TABLE: users\n
COLUMN: email\n
WHERE: status = 'active'\n
\n
```

表达式索引示例：
```http
SQTP-CREATE /db/main#index HTTP/1.1\n
NAME: idx_users_lower_email\n
TABLE: users\n
COLUMN: lower(email)\n
\n
```

**请求格式**:
- URI Fragment: `#index`（必需）
- Header `NAME`: 索引名（必需）
- Header `TABLE`: 表名（必需）
- Header `IF-NOT-EXISTS`: 如果索引已存在则忽略（可选，默认 false）
- Header `UNIQUE`: 唯一索引（可选，默认 false）
- Header `COLUMN`: 索引列或表达式（可多个，按顺序）
  * 列名格式：`name [ASC|DESC]`
  * 表达式格式：`expression [ASC|DESC]`
  * 默认排序：ASC
- Header `WHERE`: 部分索引条件（可选，仅 SQLite 3.8.0+）

**COLUMN 格式说明**:

单列索引：
```
COLUMN: email
COLUMN: created_at DESC
```

多列索引（多个 COLUMN header）：
```
COLUMN: status ASC
COLUMN: created_at DESC
```

表达式索引：
```
COLUMN: lower(email)
COLUMN: json_extract(data, '$.id')
COLUMN: substr(name, 1, 10)
```

**使用场景**:
- **普通索引**: 加速查询
- **唯一索引**: 保证数据唯一性
- **复合索引**: 多列组合查询优化
- **部分索引**: 仅索引满足条件的行，节省空间
- **表达式索引**: 对计算结果建索引（如大小写转换、JSON 提取）

**HTTP 应答协议**:

成功应答：
```http
HTTP/1.1 201 Created\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Execution-Time: 0.120\n
Location: /db/main/indexes/idx_users_email\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

**HTTP 状态码**:
- `201 Created`: 索引创建成功
- `200 OK`: 索引已存在（IF-NOT-EXISTS: true）
- `400 Bad Request`: 请求格式错误
- `404 Not Found`: 表不存在
- `409 Conflict`: 索引已存在
- `500 Internal Server Error`: 数据库内部错误

---

## CREATE TRIGGER

创建触发器以自动化数据操作逻辑

```sql
CREATE TRIGGER update_timestamp
AFTER UPDATE ON users
FOR EACH ROW
BEGIN
    UPDATE users SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS validate_age
BEFORE INSERT ON users
FOR EACH ROW
WHEN NEW.age < 0
BEGIN
    SELECT RAISE(ABORT, 'Age cannot be negative');
END;

CREATE TRIGGER cascade_delete
AFTER DELETE ON users
FOR EACH ROW
BEGIN
    DELETE FROM orders WHERE user_id = OLD.id;
    DELETE FROM sessions WHERE user_id = OLD.id;
END;

CREATE TRIGGER audit_update
AFTER UPDATE OF email, phone ON users
FOR EACH ROW
BEGIN
    INSERT INTO audit_log (table_name, record_id, action, changed_at)
    VALUES ('users', NEW.id, 'UPDATE', CURRENT_TIMESTAMP);
END;
```

**HTTP 请求协议**:

基本示例（更新时间戳）：
```http
SQTP-CREATE /db/main#trigger HTTP/1.1\n
NAME: update_timestamp\n
TABLE: users\n
TIMING: AFTER\n
EVENT: UPDATE\n
ACTION: UPDATE users SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id\n
\n
```

验证触发器示例：
```http
SQTP-CREATE /db/main#trigger HTTP/1.1\n
NAME: validate_age\n
TABLE: users\n
TIMING: BEFORE\n
EVENT: INSERT\n
WHEN: NEW.age < 0\n
ACTION: RAISE ABORT 'Age cannot be negative'\n
\n
```

复杂示例（多个动作）：
```http
SQTP-CREATE /db/main#trigger HTTP/1.1\n
NAME: cascade_delete\n
TABLE: users\n
TIMING: AFTER\n
EVENT: DELETE\n
ACTION: DELETE FROM orders WHERE user_id = OLD.id\n
ACTION: DELETE FROM sessions WHERE user_id = OLD.id\n
ACTION: INSERT INTO audit_log (action, record_id) VALUES ('DELETE', OLD.id)\n
\n
```

UPDATE OF 示例（仅特定列更新时触发）：
```http
SQTP-CREATE /db/main#trigger HTTP/1.1\n
NAME: audit_update\n
TABLE: users\n
TIMING: AFTER\n
EVENT: UPDATE\n
UPDATE-OF: email, phone\n
ACTION: INSERT INTO audit_log (table_name, record_id, action) VALUES ('users', NEW.id, 'UPDATE')\n
\n
```

**请求格式**:
- URI Fragment: `#trigger`（必需）
- Header `NAME`: 触发器名（必需）
- Header `TABLE`: 触发的表名（必需）
- Header `TIMING`: 触发时机（必需）- `BEFORE`, `AFTER`, `INSTEAD OF`
- Header `EVENT`: 触发事件（必需）- `INSERT`, `UPDATE`, `DELETE`
- Header `IF-NOT-EXISTS`: 如果触发器已存在则忽略（可选，默认 false）
- Header `FOR-EACH-ROW`: 行级触发器（可选，默认 true）
- Header `WHEN`: 触发条件（可选）
- Header `UPDATE-OF`: 仅当指定列更新时触发（可选，逗号分隔，仅 UPDATE 事件）
- Header `ACTION`: 触发器执行的 SQL 语句（可多个，按顺序执行）

**ACTION 格式说明**:

支持的 SQL 语句类型：

1. **UPDATE 语句**:
```
ACTION: UPDATE table SET column = value WHERE condition
ACTION: UPDATE users SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id
ACTION: UPDATE orders SET status = 'cancelled' WHERE user_id = OLD.id
```

2. **INSERT 语句**:
```
ACTION: INSERT INTO table (columns) VALUES (values)
ACTION: INSERT INTO audit_log (action, user_id) VALUES ('INSERT', NEW.id)
ACTION: INSERT INTO history SELECT * FROM users WHERE id = NEW.id
```

3. **DELETE 语句**:
```
ACTION: DELETE FROM table WHERE condition
ACTION: DELETE FROM sessions WHERE user_id = OLD.id
ACTION: DELETE FROM cache WHERE key LIKE 'user:' || OLD.id || '%'
```

4. **RAISE 语句**（中止操作并返回错误）:
```
ACTION: RAISE ABORT 'error message'
ACTION: RAISE FAIL 'constraint violation'
ACTION: RAISE ROLLBACK 'transaction rolled back'
ACTION: RAISE IGNORE
```

**触发器上下文**:
- `NEW.*`: 访问新插入或更新后的值（INSERT, UPDATE）
- `OLD.*`: 访问删除或更新前的值（DELETE, UPDATE）
- 触发器内可以使用完整的 SQL 表达式和函数

**注意事项**:
1. TIMING 为 `INSTEAD OF` 仅用于视图（SQTP 不支持视图，保留用于兼容性）
2. `UPDATE-OF` 仅在 EVENT 为 UPDATE 时有效
3. `FOR-EACH-ROW` 在 SQLite 中默认为 true（行级触发器）
4. 多个 ACTION header 按声明顺序依次执行
5. RAISE 会中止当前操作，后续 ACTION 不会执行

**HTTP 应答协议**:

成功应答：
```http
HTTP/1.1 201 Created\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Execution-Time: 0.025\n
Location: /db/main/triggers/update_timestamp\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

触发器已存在（IF-NOT-EXISTS: true）：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Action: SKIPPED\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

**HTTP 状态码**:
- `201 Created`: 触发器创建成功
- `200 OK`: 触发器已存在（IF-NOT-EXISTS: true）
- `400 Bad Request`: 请求格式错误、TIMING/EVENT 不支持、ACTION 语法错误
- `404 Not Found`: 表不存在
- `409 Conflict`: 触发器已存在（IF-NOT-EXISTS: false）
- `500 Internal Server Error`: 数据库内部错误

---

## DROP TABLE

删除表

```sql
DROP TABLE users;
DROP TABLE IF EXISTS users;
```

**HTTP 请求协议**:

```http
SQTP-DROP /db/main#table HTTP/1.1\n
NAME: users\n
IF-EXISTS: true\n
\n
```

**请求格式**:
- URI Fragment: `#table`（必需）
- Header `NAME`: 表名（必需）
- Header `IF-EXISTS`: 如果表不存在则忽略（可选，默认 false）

**HTTP 应答协议**:

成功应答：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Execution-Time: 0.008\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

表不存在（IF-EXISTS: true）：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Action: SKIPPED\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

**HTTP 状态码**:
- `200 OK`: 删除成功或表不存在（IF-EXISTS: true）
- `404 Not Found`: 表不存在（IF-EXISTS: false）
- `409 Conflict`: 外键约束阻止删除
- `500 Internal Server Error`: 数据库内部错误

---

## DROP INDEX

删除索引

```sql
DROP INDEX idx_users_email;
DROP INDEX IF EXISTS idx_users_email;
```

**HTTP 请求协议**:

```http
SQTP-DROP /db/main#index HTTP/1.1\n
NAME: idx_users_email\n
IF-EXISTS: true\n
\n
```

**请求格式**:
- URI Fragment: `#index`（必需）
- Header `NAME`: 索引名（必需）
- Header `IF-EXISTS`: 如果索引不存在则忽略（可选，默认 false）

**HTTP 应答协议**:

成功应答：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Execution-Time: 0.005\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

**HTTP 状态码**:
- `200 OK`: 删除成功或索引不存在（IF-EXISTS: true）
- `404 Not Found`: 索引不存在（IF-EXISTS: false）
- `500 Internal Server Error`: 数据库内部错误

---

## DROP TRIGGER

删除触发器

```sql
DROP TRIGGER update_timestamp;
DROP TRIGGER IF EXISTS update_timestamp;
```

**HTTP 请求协议**:

```http
SQTP-DROP /db/main#trigger HTTP/1.1\n
NAME: update_timestamp\n
IF-EXISTS: true\n
\n
```

**请求格式**:
- URI Fragment: `#trigger`（必需）
- Header `NAME`: 触发器名（必需）
- Header `IF-EXISTS`: 如果触发器不存在则忽略（可选，默认 false）

**HTTP 应答协议**:

成功应答：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Execution-Time: 0.006\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

**HTTP 状态码**:
- `200 OK`: 删除成功或触发器不存在（IF-EXISTS: true）
- `404 Not Found`: 触发器不存在（IF-EXISTS: false）
- `500 Internal Server Error`: 数据库内部错误

---

## ALTER TABLE

修改表结构（SQLite 功能有限）

```sql
ALTER TABLE users RENAME TO old_users;
ALTER TABLE users ADD COLUMN phone TEXT;
ALTER TABLE users ADD COLUMN age INTEGER DEFAULT 0 NOT NULL;
ALTER TABLE users DROP COLUMN age;  -- SQLite 3.35.0+
ALTER TABLE users RENAME COLUMN old_name TO new_name;  -- SQLite 3.25.0+
```

**HTTP 请求协议**:

重命名表：
```http
SQTP-ALTER /db/main#table HTTP/1.1\n
NAME: users\n
ACTION: RENAME-TABLE\n
NEW-NAME: old_users\n
\n
```

添加列（简单）：
```http
SQTP-ALTER /db/main#table HTTP/1.1\n
NAME: users\n
ACTION: ADD-COLUMN\n
COLUMN: phone TEXT\n
\n
```

添加列（带约束）：
```http
SQTP-ALTER /db/main#table HTTP/1.1\n
NAME: users\n
ACTION: ADD-COLUMN\n
COLUMN: age INTEGER DEFAULT 0\n
NOT-NULL: age\n
\n
```

重命名列：
```http
SQTP-ALTER /db/main#table HTTP/1.1\n
NAME: users\n
ACTION: RENAME-COLUMN\n
COLUMN: old_name\n
NEW-NAME: new_name\n
\n
```

删除列：
```http
SQTP-ALTER /db/main#table HTTP/1.1\n
NAME: users\n
ACTION: DROP-COLUMN\n
COLUMN: age\n
\n
```

**请求格式**:
- URI Fragment: `#table`（必需，目前仅支持 table）
- Header `NAME`: 表名（必需）
- Header `ACTION`: 操作类型（必需）
  * `RENAME-TABLE`: 重命名表
  * `ADD-COLUMN`: 添加列
  * `RENAME-COLUMN`: 重命名列
  * `DROP-COLUMN`: 删除列（SQLite 3.35.0+）

**各操作所需 Header**:

1. **RENAME-TABLE**:
   - `NEW-NAME`: 新表名（必需）

2. **ADD-COLUMN**:
   - `COLUMN`: 列定义，格式：`name type [constraint...]`（必需）
   - `NOT-NULL`: 如果新列为非空（可选）
   - 约束支持：`DEFAULT value`, `CHECK expression`
   - 注意：SQLite 不支持添加带 PRIMARY KEY 或 UNIQUE 的列

3. **RENAME-COLUMN** (SQLite 3.25.0+):
   - `COLUMN`: 旧列名（必需）
   - `NEW-NAME`: 新列名（必需）

4. **DROP-COLUMN** (SQLite 3.35.0+):
   - `COLUMN`: 要删除的列名（必需）

**SQLite 限制**:
1. ADD-COLUMN 不支持：
   - PRIMARY KEY 约束
   - UNIQUE 约束
   - FOREIGN KEY 约束
   - 带 NOT NULL 且无 DEFAULT 的列
2. DROP-COLUMN 需要 SQLite 3.35.0+
3. RENAME-COLUMN 需要 SQLite 3.25.0+
4. 不支持修改列类型、添加/删除约束等复杂操作

**HTTP 应答协议**:

成功应答：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Execution-Time: 0.012\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

**HTTP 状态码**:
- `200 OK`: 修改成功
- `400 Bad Request`: 不支持的操作或 SQLite 版本过低、约束不支持
- `404 Not Found`: 表或列不存在
- `409 Conflict`: 列已存在（ADD-COLUMN）、新名称已存在（RENAME-*）
- `500 Internal Server Error`: 数据库内部错误

---

## 安全考虑

1. **权限控制**：Schema 操作应有严格的权限限制
2. **命名规范**：表名、索引名、触发器名应符合命名规则
3. **资源限制**：限制单个表的列数、索引数
4. **审计日志**：记录所有 Schema 变更操作
5. **备份建议**：Schema 变更前建议备份数据库

