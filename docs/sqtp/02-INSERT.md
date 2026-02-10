## INSERT
```sql
INSERT INTO table_name (column1, column2) VALUES (value1, value2);
INSERT INTO table_name SELECT * FROM other_table;
```

**HTTP 请求协议**:

单行插入示例：
```http
SQTP-INSERT /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: name, email, age, status\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 48\n
ON-CONFLICT: ABORT\n
\n
["Alice", "alice@example.com", 28, "active"]
```

批量插入示例：
```http
SQTP-INSERT /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: name, email, age\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 125\n
ON-CONFLICT: IGNORE\n
\n
[
  ["Alice", "alice@example.com", 28],
  ["Bob", "bob@example.com", 32],
  ["Charlie", "charlie@example.com", 25]
]
```

从查询插入示例（INSERT SELECT）：
```http
SQTP-INSERT /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: name, email, age\n
FROM: other_table\n
WHERE: status = 'pending'\n
\n
```

**请求 Header 字段说明**:
- `TABLE`: 目标表名（必需）
- `COLUMNS`: 要插入的列名（逗号分隔，必需）
- `Content-Type`: 请求数据编码格式（INSERT SELECT 时可省略）
  - `application/json; charset=utf-8`: JSON 格式
  - `application/x-www-form-urlencoded`: 表单编码格式
  - `multipart/form-data`: 多部分表单（用于长文本和二进制数据）
- `Content-Length`: 请求体的字节长度（UTF-8 编码，INSERT SELECT 时可省略）
- `ON-CONFLICT`: 冲突处理策略
  - `ABORT`: 中止（默认）
  - `IGNORE`: 忽略冲突
  - `REPLACE`: 替换已有数据
  - `FAIL`: 失败但继续事务
  - `ROLLBACK`: 回滚整个事务
- `FROM`: INSERT SELECT 时的源表（可选）
- `WHERE`: INSERT SELECT 的过滤条件（可选）

**请求 Body 数据格式**:

数据顺序必须与 COLUMNS header 中指定的列名顺序一致。

#### 1. application/x-www-form-urlencoded（标准表单编码）

单行插入：
```http
SQTP-INSERT /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: name, email, age, status\n
Content-Type: application/x-www-form-urlencoded; charset=utf-8\n
Content-Length: 42\n
\n
Alice&alice%40example.com&28&active
```

**URL 编码规则**:
- 空格 → `%20` 或 `+`
- `@` → `%40`
- `=` → `%3D`
- `&` → `%26`
- `%` → `%25`
- 中文等 UTF-8 字符 → 百分号编码（如 `张三` → `%E5%BC%A0%E4%B8%89`）
- 换行符 `\n` → `%0A`
- 回车符 `\r` → `%0D`

**限制**: 
- 不适合长文本（通常限制在几 KB 以内）
- 不适合二进制数据
- 批量插入需要特殊命名约定（如 `rows[0][name]=Alice&rows[0][age]=28&rows[1][name]=Bob...`）

#### 2. multipart/form-data（多部分表单，推荐用于长文本和二进制）

单行插入（包含长文本）：
```http
SQTP-INSERT /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: name, email, bio, avatar\n
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW\n
Content-Length: 1256\n
\n
------WebKitFormBoundary7MA4YWxkTrZu0gW\r
Content-Disposition: form-data; name="0"\r
\r
Alice Johnson\r
------WebKitFormBoundary7MA4YWxkTrZu0gW\r
Content-Disposition: form-data; name="1"\r
\r
alice@example.com\r
------WebKitFormBoundary7MA4YWxkTrZu0gW\r
Content-Disposition: form-data; name="2"\r
Content-Type: text/plain; charset=utf-8\r
\r
This is a very long biography text that contains multiple paragraphs.

It can span many lines and contain special characters like:
- Quotes: "Hello World"
- Symbols: @#$%^&*()
- Unicode: 你好世界 🌍
- Newlines and formatting

This is much more suitable for long text content than URL encoding.\r
------WebKitFormBoundary7MA4YWxkTrZu0gW\r
Content-Disposition: form-data; name="3"; filename="avatar.png"\r
Content-Type: image/png\r
Content-Transfer-Encoding: binary\r
\r
[二进制数据...]
\r
------WebKitFormBoundary7MA4YWxkTrZu0gW--\r
```

**multipart/form-data 特点**:
- 每个字段作为独立的部分（part），用 boundary 分隔
- 字段名使用数字索引（0, 1, 2, ...），对应 COLUMNS 中的列顺序
- 适合长文本：无需转义，保持原始格式
- 支持二进制文件：直接传输二进制数据
- 可指定每个字段的 Content-Type
- 可传输文件名和文件类型
- boundary 必须唯一且不出现在数据中
- 批量插入时，按顺序排列：第一行的所有列（索引 0, 1, 2...），然后第二行的所有列（索引 3, 4, 5...）

