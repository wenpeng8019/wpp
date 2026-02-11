/*
 * Virtual File System - Implementation
 * 
 * 虚拟文件抽象层实现
 */

#include "vfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/syscall.h>
#else
#include <sys/types.h>
#endif
#else
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

/* ============ 私有辅助函数 ============ */

#ifdef __linux__
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001u
#endif

static int create_memfd(const char *name) {
#ifdef SYS_memfd_create
    return (int)syscall(SYS_memfd_create, name, MFD_CLOEXEC);
#else
    (void)name;
    return -1;
#endif
}
#endif

/**
 * 创建真实的临时文件（fallback 方案）
 */
static int create_real_tmpfile(size_t size) {
    char path[] = "/tmp/vfile_real_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return -1;
    
    // 立即 unlink，文件会在关闭后自动删除
    unlink(path);
    
    // 设置文件大小
    if (size > 0 && ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

/**
 * 创建临时匿名文件描述符
 */
static int create_anonymous_fd(size_t size) {
#ifdef _WIN32
    char tmp_path[MAX_PATH];
    char tmp_file[MAX_PATH];
    HANDLE h = INVALID_HANDLE_VALUE;
    LARGE_INTEGER file_size;
    int fd;

    if (!GetTempPathA(sizeof(tmp_path), tmp_path))
        return -1;
    if (!GetTempFileNameA(tmp_path, "vfile", 0, tmp_file))
        return -1;

    h = CreateFileA(tmp_file, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_RANDOM_ACCESS,
                    NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;

    file_size.QuadPart = (LONGLONG)size;
    if (!SetFilePointerEx(h, file_size, NULL, FILE_BEGIN) || !SetEndOfFile(h)) {
        CloseHandle(h);
        return -1;
    }

    fd = _open_osfhandle((intptr_t)h, _O_RDWR | _O_BINARY);
    if (fd < 0) {
        CloseHandle(h);
        return -1;
    }
    return fd;
#else
    int fd = -1;

#ifdef __linux__
    // Linux: 优先使用 memfd_create（支持 lseek + read/write）
    fd = create_memfd("vfile");
    if (fd >= 0) {
        // 设置文件大小
        if (size > 0 && ftruncate(fd, (off_t)size) != 0) {
            close(fd);
            return -1;
        }
        return fd;
    }
#endif

    // macOS & Linux fallback: 使用临时文件（支持完整的随机访问）
    // 注意：macOS shm_open 不支持 lseek/read/write，只能用 mmap，不适合 TCC
    fd = create_real_tmpfile(size);
    return fd;
#endif
}

/**
 * 映射文件描述符到内存
 */
static void* map_fd_memory(int fd, size_t size, void **handle) {
#ifdef _WIN32
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    HANDLE mapping;
    void *addr;

    if (h == INVALID_HANDLE_VALUE)
        return NULL;

    mapping = CreateFileMappingA(h, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!mapping)
        return NULL;

    addr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!addr) {
        CloseHandle(mapping);
        return NULL;
    }

    if (handle)
        *handle = mapping;
    
    return addr;
#else
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
        return NULL;
    
    if (handle)
        *handle = NULL;
    
    return addr;
#endif
}

/**
 * 取消内存映射
 */
static void unmap_memory(void *addr, size_t size, void *handle) {
#ifdef _WIN32
    UnmapViewOfFile(addr);
    if (handle)
        CloseHandle((HANDLE)handle);
#else
    (void)handle;
    munmap(addr, size);
#endif
}

/* ============ 公共 API 实现 ============ */

/**
 * 打开虚拟文件
 * 
 * @param vf 虚拟文件对象（调用者分配）
 * @param uri 虚拟文件路径（会被复制）
 * @param mapped 是否使用内存映射（true=mmap, false=仅创建fd）
 * @return 0 成功，-1 失败
 */
int vfile_open(vfile_st* vf, const char* uri, bool mapped) {
    if (!vf || !uri) {
        fprintf(stderr, "vfile_open: NULL pointer\n");
        return -1;
    }

    // 初始化结构体
    memset(vf, 0, sizeof(vfile_st));
    vf->fd = -1;
    vf->uri = uri;  // 直接保存指针，生命周期由调用者管理

    // 创建匿名文件描述符（大小初始为 0，后续可扩展）
    vf->fd = create_anonymous_fd(0);
    if (vf->fd < 0) {
        fprintf(stderr, "vfile_open: Failed to create fd for %s\n", uri);
        return -1;
    }

    // 如果需要映射，创建内存映射
    if (mapped && vf->size > 0) {
        void *map_handle = NULL;
        vf->mem = map_fd_memory(vf->fd, vf->size, &map_handle);
        if (!vf->mem) {
            fprintf(stderr, "vfile_open: Failed to map fd for %s\n", uri);
            close(vf->fd);
            vf->fd = -1;
            return -1;
        }
        // 注意: map_handle 在 Windows 下需要，但这里简化处理
        // 实际使用中可能需要在 vfile_st 中添加 map_handle 字段
    }

    return 0;
}

/**
 * 写入数据到虚拟文件
 * 
 * 所有平台统一使用 write() 系统调用
 */
int vfile_write(vfile_st* vf, const void* data, size_t size) {
    if (!vf || vf->fd < 0 || !data || size == 0) {
        fprintf(stderr, "vfile_write: Invalid parameters\n");
        return -1;
    }

    // 设置文件大小
    if (ftruncate(vf->fd, (off_t)size) != 0) {
        fprintf(stderr, "vfile_write: ftruncate failed: %s\n", strerror(errno));
        return -1;
    }

    // 重置文件指针到开头
    if (lseek(vf->fd, 0, SEEK_SET) < 0) {
        fprintf(stderr, "vfile_write: lseek failed: %s\n", strerror(errno));
        return -1;
    }

    // 写入数据（循环写入以处理大文件）
    size_t total_written = 0;
    while (total_written < size) {
        ssize_t written = write(vf->fd, 
                                (const char*)data + total_written, 
                                size - total_written);
        if (written < 0) {
            fprintf(stderr, "vfile_write: write failed: %s\n", strerror(errno));
            return -1;
        }
        total_written += (size_t)written;
    }

    // 重置文件指针到开头（方便后续读取）
    lseek(vf->fd, 0, SEEK_SET);

    vf->size = size;
    return 0;
}

/**
 * 关闭虚拟文件
 * 
 * 释放所有资源：内存映射、文件描述符、URI字符串
 * 
 * @param vf 虚拟文件对象
 */
void vfile_close(vfile_st* vf) {
    if (!vf) {
        return;
    }

    // 取消内存映射
    if (vf->mem && vf->size > 0) {
        unmap_memory(vf->mem, vf->size, NULL);
        vf->mem = NULL;
    }

    // 关闭文件描述符
    if (vf->fd >= 0) {
        close(vf->fd);
        vf->fd = -1;
    }

    // URI 由调用者管理，不需要释放
    vf->uri = NULL;
    vf->size = 0;
}

