
#ifndef UTIL_COMMON_H_
#define UTIL_COMMON_H_

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma ide diagnostic ignored "OCUnusedMacroInspection"
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

#include <string.h>
#include <float.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#if __STDC_VERSION__ >= 201112L
#  define TLS _Thread_local
#else
#  define TLS __thread
#endif

#ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif

#ifdef __cplusplus
extern "C" {
#endif
///////////////////////////////////////////////////////////////////////////////

#ifdef NDEBUG
#define check(expr, ...) { if (!(expr)) { printf("E: %s@%d(%d)", __FUNCTION__, __LINE__, (int)(long)(expr)); __VA_ARGS__ ;} }
#else
#define check(expr, ...) assert(expr);
#endif

typedef int32_t                 ret_t;
#define E_NONE                  0
#define E_UNKNOWN               (-1)                /* 未知错误 */
#define E_INVALID               (-2)                /* 指定的参数或操作无效。不满足规范或规定 */
#define E_CONFLICT              (-3)                /* 冲突指的是排它。即运行我的前提是没有它；或者对于单例资源来说，现在正在被（别人）使用，或者之前的使用没有被正确释放。 */
#define E_OUT_OF_RANGE          (-4)                /* 请求的范围超出限制 */
#define E_OUT_OF_MEMORY         (-5)                /* 内存不足（最基本的资源不足） */
#define E_OUT_OF_SUPPLY         (-6)                /* （通用的）请求数量大于供给能力，或者数据下溢（underflow） */
#define E_OUT_OF_CAPACITY       (-7)                /* （通用的）数据量超出设计容量；或者数据溢出（overflow） */
#define E_NONE_EXISTS           (-8)                /* 访问项不存在 */
#define E_NONE_CONTEXT          (-9)                /* 不具备可运行的状态。常见的就是没有初始化或启动 */
#define E_NONE_RELEASED         (-10)               /* 当前（资源）项未被释放，无法被析构或再次分配 */

typedef enum val_type {
    VT_I8,
    VT_I16,
    VT_I32,
    VT_I64,
    VT_U8,
    VT_U16,
    VT_U32,
    VT_U64,
    VT_F32,
    VT_F64,
    VT_NUM
} val_type_e;

static inline uint8_t
val_size(val_type_e type) {
    switch (type) {
        case VT_I8: case VT_U8: return 1;
        case VT_I16: case VT_U16: return 2;
        case VT_I32: case VT_U32: case VT_F32: return 4;
        case VT_I64: case VT_U64: case VT_F64: return 8;
        default: assert(0);
    }
}

extern const char* VAL_TYPE_NAME[VT_NUM];

///////////////////////////////////////////////////////////////////////////////

#define clock_s_f(a)     ((a).tv_sec+(a).tv_nsec/1000000000.0)
#define clock_ms_f(a)    ((a).tv_sec*1000.0+(a).tv_nsec/1000000.0)
#define clock_us_f(a)    ((a).tv_sec*1000000.0+(a).tv_nsec/1000.0)
#define clock_s(a)       ((a).tv_sec+((a).tv_nsec+500000000)/1000000000)
#define clock_ms(a)      ((a).tv_sec*1000+((a).tv_nsec+500000)/1000000)
#define clock_us(a)      ((a).tv_sec*1000000+((a).tv_nsec+500)/1000)
// 注意，不要统一转换为 ns 进行计算，因为根据 long 类型精度计算，以 ns 表示的 s 最多只能表示 2s
#define clock_s_diff(a,b)     (((a).tv_sec-(b).tv_sec)+((a).tv_nsec-(b).tv_nsec+((a).tv_nsec>=(b).tv_nsec?500000000:-500000000))/1000000000)
#define clock_ms_diff(a,b)    (((a).tv_sec-(b).tv_sec)*1000+((a).tv_nsec-(b).tv_nsec+((a).tv_nsec>=(b).tv_nsec?500000:-500000))/1000000)
#define clock_us_diff(a,b)    (((a).tv_sec-(b).tv_sec)*1000000+((a).tv_nsec-(b).tv_nsec+((a).tv_nsec>=(b).tv_nsec?500:-500))/1000)
#define clock_dec(a,b,v) {                      \
    (v).tv_sec = (a).tv_sec - (b).tv_sec;       \
    (v).tv_nsec = (a).tv_nsec - (b).tv_nsec;    \
    if ((v).tv_nsec < 0) {                      \
        (v).tv_sec -= 1;                        \
        (v).tv_nsec += 1000000000;              \
    }                                           \
}
#define clock_inc(a,b,v) {                      \
    (v).tv_sec = (a).tv_sec + (b).tv_sec;       \
    (v).tv_nsec = (a).tv_nsec + (b).tv_nsec;    \
    if ((v).tv_nsec >= 1000000000) {            \
        (v).tv_sec += 1;                        \
        (v).tv_nsec -= 1000000000;              \
    }                                           \
}
#define clock_gt(a,b)      ((a).tv_sec>(b).tv_sec || (a).tv_sec==(b).tv_sec && (a).tv_nsec>(b).tv_nsec)
#define clock_ge(a,b)      ((a).tv_sec>(b).tv_sec || (a).tv_sec==(b).tv_sec && (a).tv_nsec>=(b).tv_nsec)