批量插入示例：
```http
SQTP-INSERT /db/main HTTP/1.1\n
TABLE: users\n
COLUMNS: name, email, age\n
Content-Type: multipart/form-data; boundary=----Boundary123\n
Content-Length: 856\n
\n
------Boundary123\r
Content-Disposition: form-data; name="0"\r
\r
Alice\r
------Boundary123\r
Content-Disposition: form-data; name="1"\r
\r
alice@example.com\r
------Boundary123\r
Content-Disposition: form-data; name="2"\r
\r
28\r
------Boundary123\r
Content-Disposition: form-data; name="3"\r
\r
Bob\r
------Boundary123\r
Content-Disposition: form-data; name="4"\r
\r
bob@example.com\r
------Boundary123\r
Content-Disposition: form-data; name="5"\r
\r
32\r
------Boundary123--\r
```

#### 3. application/json（JSON 格式）

单行插入：
```json
["Alice", "alice@example.com", "This is a long text\nwith multiple lines\nand \"quotes\"", 28]
```

批量插入：
```json
[
  ["Alice", "alice@example.com", 28],
  ["Bob", "bob@example.com", 32]
]
```

**JSON 转义规则**:
- `"` → `\"`
- `\` → `\\`
- `/` → `\/`（可选）
- 换行 → `\n`
- 回车 → `\r`
- 制表符 → `\t`
- Unicode → `\uXXXX`

**特殊类型编码**:
- **NULL 值**: 
  - JSON: `null`
  - 表单: `field=` 或 `field=null`
- **布尔值**: 
  - JSON: `true` / `false`
  - 表单: `field=true` / `field=false` 或 `field=1` / `field=0`
- **二进制数据（BLOB）**: 
  - JSON: Base64 编码字符串 `"base64:iVBORw0KGgo..."`
  - multipart: 直接传输二进制，设置 `Content-Transfer-Encoding: binary`
  - 表单: Base64 编码或十六进制编码
- **日期时间**: 
  - 推荐 ISO 8601 格式: `2026-02-09T10:30:45Z`
  - Unix 时间戳: `1739097045`
  - SQLite 日期格式: `2026-02-09 10:30:45`
- **长文本**: 
  - **推荐使用 multipart/form-data**，无需转义，保持原始格式
  - JSON: 使用 `\n` 等转义字符
  - URL 编码: 每个字符都需要编码，效率低

**编码格式选择建议**:
- **短文本、简单数据**: `application/x-www-form-urlencoded`
- **长文本、文章内容**: `multipart/form-data` ⭐推荐
- **二进制文件、图片**: `multipart/form-data`
- **复杂结构、嵌套数据**: `application/json`
- **批量导入、大量数据**: `application/json`

**HTTP 应答协议**:

成功应答示例：
```http
HTTP/1.1 201 Created\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Last-Insert-Id: 12345\n
X-SQTP-Rows-Affected: 1\n
X-SQTP-Execution-Time: 0.005\n
Location: /db/main/users/12345\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

批量插入成功应答：
```http
HTTP/1.1 201 Created\n
Content-Type: application/json; charset=utf-8\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Last-Insert-Id: 12347\n
X-SQTP-Rows-Affected: 3\n
X-SQTP-Execution-Time: 0.012\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
[12345, 12346, 12347]
```

说明：返回每行插入的 ID 列表，顺序与请求中的数据顺序对应。

冲突忽略应答（ON-CONFLICT: IGNORE）：
```http
HTTP/1.1 200 OK\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Rows-Affected: 0\n
X-SQTP-Rows-Ignored: 1\n
X-SQTP-Execution-Time: 0.003\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
```

错误应答示例：
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

**HTTP 状态码定义**:
- `201 Created`: 插入成功
- `200 OK`: 插入成功但无新行（如 ON-CONFLICT: IGNORE）
- `400 Bad Request`: 请求格式错误、数据类型错误
- `409 Conflict`: 约束冲突（UNIQUE、PRIMARY KEY、FOREIGN KEY）
- `413 Payload Too Large`: 请求体过大
- `415 Unsupported Media Type`: 不支持的 Content-Type
- `422 Unprocessable Entity`: 数据验证失败
- `500 Internal Server Error`: 数据库内部错误

**响应 Header 字段说明**:
- `X-SQTP-Last-Insert-Id`: 最后插入行的 ROWID（自增 ID）
- `X-SQTP-Rows-Affected`: 受影响的行数
- `X-SQTP-Rows-Ignored`: 因冲突被忽略的行数（ON-CONFLICT: IGNORE）
- `Location`: 新创建资源的 URI（单行插入时）

