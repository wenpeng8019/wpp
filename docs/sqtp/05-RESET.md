## RESET

重置数据（INSERT OR REPLACE）
```sql
-- SQLite
REPLACE INTO users (email, name, age) VALUES ('alice@example.com', 'Alice', 28);
-- 或
INSERT OR REPLACE INTO users (email, name, age) VALUES ('alice@example.com', 'Alice', 28);

-- MySQL
REPLACE INTO users (email, name, age) VALUES ('alice@example.com', 'Alice', 28);

-- PostgreSQL （不支持 REPLACE，但可以用 ON CONFLICT DO UPDATE 模拟）
INSERT INTO users (email, name, age) VALUES ('alice@example.com', 'Alice', 28)
ON CONFLICT(email) DO UPDATE SET 
  email = EXCLUDED.email,
  name = EXCLUDED.name, 
  age = EXCLUDED.age;
-- 注意：这会更新所有字段，但不会删除重建，ROWID 不变

-- SQL Server / Oracle（不支持 REPLACE，需要先 DELETE 再 INSERT）
DELETE FROM users WHERE email = 'alice@example.com';
INSERT INTO users (email, name, age) VALUES ('alice@example.com', 'Alice', 28);
```

**HTTP 请求协议**:

RESET 是 SQLite 的 `INSERT OR REPLACE` 命令。如果存在冲突（违反 PRIMARY KEY 或 UNIQUE 约束），则**删除旧行并插入新行**（重置整行），所有未指定的列将变为默认值。

单行重置示例：
```http
SQTP-RESET /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: name, email, age, status\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 52\n
WHERE: email = 'alice@example.com'\n
\n
["Alice Johnson", "alice@example.com", 28, "active"]
```

批量重置示例：
```http
SQTP-RESET /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: name, email, age\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 125\n
\n
[
  ["Alice", "alice@example.com", 28],
  ["Bob", "bob@example.com", 32],
  ["Charlie", "charlie@example.com", 25]
]
```

有条件的 RESET（仅当满足条件时才重置）：
```http
SQTP-RESET /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: email, name, age\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 80\n
WHERE: last_login < '2025-01-01'\n
WHERE-IN: status\n
\n
{"email": "alice@example.com", "name": "Alice", "age": 28, "status": ["inactive", "suspended"]}
```

使用表单编码：
```http
SQTP-RESET /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: name, email, age, status\n
Content-Type: application/x-www-form-urlencoded; charset=utf-8\n
Content-Length: 42\n
\n
Alice&alice%40example.com&28&active
```

使用 multipart（长文本）：
```http
SQTP-RESET /db/main HTTP/1.1\n
TABLE: articles\n
COLUMNS: title, content, author\n
Content-Type: multipart/form-data; boundary=----Boundary789\n
Content-Length: 856\n
\n
------Boundary789\r
Content-Disposition: form-data; name="0"\r
\r
Article Title\r
------Boundary789\r
Content-Disposition: form-data; name="1"\r
Content-Type: text/plain; charset=utf-8\r
\r
This is the article content.

It can be very long with:
- Multiple paragraphs
- Special characters
- Unicode content
- Formatting preserved\r
------Boundary789\r
Content-Disposition: form-data; name="2"\r
\r
Alice Johnson\r
------Boundary789--\r
```

**请求 Header 字段说明**:
- `TABLE`: 目标表名（必需）
- `COLUMNS`: 要重置的列名（逗号分隔，必需）
- `WHERE`: 重置条件（可选，可多个，彼此为 AND 关系）
  - 用于明确指定要重置的记录
- `WHERE-IN`: IN 条件（可选，可多个，彼此为 AND 关系）
- `Content-Type`: 请求数据编码格式
  - `application/json; charset=utf-8`: JSON 格式
  - `application/x-www-form-urlencoded`: 表单编码格式
  - `multipart/form-data`: 多部分表单（用于长文本和二进制数据）
- `Content-Length`: 请求体的字节长度（UTF-8 编码）

**请求 Body 数据格式**:

数据顺序必须与 COLUMNS header 中指定的列名顺序一致。