///////////////////////////////////////////////////////////////////////////////

#ifndef ROOT_TAG
#define ROOT_TAG            ""                      /* 必须是预定义常量字符串 */
#endif
#ifndef MOD_TAG
#define MOD_TAG             ""                      /* 可以是一个字符串函数 */
#endif

#ifndef LOG_DEF
#define LOG_DEF             LOG_SLOT_NONE
#endif

#ifndef LOG_TAG_MAX
#define LOG_TAG_MAX         (-24)                   /* tag 格式化布局，abs 大小为 tag 宽度
                                                     * >0: 固定宽度，左对齐, 右侧补空格;
                                                     * <0: 固定宽度，右对齐, 左侧补空格;
                                                     * =0: 非固定宽度输出, tag 最大 128 字符 */
#endif
#ifndef LOG_TAG_L
#define LOG_TAG_L           "["
#endif
#ifndef LOG_TAG_R
#define LOG_TAG_R           "]"
#endif

#define LOG_LINE_MAX        2048

#ifndef LOG_OUTPUT_FILE
#define LOG_FILE_DISABLED
#endif

typedef enum {
    LOG_SLOT_VERBOSE = 0,                            /* 任何信息（常用于输出说明文档） */
    LOG_SLOT_DEBUG,                                  /* 用于程序调试 */
    LOG_SLOT_INFO,                                   /* 运行状态 */
    LOG_SLOT_WARN,                                   /* 警告信息。既可能会产生问题，或无法达到预期 */
    LOG_SLOT_ERROR,                                  /* 不应该发生的错误，但不影响程序继续运行 */
    LOG_SLOT_FATAL,                                  /* 导致程序无法再继续运行的错误 */
    LOG_SLOT_NONE,
} log_level_e;

typedef void(*log_cb)(log_level_e level, const char* tag, char *txt, int len);

void
log_output(log_cb cb_log, bool tag_separate);

void
log_slot(log_level_e level, const char* tag, const char* fmt, va_list params);