**字符编码处理**:
1. 所有文本数据必须使用 UTF-8 编码
2. Content-Length 计算的是 UTF-8 字节数，不是字符数
3. JSON 字符串中的特殊字符需要转义：
   - `"` → `\"`
   - `\` → `\\`
   - `/` → `\/`（可选）
   - `\b`, `\f`, `\n`, `\r`, `\t`
   - Unicode: `\uXXXX`
4. 二进制数据（BLOB）使用 Base64 编码，前缀 `base64:`

**JavaScript 调用示范**:

```javascript
// ========== XMLHttpRequest 方式 ==========

// 单行插入
{
  const xhr = new XMLHttpRequest();
  
  xhr.open('INSERT', 'http://localhost:8080/db/main', true);
  xhr.setRequestHeader('Protocol', 'SQTP/1.0');
  xhr.setRequestHeader('INTO', 'users');
  xhr.setRequestHeader('COLUMNS', 'name, email, age, status');
  xhr.setRequestHeader('Content-Type', 'application/json; charset=utf-8');
  
  xhr.onload = function() {
    if (xhr.status === 201) {
      const lastInsertId = xhr.getResponseHeader('X-SQTP-Last-Insert-Id');
      const location = xhr.getResponseHeader('Location');
      const rowsAffected = xhr.getResponseHeader('X-SQTP-Rows-Affected');
      
      console.log(`Inserted: ID=${lastInsertId}, Rows=${rowsAffected}`);
      console.log(`Location: ${location}`);
    } else if (xhr.status === 409) {
      console.error('Conflict:', xhr.responseText);
    } else {
      console.error('Insert failed:', xhr.responseText);
    }
  };
  
  xhr.send(JSON.stringify(["Alice", "alice@example.com", 28, "active"]));
}

// 批量插入
{
  const xhr = new XMLHttpRequest();
  
  xhr.open('INSERT', 'http://localhost:8080/db/main', true);
  xhr.setRequestHeader('Protocol', 'SQTP/1.0');
  xhr.setRequestHeader('INTO', 'users');
  xhr.setRequestHeader('COLUMNS', 'name, email, age');
  xhr.setRequestHeader('ON-CONFLICT', 'IGNORE');
  xhr.setRequestHeader('Content-Type', 'application/json; charset=utf-8');
  
  xhr.onload = function() {
    if (xhr.status === 201) {
      const insertedIds = JSON.parse(xhr.responseText);
      const rowsAffected = xhr.getResponseHeader('X-SQTP-Rows-Affected');
      
      console.log(`Inserted ${rowsAffected} rows:`, insertedIds);
    }
  };
  
  const users = [
    ["Alice", "alice@example.com", 28],
    ["Bob", "bob@example.com", 32],
    ["Charlie", "charlie@example.com", 25]
  ];
  xhr.send(JSON.stringify(users));
}

// 使用表单编码插入 - application/x-www-form-urlencoded
{
  const xhr = new XMLHttpRequest();
  
  xhr.open('INSERT', 'http://localhost:8080/db/main', true);
  xhr.setRequestHeader('Protocol', 'SQTP/1.0');
  xhr.setRequestHeader('INTO', 'users');
  xhr.setRequestHeader('COLUMNS', 'name, email, age, status');
  
  xhr.onload = function() {
    if (xhr.status === 201) {
      const lastInsertId = xhr.getResponseHeader('X-SQTP-Last-Insert-Id');
      console.log('Inserted:', lastInsertId);
    }
  };
  
  // URLSearchParams 会自动设置 Content-Type: application/x-www-form-urlencoded
  // 使用数字索引 '0', '1', '2'... 会被解析为数组格式 ["Alice", "alice@example.com", 28, "active"]
  const formData = new URLSearchParams();
  formData.append('0', 'Alice');
  formData.append('1', 'alice@example.com');
  formData.append('2', '28');
  formData.append('3', 'active');
  
  xhr.send(formData);
}

// 使用表单编码插入 - multipart/form-data
{
  const xhr = new XMLHttpRequest();
  
  xhr.open('INSERT', 'http://localhost:8080/db/main', true);
  xhr.setRequestHeader('Protocol', 'SQTP/1.0');
  xhr.setRequestHeader('INTO', 'users');
  xhr.setRequestHeader('COLUMNS', 'name, email, age, status');
  xhr.onload = function() {
    if (xhr.status === 201) {
      const lastInsertId = xhr.getResponseHeader('X-SQTP-Last-Insert-Id');
      console.log('Inserted:', lastInsertId);
    }
  };
  
  // FormData 自动设置 Content-Type: multipart/form-data; boundary=...
  // 使用数字索引 '0', '1', '2'... 会被解析为数组格式 ["Alice", "alice@example.com", 28, "active"]
  const formData = new FormData();
  formData.append('0', 'Alice');
  formData.append('1', 'alice@example.com');
  formData.append('2', '28');
  formData.append('3', 'active');
  
  xhr.send(formData);
}

