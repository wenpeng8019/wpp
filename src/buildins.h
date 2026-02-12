/*
 * Built-in Resources - Runtime API
 * 
 * 提供运行时解压和使用内建资源的接口
 */

#ifndef BUILDINS_H
#define BUILDINS_H

/* ===== 哈希表配置参数 ===== */
#define HASH_INDEX_THRESHOLD        50    /* 资源数量达到该值时启用哈希表查找 */
#define HASH_MAX_DEPTH_THRESHOLD    3     /* 单个哈希桶最大深度阈值 */
#define HASH_MIN_LOAD_FACTOR        0.3   /* 哈希表最小负载因子 */
#define HASH_MAX_ITERATIONS         5     /* 自适应调整最大迭代次数 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>
#include <string.h>
#include <stdio.h>
#include "vfile.h"

#ifdef __cplusplus
extern "C" {
#endif
///////////////////////////////////////////////////////////////////////////////

/**
 * 内建目录信息结构
 * 
 * 注意：前4个字段与 buildin_file_info_st 对齐以支持类型转换
 * flag 字段固定为 (void*)-1 用于区分目录和文件
 */
typedef struct buildin_dir_info {
    struct buildin_file_info*       next;       /* 下一个节点（文件或目录） */
    uint32_t                        id;         /* 目录 ID（URI 哈希值） */
    const char*                     uri;        /* 资源 URI 路径 */
    struct buildin_dir_info*        parent;     /* 父目录（根目录为 NULL） */
    const uint8_t*                  flag;       /* 目录标识：固定为 (void*)-1 */
} buildin_dir_info_st;

/**
 * 内建文件信息结构（链表节点）
 * 
 * 注意：前4个字段与 buildin_dir_info_st 对齐以支持类型转换
 */
typedef struct buildin_file_info {
    struct buildin_file_info*       next;       /* 下一个节点（文件或目录） */
    uint32_t                        id;         /* 文件 ID（URI 哈希值） */
    const char*                     uri;        /* 资源 URI 路径 */
    buildin_dir_info_st*            dir;        /* 文件所属目录 */
    const uint8_t*                  comp;       /* 压缩数据指针 */
    void*                           raw;        /* 解压后数据（NULL=未解压，懒加载） */
    void*                           vfile;      /* TCC 虚拟文件对象（NULL=未创建） */
    uint32_t                        comp_sz;    /* 压缩后大小 */
    uint32_t                        orig_sz;    /* 解压后大小 */
    uint32_t                        vref;       /* 虚拟文件引用计数 */
} buildin_file_info_st;

/**
 * 目录标识：当 buildin_file_info_st.comp 等于此值时，表示该条目是目录而非文件
 */
#define BUILDINS_DIR_FLAG ((const uint8_t*)(uintptr_t)-1)

///////////////////////////////////////////////////////////////////////////////

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
 * 判断 buildins 条目是否为目录
 * @param info buildins 条目
 * @return 1=目录, 0=文件
 */
int buildins_is_dir(const buildin_file_info_st *info);

/**
 * 获取解压后的资源数据
 * 
 * 如果资源尚未解压（raw == NULL），则分配内存并解压。
 * 解压后的数据缓存在 raw 字段中，后续调用直接返回。
 * 
 * 特殊情况：
 * - 空文件（orig_sz=0）：返回 (void*)1 作为标记，表示已处理但无数据
 * - 目录：应使用 buildins_is_dir() 判断，不应调用此函数
 * 
 * @param info 资源信息结构（必须是可写的）
 * @return 解压后的数据指针，空文件返回 (void*)1，失败返回 NULL
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

///////////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
}
#endif

#endif /* BUILDINS_H */
