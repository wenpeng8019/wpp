## UPDATE

```sql
UPDATE users 
SET name = 'Alice Johnson', email = 'alice@example.com', age = 28, status = 'active', updated_at = '2026-02-09T10:30:45Z'
WHERE id = 12345;

UPDATE users SET status = 'inactive';
```

**HTTP 请求协议**:

```http
SQTP-UPDATE /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: name, email, age, status, updated_at\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 85\n
WHERE: id = 12345\n
\n
["Alice Johnson", "alice@example.com", 28, "active", "2026-02-09T10:30:45Z"]
```
```http
SQTP-UPDATE /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: status\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 12\n
WHERE: *\n
\n
["inactive"]
```

使用表单编码：
```http
SQTP-UPDATE /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: status, verified, updated_at\n
Content-Type: application/x-www-form-urlencoded; charset=utf-8\n
Content-Length: 32\n
WHERE: status = 'pending' AND age > 18\n
\n
active&true&2026-02-09
```

使用 multipart（长文本更新）：
```http
SQTP-UPDATE /db/main HTTP/1.1\n
TABLE: articles\n
COLUMNS: title, content, updated_at\n
Content-Type: multipart/form-data; boundary=----Boundary456\n
Content-Length: 856\n
WHERE: id = 100\n
\n
------Boundary456\r
Content-Disposition: form-data; name="0"\r
\r
Updated Article Title\r
------Boundary456\r
Content-Disposition: form-data; name="1"\r
Content-Type: text/plain; charset=utf-8\r
\r
This is the updated article content.

It can be very long and contain:
- Multiple paragraphs
- Special characters: @#$%
- Unicode: 中文内容
- Newlines and formatting

Much better than URL encoding for long text!\r
------Boundary456\r
Content-Disposition: form-data; name="2"\r
\r
2026-02-09T10:30:45Z\r
------Boundary456--\r
```

批量更新（使用 CASE WHEN）：
```http
SQTP-UPDATE /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: status, updated_at\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 245\n
UPDATE-MODE: BATCH\n
KEY-COLUMN: id\n
\n
[
  {"id": 1, "status": "active", "updated_at": "2026-02-09T10:30:45Z"},
  {"id": 2, "status": "inactive", "updated_at": "2026-02-09T10:30:45Z"},
  {"id": 3, "status": "active", "updated_at": "2026-02-09T10:30:45Z"}
]
```

批量更新（使用 WHERE-IN）：
```http
SQTP-UPDATE /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: status, updated_at\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 40\n
WHERE-IN: id\n
\n
{"id": [1, 2, 3], "values": ["active", "2026-02-09T10:30:45Z"]}
```

使用多个 WHERE 和 WHERE-IN（AND 关系）：
```http
SQTP-UPDATE /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: verified\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 50\n
WHERE: age >= 18\n
WHERE: created_at > '2025-01-01'\n
WHERE-IN: status\n
WHERE-IN: role\n
\n
{"status": ["active", "pending"], "role": ["user", "admin"], "values": [true]}
```

**请求 Header 字段说明**:
- `TABLE`: 目标表名（必需）
- `COLUMNS`: 要更新的列名（逗号分隔，必需）
- `WHERE`: 更新条件（必需，防止全表更新，可多个，彼此为 AND 关系）
  - 如需全表更新，使用 `WHERE: *`
- `WHERE-IN`: IN 条件，指定列名（可多个，彼此为 AND 关系）
  - 单个列：`WHERE-IN: column_name`，Body 中提供值数组
  - 多个列：多个 `WHERE-IN` header，Body 为对象格式
- `Content-Type`: 请求数据编码格式
  - `application/json; charset=utf-8`: JSON 格式
  - `application/x-www-form-urlencoded`: 表单编码格式
  - `multipart/form-data`: 多部分表单（用于长文本和二进制数据）
- `Content-Length`: 请求体的字节长度（UTF-8 编码）
  - `BATCH`: 批量更新（根据 KEY-COLUMN 匹配）
- `KEY-COLUMN`: 批量更新时的主键列名（UPDATE-MODE=BATCH 时必需）
- `LIMIT`: 限制更新的行数（可选，安全机制）

**请求 Body 数据格式**:

数据顺序必须与 COLUMNS header 中指定的列名顺序一致。

与 INSERT 相同，支持三种编码格式：
1. **application/json**: 数组格式 `[value1, value2, ...]`
2. **application/x-www-form-urlencoded**: `value1&value2&...`
3. **multipart/form-data**: 使用数字索引 (0, 1, 2, ...)

**HTTP 应答协议**:

成功应答示例：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Rows-Affected: 1\n
X-SQTP-Execution-Time: 0.008\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

批量更新成功应答：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Rows-Affected: 3\n
X-SQTP-Execution-Time: 0.015\n
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

