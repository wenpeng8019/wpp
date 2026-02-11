# Third Party Libraries

## TinyCC
tinycd 精简标准库仅包括代码编译所需的部分，如变量类型、内置函数、原子变量等。
不包括任何实际的函数和系统调用。所以实际调用都依赖于宿主环境提供的（标准）库。

### 构建配置

**CONFIG_TCC_PREDEFS=1** (编译时嵌入模式)
- **目的**: 将 `tccdefs.h` 的宏定义转换为 C 字符串，编译到 libtcc.a 中
- **实现**: `tccdefs.h` → `tccdefs_.h` (转换为字符串数组)，在 `tcc_predefs()` 时自动加载
- **影响**:
  - ✅ 启动时无需从文件系统读取 `tccdefs.h`
  - ✅ 减少运行时依赖，提高初始化速度
  - ⚠️ `buildins/include/tccdefs.h` 不应通过 `tcc_add_file()` 加载（会导致宏重定义）
  - ℹ️ 基础宏（`__SIZE_TYPE__`, `__PTRDIFF_TYPE__` 等）在 `tcc_new()` 时已内置

**对比模式** (CONFIG_TCC_PREDEFS=0)：运行时从文件系统读取 `#include <tccdefs.h>`

## uthash
单头文件哈希表库，用于 C 结构体的哈希表、动态数组、链表等数据结构。
- **项目地址**: https://github.com/troydhanson/uthash
- **许可证**: BSD License (见 LICENSES/UTHASH-LICENSE)
- **使用**: 仅需 `#include "third_party/uthash/src/uthash.h"`

## SQLite
嵌入式关系型数据库引擎。
- **版本**: 3.x
- **许可证**: Public Domain (见 LICENSES/SQLITE-LICENSE)

## zlib
通用压缩库。
- **版本**: 1.3.1
- **许可证**: zlib License (见 LICENSES/ZLIB-LICENSE)

## yyjson
高性能 JSON 解析库。
- **项目地址**: https://github.com/ibireme/yyjson
