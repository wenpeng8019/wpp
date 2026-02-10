# SQTP JavaScript Client Libraries

基于 SQTP/1.0 协议的 JavaScript 客户端库，提供多种实现方式。

## 📚 库文件

### 1. sqtp.xhr.promise.js - Promise/Async-Await (推荐)
- **适用场景**：现代 Web 应用
- **技术**：XMLHttpRequest + Promise
- **大小**：~18KB (未压缩)
- **浏览器支持**：所有支持 Promise 的浏览器（IE11+ 需 Promise polyfill）
- **风格**：`await db.select('users').execute()`
- **测试文件**：[sqtp.xhr.promise.test.html](sqtp.xhr.promise.test.html) - 31 个完整测试 ✅

### 2. sqtp.xhr.callback.js - 传统回调方式
- **适用场景**：老旧项目、不支持 Promise 的环境
- **技术**：XMLHttpRequest + Error-First Callback
- **大小**：~15KB (未压缩)
- **浏览器支持**：所有浏览器（IE6+）
- **风格**：`db.select('users').execute(function(err, result) {...})`
- **文档**：[回调方式 vs Promise 对比](./CALLBACK_VS_PROMISE.md)
- **测试文件**：[sqtp.xhr.callback.test.html](sqtp.xhr.callback.test.html) - 前3个测试已转换 ⚠️

### 3. sqtp.fetch.js - Fetch API 实现
- **适用场景**：现代浏览器应用（无需 IE 支持）
- **技术**：Fetch API + Promise
- **大小**：~14KB (未压缩)
- **浏览器支持**：Chrome 42+, Firefox 39+, Safari 10.1+, Edge 14+（不支持 IE）
- **测试文件**：[sqtp.fetch.test.html](sqtp.fetch.test.html) - 需要适配 Mock ⚠️

## 📁 完整文件清单

### Promise 版本（推荐，完全可用）
```
sqtp.xhr.promise.js           - 库文件（18KB）✅
sqtp.xhr.promise.example.html - 使用示例 ✅
sqtp.xhr.promise.test.html    - 完整测试套件（31 tests）✅
```

### Callback 版本（传统风格）
```
sqtp.xhr.callback.js          - 库文件（15KB）✅
sqtp.xhr.callback.example.html - 对比示例 ✅
sqtp.xhr.callback.test.html   - 测试套件（前3个已转换）⚠️
```

### Fetch 版本（现代浏览器）
```
sqtp.fetch.js                 - 库文件（14KB）✅
sqtp.fetch.example.html       - 使用示例 ✅
sqtp.fetch.test.html          - 测试套件（需适配 Mock）⚠️
```

### 文档
```
README.md                     - 库文档（本文件）
CALLBACK_VS_PROMISE.md        - 回调 vs Promise 详细对比
```

## 🚀 快速开始

### 安装

三个库都是独立的单文件，可以直接引入：

```html
<!-- Promise 版本（推荐） -->
<script src="lib/sqtp.xhr.promise.js"></script>

<!-- 传统回调版本 -->
<script src="lib/sqtp.xhr.callback.js"></script>

<!-- Fetch API 版本 -->
<script src="lib/sqtp.fetch.js"></script>
```

也可以在 Node.js 中使用：

```javascript
// Promise 版本
const SQTP = require('./lib/sqtp.xhr.promise.js');

// 回调版本
const SQTPCallback = require('./lib/sqtp.xhr.callback.js');
```

### 初始化

```javascript
// Promise 版本
const db = new SQTP('http://localhost:8080/db/main', {
  protocol: 'SQTP/1.0',  // 协议版本
  timeout: 30000,         // 超时时间（毫秒）
  debug: false            // 调试日志
});

// 回调版本（API 完全相同，只是调用方式不同）
const db = new SQTPCallback('http://localhost:8080/db/main', {
  protocol: 'SQTP/1.0',
  timeout: 30000,
  debug: false
});
```

### 基本使用

#### Promise 方式（推荐）
```javascript
// 使用 async/await
async function queryUsers() {
  try {
    const result = await db.select('users')
      .where("age >= 18")
      .limit(10)
      .execute();
    console.log(result.data);
  } catch (err) {
    console.error(err);
  }
}

// 或使用 Promise 链
db.select('users')
  .where("age >= 18")
  .execute()
  .then(result => console.log(result.data))
  .catch(err => console.error(err));
```

