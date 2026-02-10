## UPSERT

插入或更新数据（INSERT ... ON CONFLICT DO UPDATE）
```sql
-- SQLite / PostgreSQL
INSERT INTO users (email, name, age) VALUES ('alice@example.com', 'Alice', 28)
ON CONFLICT(email) DO UPDATE SET name = excluded.name, age = excluded.age;

-- MySQL (8.0+)
INSERT INTO users (email, name, age) VALUES ('alice@example.com', 'Alice', 28)
ON DUPLICATE KEY UPDATE name = VALUES(name), age = VALUES(age);
-- 或 MySQL 8.0.19+
INSERT INTO users (email, name, age) VALUES ('alice@example.com', 'Alice', 28) AS new
ON DUPLICATE KEY UPDATE name = new.name, age = new.age;

-- SQL Server
MERGE INTO users AS target
USING (SELECT 'alice@example.com' AS email, 'Alice' AS name, 28 AS age) AS source
ON target.email = source.email
WHEN MATCHED THEN UPDATE SET name = source.name, age = source.age
WHEN NOT MATCHED THEN INSERT (email, name, age) VALUES (source.email, source.name, source.age);

-- Oracle
MERGE INTO users target
USING (SELECT 'alice@example.com' AS email, 'Alice' AS name, 28 AS age FROM dual) source
ON (target.email = source.email)
WHEN MATCHED THEN UPDATE SET name = source.name, age = source.age
WHEN NOT MATCHED THEN INSERT (email, name, age) VALUES (source.email, source.name, source.age);
```

**HTTP 请求协议**:

UPSERT 实现真正的"有则更新、无则插入"功能。服务器根据表的 PRIMARY KEY 或 UNIQUE 约束自动判断冲突。如果存在冲突，则**只更新** COLUMNS 中指定的列，**未指定的列保持不变**。

单行 UPSERT 示例：
```http
SQTP-UPSERT /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: email, name, age\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 42\n
\n
["alice@example.com", "Alice Johnson", 29]
```

批量 UPSERT 示例：
```http
SQTP-UPSERT /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: email, name, age\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 125\n
\n
[
  ["alice@example.com", "Alice", 28],
  ["bob@example.com", "Bob", 32],
  ["charlie@example.com", "Charlie", 25]
]
```

有条件的 UPSERT（仅当满足条件时才更新）：
```http
SQTP-UPSERT /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: email, name, age\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 42\n
WHERE: age >= 18\n
WHERE-IN: status\n
\n
{"email": "alice@example.com", "name": "Alice", "age": 29, "status": ["active", "verified"]}
```

**请求 Header 字段说明**:
- `TABLE`: 目标表名（必需）
- `COLUMNS`: 要插入/更新的列名（逗号分隔，必需）
- `WHERE`: 额外的更新条件（可选，可多个，彼此为 AND 关系）
  - 仅在有冲突且满足条件时才更新，否则跳过
- `WHERE-IN`: IN 条件（可选，可多个，彼此为 AND 关系）
- `Content-Type`: 请求数据编码格式
  - `application/json; charset=utf-8`: JSON 格式
  - `application/x-www-form-urlencoded`: 表单编码格式
  - `multipart/form-data`: 多部分表单（用于长文本和二进制数据）
- `Content-Length`: 请求体的字节长度（UTF-8 编码）

**请求 Body 数据格式**:

数据顺序必须与 COLUMNS header 中指定的列名顺序一致。

支持三种编码格式（与 INSERT 相同）：
1. **application/json**: 数组格式 `[value1, value2, ...]` 或 `[[...], [...]]` 批量
2. **application/x-www-form-urlencoded**: `value1&value2&...`
3. **multipart/form-data**: 使用数字索引 (0, 1, 2, ...)

**HTTP 应答协议**:

插入新行（无冲突）：
```http
HTTP/1.1 201 Created\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Last-Insert-Id: 12345\n
X-SQTP-Rows-Affected: 1\n
X-SQTP-Action: INSERT\n
X-SQTP-Execution-Time: 0.005\n
Location: /db/main/users/12345\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

更新现有行（有冲突）：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Rows-Affected: 1\n
X-SQTP-Action: UPDATE\n
X-SQTP-Execution-Time: 0.008\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

**UPSERT vs RESET vs INSERT vs UPDATE**:
- **INSERT**: 如果存在冲突则失败
- **UPDATE**: 更新现有行，不存在则失败
- **UPSERT**: 存在则更新指定列（其他列保持不变），不存在则插入
- **RESET**: 存在则删除整行后重新插入（所有未指定列变为默认值），不存在则插入

**HTTP 状态码定义**:
- `201 Created`: 插入新行成功（无冲突）
- `200 OK`: 更新成功（有冲突）
- `400 Bad Request`: 请求格式错误、数据类型错误
- `409 Conflict`: 表没有定义 PRIMARY KEY 或 UNIQUE 约束（无法判断冲突）
- `422 Unprocessable Entity`: 数据验证失败
- `500 Internal Server Error`: 数据库内部错误

**响应 Header 字段说明**:
- `X-SQTP-Last-Insert-Id`: 最后插入行的 ROWID（仅新插入时）
- `X-SQTP-Rows-Affected`: 受影响的行数（1 表示插入或更新）
- `X-SQTP-Action`: 执行的动作（INSERT 或 UPDATE）
- `Location`: 新创建资源的 URI（单行插入时）

**特别说明**:
1. **自动冲突检测**：
   - 服务器根据表的 PRIMARY KEY 或 UNIQUE 约束自动判断冲突
   - 不需要客户端指定冲突字段
   - 如果表没有 PRIMARY KEY 或 UNIQUE 约束，返回 409 错误

2. **UPSERT 的行为**：
   - 无冲突：插入新行（返回 201）
   - 有冲突：只更新 COLUMNS 中指定的字段（返回 200）
   - 未在 COLUMNS 中的字段保持不变

3. **与 RESET 的区别**：
   - **UPSERT**：只更新指定列，其他列保持不变（推荐）
   - **RESET**：删除整行后重新插入，未指定列变为默认值

**字符编码处理**:
与 INSERT 相同：
1. 所有文本数据必须使用 UTF-8 编码
2. Content-Length 计算的是 UTF-8 字节数
3. JSON 字符串中的特殊字符需要转义
4. 二进制数据（BLOB）使用 Base64 编码，前缀 `base64:`

---


