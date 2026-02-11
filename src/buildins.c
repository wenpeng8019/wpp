/*
 * Built-in Resources - Runtime Implementation
 */

#include "buildins.h"
#include "vfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <errno.h>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#else
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

/**
 * DJB2 哈希算法
 * 
 * ⚠️  必须与 tools/make_buildins.sh 中的 hash_string() 保持完全一致！
 * 算法: hash = hash * 33 + c (即 (hash << 5) + hash + c)，初值 5381
 * 
 * @param str 输入字符串
 * @return 32位哈希值
 */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}

/**
 * 解压 zlib 数据到内存
 */
static int decompress_gzip(const uint8_t *src, size_t src_len,
                           uint8_t *dst, size_t dst_len) {
    uLongf out_len = (uLongf)dst_len;
    int ret = uncompress(dst, &out_len, src, src_len);
    if (ret != Z_OK) {
        fprintf(stderr, "Buildins: Decompression failed (error %d)\n", ret);
        return -1;
    }
    // 验证解压后大小是否合理（允许一定误差）
    if (out_len > (uLongf)dst_len) {
        fprintf(stderr, "Buildins: Decompressed size (%lu) exceeds buffer (%zu)\n", 
                out_len, dst_len);
        return -1;
    }
    return 0;
}

/**
 * 初始化所有内建资源 [已废弃]
 * 
 * 预构建的静态数据可以直接使用，此函数仅用于向后兼容。
 */
int buildins_init(void) {
    return 0;  // 什么也不做
}

/**
 * 清理所有内建资源
 */
void buildins_cleanup(void) {
    // 遍历链表，释放所有资源
    buildin_file_info_st *node = (buildin_file_info_st*)BUILDINS_LS;
    while (node) {
        // 释放解压后的数据
        if (node->raw) {
            free(node->raw);
            node->raw = NULL;
        }
        
        // 释放 vfile 对象
        if (node->vfile) {
            vfile_st *vf = (vfile_st*)node->vfile;
            vfile_close(vf);
            free(vf);
            node->vfile = NULL;
        }
        
        node->vref = 0;
        node = (buildin_file_info_st*)node->next;
    }
}

/**
 * 根据 URI 查找资源（混合策略：静态哈希表 or 二分查找）
 */
buildin_file_info_st* buildins_find(const char *uri) {
    if (!uri || BUILDINS_FT_SIZE == 0) {
        return NULL;
    }
    
    // 计算 URI 哈希
    uint32_t target_id = hash_string(uri);
    
#ifdef BUILDINS_HASH_TABLE_SIZE
    // 使用静态哈希表查找 O(1)
    uint32_t slot = target_id % BUILDINS_HASH_TABLE_SIZE;
    const buildin_file_info_st **bucket = BUILDINS_HT[slot];
    
    if (bucket) {
        for (int i = 0; bucket[i]; i++) {
            if (bucket[i]->id == target_id && strcmp(bucket[i]->uri, uri) == 0) {
                return (buildin_file_info_st*)bucket[i];
            }
        }
    }
#else
    // 使用二分查找 O(log n)
    int left = 0;
    int right = BUILDINS_FT_SIZE - 1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        buildin_file_info_st *item = BUILDINS_FT[mid];
        
        if (item->id == target_id) {
            // ID 匹配，再验证 URI（防止哈希冲突）
            int cmp = strcmp(item->uri, uri);
            if (cmp == 0) {
                return item;
            } else if (cmp < 0) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        } else if (item->id < target_id) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
#endif
    
    return NULL;
}

/**
 * 获取解压后的资源数据
 */
void* buildins_decompressed(buildin_file_info_st *info) {
    if (!info) {
        fprintf(stderr, "Buildins: NULL info pointer\n");
        return NULL;
    }
    
    // 如果已经解压过，直接返回
    if (info->raw) {
        return info->raw;
    }
    
    // 分配内存
    void *raw_data = malloc(info->orig_sz);
    if (!raw_data) {
        fprintf(stderr, "Buildins: Failed to allocate %u bytes for %s\n",
                info->orig_sz, info->uri);
        return NULL;
    }
    
    // 解压
    if (decompress_gzip(info->comp, info->comp_sz, 
                       (uint8_t*)raw_data, info->orig_sz) < 0) {
        fprintf(stderr, "Buildins: Failed to decompress %s\n", info->uri);
        free(raw_data);
        return NULL;
    }
    
    // 缓存解压后的数据
    info->raw = raw_data;
    
    return raw_data;
}

/**
 * 获取 vfile 对象（如果不存在则创建）
 * 
 * ⚠️  设计约束：正常流程中 vfile fd 应始终有效
 * - 主进程 tcc_configure() 预加载文件 → 创建 vfile → 缓存 fd
 * - fork 子进程继承 fd（应保持有效）
 * - CGI 子子进程会关闭 fd≥3（httpd.c:3407），但这发生在 TCC 使用之后
 * - 如果发现 fd 失效，说明流程有问题，应该修复根本原因而非兜底恢复
 */
