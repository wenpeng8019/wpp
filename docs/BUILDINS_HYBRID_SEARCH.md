# Buildins 自适应静态哈希表

## 核心设计：编译时预构建哈希表

与运行时构建不同，哈希表在**构建脚本中**预生成为静态 C 代码，实现零运行时开销。

## 自适应构建算法

### 构建流程
```bash
1. 扫描资源，计算 URI 哈希
2. 如果资源数 >= 50，启动自适应优化：
   a. 初始表大小 = next_pow2(资源数 * 2)
   b. 模拟哈希分配，统计冲突
   c. 如果 max_depth > 3，表大小 *= 2，重试
   d. 如果 load_factor < 0.3，表大小 /= 2，重试
   e. 最多迭代 5 次，找到最优配置
3. 生成静态哈希表 C 代码
```

### 关键指标
- **最大桶深度** (max_depth): <= 3（保证查找性能）
- **负载因子** (load_factor): 0.3 - 0.75（平衡空间）
- **表大小**: 2 的幂（优化取模：`hash & (size-1)`）

## 生成的代码结构

### 头文件 (sysroot.h)
```c
/* 资源数量 */
#define BUILDINS_FT_SIZE 150

/* 静态哈希表配置（预构建） */
#define BUILDINS_HASH_TABLE_SIZE 256
#define BUILDINS_HASH_MAX_DEPTH 2
#define BUILDINS_HASH_LOAD_FACTOR 0.586f
extern const buildin_file_info_st** BUILDINS_HT[256];
```

### 实现文件 (sysroot.c)
```c
/* Bucket 5 (collision chain) */
static const buildin_file_info_st* hash_bucket_5[] = { 
    &file_a, &file_b, NULL 
};

/* Bucket 17 (no collision) */
static const buildin_file_info_st* hash_bucket_17[] = { 
    &file_c, NULL 
};

/* 哈希表主数组 */
const buildin_file_info_st** BUILDINS_HT[256] = {
    NULL,           // slot 0 空
    ...
    hash_bucket_5,  // slot 5
    ...
    hash_bucket_17, // slot 17
    ...
};
```

## 查找实现

```c
const buildin_file_info_st* buildins_find(const char *uri) {
    uint32_t target_id = hash_string(uri);
    
#ifdef BUILDINS_HASH_TABLE_SIZE
    // 静态哈希表查找 O(1)
    uint32_t slot = target_id % BUILDINS_HASH_TABLE_SIZE;
    const buildin_file_info_st** bucket = BUILDINS_HT[slot];
    
    if (bucket != NULL) {
        // 遍历桶内冲突链（通常 <= 3 项）
        for (int i = 0; bucket[i] != NULL; i++) {
            if (bucket[i]->id == target_id && 
                strcmp(bucket[i]->uri, uri) == 0) {
                return bucket[i];
            }
        }
    }
    return NULL;
#else
    // 二分查找 O(log n) - 资源数 < 50
    // ...
#endif
}
```

## 性能对比

| 资源数 | 方法 | 耗时 | 内存 |
|-------|------|------|------|
| 15 | 二分查找 | 0.038 μs | 120B ✅ |
| 50 | 静态哈希表 | ~0.02 μs | ~2KB |
| 500 | 静态哈希表 | ~0.02 μs | ~10KB |

## 哈希算法一致性

⚠️  DJB2 哈希算法在 Shell 和 C 中**完全一致**：

**Shell版本** (`tools/make_buildins.sh`):
```bash
# DJB2 哈希函数
# ⚠️  必须与 src/buildins.c 中的 hash_string() 保持完全一致！
hash_string() {
    local hash=5381
    for (( i=0; i<${#str}; i++ )); do
        local c=$(printf '%d' "'${str:$i:1}")
        hash=$(( (hash * 33 + c) & 0xFFFFFFFF ))
    done
    echo "$hash"
}
```

**C版本** (`src/buildins.c`):
```c
/**
 * DJB2 哈希算法
 * ⚠️  必须与 tools/make_buildins.sh 中的 hash_string() 保持完全一致！
 */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}
```

## 优势总结

✅ **零运行时开销**: 哈希表编译时生成，无需 malloc  
✅ **自适应优化**: 根据冲突自动调整表大小  
✅ **智能阈值**: < 50 资源用二分，>= 50 用哈希表  
✅ **纯 Shell 实现**: 无需外部工具，零依赖  
✅ **哈希一致性**: Shell/C 算法完全匹配  
✅ **性能优异**: O(1) 查找，桶深 <= 3  

## 构建示例输出

```bash
=== 生成内建资源（链表结构） ===
[1/4] 扫描文件...
  ✓ 找到 150 个文件

资源数 >= 50，开始自适应哈希表优化...
  [尝试 1] 表大小=256, 最大桶深=2, 负载因子=0.586, 冲突率=12.3%
  ✓ 找到最优配置: 表大小=256, 最大桶深=2, 负载=0.586

[2/4] 生成头文件...
  ✓ 头文件生成

[3/4] 压缩并生成数据...
[4/4] 生成索引表...

=== 统计信息 ===
  资源数量: 150
  哈希表: 256 槽位, 最大桶深 2, 负载 0.59
✓ 内建资源生成完成！
```