#### 回调方式（传统）
```javascript
// 使用 error-first callback
db.select('users')
  .where("age >= 18")
  .limit(10)
  .execute(function(err, result) {
    if (err) {
      console.error(err);
      return;
    }
    console.log(result.data);
  });

// 嵌套回调示例
db.insert('users')
  .values({ name: 'Alice', age: 25 })
  .execute(function(err, insertResult) {
    if (err) return console.error(err);
    
    const userId = insertResult.headers['X-SQTP-Last-Insert-Id'];
    
    db.select('users')
      .where("id = " + userId)
      .execute(function(err, selectResult) {
        if (err) return console.error(err);
        console.log('新用户:', selectResult.data);
      });
  });
```

**💡 查看详细对比**：[回调方式 vs Promise 完整指南](./CALLBACK_VS_PROMISE.md)

## 📖 API 文档

两个库提供**完全相同的 API**，可以无缝切换。

### SELECT 查询

```javascript
// 基本查询
db.select('users')
  .columns('id', 'name', 'email')
  .where("status = 'active'")
  .orderBy('name ASC')
  .limit(10)
  .execute()
  .then(result => console.log(result.data));

// 复杂查询 - JOIN + WHERE-IN
db.select('users')
  .columns('users.id', 'users.name', 'COUNT(orders.id) AS order_count')
  .leftJoin('orders', 'users.id = orders.user_id')
  .whereIn('users.status', ['active', 'pending'])
  .groupBy('users.id', 'users.name')
  .having('COUNT(orders.id) > 0')
  .orderBy('order_count DESC')
  .limit(20)
  .execute();

// 使用 async/await（推荐）
async function getActiveUsers() {
  const result = await db.select('users')
    .where("status = 'active'")
    .execute();
  return result.data;
}
```

**SELECT 方法：**
- `.columns(...cols)` - 指定列（可多次调用）
- `.where(condition)` - WHERE 条件（可多次调用添加多个条件）
- `.whereIn(column, values)` - WHERE IN 条件
- `.orderBy(...orders)` - 排序
- `.groupBy(...cols)` - 分组
- `.having(condition)` - HAVING 条件
- `.limit(count)` - 限制行数
- `.offset(count)` - 偏移量（分页）
- `.distinct()` - 去重
- `.view(format)` - 视图格式：'object'（默认）, 'row', 'column'
- `.join(table, condition)` - INNER JOIN
- `.leftJoin(table, condition)` - LEFT JOIN

### INSERT 插入

```javascript
// 基本插入
db.insert('users')
  .values({
    name: 'Alice',
    email: 'alice@example.com',
    status: 'active'
  })
  .execute();

// 插入不存在（避免重复）
db.insert('users')
  .values({ email: 'bob@example.com', name: 'Bob' })
  .ifNotExists(true)
  .execute();
```

**INSERT 方法：**
- `.values(data)` - 要插入的数据对象
- `.ifNotExists(boolean)` - 如果不存在才插入

### UPDATE 更新

```javascript
// 条件更新
db.update('users')
  .set({ status: 'inactive', updated_at: 'CURRENT_TIMESTAMP' })
  .where("email = 'alice@example.com'")
  .execute();

// 更新全表（必须显式指定）
db.update('users')
  .set({ last_checked: 'CURRENT_TIMESTAMP' })
  .where('*')  // 安全机制：必须显式指定 '*'
  .execute();

// 使用 WHERE-IN
db.update('users')
  .set({ status: 'archived' })
  .whereIn('status', ['inactive', 'deleted'])
  .execute();
```

**UPDATE 方法：**
- `.set(data)` - 要更新的数据对象
- `.where(condition)` - WHERE 条件（**必需**，使用 `'*'` 更新全表）
- `.whereIn(column, values)` - WHERE IN 条件

### UPSERT 插入或更新

