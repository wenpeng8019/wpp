#!/bin/bash
#
# generate_buildins.sh - 生成内建资源文件
#
# 遍历 buildins/ 目录，压缩所有文件，生成 sysroot.c 和 sysroot.h
# 使用链表结构，文件按反序定义以保证链表正序遍历
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILDINS_SRC="$PROJECT_ROOT/buildins"
BUILDINS_OUT="$PROJECT_ROOT/src/buildins"

# 颜色输出
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=== 生成内建资源（链表结构） ===${NC}"
echo "源目录: $BUILDINS_SRC"
echo "输出目录: $BUILDINS_OUT"
echo

# 检查依赖
command -v gzip >/dev/null 2>&1 || { echo "错误: 需要 gzip"; exit 1; }
command -v xxd >/dev/null 2>&1 || { echo "错误: 需要 xxd"; exit 1; }

# 检查源目录
if [ ! -d "$BUILDINS_SRC" ]; then
    echo -e "${YELLOW}警告: buildins/ 目录不存在，创建空资源${NC}"
    mkdir -p "$BUILDINS_SRC"
fi

mkdir -p "$BUILDINS_OUT"

# 临时文件
TEMP_FILES=$(mktemp)
TEMP_C=$(mktemp)
TEMP_H=$(mktemp)

cleanup() {
    rm -f "$TEMP_FILES" "$TEMP_C" "$TEMP_H"
}
trap cleanup EXIT

# 从头文件中读取哈希表配置参数
BUILDINS_H="$PROJECT_ROOT/src/buildins.h"
read_hash_config() {
    local param=$1
    grep "^#define ${param}" "$BUILDINS_H" | awk '{print $3}'
}

HASH_INDEX_THRESHOLD=$(read_hash_config "HASH_INDEX_THRESHOLD")
HASH_MAX_DEPTH_THRESHOLD=$(read_hash_config "HASH_MAX_DEPTH_THRESHOLD")
HASH_MIN_LOAD_FACTOR=$(read_hash_config "HASH_MIN_LOAD_FACTOR")
HASH_MAX_ITERATIONS=$(read_hash_config "HASH_MAX_ITERATIONS")

