## SELECT
```sql
SELECT * FROM table_name;
SELECT column1, column2 FROM table_name WHERE condition;
SELECT COUNT(*) FROM table_name;

SELECT id, name, email, COUNT(orders.id) AS order_count
FROM users
LEFT JOIN orders ON users.id = orders.user_id
WHERE users.created_at > '2025-01-01' AND users.status IN ('active', 'pending')
GROUP BY users.id, users.name, users.email
HAVING COUNT(orders.id) > 0
ORDER BY order_count DESC, users.name ASC
LIMIT 50 OFFSET 0;
```

**HTTP 请求协议**:

单表查询：
```http
SQTP-SELECT /db/main HTTP/1.1\n
FROM: table_name\n
\n
```
```http
SQTP-SELECT /db/main HTTP/1.1\n
COLUMNS: column1, column2\n
FROM: table_name\n
WHERE: condition\n
\n
```
```http
SQTP-SELECT /db/main HTTP/1.1\n
COLUMNS: COUNT(*)\n
FROM: table_name\n
\n
```

多表关联查询：
```http
SQTP-SELECT /db/main HTTP/1.1\n
COLUMNS: id, name, email, COUNT(orders.id) AS order_count\n
FROM-JOIN: users LEFT JOIN orders ON users.id = orders.user_id\n
WHERE: users.created_at > '2025-01-01'\n
WHERE-IN: users.status\n
GROUP-BY: users.id, users.name, users.email\n
HAVING: COUNT(orders.id) > 0\n
ORDER-BY: order_count DESC, users.name ASC\n
LIMIT: 50\n
OFFSET: 0\n
DISTINCT: false\n
X-SQTP-VIEW: object\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 22\n
\n
["active", "pending"]\n
```

或使用多个 WHERE 条件：
```http
SQTP-SELECT /db/main HTTP/1.1\n
COLUMNS: id, name, email\n
FROM: users\n
WHERE: created_at > '2025-01-01'\n
WHERE: age >= 18\n
WHERE-IN: status\n
WHERE-IN: role\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 50\n
\n
{"status": ["active", "pending"], "role": ["user", "admin"]}\n
```

**HTTP 应答协议**:

成功应答示例（X-SQTP-VIEW: object，默认）：
```http
HTTP/1.1 200 OK\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 312\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Execution-Time: 0.023\n
X-SQTP-Row-Count: 3\n
X-SQTP-Total-Rows: 150\n
X-SQTP-Has-More: true\n
X-SQTP-Columns: id, name, email, order_count\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
[
  {"id": 1, "name": "Alice", "email": "alice@example.com", "order_count": 15},
  {"id": 2, "name": "Bob", "email": "bob@example.com", "order_count": 8},
  {"id": 3, "name": "Charlie", "email": "charlie@example.com", "order_count": 3}
]
```

成功应答示例（X-SQTP-VIEW: row）：
```http
HTTP/1.1 200 OK\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 156\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Execution-Time: 0.023\n
X-SQTP-Row-Count: 3\n
X-SQTP-Total-Rows: 150\n
X-SQTP-Has-More: true\n
X-SQTP-Columns: id, name, email, order_count\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
[
  [1, "Alice", "alice@example.com", 15],
  [2, "Bob", "bob@example.com", 8],
  [3, "Charlie", "charlie@example.com", 3]
]
```

成功应答示例（X-SQTP-VIEW: column）：
```http
HTTP/1.1 200 OK\n
Content-Type: application/json; charset=utf-8\n
Content-Length: 198\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Execution-Time: 0.023\n
X-SQTP-Row-Count: 3\n
X-SQTP-Total-Rows: 150\n
X-SQTP-Has-More: true\n
X-SQTP-Columns: id, name, email, order_count\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
[
  [1, 2, 3],
  ["Alice", "Bob", "Charlie"],
  ["alice@example.com", "bob@example.com", "charlie@example.com"],
  [15, 8, 3]
]
```