static inline void printf_slot(const char* fmt, ...) {

    // +3: "[]\0"
#   if LOG_TAG_MAX > 0
    static char s_tag[LOG_TAG_MAX + 3];
#   elif LOG_TAG_MAX < 0
    static char s_tag[-(LOG_TAG_MAX) + 3];
#   else
    static char s_tag[128 + 3];
#   endif
    // 初始化 mod tag，只执行一次
    if (!*s_tag) {

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantConditionsOC"
        int n = sizeof(ROOT_TAG);                               // 注意 sizeof 得到的长度(即数组长度), 包括了 \0, 也就是 n = strlen + 1
        // 左对齐固定输出（右侧补空格）
#       if LOG_TAG_MAX > 0
        if (n > LOG_TAG_MAX)                                    // ROOT TAG 太长，大于等于 TAG_MAX，则只显示 ROOT TAG
            sprintf(s_tag, LOG_TAG_L "%.*s" LOG_TAG_R, LOG_TAG_MAX, ROOT_TAG);    // 限制最大输出（从后截断）
        else { int m = strlen(MOD_TAG);
            if (n + m <= LOG_TAG_MAX + 1)                       // 总长度小于等于固定宽，则全输出，右侧补空格
                sprintf(s_tag, LOG_TAG_L "%s%s%-*s" LOG_TAG_R, ROOT_TAG, MOD_TAG, LOG_TAG_MAX + 1 - n - m, "");
            else {                                              // 否则 ROOT 全输出，MOD 仅输出后部分，中间用 ".." 间隔
                int offset = (LOG_TAG_MAX - 1 - n); assert(offset >= -2);
                if (offset <= 0) sprintf(s_tag, LOG_TAG_L "%s%.*s" LOG_TAG_R, ROOT_TAG, 2+offset, "..");
                else sprintf(s_tag, LOG_TAG_L "%s..%s" LOG_TAG_R, ROOT_TAG, (char*)MOD_TAG + m - offset);
            }
        }
        // 右对齐固定输出（左侧补空格）
#       elif LOG_TAG_MAX < 0
        if (n > -(LOG_TAG_MAX))                                 // ROOT TAG 太长，大于等于 TAG_MAX，则只显示 ROOT TAG
            sprintf(s_tag, LOG_TAG_L "%.*s" LOG_TAG_R, -(LOG_TAG_MAX), ROOT_TAG); // 限制最大输出（从后截断）
        else { int m = strlen(MOD_TAG);
            if (n + m <= -(LOG_TAG_MAX) + 1)                    // 总长度小于等于固定宽，则 MOD 全输出、ROOT 固定宽度，宽度为总长度去除 MOD 长度，前面补空格
                sprintf(s_tag, LOG_TAG_L "%*s%s" LOG_TAG_R, -(LOG_TAG_MAX) - m, ROOT_TAG, MOD_TAG);
            else {                                              // 否则 ROOT 全输出，MOD 仅输出后部分，中间用 ".." 间隔
                int offset = (-(LOG_TAG_MAX) - 1 - n); assert(offset >= -2);
                if (offset <= 0) sprintf(s_tag, LOG_TAG_L "%s%.*s" LOG_TAG_R, ROOT_TAG, 2+offset, "..");
                else sprintf(s_tag, LOG_TAG_L "%s..%s" LOG_TAG_R, ROOT_TAG, (char*)MOD_TAG + m - offset);
            }
        }
        // 非固定宽度输出，则仅限制最大输出尺寸
#       else
        if (n > 128) sprintf(s_tag, LOG_TAG_L "%.*s" LOG_TAG_R, 128, ROOT_TAG);   // ROOT TAG 太长，大于等于 128，则只显示 ROOT TAG
        else sprintf(s_tag, LOG_TAG_L "%s%.*s" LOG_TAG_R, ROOT_TAG, 128 + 1 - n, (char*)MOD_TAG);
#       endif
#pragma clang diagnostic pop
    }

    assert(fmt); va_list args = {};
    static TLS bool s_begin = false;

    // 如果 printf(""), 开启缓存模式。此外，如果之前已经开启缓存，则清空缓存，从头开始
    if (!*fmt) { s_begin = true;
        log_slot(LOG_SLOT_NONE, NULL, NULL, args);
        return;
    }
    if (*fmt == ':') {
        // 如果以双 ':' 开头，则在调整状态下，直接输出到标准输出
        if (*++fmt == ':') {
#ifndef NDEBUG
            fmt += fmt[1] == ' ' ? 2 : 1;
            va_start(args, fmt);
            vprintf(fmt, args);
            va_end(args);
#endif
            return;
        }
        // 否则如果以单 ':' 开头，则同 printf("") 一样开启缓存模式，并直接输出到缓存
        s_begin = true;
        va_start(args, fmt);
        log_slot(LOG_SLOT_NONE, NULL, fmt, args);
        va_end(args);
        return;
    }

    log_level_e level = LOG_DEF;
    if (fmt[1] == ':') {
        const char *q="VDIWEF", *p = strchr(q, *fmt);
        if (p) { s_begin = false;                           // 只要指定 level 标识，就会关闭缓存模式
            level = (log_level_e)(p - q); fmt+=2;
            if (*fmt == ' ') ++fmt;                         // 忽略 1 个且只忽略 1 个空格（即允许多个空格作为缩进）
        }
    }

    if (s_begin) {
        va_start(args, fmt);
        log_slot(LOG_SLOT_NONE, NULL, fmt, args);
        va_end (args);
        return;
    }

    if (level >= LOG_SLOT_NONE) return;                     // 如果默认级别为 NONE，则默认不输出任何内容

    va_start(args, fmt);
    log_slot(level, s_tag, fmt, args);
    va_end (args);
}

