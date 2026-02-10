## DELETE

删除数据
```sql
DELETE FROM table_name WHERE condition;
```

**HTTP 请求协议**:

单条删除示例：
```http
SQTP-DELETE /db/main HTTP/1.1\n
TABLE: users\n
WHERE: id = 12345\n
\n
```

条件删除示例：
```http
SQTP-DELETE /db/main HTTP/1.1\n
TABLE: users\n
WHERE: status = 'inactive' AND last_login < '2025-01-01'\n
LIMIT: 100\n
\n
```

全表删除示例：
```http
SQTP-DELETE /db/main HTTP/1.1\n
TABLE: temp_cache\n
WHERE: *\n
\n
```

批量删除（通过 ID 列表）：
```http
SQTP-DELETE /db/main HTTP/1.1\n
TABLE: users\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 35\n
WHERE-IN: id\n
\n
[12345, 12346, 12347, 12348, 12349]
```

使用表单编码：
```http
SQTP-DELETE /db/main HTTP/1.1\n
TABLE: logs\n
Content-Type: application/x-www-form-urlencoded; charset=utf-8\n
Content-Length: 15\n
WHERE-IN: id\n
\n
100&101&102&103
```

**请求 Header 字段说明**:
- `TABLE`: 目标表名（必需）
- `WHERE`: 删除条件（**必需**，防止意外全表删除）
  - 如需删除全表，使用 `WHERE: *`
- `WHERE-IN`: 用于批量删除，指定匹配的列名（可选）
  - 配合 Body 中的 ID 列表使用
- `LIMIT`: 限制删除的行数（可选，安全机制）
- `Content-Type`: 仅批量删除时需要（当使用 Body 传递数据）
  - `application/json; charset=utf-8`: JSON 格式
  - `application/x-www-form-urlencoded`: 表单编码格式
- `Content-Length`: 请求体的字节长度（仅批量删除时）

**请求行说明**:
- `DELETE SQTP/1.0`: DELETE 不需要指定列名，因为删除的是整行

**请求 Body 数据格式**（可选，用于批量删除）:

当使用 `WHERE-IN: column` 时，Body 直接提供值的数组/列表：

1. **JSON 数组（推荐）**:
```json
[1, 2, 3, 4, 5]
```

2. **表单编码（简洁格式）**:
```
100&101&102&103
```

或使用标准格式（兼容性更好）：
```
id=100&id=101&id=102&id=103
```

说明：
- 列名已在 WHERE-IN header 中指定，Body 中只需提供值列表
- 表单编码支持简洁格式（直接用 `&` 分隔）和标准格式（`key=value&key=value`）
- 值中包含特殊字符时需要 URL 编码（如 `&` → `%26`, `=` → `%3D`）

**HTTP 应答协议**:

成功删除应答：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Rows-Affected: 5\n
X-SQTP-Execution-Time: 0.012\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

无匹配行应答：
```http
HTTP/1.1 404 Not Found\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Rows-Affected: 0\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

批量删除成功应答：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Rows-Affected: 5\n
X-SQTP-Execution-Time: 0.018\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

外键约束错误应答：
```http
HTTP/1.1 409 Conflict\n
Content-Type: text/plain; charset=utf-8\n
Content-Length: 61\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Error-Code: 19\n
X-SQTP-Error-Type: SQLITE_CONSTRAINT\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
FOREIGN KEY constraint failed: Cannot delete referenced row
```

缺少 WHERE 条件错误：
```http
HTTP/1.1 400 Bad Request\n
Content-Type: text/plain; charset=utf-8\n
Content-Length: 79\n
X-SQTP-Protocol: SQTP/1.0\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
WHERE clause is required for DELETE. To delete all rows, use WHERE: *
```

**HTTP 状态码定义**:
- `200 OK`: 删除成功
- `404 Not Found`: 没有匹配的行被删除
- `400 Bad Request`: 缺少 WHERE 条件、语法错误
- `409 Conflict`: 外键约束冲突（有其他表引用）
- `413 Payload Too Large`: 请求体过大（批量删除 ID 过多）
- `415 Unsupported Media Type`: 不支持的 Content-Type
- `500 Internal Server Error`: 数据库内部错误

**响应 Header 字段说明**:
- `X-SQTP-Rows-Affected`: 受影响（删除）的行数

**安全特性**:
1. **必需 WHERE 条件**：防止意外全表删除（全表删除使用 `WHERE: *`）
2. **LIMIT 支持**：限制单次删除的最大行数
3. **返回受影响行数**：便于验证删除结果
4. **外键约束检查**：防止破坏数据完整性

**DELETE vs TRUNCATE**:
- **DELETE**: 逐行删除，触发触发器，可以回滚，可以有 WHERE 条件
- **TRUNCATE** (如果实现): 清空整个表，速度快，不触发触发器，不可指定条件

**批量删除最佳实践**:
1. **少量 ID（< 100）**: 使用 WHERE-IN + Body
2. **大量 ID（> 100）**: 分批删除或使用临时表
3. **复杂条件**: 使用 WHERE header
4. **全表删除**: 明确使用 `WHERE: *` 并考虑使用 TRUNCATE

**触发器说明**:
- DELETE 操作会触发 DELETE 触发器
- 可以在触发器中记录删除日志、软删除等

**字符编码处理**:
1. 所有文本数据必须使用 UTF-8 编码
2. Content-Length 计算的是 UTF-8 字节数（仅批量删除时）
3. JSON 字符串中的特殊字符需要转义

---


