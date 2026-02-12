# Buildins - 内置虚拟文件系统

## 什么是 Buildins？

Buildins 是 wpp 的核心特性之一，它将所有必需的文件（C 标准头文件、运行时库、示例页面等）**压缩并编译到可执行文件内部**，形成一个虚拟文件系统，类似于编译器的 sysroot 概念，但这些文件直接编译进二进制，无需外部依赖。

这使得 wpp 成为一个**真正的单文件 Web 服务器**，无需任何外部依赖，下载即可运行。

## 核心优势

### 🚀 零依赖部署
- 不需要安装任何 C 编译器或开发工具
- 不需要配置包含路径或库路径
- 一个可执行文件包含所有必需资源

### 📦 即开即用
- 内置完整的 C 标准库头文件（`stddef.h`, `stdio.h`, `stdlib.h` 等）
- 内置 TinyCC 运行时库
- 内置示例和欢迎页面

### ⚡ 快速访问
- 编译时预构建哈希表，零运行时开销
- 所有文件经过 zlib 压缩，体积小
- 内存映射访问，性能优异

## 默认包含的内容

### 1. C 标准头文件（必须）
位于虚拟路径 `/include/`，包括：
- **基础头文件**: `stddef.h`, `stdarg.h`, `stdbool.h`, `stdint.h`
- **类型定义**: `float.h`, `limits.h`
- **特殊支持**: `stdalign.h`, `stdatomic.h`, `stdnoreturn.h`, `tgmath.h`

这些头文件由 TinyCC 提供，确保 C 脚本能够正常编译。

### 2. 运行时库（必须）
位于虚拟路径 `/lib/`：
- TinyCC 运行时库（支持动态编译）
- SQTP 客户端库（位于 `/lib/sqtp/`）

### 3. 示例和欢迎页面（默认）
- `/hello.html` - 项目主页和功能介绍
- `/hello.c` - C CGI 示例程序（展示环境变量、请求信息等）

前两项（C 标准头文件和运行时库）是 wpp 必须的，第三项（示例页面）是默认包含的示例资源。

## 虚拟文件系统的工作原理

### 编译时
```bash
# 构建脚本扫描 buildins/ 目录（该目录结构和 sysroot 一样）
./tools/make_buildins.sh

# 生成的文件
src/buildins/sysroot.h  # 哈希表定义和配置
src/buildins/sysroot.c  # 压缩数据和哈希表实现
```

### 运行时
```c
// HTTP 服务器查找文件
buildin_file_info_st *file = buildins_find("/hello.html");

// TinyCC 查找头文件
#include <stdio.h>  // 自动从 buildins /include/stdio.h 加载
```

### 关键机制
1. **URI 哈希查找**: O(1) 平均时间复杂度
2. **按需解压**: 只有访问时才解压文件内容
3. **内存管理**: 解压缓存由 buildins 模块统一管理

## 如何自定义 Buildins

### 添加新文件

1. 将文件放入 `buildins/` 目录：
```bash
# 添加自定义主页
cp my-index.html buildins/index.html

# 添加静态资源
cp logo.png buildins/assets/logo.png
```

2. 重新构建：
```bash
./tools/make_buildins.sh  # 重新生成哈希表
make                       # 重新编译
```

3. 访问文件：
```
http://localhost:8080/index.html
http://localhost:8080/assets/logo.png
```

wpp 文件访问优先级

WPP 的文件查找顺序：
1. **文件系统**：首先检查磁盘上的实际文件
2. **Buildins**：如果文件不存在，从虚拟文件系统查找

这意味着：
- 你可以通过创建同名文件来**覆盖** buildins 中的文件
- buildins 文件作为默认资源和**回退选项**

示例：
```bash
# buildins 中有 /hello.html
curl http://localhost:8080/hello.html  # → 返回 buildins 版本

# 在当前目录创建同名文件
echo "My Custom Page" > hello.html
curl http://localhost:8080/hello.html  # → 返回自定义版本
```

## 性能特性

### 编译时优化
- **自适应哈希表大小**: 根据文件数量自动调整
- **冲突链优化**: 确保最大桶深度 ≤ 3
- **负载因子平衡**: 维持在 0.3 - 0.75 之间

### 运行时特性
- **查找速度**: O(1) 平均，最坏 O(3)
- **内存占用**: 仅存储压缩数据，按需解压
- **零锁开销**: 只读数据结构，天然线程安全

## 技术细节

如果你对 Buildins 的实现细节感兴趣（哈希表算法、压缩策略、冲突解决等），请参阅：

📚 [Buildins 自适应静态哈希表技术文档](BUILDINS_HYBRID_SEARCH.md)

## 使用场景

### 1. 快速原型开发
```bash
# 无需安装任何工具，立即开始开发
./wpp
# 浏览器访问 http://localhost:8080/hello.c 查看示例
```

### 2. 便携式 Web 应用
```bash
# 单文件分发，无需担心依赖
scp wpp user@remote:/usr/local/bin/
ssh user@remote 'wpp /var/www'
```

### 3. 嵌入式系统
```bash
# 最小化依赖，适合资源受限环境
./wpp --port 80
```

### 4. 教学演示
- 学生只需下载一个文件即可开始学习 C CGI 编程
- 无需配置复杂的编译环境
- 示例代码即时运行

## 限制和注意事项

### Buildins 文件的限制
- **只读**: 虚拟文件系统中的文件不能被修改（编译进二进制）
- **固定内容**: 更新 buildins 文件需要重新编译服务器
- **磁盘覆盖**: 文件系统中的同名文件会**隐藏** buildins 版本

### 最佳实践
1. **开发阶段**: 使用文件系统中的文件（便于修改）
2. **生产阶段**: 将稳定资源编译进 buildins（简化部署）
3. **混合使用**: 静态资源用 buildins，动态内容用文件系统

## 总结

Buildins 虚拟文件系统是 wpp "零依赖、即开即用" 理念的核心实现：

- ✅ 单一可执行文件包含所有必需资源
- ✅ 无需外部依赖或配置
- ✅ 高性能哈希查找和压缩存储
- ✅ 灵活的覆盖机制，平衡便利与可定制性

这使得 WPP 成为最易部署的 C CGI Web 服务器。