```javascript
// 基于 email 判断是否存在
db.upsert('users')
  .key('email')
  .values({
    email: 'charlie@example.com',
    name: 'Charlie',
    status: 'active'
  })
  .execute();

// 复合键
db.upsert('order_items')
  .key('order_id', 'product_id')
  .values({
    order_id: 1,
    product_id: 100,
    quantity: 5
  })
  .execute();
```

**UPSERT 方法：**
- `.key(...columns)` - 判断唯一性的键
- `.values(data)` - 要插入或更新的数据对象

### DELETE 删除

```javascript
// 条件删除
db.delete('users')
  .where("status = 'inactive'")
  .execute();

// 使用 WHERE-IN
db.delete('users')
  .whereIn('status', ['banned', 'deleted'])
  .execute();

// 删除全表（必须显式指定）
db.delete('users')
  .where('*')  // 安全机制
  .execute();
```

**DELETE 方法：**
- `.where(condition)` - WHERE 条件（**必需**，使用 `'*'` 删除全表）
- `.whereIn(column, values)` - WHERE IN 条件

### CREATE TABLE 创建表

```javascript
// 基本表
db.createTable('users')
  .ifNotExists(true)
  .column('id', 'INTEGER')
  .column('name', 'TEXT')
  .column('email', 'TEXT')
  .column('created_at', 'TEXT', 'DEFAULT CURRENT_TIMESTAMP')
  .primaryKey('id')
  .notNull('name', 'email')
  .unique('email')
  .autoinc('id')
  .execute();

// 复杂表 - 复合主键 + 外键
db.createTable('order_items')
  .column('order_id', 'INTEGER')
  .column('product_id', 'INTEGER')
  .column('quantity', 'INTEGER', 'DEFAULT 1')
  .column('price', 'REAL')
  .primaryKey('order_id', 'product_id')
  .notNull('order_id', 'product_id', 'price')
  .foreignKey('order_id REFERENCES orders(id) ON DELETE CASCADE')
  .foreignKey('product_id REFERENCES products(id)')
  .withoutRowid(true)
  .execute();

// 临时表
db.createTable('temp_data')
  .type('temporary')
  .column('id', 'INTEGER')
  .column('data', 'TEXT')
  .execute();
```

**CREATE TABLE 方法：**
- `.column(name, type, ...constraints)` - 添加列（可多次调用）
- `.primaryKey(...columns)` - 主键
- `.notNull(...columns)` - NOT NULL 约束
- `.unique(...columns)` - UNIQUE 约束（可多次调用创建多个）
- `.autoinc(column)` - AUTOINCREMENT
- `.foreignKey(definition)` - 外键约束（可多次调用）
- `.ifNotExists(boolean)` - IF NOT EXISTS
- `.type(tableType)` - 表类型：'temporary', 'memory'
- `.withoutRowid(boolean)` - WITHOUT ROWID

### DROP TABLE 删除表

```javascript
// 删除表
db.dropTable('users', true)  // true = IF EXISTS
  .then(result => console.log('Table dropped'));
```

### 事务控制

```javascript
// 使用 Promise 链
db.begin()
  .then(() => db.insert('users').values({name: 'Test'}).execute())
  .then(() => db.update('users').set({status: 'active'}).where("name = 'Test'").execute())
  .then(() => db.commit())
  .catch(err => {
    console.error(err);
    return db.rollback();
  });

// 使用 async/await（推荐）
async function transferBalance() {
  try {
    await db.begin();
    
    await db.update('accounts')
      .set({ balance: 'balance - 100' })
      .where("id = 1")
      .execute();
    
    await db.update('accounts')
      .set({ balance: 'balance + 100' })
      .where("id = 2")
      .execute();
    
    await db.commit();
  } catch (err) {
    await db.rollback();
    throw err;
  }
}

// 保存点
await db.begin();
await db.savepoint('sp1');
// ... 操作
await db.rollback(); // 回滚到 sp1
```

**事务方法：**
- `.begin()` - 开始事务
- `.commit()` - 提交事务
- `.rollback()` - 回滚事务
- `.savepoint(name)` - 创建保存点

## 🔄 两个库的对比