与 INSERT 相同，支持三种编码格式：
1. **application/json**: 数组格式 `[value1, value2, ...]` 或 `[[...], [...]]` 批量
2. **application/x-www-form-urlencoded**: `value1&value2&...`
3. **multipart/form-data**: 使用数字索引 (0, 1, 2, ...)

**RESET vs UPSERT vs INSERT vs UPDATE**:
- **INSERT**: 如果存在冲突则失败（或根据 ON-CONFLICT 策略处理）
- **UPDATE**: 更新现有行（必需 WHERE 条件）
- **UPSERT**: 存在则更新指定列（其他列保持不变），不存在则插入
- **RESET**: 如果存在冲突则删除旧行并插入新行（重置整行，未指定列变为默认值）

**请求 Body 数据格式**:

与 INSERT 和 UPDATE 相同，支持三种编码格式：
1. **application/json**
2. **application/x-www-form-urlencoded**
3. **multipart/form-data**

**HTTP 应答协议**:

插入新行（无冲突）：
```http
HTTP/1.1 201 Created\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 128\n
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

替换旧行（有冲突）：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Last-Insert-Id: 12345\n
X-SQTP-Rows-Affected: 2\n
X-SQTP-Action: RESET\n
X-SQTP-Rows-Deleted: 1\n
X-SQTP-Execution-Time: 0.008\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

批量重置成功应答：
```http
HTTP/1.1 200 OK\n
Content-Type: application/json; charset=utf-8\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Last-Insert-Id: 12347\n
X-SQTP-Rows-Affected: 5\n
X-SQTP-Execution-Time: 0.015\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
[12345, 12346, 12347]
```

说明：返回每行插入或重置的 ID 列表，顺序与请求中的数据顺序对应。

错误应答示例：
```http
HTTP/1.1 400 Bad Request\n
Content-Type: text/plain; charset=utf-8\n
Content-Length: 51\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Error-Code: 1\n
X-SQTP-Error-Type: SQLITE_ERROR\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
Cannot convert 'abc' to INTEGER for column 'age'
```

**HTTP 状态码定义**:
- `201 Created`: 插入新行成功（无冲突）
- `200 OK`: 重置成功（有冲突，删除旧行并插入新行）
- `400 Bad Request`: 请求格式错误、数据类型错误
- `413 Payload Too Large`: 请求体过大
- `415 Unsupported Media Type`: 不支持的 Content-Type
- `422 Unprocessable Entity`: 数据验证失败
- `500 Internal Server Error`: 数据库内部错误

**响应 Header 字段说明**:
- `X-SQTP-Last-Insert-Id`: 最后插入行的 ROWID（自增 ID）
- `X-SQTP-Rows-Affected`: 受影响的行数（删除+插入，通常是 1 或 2）
- `X-SQTP-Action`: 执行的动作（INSERT 或 RESET）
- `X-SQTP-Rows-Deleted`: 被删除的行数（仅 RESET 时）
- `Location`: 新创建资源的 URI（单行插入时）

**特别说明**:
1. **RESET 操作分两步**：
   - 如果有冲突，先删除旧行（触发 DELETE 触发器）
   - 然后插入新行（触发 INSERT 触发器）
   - rowsAffected = 2 表示删除了 1 行并插入了 1 行

2. **与 SQLite REPLACE 的对应关系**：
   - HTTP 的 RESET 命令对应 SQLite 的 `INSERT OR REPLACE` 或 `REPLACE INTO`
   - 命名为 RESET 更准确地反映了"删除重建"的语义

3. **注意事项**：
   - RESET 会改变 ROWID（即使数据完全相同）
   - 未指定的列会变为默认值（可能丢失数据）
   - 外键约束可能受影响
   - 触发器会被执行两次（DELETE + INSERT）
   - 适用场景：缓存表、配置表、简单键值存储等

4. **RESET vs UPSERT**：
   - **RESET**：删除整行后重新插入，所有未指定列变为默认值
   - **UPSERT**：只更新指定列，其他列保持不变（推荐用于大多数场景）

**字符编码处理**:
与 INSERT 和 UPDATE 相同：
1. 所有文本数据必须使用 UTF-8 编码
2. Content-Length 计算的是 UTF-8 字节数
3. JSON 字符串中的特殊字符需要转义
4. 二进制数据（BLOB）使用 Base64 编码，前缀 `base64:`

---


