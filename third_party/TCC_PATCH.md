# TinyCC 文件打开回调（WPP Patch）

本项目对 TinyCC 做了轻量级补丁，添加文件打开回调机制，允许外部拦截文件打开操作。

## 背景与动机

TinyCC 原本的文件访问完全依赖 `open()` 系统调用，无法：
- 从内存中提供文件内容（如压缩的 libtcc1.a）
- 从虚拟文件系统读取头文件
- 实现自定义文件查找逻辑

WPP 需要将 TCC 运行时库（libtcc1.a、runmain.o）和标准头文件嵌入可执行文件，通过虚拟文件系统提供，因此添加了此回调机制。

## 补丁内容

### 1. 新增 API（libtcc.h）

```c
typedef int (*TCCFileOpenCallback)(void *opaque, const char *filename);

LIBTCCAPI void tcc_set_file_open_callback(TCCState *s, void *opaque, 
                                          TCCFileOpenCallback callback);
```

### 2. TCCState 新增字段（tcc.h）

```c
struct TCCState {
    // ...
    void *file_open_opaque;
    int (*file_open_callback)(void *opaque, const char *filename);
    // ...
};
```

### 3. 拦截点（libtcc.c）

#### 拦截点 1：`_tcc_open()` - 通用文件打开
- 覆盖范围：`#include` 预处理指令、`tcc_add_file()` 调用
- 位置：third_party/tinycc/libtcc.c:785-787

```c
fd = -1;
if (s1->file_open_callback) {
    fd = s1->file_open_callback(s1->file_open_opaque, filename);
}
if (fd < 0) {
    fd = open(filename, O_RDONLY | O_BINARY);  // fallback
}
```

#### 拦截点 2：`tcc_add_support()` - TCC 运行时库
- 覆盖范围：libtcc1.a、runmain.o、bcheck.o 等
- 位置：third_party/tinycc/libtcc.c:1355-1361

```c
fd = -1;
if (s1->file_open_callback) {
    fd = s1->file_open_callback(s1->file_open_opaque, filename);
}
if (fd >= 0) {
    return tcc_add_binary(s1, AFF_TYPE_BIN | AFF_PRINT_ERROR, filename, fd);
}
// fallback 到原有库路径搜索
return tcc_add_dll(s1, filename, AFF_PRINT_ERROR);
```

## 回调协议

**函数签名**：
```c
int callback(void *opaque, const char *filename);
```

**返回值**：
- `>= 0`：已打开的文件描述符（fd），TCC 将使用此 fd 读取文件并负责关闭
- `< 0`：未处理此文件，TCC fallback 到系统 `open()`

**注意事项**：
- 回调必须返回可读的 fd（`O_RDONLY`）
- fd 必须定位到文件开头（`lseek(fd, 0, SEEK_SET)`）
- TCC 会在读取完成后自动 `close(fd)`

## WPP 集成示例

### 1. 设置回调（src/tcc_evn.c）

```c
static int tcc_file_open_callback(void *opaque, const char *filename) {
    buildin_file_info_st *node = buildins_find(filename);
    if (!node) {
        return -1;  // 未命中，TCC 走正常文件系统
    }
    
    vfile_st *vf = buildins_acquire_vfile(node);
    int fd = dup(vf->fd);  // 复制 fd，TCC 会关闭它
    lseek(fd, 0, SEEK_SET);
    return fd;
}

tcc_set_file_open_callback(s, NULL, tcc_file_open_callback);
```

### 2. 配置虚拟路径

```c
tcc_set_lib_path(s, "/lib");                  // buildins 虚拟路径
tcc_add_sysinclude_path(s, "/include");       // buildins 头文件
```

### 3. 加载虚拟文件

```c
tcc_add_file(s, "/lib/libtcc1.a");  // 回调拦截，返回虚拟文件 fd
tcc_add_file(s, "/lib/runmain.o");  // 回调拦截，返回虚拟文件 fd
```

## 代码位置

- **API 声明**：[third_party/tinycc/libtcc.h](tinycc/libtcc.h)
- **结构定义**：[third_party/tinycc/tcc.h](tinycc/tcc.h)
- **实现代码**：[third_party/tinycc/libtcc.c](tinycc/libtcc.c)

## 补丁影响

- ✅ **非侵入性**：仅添加回调钩子，不影响原有逻辑
- ✅ **可选功能**：未设置回调时行为与原版 TCC 完全一致
- ✅ **零性能损耗**：未设置回调时仅增加一次 `if (callback)` 判断
- ✅ **向后兼容**：不修改任何现有 API 语义

## 移除补丁

如需移除此补丁：
1. 删除 `tcc.h` 中的两个字段
2. 删除 `libtcc.h` 中的 API 声明
3. 删除 `libtcc.c` 中的三处回调调用
4. WPP 改用临时文件方案（写入 /tmp，使用原始 API）
