/*
 * Built-in Resources Index
 * 
 * Auto-generated from buildins/ directory
 * Do not edit manually!
 */

#ifndef BUILDINS_SYSROOT_H
#define BUILDINS_SYSROOT_H

#include "../buildins.h"

#ifdef __cplusplus
extern "C" {
#endif

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

extern buildin_dir_info_st  _;  // [DIR]
extern buildin_file_info_st hello_c;  // (2,801/12,148, 23.1%)
extern buildin_file_info_st hello_html;  // (2,104/6,968, 30.2%)
extern buildin_dir_info_st  _include;  // [DIR]
extern buildin_file_info_st include_float_h;  // (662/1,930, 34.3%)
extern buildin_file_info_st include_stdalign_h;  // (210/354, 59.3%)
extern buildin_file_info_st include_stdarg_h;  // (202/335, 60.3%)
extern buildin_file_info_st include_stdatomic_h;  // (1,546/7,882, 19.6%)
extern buildin_file_info_st include_stdbool_h;  // (139/176, 79.0%)
extern buildin_file_info_st include_stddef_h;  // (557/1,072, 52.0%)
extern buildin_file_info_st include_stdnoreturn_h;  // (109/125, 87.2%)
extern buildin_file_info_st include_tgmath_h;  // (849/3,954, 21.5%)
extern buildin_dir_info_st  _lib;  // [DIR]
extern buildin_file_info_st lib_libtcc1_a;  // (8,078/40,138, 20.1%)
extern buildin_file_info_st lib_runmain_o;  // (918/3,056, 30.0%)
extern buildin_dir_info_st  _lib_sqtp;  // [DIR]
extern buildin_file_info_st lib_sqtp_sqtp_fetch_js;  // (4,115/20,302, 20.3%)
extern buildin_file_info_st lib_sqtp_sqtp_xhr_callback_js;  // (3,671/16,509, 22.2%)
extern buildin_file_info_st lib_sqtp_sqtp_xhr_promise_js;  // (3,896/19,039, 20.5%)

/* 资源数量 */
#define BUILDINS_FT_SIZE 19
/* 总大小: 133,988 B, 压缩后: 29,857 B, 占比: 22.3% */

#ifdef __cplusplus
}
#endif

#endif /* BUILDINS_SYSROOT_H */