// INSERT SELECT
{
  const xhr = new XMLHttpRequest();
  
  xhr.open('INSERT', 'http://localhost:8080/db/main', true);
  xhr.setRequestHeader('Protocol', 'SQTP/1.0');
  xhr.setRequestHeader('INTO', 'users');
  xhr.setRequestHeader('COLUMNS', 'name, email, age');
  xhr.setRequestHeader('FROM', 'other_table');
  xhr.setRequestHeader('WHERE', "status = 'pending'");
  
  xhr.onload = function() {
    if (xhr.status === 201) {
      const rowsAffected = xhr.getResponseHeader('X-SQTP-Rows-Affected');
      console.log(`Inserted ${rowsAffected} rows from SELECT`);
    }
  };
  
  xhr.send();
}
```

```javascript
// ========== fetch API 方式 ==========

// 单行插入
{
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'INSERT',
    headers: {
      'Protocol': 'SQTP/1.0',
      'INTO': 'users',
      'COLUMNS': 'name, email, age, status',
      'Content-Type': 'application/json; charset=utf-8'
    },
    body: JSON.stringify(["Alice", "alice@example.com", 28, "active"])
  });
  
  if (response.status === 201) {
    const lastInsertId = response.headers.get('X-SQTP-Last-Insert-Id');
    const rowsAffected = response.headers.get('X-SQTP-Rows-Affected');
    const location = response.headers.get('Location');
    
    console.log(`Inserted successfully: ID=${lastInsertId}, Location=${location}`);
  } else {
    const error = await response.text();
    console.error('Insert failed:', error);
  }
}

// 批量插入
{
  const users = [
    ["Alice", "alice@example.com", 28],
    ["Bob", "bob@example.com", 32],
    ["Charlie", "charlie@example.com", 25]
  ];
  
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'INSERT',
    headers: {
      'Protocol': 'SQTP/1.0',
      'INTO': 'users',
      'COLUMNS': 'name, email, age',
      'ON-CONFLICT': 'IGNORE',
      'Content-Type': 'application/json; charset=utf-8'
    },
    body: JSON.stringify(users)
  });
  
  if (response.status === 201) {
    const insertedIds = await response.json();
    const rowsAffected = response.headers.get('X-SQTP-Rows-Affected');
    
    console.log(`Inserted ${rowsAffected} rows:`, insertedIds);
  }
}

// 使用表单编码插入 - application/x-www-form-urlencoded
{
  // URLSearchParams 自动设置 Content-Type: application/x-www-form-urlencoded
  // 使用数字索引 '0', '1', '2'... 会被解析为数组格式 ["Alice", "alice@example.com", 28, "active"]
  const formData = new URLSearchParams();
  formData.append('0', 'Alice');
  formData.append('1', 'alice@example.com');
  formData.append('2', '28');
  formData.append('3', 'active');
  
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'INSERT',
    headers: {
      'Protocol': 'SQTP/1.0',
      'INTO': 'users',
      'COLUMNS': 'name, email, age, status'
    },
    body: formData
  });
  
  if (response.status === 201) {
    console.log('Inserted:', response.headers.get('X-SQTP-Last-Insert-Id'));
  }
}

// 使用表单编码插入 - multipart/form-data
{
  // FormData 自动设置 Content-Type: multipart/form-data; boundary=...
  // 使用数字索引 '0', '1', '2'... 会被解析为数组格式 ["Alice", "alice@example.com", 28, "active"]
  const formData = new FormData();
  formData.append('0', 'Alice');
  formData.append('1', 'alice@example.com');
  formData.append('2', '28');
  formData.append('3', 'active');
  
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'INSERT',
    headers: {
      'Protocol': 'SQTP/1.0',
      'INTO': 'users',
      'COLUMNS': 'name, email, age, status'
    },
    body: formData
  });
  
  if (response.status === 201) {
    console.log('Inserted:', response.headers.get('X-SQTP-Last-Insert-Id'));
  }
}

// INSERT SELECT
{
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'INSERT',
    headers: {
      'Protocol': 'SQTP/1.0',
      'INTO': 'users',
      'COLUMNS': 'name, email, age',
      'FROM': 'other_table',
      'WHERE': "status = 'pending'"
    }
  });
  
  if (response.status === 201) {
    const rowsAffected = response.headers.get('X-SQTP-Rows-Affected');
    console.log(`Inserted ${rowsAffected} rows from SELECT`);
  }
}
```



