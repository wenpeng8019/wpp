# Third Party Libraries

## TinyCC
tinycd 精简标准库仅包括代码编译所需的部分，如变量类型、内置函数、原子变量等。
不包括任何实际的函数和系统调用。所以实际调用都依赖于宿主环境提供的（标准）库。

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
