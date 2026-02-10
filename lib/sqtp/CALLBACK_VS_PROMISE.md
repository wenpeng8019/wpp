# SQTP JavaScript Client - 两种风格对比

## 📚 概述

SQTP 提供了两种调用风格：

1. **`sqtp.xhr.promise.js`** - Promise/Async-Await (现代推荐)
2. **`sqtp.xhr.callback.js`** - 传统回调方式 (兼容性好)

## 🔄 传统回调方式 (Callback Style)

### 特点
- ✅ **兼容性好**：支持所有浏览器（包括 IE11）
- ✅ **熟悉的模式**：Node.js error-first callback 标准
- ✅ **无需 polyfill**：不依赖 Promise
- ❌ **回调地狱**：多层嵌套难以维护
- ❌ **错误处理繁琐**：每个回调都要检查错误

### 使用方式

```javascript
// 引入回调版本
const db = new SQTPCallback('http://localhost:8080/db/test');

// 基本查询
db.select('users')
  .where("age >= 18")
  .execute(function(err, result) {
    if (err) {
      console.error('查询失败:', err);
      return;
    }
    console.log('查询成功:', result.data);
  });

// 插入数据
db.insert('users')
  .values({ name: 'Alice', age: 25 })
  .execute(function(err, result) {
    if (err) {
      console.error('插入失败:', err);
      return;
    }
    console.log('插入成功, ID:', result.headers['X-SQTP-Last-Insert-Id']);
  });

// 多步骤操作（嵌套）
db.insert('users')
  .values({ name: 'Bob', age: 30 })
  .execute(function(err, insertResult) {
    if (err) return console.error(err);
    
    // 插入后查询
    db.select('users')
      .where("name = 'Bob'")
      .execute(function(err, selectResult) {
        if (err) return console.error(err);
        
        console.log('找到用户:', selectResult.data);
      });
  });
```

### Error-First Callback 模式

所有回调函数都遵循 `(err, result)` 签名：

```javascript
function callback(err, result) {
  if (err) {
    // 第一个参数：错误对象（失败时）
    console.error('操作失败:', err.message);
    console.error('HTTP 状态码:', err.status);
    console.error('响应内容:', err.response);
    return;
  }
  
  // 第二个参数：成功结果（成功时）
  console.log('状态码:', result.status);
  console.log('响应头:', result.headers);
  console.log('数据:', result.data);
}
```

## 🚀 Promise/Async-Await (现代推荐)

### 特点
- ✅ **代码清晰**：避免回调地狱
- ✅ **错误处理统一**：try-catch 或 .catch()
- ✅ **现代标准**：ES6+ 推荐方案
- ✅ **更好的工具支持**：IDE 自动补全更准确
- ❌ **兼容性**：需要 Promise 支持（或 polyfill）

### 使用方式

```javascript
// 引入 Promise 版本
const db = new SQTP('http://localhost:8080/db/test');

// 使用 async/await
async function example() {
  try {
    // 基本查询
    const result = await db.select('users')
      .where("age >= 18")
      .execute();
    console.log('查询成功:', result.data);
    
    // 插入数据
    const insertResult = await db.insert('users')
      .values({ name: 'Alice', age: 25 })
      .execute();
    console.log('插入成功, ID:', insertResult.headers['X-SQTP-Last-Insert-Id']);
    
    // 多步骤操作（顺序清晰）
    const insertResult2 = await db.insert('users')
      .values({ name: 'Bob', age: 30 })
      .execute();
    
    const selectResult = await db.select('users')
      .where("name = 'Bob'")
      .execute();
    
    console.log('找到用户:', selectResult.data);
    
  } catch (err) {
    console.error('操作失败:', err.message);
  }
}

// 或使用 Promise 链
db.select('users').execute()
  .then(result => {
    console.log('成功:', result.data);
  })
  .catch(err => {
    console.error('失败:', err.message);
  });
```

## 📊 对比表格

| 特性 | 回调方式 | Promise/Async-Await |
|------|---------|---------------------|
| **浏览器兼容** | IE6+ | IE11+ (或需 polyfill) |
| **代码可读性** | ⭐⭐ | ⭐⭐⭐⭐⭐ |
| **错误处理** | 每层检查 | 统一 try-catch |
| **学习曲线** | 简单 | 需要理解 Promise |
| **多步骤操作** | 嵌套深 | 顺序清晰 |
| **并发控制** | 困难 | Promise.all() |
| **现代性** | 传统 | 推荐 |