错误应答示例：
```http
HTTP/1.1 400 Bad Request\n
Content-Type: text/plain; charset=utf-8\n
Content-Length: 62\n
X-SQTP-Protocol: SQTP/1.0\n
X-SQTP-Error-Code: 1\n
X-SQTP-Error-Type: SQLITE_ERROR\n
Date: Sun, 09 Feb 2026 10:30:45 GMT\n
Server: WPP-SQLite/1.0\n
\n
Syntax error in SQL statement: near "FORM": syntax error
```

**HTTP 状态码定义**:
- `200 OK`: 查询成功
- `204 No Content`: 查询成功但无数据返回
- `400 Bad Request`: SQL 语法错误或参数错误
- `401 Unauthorized`: 未授权访问
- `403 Forbidden`: 无权限访问该表或数据库
- `404 Not Found`: 表或数据库不存在
- `429 Too Many Requests`: 请求过于频繁
- `500 Internal Server Error`: 数据库内部错误
- `503 Service Unavailable`: 数据库锁定或不可用

**HTTP Header 字段说明**:
- `Content-Type`: 响应内容类型，固定为 `application/json; charset=utf-8`
- `Content-Length`: JSON 数据的字节长度
- `X-SQTP-Protocol`: SQL HTTP 协议版本
- `X-SQTP-Execution-Time`: SQL 执行时间（秒）
- `X-SQTP-Row-Count`: 当前返回的行数
- `X-SQTP-Total-Rows`: 符合条件的总行数（用于分页）
- `X-SQTP-Has-More`: 是否还有更多数据（true/false）
- `X-SQTP-Columns`: 返回的列名列表（逗号分隔），用于 row/column 格式时明确字段顺序
- `X-SQTP-Error-Code`: SQLite 错误代码（仅错误时）
- `X-SQTP-Error-Type`: 错误类型（仅错误时）
- `Date`: 响应时间（RFC 7231 格式）
- `Server`: 服务器标识

**请求 Header 字段说明**:
- `X-SQTP-VIEW`: 数据视图格式（可选，默认为 object）
  - `object`: 对象数组格式 `[{col: val, ...}, ...]` （默认）
  - `row`: 行数组格式 `[[val, val, ...], ...]` - 每个内层数组是一行数据
  - `column`: 列数组格式 `[[val, val, ...], ...]` - 每个内层数组是一列数据
- `WHERE`: WHERE 条件（可多个，彼此为 AND 关系）
- `WHERE-IN`: IN 条件，指定列名（可多个，彼此为 AND 关系）
  - 单个列：`WHERE-IN: column_name`，Body 为数组 `[val1, val2, ...]`
  - 多个列：多个 `WHERE-IN` header，Body 为对象 `{"col1": [val1, val2], "col2": [val3, val4]}`
- `COLUMNS`: 需要查询的列名（逗号分隔，可选，默认为 *）
- `FROM`: 单表查询的表名（必需，与 FROM-JOIN 二选一）
- `FROM-JOIN`: 多表关联查询的完整 FROM 和 JOIN 子句（必需，与 FROM 二选一）
- `GROUP-BY`: 分组字段（逗号分隔，可选）
- `HAVING`: 分组过滤条件（可选）
- `ORDER-BY`: 排序字段（逗号分隔，支持 ASC/DESC，可选）
- `LIMIT`: 返回的最大行数（可选）
- `OFFSET`: 跳过的行数（可选）
- `DISTINCT`: 是否去重（true/false，可选，默认 false）
- `Content-Type`: 使用 WHERE-IN 时需要（Body 中传递值列表）
- `Content-Length`: Body 长度（使用 WHERE-IN 时）

**JavaScript 调用示范**:

```javascript
// ========== XMLHttpRequest 方式 ==========

// 基础查询
{
  const xhr = new XMLHttpRequest();
  
  xhr.open('SELECT', 'http://localhost:8080/db/main', true);
  xhr.setRequestHeader('Protocol', 'SQTP/1.0');
  xhr.setRequestHeader('FROM-JOIN', 'users LEFT JOIN orders ON users.id = orders.user_id');
  xhr.setRequestHeader('WHERE', "users.created_at > '2025-01-01'");
  xhr.setRequestHeader('GROUP-BY', 'users.id, users.name');
  xhr.setRequestHeader('HAVING', 'COUNT(orders.id) > 0');
  xhr.setRequestHeader('ORDER-BY', 'COUNT(orders.id) DESC');
  xhr.setRequestHeader('LIMIT', '50');
  
  xhr.onload = function() {
    if (xhr.status === 200) {
      // 获取响应数据
      const data = JSON.parse(xhr.responseText);
      console.log('Data:', data);
      
      // 获取响应 Headers
      const executionTime = xhr.getResponseHeader('X-SQTP-Execution-Time');
      const rowCount = xhr.getResponseHeader('X-SQTP-Row-Count');
      const hasMore = xhr.getResponseHeader('X-SQTP-Has-More');
      const columns = xhr.getResponseHeader('X-SQTP-Columns');
      
      console.log(`Executed in ${executionTime}s, returned ${rowCount} rows, has more: ${hasMore}`);
      console.log(`Columns: ${columns}`);
    } else {
      console.error('Query failed:', xhr.responseText);
    }
  };
  
  xhr.send();
}
```

```javascript
// ========== fetch API 方式 ==========

// 基础查询
{
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'SELECT',
    headers: {
      'Protocol': 'SQTP/1.0',
      'FROM': 'users',
      'WHERE': "status = 'active'",
      'ORDER-BY': 'name ASC',
      'LIMIT': '10'
    }
  });
  
  if (!response.ok) {
    const error = await response.text();
    throw new Error(error);
  }
  
  const data = await response.json();
  const headers = {
    executionTime: response.headers.get('X-SQTP-Execution-Time'),
    rowCount: response.headers.get('X-SQTP-Row-Count'),
    totalRows: response.headers.get('X-SQTP-Total-Rows'),
    hasMore: response.headers.get('X-SQTP-Has-More') === 'true',
    columns: response.headers.get('X-SQTP-Columns')
  };
}

// 使用不同的视图格式 - row 格式
{
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'SELECT',
    headers: {
      'Protocol': 'SQTP/1.0',
      'FROM': 'users',
      'X-SQTP-VIEW': 'row',
      'LIMIT': '100'
    }
  });
  
  const data = await response.json(); // [[1, "Alice", "alice@example.com"], ...]
  const columns = response.headers.get('X-SQTP-Columns').split(', ');
  
  // 转换为对象格式（如果需要）
  const objects = data.map(row => {
    const obj = {};
    columns.forEach((col, i) => obj[col] = row[i]);
    return obj;
  });
}

// column 格式 - 用于图表
{
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'SELECT',
    headers: {
      'Protocol': 'SQTP/1.0',
      'FROM': 'users',
      'X-SQTP-VIEW': 'column',
      'LIMIT': '100'
    }
  });
  
  const data = await response.json(); // [[1, 2, 3], ["Alice", "Bob", "Charlie"], ...]
  const columns = response.headers.get('X-SQTP-Columns').split(', ');
  
  const chartData = {
    labels: columns,
    datasets: data
  };
}

// 分页查询
{
  const page = 0;
  const pageSize = 20;
  const offset = page * pageSize;
  
  const response = await fetch('http://localhost:8080/db/main', {
    method: 'SELECT',
    headers: {
      'Protocol': 'SQTP/1.0',
      'FROM': 'users',
      'ORDER-BY': 'id ASC',
      'LIMIT': pageSize.toString(),
      'OFFSET': offset.toString()
    }
  });
  
  const data = await response.json();
  const totalRows = parseInt(response.headers.get('X-SQTP-Total-Rows'));
  const totalPages = Math.ceil(totalRows / pageSize);
}
```

---