约束冲突应答：
```http
HTTP/1.1 409 Conflict\n
Content-Type: text/plain; charset=utf-8\n
Content-Length: 45\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Error-Code: 19\n
X-SQTP-Error-Type: SQLITE_CONSTRAINT\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
UNIQUE constraint failed: users.email
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
WHERE clause is required for UPDATE. To update all rows, use WHERE: *
```

**HTTP 状态码定义**:
- `200 OK`: 更新成功
- `404 Not Found`: 没有匹配的行被更新
- `400 Bad Request`: 缺少 WHERE 条件、语法错误
- `409 Conflict`: 约束冲突（UNIQUE、PRIMARY KEY、FOREIGN KEY）
- `413 Payload Too Large`: 请求体过大
- `415 Unsupported Media Type`: 不支持的 Content-Type
- `422 Unprocessable Entity`: 数据验证失败
- `500 Internal Server Error`: 数据库内部错误

**响应 Header 字段说明**:
- `X-SQTP-Rows-Affected`: 受影响的行数

**安全特性**:
1. **必需 WHERE 条件**：防止意外全表更新（全表更新使用 `WHERE: *`）
2. **LIMIT 支持**：限制单次更新的最大行数
3. **返回受影响行数**：便于验证更新结果

**字符编码处理**:
与 INSERT 相同：
1. 所有文本数据必须使用 UTF-8 编码
2. Content-Length 计算的是 UTF-8 字节数
3. JSON 字符串中的特殊字符需要转义
4. 二进制数据（BLOB）使用 Base64 编码，前缀 `base64:`

**JavaScript 调用示范**:

```javascript
// ========== XMLHttpRequest 方式 ==========

// 单行更新
{
  const xhr = new XMLHttpRequest();
  
  xhr.open('UPDATE', 'http://localhost:8080/db/main', true);
  xhr.setRequestHeader('Protocol', 'SQTP/1.0');
  xhr.setRequestHeader('TABLE', 'users');
  xhr.setRequestHeader('COLUMNS', 'name, email, age, status, updated_at');
  xhr.setRequestHeader('WHERE', 'id = 12345');
  xhr.setRequestHeader('Content-Type', 'application/json; charset=utf-8');
  
  xhr.onload = function() {
    if (xhr.status === 200) {
      const rowsAffected = xhr.getResponseHeader('X-SQTP-Rows-Affected');
      const executionTime = xhr.getResponseHeader('X-SQTP-Execution-Time');
      
      console.log(`Updated ${rowsAffected} rows in ${executionTime}s`);
    } else if (xhr.status === 404) {
      console.log('No matching rows');
    } else {
      console.error('Update failed:', xhr.responseText);
    }
  };
  
  xhr.send(JSON.stringify(["Alice Johnson", "alice@example.com", 28, "active", "2026-02-09T10:30:45Z"]));
}
```

```javascript
// ========== fetch API 方式 ==========

// 单行更新
{
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'UPDATE',
    headers: {
      'Protocol': 'SQTP/1.0',
      'TABLE': 'users',
      'COLUMNS': 'name, email, age, status, updated_at',
      'WHERE': 'id = 12345',
      'Content-Type': 'application/json; charset=utf-8'
    },
    body: JSON.stringify(["Alice Johnson", "alice@example.com", 28, "active", "2026-02-09T10:30:45Z"])
  });
  
  if (response.status === 200) {
    const rowsAffected = response.headers.get('X-SQTP-Rows-Affected');
    const executionTime = response.headers.get('X-SQTP-Execution-Time');
    
    console.log(`Updated ${rowsAffected} rows in ${executionTime}s`);
  } else if (response.status === 404) {
    console.log('No matching rows found');
  } else {
    const error = await response.text();
    console.error('Update failed:', error);
  }
}

// 全表更新
{
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'UPDATE',
    headers: {
      'Protocol': 'SQTP/1.0',
      'TABLE': 'users',
      'COLUMNS': 'status',
      'WHERE': '*',
      'Content-Type': 'application/json; charset=utf-8'
    },
    body: JSON.stringify(["inactive"])
  });
  
  if (response.status === 200) {
    const rowsAffected = response.headers.get('X-SQTP-Rows-Affected');
    console.log(`Updated all ${rowsAffected} rows`);
  }
}

// 使用表单编码更新
{
  const formData = new URLSearchParams();
  formData.append('0', 'active');
  formData.append('1', 'true');
  formData.append('2', '2026-02-09');
  
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'UPDATE',
    headers: {
      'Protocol': 'SQTP/1.0',
      'TABLE': 'users',
      'COLUMNS': 'status, verified, updated_at',
      'WHERE': "status = 'pending' AND age > 18"
    },
    body: formData
  });
  
  if (response.status === 200) {
    console.log('Updated:', response.headers.get('X-SQTP-Rows-Affected'), 'rows');
  }
}
```