#define printf(fmt, ...)    printf_slot(fmt, ##__VA_ARGS__)

///////////////////////////////////////////////////////////////////////////////

/**
 * args 使用示例:
 * -------------------------------------------------------------------------
 *
 * // 定义参数（全局作用域）
 * ARGS_B(false, verbose, 'v', "verbose", "Enable verbose output");
 * ARGS_S(true,  input,   'i', "input",   "Input file path");
 * ARGS_I(false, count,   'n', "count",   "Number of iterations");
 *
 * int main(int argc, char** argv) {
 *
 *     // 设置帮助信息（可选），usage_ex 中的 $0 会被替换为程序名
 *     ARGS_usage("<file>...",
 *         "Examples:\n"
 *         "  $0 -i input.txt -n 10 file1 file2\n"
 *         "  $0 --verbose -i data.bin");
 *
 *     // 解析参数
 *     int pos_count = ARGS_parse(argc, argv,
 *         &ARGS_DEF_verbose,
 *         &ARGS_DEF_input,
 *         &ARGS_DEF_count,
 *         NULL);
 *
 *     // 使用参数
 *     if (ARGS_verbose.i64) printf("Verbose mode\n");
 *     printf("Input: %s\n", ARGS_input.str);
 *     printf("Count: %lld\n", ARGS_count.i64);
 *
 *     // 访问位置参数
 *     for (int i = 0; i < pos_count; i++) {
 *         printf("Positional[%d]: %s\n", i, argv[argc - pos_count + i]);
 *     }
 *     return 0;
 * }
 * -------------------------------------------------------------------------
 */

typedef enum arg_type {
    ARG_FLOAT = -3,
    ARG_INT,
    ARG_BOOL,
    ARG_STR,
    ARG_DIR,                    ///< 目录路径，自动规范化：展开~、移除末尾/、合并//、处理/./
    ARG_LS,
} arg_type_e;

/**
 * 参数值联合体
 *
 * 各类型的默认值（选项未出现在命令行时）：
 *   ARG_BOOL   -> i64 = 0
 *   ARG_INT    -> i64 = 0
 *   ARG_FLOAT  -> f64 = 0.0
 *   ARG_STR    -> str = NULL
 *   ARG_DIR    -> str = NULL
 *   ARG_LS     -> ls  = NULL
 *
 * 各类型对空字符串值（如 --opt ""）的处理：
 *   ARG_INT    -> 0   (strtoll 返回 0)
 *   ARG_FLOAT  -> 0.0 (strtod 返回 0.0)
 *   ARG_STR    -> ""  (指向空字符串)
 *   ARG_DIR    -> ""  (指向空字符串)
 *
 * @note 选项后缺少值时（如 -n 后无参数）会报错退出
 * @note INT/FLOAT 标记为必选时，无法区分"未设置"和"设置为0"
 */