| 特性 | sqtp.fetch.js | sqtp.xhr.js |
|-----|--------------|-------------|
| **Promise 支持** | ✅ 原生 Promise | ⚠️ 手动包装 |
| **async/await** | ✅ 完美支持 | ✅ 支持 |
| **浏览器兼容性** | 现代浏览器 | 所有浏览器（含 IE11+） |
| **代码简洁度** | 更简洁 | 相对冗长 |
| **超时控制** | AbortController | XHR timeout 属性 |
| **文件大小** | 较小 (~19KB) | 较大 (~21KB) |
| **推荐场景** | 新项目、现代应用 | 需要广泛兼容性 |

**选择建议：**
- 🎯 **新项目**：使用 `sqtp.fetch.js`（更现代、更简洁）
- 🔧 **旧项目**：使用 `sqtp.xhr.js`（更好的兼容性）
- 💡 **切换成本**：API 完全相同，只需更换引入的文件

## 📋 响应格式

所有操作的响应都包含：

```javascript
{
  data: [...],        // 数据（数组或对象）
  headers: {          // HTTP 响应头
    'X-SQTP-Affected-Rows': '10',
    'X-SQTP-Total-Rows': '100',
    'X-SQTP-Last-Insert-Id': '42',
    ...
  },
  status: 200         // HTTP 状态码
}
```

**常用响应头：**
- `X-SQTP-Affected-Rows` - 受影响的行数
- `X-SQTP-Total-Rows` - 总行数（用于分页）
- `X-SQTP-Last-Insert-Id` - 最后插入的 ID
- `X-SQTP-Changes` - 更改的行数

## 🔍 调试模式

```javascript
const db = new SQTP('http://localhost:8080/db/main', {
  debug: true  // 开启调试日志
});

// 控制台输出：
// [SQTP Debug] Request: { method: 'SELECT', url: '...', headers: {...} }
// [SQTP Debug] Response: { status: 200, headers: {...} }
// [SQTP Debug] Data: [...]
```

## 🛡️ 错误处理

```javascript
// Promise catch
db.select('users')
  .execute()
  .then(result => console.log(result.data))
  .catch(err => {
    console.error('Error:', err.message);
  });

// async/await try-catch
try {
  const result = await db.select('users').execute();
  console.log(result.data);
} catch (err) {
  if (err.message.includes('timeout')) {
    console.error('请求超时');
  } else if (err.message.includes('HTTP 404')) {
    console.error('资源不存在');
  } else if (err.message.includes('HTTP 500')) {
    console.error('服务器错误');
  } else {
    console.error('未知错误:', err);
  }
}
```

## 📝 示例文件

- `sqtp.xhr.example.html` - XMLHttpRequest 版本的完整示例
- `sqtp.fetch.example.html` - Fetch API 版本的完整示例

在浏览器中打开这些文件可以测试各种操作（需要 SQTP 服务器运行）。

## 🔗 协议文档

SQTP/1.0 协议完整文档：

**协议概览：**
- [00-OVERVIEW.md](../docs/sqtp/00-OVERVIEW.md) - 协议概述、设计目标、核心概念

**数据查询（Data Query）：**
- [01-SELECT.md](../docs/sqtp/01-SELECT.md) - SELECT 查询操作

**数据操作（Data Manipulation）：**
- [02-INSERT.md](../docs/sqtp/02-INSERT.md) - INSERT 插入操作
- [03-UPDATE.md](../docs/sqtp/03-UPDATE.md) - UPDATE 更新操作
- [04-UPSERT.md](../docs/sqtp/04-UPSERT.md) - UPSERT 插入或更新操作
- [05-RESET.md](../docs/sqtp/05-RESET.md) - RESET 重置操作（DELETE + INSERT）
- [06-DELETE.md](../docs/sqtp/06-DELETE.md) - DELETE 删除操作

**数据定义（Data Definition）：**
- [07-SCHEMA.md](../docs/sqtp/07-SCHEMA.md) - CREATE TABLE/INDEX/TRIGGER, DROP, ALTER 操作

**事务控制（Transaction Control）：**
- [08-TRANSACTIONS.md](../docs/sqtp/08-TRANSACTIONS.md) - BEGIN/COMMIT/ROLLBACK/SAVEPOINT 操作

## 📄 许可证

MIT License
