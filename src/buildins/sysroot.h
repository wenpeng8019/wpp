/*
 * Built-in Resources Index
 * 
 * Auto-generated from buildins/ directory
 * Do not edit manually!
 */

#ifndef BUILDINS_SYSROOT_H
#define BUILDINS_SYSROOT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 内建文件信息结构（链表节点）
 */
typedef struct buildin_file_info {
    const struct buildin_file_info*  next;      /* 下一个节点 */
    const char*                     uri;        /* 资源 URI 路径 */
    const uint8_t*                  comp;       /* 压缩数据指针 */
    void*                           raw;        /* 解压后数据（NULL=未解压，懒加载） */
    void*                           vfile;      /* TCC 虚拟文件对象（NULL=未创建） */
    uint32_t                        comp_sz;    /* 压缩后大小 */
    uint32_t                        orig_sz;    /* 解压后大小 */
    uint32_t                        id;         /* 文件 ID（URI 哈希值） */
    uint32_t                        vref;       /* 虚拟文件引用计数 */
} buildin_file_info_st;

/**
 * 内建文件索引表（按 id 排序，支持二分查找）
 * 
 * 通过 URI 哈希进行二分查找定位资源
 * 数组大小: BUILDINS_FT_SIZE
 */
extern buildin_file_info_st* BUILDINS_FT[];

/**
 * 内建文件链表头
 * 
 * 遍历所有文件: for (const buildin_file_info_st *p = BUILDINS_LS; p; p = p->next)
 */
extern buildin_file_info_st* BUILDINS_LS;

extern buildin_file_info_st include_float_h;  // (642/1,930, 33.3%)
extern buildin_file_info_st include_stdalign_h;  // (187/354, 52.8%)
extern buildin_file_info_st include_stdarg_h;  // (181/335, 54.0%)
extern buildin_file_info_st include_stdatomic_h;  // (1,522/7,882, 19.3%)
extern buildin_file_info_st include_stdbool_h;  // (117/176, 66.5%)
extern buildin_file_info_st include_stddef_h;  // (536/1,072, 50.0%)
extern buildin_file_info_st include_stdnoreturn_h;  // (83/125, 66.4%)
extern buildin_file_info_st include_tccdefs_h;  // (3,662/13,020, 28.1%)
extern buildin_file_info_st include_tgmath_h;  // (828/3,954, 20.9%)
extern buildin_file_info_st include_varargs_h;  // (248/355, 69.9%)
extern buildin_file_info_st lib_libtcc1_a;  // (8,056/40,138, 20.1%)
extern buildin_file_info_st lib_runmain_o;  // (896/3,056, 29.3%)
extern buildin_file_info_st lib_sqtp_sqtp_fetch_js;  // (4,089/20,302, 20.1%)
extern buildin_file_info_st lib_sqtp_sqtp_xhr_callback_js;  // (3,638/16,509, 22.0%)
extern buildin_file_info_st lib_sqtp_sqtp_xhr_promise_js;  // (3,864/19,039, 20.3%)

/* 资源数量 */
#define BUILDINS_FT_SIZE 15
/* 总大小: 128,247 B, 压缩后: 28,549 B, 占比: 22.3% */

#ifdef __cplusplus
}
#endif

#endif /* BUILDINS_SYSROOT_H */