typedef union arg_var {
    const char*  str;           ///< ARG_STR, ARG_DIR
    int64_t      i64;           ///< ARG_BOOL, ARG_INT
    double       f64;           ///< ARG_FLOAT
    const char** ls;            ///< ARG_LS，用 ARGS_ls_count() 获取数量
} arg_var_st;

typedef struct arg_def {
    const char* name;
    const char* desc;
    arg_type_e  type;
    char        s;
    const char* l;
    bool        req;
    arg_var_st* var;
    struct arg_def* next;
} arg_def_st;


#define ARGS_DEF(req, name, type, s_cmd, l_cmd, desc)   \
extern arg_var_st ARGS_##name;                          \
arg_var_st ARGS_##name;                                 \
static arg_def_st ARGS_DEF_##name = {                   \
    #name, desc, ARG_##type, s_cmd, l_cmd, req,         \
    &ARGS_##name, NULL                                  \
}
#define ARGS_B(req, name, s_cmd, l_cmd, desc)    ARGS_DEF(req, name, BOOL, s_cmd, l_cmd, desc)
#define ARGS_I(req, name, s_cmd, l_cmd, desc)    ARGS_DEF(req, name, INT, s_cmd, l_cmd, desc)
#define ARGS_F(req, name, s_cmd, l_cmd, desc)    ARGS_DEF(req, name, FLOAT, s_cmd, l_cmd, desc)
#define ARGS_S(req, name, s_cmd, l_cmd, desc)    ARGS_DEF(req, name, STR, s_cmd, l_cmd, desc)
#define ARGS_D(req, name, s_cmd, l_cmd, desc)    ARGS_DEF(req, name, DIR, s_cmd, l_cmd, desc)
#define ARGS_L(req, name, s_cmd, l_cmd, desc)    ARGS_DEF(req, name, LS, s_cmd, l_cmd, desc)

#define ARGS(name) extern arg_var_st ARGS_##name

/**
 * 设置命令行帮助信息
 * @param pos_desc  位置参数描述，显示在 Usage 行中，如 "<subcommand>" 或 "<file>..."
 *                  传 NULL 时自动根据解析结果生成（有位置参数则显示 "ARGS..."）
 * @param usage_ex  额外的帮助说明，显示在选项列表之后，如子命令说明或使用示例
 *                  支持 $0 占位符，输出时会被替换为程序名（argv[0]）
 *                  传 NULL 或空字符串时不显示
 */
void ARGS_usage(const char* pos_desc, const char* usage_ex);

/**
 * 解析命令行参数
 * @param argc      main() 的 argc
 * @param argv      main() 的 argv，解析后会重排：选项在前，位置参数在后
 * @param ...       参数定义列表，以 NULL 结尾，每项为 &ARGS_DEF_xxx
 * @return          位置参数数量（可通过 argv[argc - 返回值] 访问位置参数）
 *
 * @note 遇到 -h/--help 或无参数时自动打印帮助并退出
 * @note 必选参数缺失时打印错误并退出
 */
int ARGS_parse(int argc, char** argv, .../* end with NULL */);

/**
 * 打印命令行帮助信息
 * @param arg0      程序名（通常传 argv[0]）
 * @return          始终返回 0
 *
 * @note 通常由 ARGS_parse 自动调用，也可手动调用
 */
int ARGS_print(const char* arg0);

/**
 * 获取列表类型参数的元素数量
 * @param var       ARGS_L 定义的参数变量指针
 * @return          列表元素数量，若未设置返回 0
 */
int ARGS_ls_count(arg_var_st* var);

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
#ifdef __cplusplus
}
#endif
#pragma clang diagnostic pop
#endif  // UTIL_COMMON_H_