# DJB2 哈希函数
# ⚠️  必须与 src/buildins.c 中的 hash_string() 保持完全一致！
# 算法: hash = hash * 33 + c，初值 5381
hash_string() {
    local str="$1"
    local hash=5381
    local i
    for (( i=0; i<${#str}; i++ )); do
        local c=$(printf '%d' "'${str:$i:1}")
        hash=$(( (hash * 33 + c) & 0xFFFFFFFF ))
    done
    echo "$hash"
}

# 数字加千分位分隔符
format_number() {
    awk -v n="$1" 'BEGIN {
        s = n "";
        sign = "";
        if (substr(s,1,1) == "-") { sign = "-"; s = substr(s,2); }
        out = "";
        while (length(s) > 3) {
            out = "," substr(s, length(s)-2, 3) out;
            s = substr(s, 1, length(s)-3);
        }
        out = sign s out;
        print out;
    }'
}

# 扫描文件
echo -e "${GREEN}[1/4] 扫描文件...${NC}"
FILE_COUNT=0

if [ -d "$BUILDINS_SRC" ]; then
    # -L 跟随符号链接，支持软链接和真实文件
    while IFS= read -r -d '' file; do
        rel_path="${file#$BUILDINS_SRC/}"
        
        # 跳过隐藏文件
        [[ "$rel_path" =~ ^\. ]] || [[ "$rel_path" =~ /\. ]] && continue
        
        if [ -f "$file" ]; then
            # 计算 ID（路径哈希）
            uri="/$rel_path"
            id=$(hash_string "$uri")
            
            # 生成符号名（路径转下划线）
            symbol=$(echo "$rel_path" | sed 's/[\/\.]/_/g')
            
            # 检测文件类型（符号链接或真实文件）
            if [ -L "$BUILDINS_SRC/$rel_path" ]; then
                file_type="symlink"
                target=$(readlink "$BUILDINS_SRC/$rel_path")
            else
                file_type="file"
                target=""
            fi
            
            echo "$FILE_COUNT|$file|$uri|$id|$symbol|$file_type|$target" >> "$TEMP_FILES"
            if [ "$file_type" = "symlink" ]; then
                echo "  [$((FILE_COUNT + 1))] $uri (id=$id) [symlink → $target]"
            else
                echo "  [$((FILE_COUNT + 1))] $uri (id=$id)"
            fi
            FILE_COUNT=$((FILE_COUNT + 1))
        fi
    done < <(find -L "$BUILDINS_SRC" -type f -print0 2>/dev/null | sort -z)
fi

echo -e "  ${GREEN}✓${NC} 找到 $FILE_COUNT 个文件"
echo

if [ $FILE_COUNT -eq 0 ]; then
    echo -e "${YELLOW}警告: 没有找到文件，生成空资源${NC}"
fi

# 提取所有目录路径
TEMP_DIRS=$(mktemp)
if [ -f "$TEMP_FILES" ] && [ $FILE_COUNT -gt 0 ]; then
    # 从文件路径中提取所有父目录
    awk -F'|' '{print $3}' "$TEMP_FILES" | while IFS= read -r uri; do
        # 逐级提取父目录：/a/b/c.txt -> /a/b -> /a -> /
        dir="${uri%/*}"
        while [ -n "$dir" ] && [ "$dir" != "/" ]; do
            echo "$dir"
            dir="${dir%/*}"
        done
        # 添加根目录
        [ -n "$uri" ] && echo "/"
    done | sort -u > "$TEMP_DIRS"
    
    DIR_COUNT=$(wc -l < "$TEMP_DIRS" | tr -d ' ')
    echo -e "  ${GREEN}✓${NC} 提取 $DIR_COUNT 个目录"
    
    # 为每个目录生成条目
    while IFS= read -r dir; do
        id=$(hash_string "$dir")
        # 目录符号名：用 _ 开头，根目录直接是 _
        if [ "$dir" = "/" ]; then
            symbol="_"
        else
            # /include -> _include, /lib/sqtp -> _lib_sqtp
            symbol=$(echo "$dir" | sed 's/^\//_/; s/\//_/g')
        fi
        # 目录条目：索引|文件路径(空)|URI|ID|符号名|类型|目标(空)
        echo "$FILE_COUNT|DIR|$dir|$id|$symbol|directory|" >> "$TEMP_FILES"
        FILE_COUNT=$((FILE_COUNT + 1))
    done < "$TEMP_DIRS"
    
    rm -f "$TEMP_DIRS"
    echo -e "  ${GREEN}✓${NC} 总计 $FILE_COUNT 个条目（文件+目录）"
fi
echo

# 按 URI 重新排序并重新编号（确保链表按 URI 字符串顺序）
if [ -f "$TEMP_FILES" ] && [ $FILE_COUNT -gt 0 ]; then
    TEMP_SORTED=$(mktemp)
    # 按 URI（第3列）排序，使用 C locale 确保目录排在其内容之前
    # -k3,3 明确指定只排序第3字段，不包含后续字段
    LC_ALL=C sort -t'|' -k3,3 "$TEMP_FILES" | awk -F'|' 'BEGIN{idx=0} {printf "%d|%s|%s|%s|%s|%s|%s\n", idx++, $2, $3, $4, $5, $6, $7}' > "$TEMP_SORTED"
    
    mv "$TEMP_SORTED" "$TEMP_FILES"
    echo -e "${GREEN}按 URI 排序完成，链表遍历顺序: URI 字典序${NC}"
    echo
fi

# ===== 自适应哈希表构建 =====
# 计算下一个 2 的幂
next_pow2() {
    local n=$1
    local p=1
    while [ $p -lt $n ]; do
        p=$((p * 2))
    done
    echo $p
}

# 构建哈希表并统计冲突
build_and_analyze_hashtable() {
    local table_size=$1
    local temp_buckets=$(mktemp)
    
    # 初始化桶（记录每个桶的项数）
    for (( i=0; i<table_size; i++ )); do
        echo "0" >> "$temp_buckets"
    done
    
    # 分配资源到桶
    while IFS='|' read -r idx file uri id symbol file_type target; do
        local slot=$((id % table_size))
        local count=$(sed -n "$((slot+1))p" "$temp_buckets")
        sed -i.bak "$((slot+1))s/.*/$((count+1))/" "$temp_buckets"
    done < "$TEMP_FILES"
    
    # 统计
    local max_depth=0
    local used_buckets=0
    local total_items=0
    
    while IFS= read -r count; do
        total_items=$((total_items + count))
        if [ $count -gt 0 ]; then
            used_buckets=$((used_buckets + 1))
        fi
        if [ $count -gt $max_depth ]; then
            max_depth=$count
        fi
    done < "$temp_buckets"
    
    local load_factor=$(awk "BEGIN {printf \"%.3f\", $total_items / $table_size}")
    local collision_rate=$(awk "BEGIN {printf \"%.2f\", ($total_items - $used_buckets) * 100.0 / $total_items}")
    
    rm -f "$temp_buckets" "$temp_buckets.bak"
    
    # 返回统计结果：table_size|max_depth|load_factor|collision_rate
    echo "$table_size|$max_depth|$load_factor|$collision_rate"
}

# 自适应选择最优表大小
HASH_TABLE_SIZE=0
HASH_MAX_DEPTH=0
HASH_LOAD_FACTOR=0

if [ $FILE_COUNT -ge $HASH_INDEX_THRESHOLD ]; then
    echo -e "${GREEN}资源数 >= $HASH_INDEX_THRESHOLD，开始自适应哈希表优化...${NC}"
    
    # 初始表大小：资源数 * 2 的下一个 2 的幂
    HASH_TABLE_SIZE=$(next_pow2 $((FILE_COUNT * 2)))
    
    for (( iter=0; iter<HASH_MAX_ITERATIONS; iter++ )); do
        result=$(build_and_analyze_hashtable $HASH_TABLE_SIZE)
        
        size=$(echo "$result" | cut -d'|' -f1)
        max_depth=$(echo "$result" | cut -d'|' -f2)
        load_factor=$(echo "$result" | cut -d'|' -f3)
        collision=$(echo "$result" | cut -d'|' -f4)
        
        echo "  [尝试 $((iter+1))] 表大小=$size, 最大桶深=$max_depth, 负载因子=$load_factor, 冲突率=$collision%"
        
        # 判断是否满足条件
        if [ $max_depth -gt $HASH_MAX_DEPTH_THRESHOLD ]; then
            # 冲突太多，表太小
            HASH_TABLE_SIZE=$((HASH_TABLE_SIZE * 2))
            echo "    → 最大桶深过大，表大小翻倍至 $HASH_TABLE_SIZE"
        elif awk "BEGIN {exit !($load_factor < $HASH_MIN_LOAD_FACTOR)}" && [ $((HASH_TABLE_SIZE / 2)) -ge $FILE_COUNT ]; then
            # 负载太低，表太大
            HASH_TABLE_SIZE=$((HASH_TABLE_SIZE / 2))
            echo "    → 负载因子过低，表大小减半至 $HASH_TABLE_SIZE"
        else
            # 找到最优配置
            HASH_MAX_DEPTH=$max_depth
            HASH_LOAD_FACTOR=$load_factor
            echo -e "  ${GREEN}✓ 找到最优配置: 表大小=$HASH_TABLE_SIZE, 最大桶深=$max_depth, 负载=$load_factor${NC}"
            break
        fi
    done
    
    echo
fi

# 生成 .h 文件
echo -e "${GREEN}[2/4] 生成头文件...${NC}"

cat > "$TEMP_H" << 'EOF'
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

EOF

# 为每个文件声明 extern（带压缩信息注释）
if [ -f "$TEMP_FILES" ]; then
    while IFS='|' read -r idx file uri id symbol file_type target; do
        if [ "$file_type" = "directory" ]; then
            # 目录条目：使用 buildin_dir_info_st 类型（2个空格对齐）
            echo "extern buildin_dir_info_st  ${symbol};  // [DIR]" >> "$TEMP_H"
        else
            # 文件条目：使用 buildin_file_info_st 类型，计算文件大小和压缩比
            orig_size=$(wc -c < "$file" | tr -d ' ')
            comp_size=$(gzip -c "$file" | wc -c | tr -d ' ')
            if [ $orig_size -gt 0 ]; then
                ratio=$(awk "BEGIN {printf \"%.1f\", $comp_size * 100.0 / $orig_size}")
            else
                ratio="0.0"
            fi
            comp_size_fmt=$(format_number "$comp_size")
            orig_size_fmt=$(format_number "$orig_size")
            echo "extern buildin_file_info_st ${symbol};  // (${comp_size_fmt}/${orig_size_fmt}, ${ratio}%)" >> "$TEMP_H"
        fi
    done < "$TEMP_FILES"
fi

# 生成 .c 文件
echo -e "${GREEN}[3/4] 压缩并生成数据...${NC}"

cat > "$TEMP_C" << 'EOF'
/*
 * Built-in Resources Data
 * 
 * Auto-generated from buildins/ directory
 * Do not edit manually!
 */

#include "sysroot.h"
#include <stdint.h>

EOF

# 先生成所有目录对象（从后往前，因为链表是反序的）
if [ -f "$TEMP_FILES" ]; then
    if command -v tac &> /dev/null; then
        REV_CMD="tac"
    else
        REV_CMD="tail -r"
    fi
    
    TEMP_REV=$(mktemp)
    $REV_CMD "$TEMP_FILES" > "$TEMP_REV"
    
    # 先生成目录对象
    while IFS='|' read -r idx file uri id symbol file_type target; do
        [ "$file_type" != "directory" ] && continue
        
        # 计算父目录
        parent_uri="${uri%/*}"
        [ "$parent_uri" = "" ] && parent_uri="/"
        [ "$uri" = "/" ] && parent_uri=""
        
        if [ -n "$parent_uri" ]; then
            parent_symbol=$(grep "|$parent_uri|" "$TEMP_FILES" | cut -d'|' -f5)
            parent_ref="&${parent_symbol}"
        else
            parent_ref="NULL"
        fi
        
        # 判断 next 指针
        if [ "$idx" -eq "$((FILE_COUNT - 1))" ]; then
            next_symbol="NULL"
        else
            next_idx=$((idx + 1))
            next_symbol=$(grep "^${next_idx}|" "$TEMP_FILES" | cut -d'|' -f5)
            next_symbol="&${next_symbol}"
        fi
        
        echo "/* $uri (id=$id) [directory] */" >> "$TEMP_C"
        cat >> "$TEMP_C" << CEOF
buildin_dir_info_st ${symbol} = {
    .next = ${next_symbol},
    .id = ${id},
    .uri = "${uri}",
    .parent = ${parent_ref},
    .flag = (const uint8_t*)(uintptr_t)-1  /* 目录标识 */
};
CEOF
        echo >> "$TEMP_C"
    done < "$TEMP_REV"
    
    rm -f "$TEMP_REV"
fi

# 生成压缩数据（反序，最后一个文件先定义）
TOTAL_ORIG=0
TOTAL_COMP=0

if [ -f "$TEMP_FILES" ]; then
    # 反序读取文件列表（macOS 使用 tail -r，Linux 使用 tac）
    if command -v tac &> /dev/null; then
        REV_CMD="tac"
    else
        REV_CMD="tail -r"
    fi
    
    # 使用临时文件保存反序列表，避免管道中的子shell问题
    TEMP_REV=$(mktemp)
    $REV_CMD "$TEMP_FILES" > "$TEMP_REV"
    
    while IFS='|' read -r idx file uri id symbol file_type target; do
        # 判断是否是最后一个（实际上是第一个，因为反序）
        if [ "$idx" -eq "$((FILE_COUNT - 1))" ]; then
            next_symbol="NULL"
        else
            # 读取下一个（实际上是上一个）文件的符号名
            next_idx=$((idx + 1))
            next_symbol=$(grep "^${next_idx}|" "$TEMP_FILES" | cut -d'|' -f5)
            next_symbol="&${next_symbol}"
        fi
        
        # 目录条目：跳过，已经在前面生成过了
        if [ "$file_type" = "directory" ]; then
            continue
        fi
        
        # 文件条目：生成压缩数据
        orig_size=$(wc -c < "$file" | tr -d ' ')
        
        # 计算文件所属目录
        dir_uri="${uri%/*}"
        [ "$dir_uri" = "" ] && dir_uri="/"
        dir_symbol=$(grep "|$dir_uri|" "$TEMP_FILES" | grep "|directory|" | cut -d'|' -f5)
        if [ -n "$dir_symbol" ]; then
            dir_ref="&${dir_symbol}"
        else
            dir_ref="NULL"
        fi
        
        # 压缩并生成数组
        if [ "$file_type" = "symlink" ]; then
            echo "/* $uri (id=$id) [symlink → $target] */" >> "$TEMP_C"
        else
            echo "/* $uri (id=$id) */" >> "$TEMP_C"
        fi
        echo "static const uint8_t ${symbol}_z[] = {" >> "$TEMP_C"
        gzip -c "$file" | xxd -i >> "$TEMP_C"
        echo "};" >> "$TEMP_C"
        
        comp_size=$(gzip -c "$file" | wc -c | tr -d ' ')
        
        cat >> "$TEMP_C" << CEOF
buildin_file_info_st ${symbol} = {
    .next = ${next_symbol},
    .id = ${id},
    .uri = "${uri}",
    .dir = ${dir_ref},
    .comp = ${symbol}_z,
    .raw = NULL,
    .vfile = NULL,
    .comp_sz = sizeof(${symbol}_z),
    .orig_sz = ${orig_size},
    .vref = 0
};

CEOF
        
        echo "  [$((FILE_COUNT - idx))] ${file} (${orig_size}B → ${comp_size}B)"
        
        # 只统计文件大小，不包含目录
        TOTAL_ORIG=$((TOTAL_ORIG + orig_size))
        TOTAL_COMP=$((TOTAL_COMP + comp_size))
    done < "$TEMP_REV"
    
    rm -f "$TEMP_REV"
fi

# 添加资源数量宏
echo >> "$TEMP_H"
echo "/* 资源数量 */" >> "$TEMP_H"
echo "#define BUILDINS_FT_SIZE ${FILE_COUNT}" >> "$TEMP_H"

# 添加总大小统计（写入头文件注释）
if [ $TOTAL_ORIG -gt 0 ]; then
    total_ratio=$(awk "BEGIN {printf \"%.1f\", $TOTAL_COMP * 100.0 / $TOTAL_ORIG}")
    total_orig_fmt=$(format_number "$TOTAL_ORIG")
    total_comp_fmt=$(format_number "$TOTAL_COMP")
    echo "/* 总大小: ${total_orig_fmt} B, 压缩后: ${total_comp_fmt} B, 占比: ${total_ratio}% */" >> "$TEMP_H"
else
    echo "/* 总大小: 0 B, 压缩后: 0 B, 占比: 0.0% */" >> "$TEMP_H"
fi

# 添加哈希表配置（如果启用）
if [ $HASH_TABLE_SIZE -gt 0 ]; then
    echo >> "$TEMP_H"
    echo "/* 静态哈希表配置（预构建，基于 buildins.h 中的参数自适应计算） */" >> "$TEMP_H"
    echo "#define BUILDINS_HASH_TABLE_SIZE ${HASH_TABLE_SIZE}" >> "$TEMP_H"
    echo "#define BUILDINS_HASH_MAX_DEPTH ${HASH_MAX_DEPTH}" >> "$TEMP_H"
    echo "#define BUILDINS_HASH_LOAD_FACTOR ${HASH_LOAD_FACTOR}f" >> "$TEMP_H"
    echo "extern buildin_file_info_st** BUILDINS_HT[${HASH_TABLE_SIZE}];" >> "$TEMP_H"
fi

cat >> "$TEMP_H" << 'EOF'

#ifdef __cplusplus
}
#endif

#endif /* BUILDINS_SYSROOT_H */
EOF

echo -e "  ${GREEN}✓${NC} 头文件生成"
echo

# 生成索引表
echo -e "${GREEN}[4/4] 生成索引表...${NC}"

cat >> "$TEMP_C" << 'EOF'

/* 索引表（按 id 排序，支持二分查找） */
buildin_file_info_st* BUILDINS_FT[] = {
EOF

if [ -f "$TEMP_FILES" ]; then
    # 按 id 排序输出（避免使用关联数组，兼容 bash 3.x）
    first=1
    sort -t'|' -k4 -n "$TEMP_FILES" | while IFS='|' read -r idx file uri id symbol file_type target; do
        [ $first -eq 0 ] && echo "," >> "$TEMP_C"
        # 目录条目需要类型转换
        if [ "$file_type" = "directory" ]; then
            echo -n "    (buildin_file_info_st*)&${symbol}" >> "$TEMP_C"
        else
            echo -n "    &${symbol}" >> "$TEMP_C"
        fi
        first=0
    done
    echo >> "$TEMP_C"
fi

cat >> "$TEMP_C" << EOF
};

/* 链表头指针 */
EOF

if [ $FILE_COUNT -gt 0 ]; then
    first_line=$(head -1 "$TEMP_FILES")
    first_symbol=$(echo "$first_line" | cut -d'|' -f5)
    first_type=$(echo "$first_line" | cut -d'|' -f6)
    # 根目录是第一个条目，需要类型转换
    if [ "$first_type" = "directory" ]; then
        echo "buildin_file_info_st* BUILDINS_LS = (buildin_file_info_st*)&${first_symbol};" >> "$TEMP_C"
    else
        echo "buildin_file_info_st* BUILDINS_LS = &${first_symbol};" >> "$TEMP_C"
    fi
else
    echo "buildin_file_info_st* BUILDINS_LS = NULL;" >> "$TEMP_C"
fi

# 生成静态哈希表（如果启用）
if [ $HASH_TABLE_SIZE -gt 0 ] && [ $FILE_COUNT -gt 0 ]; then
    echo >> "$TEMP_C"
    echo "/* ==================== 静态哈希表（预构建） ==================== */" >> "$TEMP_C"
    echo >> "$TEMP_C"
    
    # 为每个桶收集资源
    declare -a buckets
    for (( i=0; i<HASH_TABLE_SIZE; i++ )); do
        buckets[$i]=""
    done
    
    while IFS='|' read -r idx file uri id symbol file_type target; do
        slot=$((id % HASH_TABLE_SIZE))
        if [ -z "${buckets[$slot]}" ]; then
            buckets[$slot]="$symbol"
        else
            buckets[$slot]="${buckets[$slot]} $symbol"
        fi
    done < "$TEMP_FILES"
    
    # 生成桶数组
    for (( i=0; i<HASH_TABLE_SIZE; i++ )); do
        if [ -n "${buckets[$i]}" ]; then
            echo "/* Bucket $i (collision chain) */" >> "$TEMP_C"
            echo -n "static buildin_file_info_st* hash_bucket_$i[] = { " >> "$TEMP_C"
            
            first=1
            for sym in ${buckets[$i]}; do
                [ $first -eq 0 ] && echo -n ", " >> "$TEMP_C"
                echo -n "&$sym" >> "$TEMP_C"
                first=0
            done
            
            echo ", NULL };" >> "$TEMP_C"
        fi
    done
    
    echo >> "$TEMP_C"
    echo "/* 哈希表主数组 */" >> "$TEMP_C"
    echo "buildin_file_info_st** BUILDINS_HT[$HASH_TABLE_SIZE] = {" >> "$TEMP_C"
    
    for (( i=0; i<HASH_TABLE_SIZE; i++ )); do
        if [ -n "${buckets[$i]}" ]; then
            echo "    hash_bucket_$i," >> "$TEMP_C"
        else
            echo "    NULL," >> "$TEMP_C"
        fi
    done
    
    echo "};" >> "$TEMP_C"
fi

# 移动到输出目录
mv "$TEMP_H" "$BUILDINS_OUT/sysroot.h"
mv "$TEMP_C" "$BUILDINS_OUT/sysroot.c"

echo -e "  ${GREEN}✓${NC} 索引表生成"
echo

# 统计信息
echo -e "${BLUE}=== 统计信息 ===${NC}"
echo "  资源数量: $FILE_COUNT"
if [ $TOTAL_ORIG -gt 0 ]; then
    echo "  原始大小: ${TOTAL_ORIG}B"
    echo "  压缩大小: ${TOTAL_COMP}B"
    RATIO=$(awk "BEGIN {printf \"%.1f\", $TOTAL_COMP * 100.0 / $TOTAL_ORIG}")
    echo "  压缩比: ${RATIO}%"
else
    echo "  原始大小: 0B"
    echo "  压缩大小: 0B"
fi
echo
echo -e "${GREEN}✓ 内建资源生成完成！${NC}"
echo "  输出: $BUILDINS_OUT/sysroot.h"
echo "  输出: $BUILDINS_OUT/sysroot.c"
