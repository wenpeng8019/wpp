# TinyCC 虚拟文件（WPP Patch）

本项目对 TinyCC 做了小幅补丁，使二进制输入（例如 libtcc1.a、runmain.o）
可以直接从内存提供，而不必先落盘。

## 背景与动机

TinyCC 的公开接口只接受文件名（`tcc_add_file()` 以及内部 `tcc_add_support()`），
无法直接使用内嵌在可执行文件中的运行时对象。WPP 为了保持运行时常驻内存，
引入“虚拟文件”机制，让 TinyCC 以“可打开的 fd”形式读取内存数据。

## 新增 API

```c
int tcc_add_virtual_file(TCCState *s, const char *name,
                         const void *data, unsigned long size);

int tcc_add_virtual_file_fd(TCCState *s, const char *name,
                            int fd, unsigned long size,
                            int take_ownership);
```

### tcc_add_virtual_file

- 基于数据（data/size）注册虚拟文件。
- `name` 建议唯一；查找时会同时按完整路径和 basename 匹配。
- `data` 必须在 TCC 使用期间保持有效（例如静态区或持久缓冲）。

### tcc_add_virtual_file_fd

- 基于 fd 注册虚拟文件（FD-backed）。
- 打开时才 `dup()`，尽量保持 0 拷贝读取路径。
- `take_ownership` 决定 TCC 是否负责 `close(fd)`。
- `size` 仅用于记录大小，函数本身不校验 fd 的可用性。

## 工作机制

1) `_tcc_open()` 在打开文件时先尝试命中虚拟文件。
2) 命中则走 `tcc_open_virtual_file()`，获取可读 fd。
3) 命不中才回退到磁盘 `open()`。

对于二进制支持库（`tcc_add_support()`），也优先走虚拟文件。

## 平台细节（tcc_open_virtual_file）

### FD-backed（vf->fd >= 0）

- 直接 `dup()` 并 `lseek` 回到开头，属于 0 拷贝路径。

### Data-backed（vf->data）

- **Linux**：优先 `memfd_create()`，失败后回退 `shm_open()`。
- **macOS**：`shm_open()` + `mmap` + `memcpy` 写入。
- **Windows**：临时文件句柄 + `_open_osfhandle()`。
- 对所有平台：若 `lseek` 不可用，回退到临时文件方案（写入后 `unlink`，
  仅保留 fd 访问）。

## WPP 集成示例

WPP 将 TinyCC 的库搜索路径设为虚拟根：

- `tcc_set_lib_path(s, "/__wpp_tcc")`
- `tcc_add_library_path(s, "/__wpp_tcc")`

随后注册运行时对象：

- `/__wpp_tcc/libtcc1.a`
- `/__wpp_tcc/runmain.o`

## 代码位置

- API 声明：third_party/tinycc/libtcc.h
- 虚拟文件结构：third_party/tinycc/tcc.h
- 打开与拦截逻辑：third_party/tinycc/libtcc.c

如需移除补丁，可改回“写临时文件 + 使用原始 API”的方式，并删除上述新增 API。