## 🎯 选择建议

### 使用回调方式当：
- 维护老旧项目（不支持 ES6）
- 目标浏览器不支持 Promise
- 团队更熟悉回调模式
- 不想引入 Promise polyfill

### 使用 Promise 方式当：
- 新项目（推荐）
- 目标浏览器支持 ES6+
- 需要复杂的异步流程控制
- 想要更好的代码可读性

## 📝 完整 API 对比

### SELECT 查询

```javascript
// 回调方式
db.select('users')
  .columns('id', 'name', 'email')
  .where("age >= 18")
  .orderBy('name', 'ASC')
  .limit(10)
  .execute(function(err, result) {
    if (err) return console.error(err);
    console.log(result.data);
  });

// Promise 方式
const result = await db.select('users')
  .columns('id', 'name', 'email')
  .where("age >= 18")
  .orderBy('name', 'ASC')
  .limit(10)
  .execute();
console.log(result.data);
```

### INSERT 插入

```javascript
// 回调方式
db.insert('users')
  .values({ name: 'Alice', age: 25 })
  .ifNotExists(true)
  .execute(function(err, result) {
    if (err) return console.error(err);
    console.log('ID:', result.headers['X-SQTP-Last-Insert-Id']);
  });

// Promise 方式
const result = await db.insert('users')
  .values({ name: 'Alice', age: 25 })
  .ifNotExists(true)
  .execute();
console.log('ID:', result.headers['X-SQTP-Last-Insert-Id']);
```

### UPDATE 更新

```javascript
// 回调方式
db.update('users')
  .set({ age: 26 })
  .where("name = 'Alice'")
  .execute(function(err, result) {
    if (err) return console.error(err);
    console.log('影响行数:', result.headers['X-SQTP-Changes']);
  });

// Promise 方式
const result = await db.update('users')
  .set({ age: 26 })
  .where("name = 'Alice'")
  .execute();
console.log('影响行数:', result.headers['X-SQTP-Changes']);
```

### DELETE 删除

```javascript
// 回调方式
db.delete('users')
  .where("age < 18")
  .execute(function(err, result) {
    if (err) return console.error(err);
    console.log('删除行数:', result.headers['X-SQTP-Changes']);
  });

// Promise 方式
const result = await db.delete('users')
  .where("age < 18")
  .execute();
console.log('删除行数:', result.headers['X-SQTP-Changes']);
```

### 事务操作

```javascript
// 回调方式（嵌套）
db.begin(function(err) {
  if (err) return console.error(err);
  
  db.insert('users').values({...}).execute(function(err) {
    if (err) {
      return db.rollback(function() {
        console.error('回滚成功');
      });
    }
    
    db.commit(function(err) {
      if (err) return console.error(err);
      console.log('提交成功');
    });
  });
});

// Promise 方式（清晰）
try {
  await db.begin();
  await db.insert('users').values({...}).execute();
  await db.commit();
  console.log('提交成功');
} catch (err) {
  await db.rollback();
  console.error('回滚成功');
}
```

## 🔧 实际应用示例

### 场景：用户注册流程

#### 回调方式
```javascript
function registerUser(userData, callback) {
  // 1. 检查用户是否存在
  db.select('users')
    .where("email = '" + userData.email + "'")
    .execute(function(err, result) {
      if (err) return callback(err);
      
      if (result.data.length > 0) {
        return callback(new Error('邮箱已存在'));
      }
      
      // 2. 插入用户
      db.insert('users')
        .values(userData)
        .execute(function(err, result) {
          if (err) return callback(err);
          
          const userId = result.headers['X-SQTP-Last-Insert-Id'];
          
          // 3. 创建用户配置
          db.insert('user_settings')
            .values({ user_id: userId, theme: 'light' })
            .execute(function(err) {
              if (err) return callback(err);
              
              // 4. 返回完整用户信息
              db.select('users')
                .where("id = " + userId)
                .execute(callback);
            });
        });
    });
}
```