vfile_st* buildins_acquire_vfile(buildin_file_info_st *info) {
    if (!info) {
        fprintf(stderr, "Buildins: NULL info pointer\n");
        return NULL;
    }
    
    // 如果虚拟文件已存在，直接复用缓存
    if (info->vfile) {
        vfile_st *cached_vf = (vfile_st*)info->vfile;
        info->vref++;
        
        /* [开发阶段] 验证 fd 有效性，确保流程正确
         * 如果这里报错，说明 fd 被意外关闭，需要检查：
         * 1. httpd.c:3407 的 fd 关闭时机是否过早
         * 2. fork 子进程后 fd 继承是否正常
         * 3. TCC 是否错误地关闭了原始 fd（应该只关闭 dup 的副本）
         */
        char test_byte;
        if (read(cached_vf->fd, &test_byte, 0) < 0) {
            fprintf(stderr, "  ⚠️  缓存vfile的fd已失效: fd=%d (%s), vref=%d\n", 
                    cached_vf->fd, strerror(errno), info->vref);
            fprintf(stderr, "      这表明流程有问题，需要修复根本原因！\n");
            // 开发阶段：让程序继续运行以便调试，生产环境应该直接失败
        }
        
        fprintf(stderr, "  → 复用缓存vfile: fd=%d, vref=%d\n", 
                cached_vf->fd, info->vref);
        return cached_vf;
        
        /* [兜底方案 - 已禁用] 自动恢复失效的 fd
         * 如果未来确实需要处理 fd 被关闭的场景，可以启用以下代码：
         *
         * if (read(cached_vf->fd, &test_byte, 0) < 0) {
         *     fprintf(stderr, "  → 缓存vfile失效: fd=%d (%s), 重新创建\n", 
         *             cached_vf->fd, strerror(errno));
         *     vfile_close(cached_vf);
         *     free(cached_vf);
         *     info->vfile = NULL;
         *     info->vref = 0;
         *     // 继续下面的代码重新创建
         * } else {
         *     info->vref++;
         *     fprintf(stderr, "  → 复用缓存vfile: fd=%d, vref=%d (fd有效)\n", 
         *             cached_vf->fd, info->vref);
         *     return cached_vf;
         * }
         */
    }
    
    // 懒加载解压
    void *raw_data = buildins_decompressed(info);
    if (!raw_data) {
        return NULL;
    }
    
    // 分配 vfile 对象
    vfile_st *vf = (vfile_st*)malloc(sizeof(vfile_st));
    if (!vf) {
        fprintf(stderr, "Buildins: Failed to allocate vfile for %s\n", info->uri);
        return NULL;
    }
    
    // 创建虚拟文件
    if (vfile_open(vf, info->uri, false) != 0) {
        fprintf(stderr, "Buildins: Failed to open vfile for %s\n", info->uri);
        free(vf);
        return NULL;
    }
    
    fprintf(stderr, "  → 新建vfile: fd=%d\n", vf->fd);
    
    // 写入数据
    if (vfile_write(vf, raw_data, info->orig_sz) != 0) {
        fprintf(stderr, "Buildins: Failed to write vfile for %s\n", info->uri);
        vfile_close(vf);
        free(vf);
        return NULL;
    }
    
    fprintf(stderr, "  → vfile写入完成: fd=%d, size=%zu\n", vf->fd, vf->size);
    
    // 缓存 vfile 对象
    info->vfile = vf;
    info->vref = 1;
    
    return vf;
}

/**
 * 释放资源的虚拟文件
 */
void buildins_release_vfile(buildin_file_info_st *info) {
    if (!info || !info->vfile) {
        return;
    }
    
    if (info->vref > 0) {
        info->vref--;
        
        if (info->vref == 0) {
            vfile_st *vf = (vfile_st*)info->vfile;
            vfile_close(vf);
            free(vf);
            info->vfile = NULL;
        }
    }
}

/**
 * 将内置文件解压到临时文件（返回 FILE*）
 */
FILE* buildins_to_tmp_file(buildin_file_info_st *info) {
    if (!info) {
        fprintf(stderr, "Buildins: NULL info pointer\n");
        return NULL;
    }
    
    // 解压数据
    void *raw_data = buildins_decompressed(info);
    if (!raw_data) {
        fprintf(stderr, "Buildins: Failed to decompress %s\n", info->uri);
        return NULL;
    }
    
    // 创建匿名临时文件（关闭时自动删除）
    FILE *fp = tmpfile();
    if (!fp) {
        fprintf(stderr, "Buildins: Failed to create tmpfile for %s: %s\n", 
                info->uri, strerror(errno));
        return NULL;
    }
    
    // 写入数据
    size_t written = fwrite(raw_data, 1, info->orig_sz, fp);
    if (written != info->orig_sz) {
        fprintf(stderr, "Buildins: Failed to write tmpfile for %s: wrote %zu/%u bytes\n",
                info->uri, written, info->orig_sz);
        fclose(fp);
        return NULL;
    }
    
    // 定位回文件开头
    rewind(fp);
    
    return fp;
}

/**
 * 将内置文件解压到临时文件（返回 fd）
 */
int buildins_to_tmp_fd(buildin_file_info_st *info, char **out_path) {
    if (!info) {
        fprintf(stderr, "Buildins: NULL info pointer\n");
        return -1;
    }
    
    // 解压数据
    void *raw_data = buildins_decompressed(info);
    if (!raw_data) {
        fprintf(stderr, "Buildins: Failed to decompress %s\n", info->uri);
        return -1;
    }
    
    // 创建临时文件（需要手动删除）
    char template[] = "/tmp/buildins_XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0) {
        fprintf(stderr, "Buildins: Failed to create temp file for %s: %s\n",
                info->uri, strerror(errno));
        return -1;
    }
    
    // 写入数据
    ssize_t written = write(fd, raw_data, info->orig_sz);
    if (written != (ssize_t)info->orig_sz) {
        fprintf(stderr, "Buildins: Failed to write temp file for %s: wrote %zd/%u bytes\n",
                info->uri, written, info->orig_sz);
        close(fd);
        unlink(template);
        return -1;
    }
    
    // 定位回文件开头
    lseek(fd, 0, SEEK_SET);
    
    // 如果需要返回路径，复制字符串
    if (out_path) {
        *out_path = strdup(template);
        if (!*out_path) {
            fprintf(stderr, "Buildins: Failed to allocate path string\n");
            close(fd);
            unlink(template);
            return -1;
        }
    }
    
    return fd;
}

