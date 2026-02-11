/*
 * Built-in Resources - Runtime API
 * 
 * 提供运行时解压和使用内建资源的接口
 */

#ifndef BUILDINS_H
#define BUILDINS_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include "buildins/sysroot.h"
#include "vfile.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化所有内建资源
 * 
 * [已废弃] 预构建的静态数据可以直接使用，无需初始化。
 * 该函数保留仅用于向后兼容，实际上什么也不做。
 * 
 * @return 始终返回 0
 * @deprecated 不再需要调用，静态数据可直接访问
 */
int buildins_init(void);

/**
 * 清理所有内建资源
 * 
 * 释放所有已解压的内存和虚拟文件对象
 */
void buildins_cleanup(void);

/**
 * 根据 URI 查找资源
 * 
 * 支持两种查找策略：
 * 1. 如果资源数 >= 50 且启用了静态哈希表，使用哈希表查找 O(1)
 * 2. 否则使用二分查找 O(log n)
 * 
 * @param uri 资源URI
 * @return 资源信息结构指针，如果未找到返回 NULL
 */
buildin_file_info_st* buildins_find(const char *uri);

/**
 * 获取解压后的资源数据
 * 
 * 如果资源尚未解压（raw == NULL），则分配内存并解压。
 * 解压后的数据缓存在 raw 字段中，后续调用直接返回。
 * 
 * @param info 资源信息结构（必须是可写的）
 * @return 解压后的数据指针，失败返回 NULL
 */
void* buildins_decompressed(buildin_file_info_st *info);

/**
 * 获取 vfile 对象（如果不存在则创建）
 * 
 * 懒加载创建 vfile，如果 vfile 已存在则增加引用计数。
 * 调用者使用完毕后应调用 buildins_release_vfile() 释放。
 * 
 * @param info 资源信息结构
 * @return vfile 对象指针，失败返回 NULL
 */
vfile_st* buildins_acquire_vfile(buildin_file_info_st *info);

/**
 * 释放资源的虚拟文件
 * 
 * 减少引用计数，当 vref 降为 0 时释放虚拟文件对象。
 * 注意：不会释放 raw 数据，只释放虚拟文件。
 * 
 * @param info 资源信息结构
 */
void buildins_release_vfile(buildin_file_info_st *info);

/**
 * 将内置文件解压到临时文件（返回 FILE*）
 * 
 * 创建一个匿名临时文件，解压数据并写入，文件在关闭时自动删除。
 * 调用者负责调用 fclose() 关闭文件。
 * 
 * @param info 资源信息结构
 * @return FILE* 指针（已定位到文件开头），失败返回 NULL
 */
FILE* buildins_to_tmp_file(buildin_file_info_st *info);

/**
 * 将内置文件解压到临时文件（返回 fd）
 * 
 * 创建一个命名临时文件，解压数据并写入，返回文件描述符。
 * 文件位于 /tmp 目录，调用者负责关闭 fd 和删除文件。
 * 
 * @param info 资源信息结构
 * @param out_path 可选的输出参数，返回临时文件路径（如果非 NULL，需调用者 free）
 * @return 文件描述符（已定位到文件开头），失败返回 -1
 */
int buildins_to_tmp_fd(buildin_file_info_st *info, char **out_path);

#ifdef __cplusplus
}
#endif

#endif /* BUILDINS_H */
