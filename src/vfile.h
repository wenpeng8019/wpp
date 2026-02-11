/*
 * Virtual File System - Header
 */

#ifndef VFILE_H
#define VFILE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 虚拟文件对象
 * 
 * 可以表示两种类型的文件：
 * - mem != NULL: 内存虚拟文件
 * - fd >= 0: 文件描述符虚拟文件
 */
typedef struct vfile {
    int                 fd;         /* 文件描述符（-1=未使用FD模式） */
    const char*         uri;        /* 虚拟文件路径（⚠️ 调用者需保证生命周期） */
    void*               mem;        /* 内存数据指针（NULL=未使用内存模式） */
    size_t              size;       /* 文件/内存大小 */
} vfile_st;

///////////////////////////////////////////////////////////////////////////////

int
vfile_open(vfile_st* vf, const char* uri, bool mapped);

int
vfile_write(vfile_st* vf, const void* data, size_t size);

void
vfile_close(vfile_st* vf);


///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* VFILE_H */