#### Promise 方式
```javascript
async function registerUser(userData) {
  // 1. 检查用户是否存在
  const existing = await db.select('users')
    .where("email = '" + userData.email + "'")
    .execute();
  
  if (existing.data.length > 0) {
    throw new Error('邮箱已存在');
  }
  
  // 2. 插入用户
  const result = await db.insert('users')
    .values(userData)
    .execute();
  
  const userId = result.headers['X-SQTP-Last-Insert-Id'];
  
  // 3. 创建用户配置
  await db.insert('user_settings')
    .values({ user_id: userId, theme: 'light' })
    .execute();
  
  // 4. 返回完整用户信息
  return await db.select('users')
    .where("id = " + userId)
    .execute();
}
```

**代码行数对比：回调方式 35 行 vs Promise 方式 20 行**

## 🌐 XMLHttpRequest 内部实现

两个版本在底层都使用原生 XMLHttpRequest，只是封装方式不同：

### 回调版本 (sqtp.xhr.callback.js)
```javascript
SQTP.prototype._request = function(method, url, headers, body, callback) {
  const xhr = new XMLHttpRequest();
  xhr.open(method, url, true);
  
  // 传统事件处理
  xhr.onload = function() {
    if (xhr.status >= 200 && xhr.status < 300) {
      callback(null, result);  // 成功
    } else {
      callback(new Error('Request failed'), null);  // 失败
    }
  };
  
  xhr.onerror = function() {
    callback(new Error('Network error'), null);
  };
  
  xhr.send(body);
};
```

### Promise 版本 (sqtp.xhr.js)
```javascript
SQTP.prototype._request = function(method, url, headers, body) {
  return new Promise(function(resolve, reject) {
    const xhr = new XMLHttpRequest();
    xhr.open(method, url, true);
    
    // Promise 封装
    xhr.onload = function() {
      if (xhr.status >= 200 && xhr.status < 300) {
        resolve(result);  // 成功
      } else {
        reject(new Error('Request failed'));  // 失败
      }
    };
    
    xhr.onerror = function() {
      reject(new Error('Network error'));
    };
    
    xhr.send(body);
  });
};
```

**核心区别**：
- 回调版本：接受 `callback` 参数，直接调用
- Promise 版本：返回 `Promise`，通过 `resolve/reject` 通知结果

两者底层使用的都是**原生 XMLHttpRequest**，没有使用 Fetch API 或其他现代异步框架。

## 📦 文件清单

| 文件 | 说明 |
|------|------|
| `sqtp.xhr.promise.js` | Promise 版本（推荐） |
| `sqtp.xhr.callback.js` | 回调版本（兼容） |
| `sqtp.xhr.test.html` | Promise 版本测试套件 |
| `sqtp.callback.example.html` | 两种方式对比示例 |

## 🚦 快速开始

### 使用回调版本
```html
<script src="sqtp.xhr.callback.js"></script>
<script>
  const db = new SQTPCallback('http://localhost:8080/db/test');
  
  db.select('users').execute(function(err, result) {
    if (err) return console.error(err);
    console.log(result.data);
  });
</script>
```

### 使用 Promise 版本
```html
<script src="sqtp.xhr.promise.js"></script>
<script>
  const db = new SQTP('http://localhost:8080/db/test');
  
  db.select('users').execute()
    .then(result => console.log(result.data))
    .catch(err => console.error(err));
    
  // 或使用 async/await
  async function query() {
    const result = await db.select('users').execute();
    console.log(result.data);
  }
</script>
```

## 💡 小贴士

1. **新项目优先使用 Promise 版本**：代码更简洁，维护更容易
2. **回调版本适合老项目**：无需修改现有代码风格
3. **两个版本 API 完全一致**：只是 execute() 的调用方式不同
4. **可以混用**：在同一个项目中根据需求选择不同版本
5. **调试模式都支持**：传入 `{ debug: true }` 查看详细日志

## 🔗 相关资源

- [XMLHttpRequest MDN 文档](https://developer.mozilla.org/en-US/docs/Web/API/XMLHttpRequest)
- [Promise MDN 文档](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)
- [Async/Await 教程](https://javascript.info/async-await)
- [Error-First Callbacks 规范](http://fredkschott.com/post/2014/03/understanding-error-first-callbacks-in-node-js/)
