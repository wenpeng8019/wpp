/*
 * TinyCC Environment - Unified Integration
 */

#include "tcc_evn.h"
#include "buildins.h"
#include "vfile.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <zlib.h>

static int g_tcc_evn_initialized = 0;

/**
 * API 声明字符串（预编译给用户代码使用）
 * 用户 C 脚本无需 #include 即可直接调用这些 API
 */
static const char *BUILDINS_API_DECLS = 
    "/* SQLite3 API - 类型定义 */\n"
    "typedef struct sqlite3 sqlite3;\n"
    "typedef struct sqlite3_stmt sqlite3_stmt;\n"
    "typedef long long sqlite3_int64;\n"
    "typedef unsigned long long sqlite3_uint64;\n"
    "\n"
    "/* SQLite3 API - 数据库操作 */\n"
    "int sqlite3_open(const char *filename, sqlite3 **ppDb);\n"
    "int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs);\n"
    "int sqlite3_close(sqlite3*);\n"
    "int sqlite3_exec(sqlite3*, const char *sql, int (*callback)(void*,int,char**,char**), void *, char **errmsg);\n"
    "\n"
    "/* SQLite3 API - 预编译语句 */\n"
    "int sqlite3_prepare_v2(sqlite3 *db, const char *zSql, int nByte, sqlite3_stmt **ppStmt, const char **pzTail);\n"
    "int sqlite3_step(sqlite3_stmt*);\n"
    "int sqlite3_finalize(sqlite3_stmt *pStmt);\n"
    "int sqlite3_reset(sqlite3_stmt *pStmt);\n"
    "\n"
    "/* SQLite3 API - 参数绑定 */\n"
    "int sqlite3_bind_text(sqlite3_stmt*, int, const char*, int, void(*)(void*));\n"
    "int sqlite3_bind_int(sqlite3_stmt*, int, int);\n"
    "int sqlite3_bind_int64(sqlite3_stmt*, int, sqlite3_int64);\n"
    "int sqlite3_bind_double(sqlite3_stmt*, int, double);\n"
    "int sqlite3_bind_null(sqlite3_stmt*, int);\n"
    "int sqlite3_bind_blob(sqlite3_stmt*, int, const void*, int n, void(*)(void*));\n"
    "int sqlite3_bind_parameter_count(sqlite3_stmt*);\n"
    "const char *sqlite3_bind_parameter_name(sqlite3_stmt*, int);\n"
    "int sqlite3_bind_parameter_index(sqlite3_stmt*, const char *zName);\n"
    "int sqlite3_clear_bindings(sqlite3_stmt*);\n"
    "\n"
    "/* SQLite3 API - 结果读取 */\n"
    "int sqlite3_column_count(sqlite3_stmt *pStmt);\n"
    "const char *sqlite3_column_name(sqlite3_stmt*, int N);\n"
    "int sqlite3_column_type(sqlite3_stmt*, int iCol);\n"
    "const unsigned char *sqlite3_column_text(sqlite3_stmt*, int iCol);\n"
    "int sqlite3_column_int(sqlite3_stmt*, int iCol);\n"
    "sqlite3_int64 sqlite3_column_int64(sqlite3_stmt*, int iCol);\n"
    "double sqlite3_column_double(sqlite3_stmt*, int iCol);\n"
    "const void *sqlite3_column_blob(sqlite3_stmt*, int iCol);\n"
    "int sqlite3_column_bytes(sqlite3_stmt*, int iCol);\n"
    "\n"
    "/* SQLite3 API - 错误处理 */\n"
    "const char *sqlite3_errmsg(sqlite3*);\n"
    "int sqlite3_errcode(sqlite3 *db);\n"
    "\n"
    "/* SQLite3 API - 其他 */\n"
    "int sqlite3_changes(sqlite3*);\n"
    "sqlite3_int64 sqlite3_changes64(sqlite3*);\n"
    "sqlite3_int64 sqlite3_last_insert_rowid(sqlite3*);\n"
    "void sqlite3_free(void*);\n"
    "const char *sqlite3_libversion(void);\n"
    "int sqlite3_libversion_number(void);\n"
    "int sqlite3_busy_timeout(sqlite3*, int ms);\n"
    "char *sqlite3_mprintf(const char*, ...);\n"
    "\n"
    "/* zlib API - 类型定义 */\n"
    "typedef unsigned char Bytef;\n"
    "typedef unsigned int uInt;\n"
    "typedef unsigned long uLong;\n"
    "typedef unsigned long uLongf;\n"
    "typedef void *voidpf;\n"
    "typedef struct z_stream_s {\n"
    "    const Bytef *next_in;\n"
    "    uInt avail_in;\n"
    "    uLong total_in;\n"
    "    Bytef *next_out;\n"
    "    uInt avail_out;\n"
    "    uLong total_out;\n"
    "    const char *msg;\n"
    "    struct internal_state *state;\n"
    "    voidpf (*zalloc)(voidpf opaque, uInt items, uInt size);\n"
    "    void (*zfree)(voidpf opaque, voidpf address);\n"
    "    voidpf opaque;\n"
    "    int data_type;\n"
    "    uLong adler;\n"
    "    uLong reserved;\n"
    "} z_stream;\n"
    "typedef z_stream *z_streamp;\n"
    "\n"
    "/* zlib API - 简单接口（一次性压缩/解压） */\n"
    "const char *zlibVersion(void);\n"
    "int compress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen);\n"
    "int compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level);\n"
    "uLong compressBound(uLong sourceLen);\n"
    "int uncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen);\n"
    "int uncompress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong *sourceLen);\n"
    "\n"
    "/* zlib API - 流式接口（分块处理） */\n"
    "int deflateInit_(z_streamp strm, int level, const char *version, int stream_size);\n"
    "int deflate(z_streamp strm, int flush);\n"
    "int deflateEnd(z_streamp strm);\n"
    "int inflateInit_(z_streamp strm, const char *version, int stream_size);\n"
    "int inflate(z_streamp strm, int flush);\n"
    "int inflateEnd(z_streamp strm);\n"
    "#define deflateInit(strm, level) deflateInit_((strm), (level), zlibVersion(), sizeof(z_stream))\n"
    "#define inflateInit(strm) inflateInit_((strm), zlibVersion(), sizeof(z_stream))\n"
    "\n"
    "/* zlib API - 校验和 */\n"
    "uLong adler32(uLong adler, const Bytef *buf, uInt len);\n"
    "uLong crc32(uLong crc, const Bytef *buf, uInt len);\n"
    "\n"
    "/* zlib 常量 */\n"
    "#define Z_OK 0\n"
    "#define Z_STREAM_END 1\n"
    "#define Z_NEED_DICT 2\n"
    "#define Z_ERRNO (-1)\n"
    "#define Z_STREAM_ERROR (-2)\n"
    "#define Z_DATA_ERROR (-3)\n"
    "#define Z_MEM_ERROR (-4)\n"
    "#define Z_BUF_ERROR (-5)\n"
    "#define Z_VERSION_ERROR (-6)\n"
    "#define Z_NO_FLUSH 0\n"
    "#define Z_PARTIAL_FLUSH 1\n"
    "#define Z_SYNC_FLUSH 2\n"
    "#define Z_FULL_FLUSH 3\n"
    "#define Z_FINISH 4\n"
    "#define Z_BLOCK 5\n"
    "#define Z_TREES 6\n"
    "#define Z_NO_COMPRESSION 0\n"
    "#define Z_BEST_SPEED 1\n"
    "#define Z_BEST_COMPRESSION 9\n"
    "#define Z_DEFAULT_COMPRESSION (-1)\n"
    "#define Z_NULL ((void*)0)\n"
    "\n"
    "/* SQLite 常量定义 */\n"
    "#define SQLITE_OK 0\n"
    "#define SQLITE_ERROR 1\n"
    "#define SQLITE_ROW 100\n"
    "#define SQLITE_DONE 101\n"
    "#define SQLITE_INTEGER 1\n"
    "#define SQLITE_FLOAT 2\n"
    "#define SQLITE_TEXT 3\n"
    "#define SQLITE_BLOB 4\n"
    "#define SQLITE_NULL 5\n"
    "#define SQLITE_OPEN_READONLY 0x00000001\n"
    "#define SQLITE_OPEN_READWRITE 0x00000002\n"
    "#define SQLITE_OPEN_CREATE 0x00000004\n"
    "#define SQLITE_OPEN_URI 0x00000040\n"
    "#define SQLITE_TRANSIENT ((void(*)(void*))-1)\n";

/**
 * 注册内置 API 符号到 TCC
 * 步骤：
 * 1. tcc_add_symbol() - 注册函数地址（运行时链接）
 * 2. tcc_compile_string() - 预编译函数声明（编译时类型检查）
 */
static int tcc_register_builtin_symbols(TCCState *s) {
    // ========== 注册 SQLite3 符号 ==========
    tcc_add_symbol(s, "sqlite3_open", sqlite3_open);
    tcc_add_symbol(s, "sqlite3_open_v2", sqlite3_open_v2);
    tcc_add_symbol(s, "sqlite3_close", sqlite3_close);
    tcc_add_symbol(s, "sqlite3_exec", sqlite3_exec);
    tcc_add_symbol(s, "sqlite3_prepare_v2", sqlite3_prepare_v2);
    tcc_add_symbol(s, "sqlite3_step", sqlite3_step);
    tcc_add_symbol(s, "sqlite3_finalize", sqlite3_finalize);
    tcc_add_symbol(s, "sqlite3_reset", sqlite3_reset);
    tcc_add_symbol(s, "sqlite3_bind_text", sqlite3_bind_text);
    tcc_add_symbol(s, "sqlite3_bind_int", sqlite3_bind_int);
    tcc_add_symbol(s, "sqlite3_bind_int64", sqlite3_bind_int64);
    tcc_add_symbol(s, "sqlite3_bind_double", sqlite3_bind_double);
    tcc_add_symbol(s, "sqlite3_bind_null", sqlite3_bind_null);
    tcc_add_symbol(s, "sqlite3_bind_blob", sqlite3_bind_blob);
    tcc_add_symbol(s, "sqlite3_bind_parameter_count", sqlite3_bind_parameter_count);
    tcc_add_symbol(s, "sqlite3_bind_parameter_name", sqlite3_bind_parameter_name);
    tcc_add_symbol(s, "sqlite3_bind_parameter_index", sqlite3_bind_parameter_index);
    tcc_add_symbol(s, "sqlite3_clear_bindings", sqlite3_clear_bindings);
    tcc_add_symbol(s, "sqlite3_column_count", sqlite3_column_count);
    tcc_add_symbol(s, "sqlite3_column_name", sqlite3_column_name);
    tcc_add_symbol(s, "sqlite3_column_type", sqlite3_column_type);
    tcc_add_symbol(s, "sqlite3_column_text", sqlite3_column_text);
    tcc_add_symbol(s, "sqlite3_column_int", sqlite3_column_int);
    tcc_add_symbol(s, "sqlite3_column_int64", sqlite3_column_int64);
    tcc_add_symbol(s, "sqlite3_column_double", sqlite3_column_double);
    tcc_add_symbol(s, "sqlite3_column_blob", sqlite3_column_blob);
    tcc_add_symbol(s, "sqlite3_column_bytes", sqlite3_column_bytes);
    tcc_add_symbol(s, "sqlite3_errmsg", sqlite3_errmsg);
    tcc_add_symbol(s, "sqlite3_errcode", sqlite3_errcode);
    tcc_add_symbol(s, "sqlite3_changes", sqlite3_changes);
    tcc_add_symbol(s, "sqlite3_changes64", sqlite3_changes64);
    tcc_add_symbol(s, "sqlite3_last_insert_rowid", sqlite3_last_insert_rowid);
    tcc_add_symbol(s, "sqlite3_free", sqlite3_free);
    tcc_add_symbol(s, "sqlite3_libversion", sqlite3_libversion);
    tcc_add_symbol(s, "sqlite3_libversion_number", sqlite3_libversion_number);
    tcc_add_symbol(s, "sqlite3_busy_timeout", sqlite3_busy_timeout);
    tcc_add_symbol(s, "sqlite3_mprintf", sqlite3_mprintf);

    // ========== 注册 zlib 符号 ==========
    // 简单接口
    tcc_add_symbol(s, "zlibVersion", zlibVersion);
    tcc_add_symbol(s, "compress", compress);
    tcc_add_symbol(s, "compress2", compress2);
    tcc_add_symbol(s, "compressBound", compressBound);
    tcc_add_symbol(s, "uncompress", uncompress);
    tcc_add_symbol(s, "uncompress2", uncompress2);
    
    // 流式接口
    tcc_add_symbol(s, "deflateInit_", deflateInit_);
    tcc_add_symbol(s, "deflate", deflate);
    tcc_add_symbol(s, "deflateEnd", deflateEnd);
    tcc_add_symbol(s, "inflateInit_", inflateInit_);
    tcc_add_symbol(s, "inflate", inflate);
    tcc_add_symbol(s, "inflateEnd", inflateEnd);
    
    // 校验和
    tcc_add_symbol(s, "adler32", adler32);
    tcc_add_symbol(s, "crc32", crc32);

    // ========== 预编译 API 声明 ==========
    // 用户 C 脚本无需 #include 即可直接调用
    if (tcc_compile_string(s, BUILDINS_API_DECLS) < 0) {
        fprintf(stderr, "TCC EVN: Failed to pre-compile API declarations\n");
        return -1;
    }

    return 0;
}

/**
 * TCC 文件打开回调：尝试从 buildins 查找文件
 * 
 * @param opaque 回调上下文（未使用）
 * @param filename 请求的文件名
 * @return fd >= 0 表示命中并返回文件描述符；-1 表示未命中，TCC 走正常路径
 */
static int tcc_file_open_callback(void *opaque, const char *filename) {
    // 从 buildins 查找文件
    buildin_file_info_st *node = buildins_find(filename);
    if (!node) {
        return -1;  // 未命中，TCC 走正常文件系统路径
    }

    // 只记录成功找到的文件
    fprintf(stderr, "[buildins] Found: %s\n", node->uri);
    
    // 获取虚拟文件
    vfile_st *vf = buildins_acquire_vfile(node);
    if (!vf) {
        fprintf(stderr, "  → buildins_acquire_vfile() returned NULL\n");
        return -1;
    }
    
    if (vf->fd < 0) {
        fprintf(stderr, "  → vfile fd is invalid (%d)\n", vf->fd);
        return -1;
    }

    fprintf(stderr, "  → vfile: fd=%d, size=%zu\n", vf->fd, vf->size);

    // 返回独立的 fd（TCC 会自己关闭它）
    int fd = dup(vf->fd);
    if (fd >= 0) {
        lseek(fd, 0, SEEK_SET);  // 重置到文件开头
        fprintf(stderr, "  → Successfully duped to fd: %d\n", fd);
    } else {
        fprintf(stderr, "  → dup() failed: errno=%d (%s)\n", errno, strerror(errno));
    }
    return fd;
}

int tcc_evn_init(void) {
    if (g_tcc_evn_initialized) {
        return 0;
    }
    g_tcc_evn_initialized = 1;
    return 0;
}

void tcc_evn_cleanup(void) {
    g_tcc_evn_initialized = 0;
}

int tcc_configure(TCCState *s) {
    if (!s) {
        fprintf(stderr, "TCC EVN: NULL TCCState pointer\n");
        return -1;
    }

    /* ========== tcc_set_file_open_callback() ========== 
     * 用途：拦截 TCC 的所有文件打开操作
     * 拦截点：_tcc_open() 函数，优先于 open() 系统调用
     * 覆盖范围：
     *   - #include 预处理指令
     *   - tcc_add_file() 调用
     *   - libtcc1.a 等运行时库文件
     * 回调返回：fd >= 0 命中 → 使用虚拟文件；-1 未命中 → fallback 到正常文件系统
     * 注意：必须在路径配置之前设置回调，因为回调会拦截并返回 buildins 虚拟文件
     */
    tcc_set_file_open_callback(s, NULL, tcc_file_open_callback);

    /* ========== tcc_set_lib_path() ========== 
     * 用途：设置 TCC 自己的运行时库路径（作为 sysroot）
     * 存储：s->tcc_lib_path (单个字符串，覆盖式)
     * 查找：libtcc1.a (TCC 运行时库), runmain.o (程序入口), bcheck.o/bt-*.o (调试/追踪)
     * 路径：/lib - 运行时库位于 buildins 虚拟文件系统，回调将拦截并返回 vfile
     * 注意：支持 {B} 占位符，其他路径可用 "{B}/include" 引用
     */
    tcc_set_lib_path(s, "/lib");

    /* ========== tcc_add_sysinclude_path() ========== 
     * 用途：添加系统级 include 路径（标准库头文件）
     * 存储：s->sysinclude_paths[] (字符串数组，累加式，可多次调用)
     * 查找：在 include_paths 搜索失败后才搜索这里
     * 优先级：低于 include_paths，作为 fallback
     * 
     * /include - TCC 基础头文件 (stddef.h, stdarg.h, stdbool.h 等) 位于 buildins
     * /usr/include - 系统标准库头文件 (stdio.h, stdlib.h 等)
     * /usr/local/include - 本地安装的库头文件
     */
    tcc_add_sysinclude_path(s, "/include");
    tcc_add_sysinclude_path(s, "/usr/include");
    tcc_add_sysinclude_path(s, "/usr/local/include");

    /* ========== tcc_add_library_path() ========== 
     * 用途：添加用户库搜索路径（用于查找 .a/.so 文件）
     * 存储：s->library_paths[] (字符串数组，累加式，可多次调用)
     * 查找：tcc_add_library("foo") 时在这些路径中搜索 libfoo.a 或 libfoo.so
     * 重要：与 tcc_set_lib_path 完全不同！
     *       tcc_set_lib_path   → TCC 内置文件 (libtcc1.a, runmain.o)
     *       tcc_add_library_path → 用户链接库 (libsqlite3.a, libz.so)
     */
    tcc_add_library_path(s, "/lib");
    tcc_add_library_path(s, "/usr/lib");
    tcc_add_library_path(s, "/usr/local/lib");

    /* ========== TCC 运行时库自动处理说明 ========== 
     * TinyCC 会自动处理以下运行时库文件，无需手动添加：
     * 
     * 1. libtcc1.a - TCC 运行时库（内置函数实现）
     *    由 tccelf.c:1847 自动调用 tcc_add_support(s1, TCC_LIBTCC1)
     * 
     * 2. runmain.o - 程序入口对象（tcc_run 需要）  
     *    由 tccrun.c:227 自动调用 tcc_add_support(s1, "runmain.o")
     * 
     * buildins/lib/ 中的这些文件仅用作虚拟文件系统中的默认查找路径，
     * 当 TCC 内部查找这些文件时会通过回调函数拦截并提供。
     * 
     * 手动 tcc_add_file() 会导致符号重复定义错误，应避免。
     */

    /* ========== 预编译基础标准头文件 ========== 
     * 用途：预先编译 TCC 基础头文件，加速后续用户代码编译
     * 文件：buildins/include 下的全部头文件（由 TCC 提供的编译器内置头文件）
     *   - stddef.h, stdarg.h, stdbool.h, stdalign.h, stdnoreturn.h, stdatomic.h
     *   - float.h, tgmath.h, tccdefs.h
     * 注意：通过回调从 buildins 虚拟文件系统读取
     */
    const char *builtin_headers[] = {
        "/include/stddef.h",
        "/include/stdarg.h",
        "/include/stdbool.h",
        "/include/stdalign.h",
        "/include/stdnoreturn.h",
        "/include/stdatomic.h",
        "/include/float.h",
        "/include/tgmath.h",
        // 注意：由于使用了 CONFIG_TCC_PREDEFS=1 构建选项
        // 即 tccdefs.h 已编译到 libtcc.a，并会被 tcc_predefs() 自动加载，
        // 因此，这里显式加载会导致宏重定义错误
        /* "/include/tccdefs.h", */ 
        NULL
    };
    for (int i = 0; builtin_headers[i] != NULL; i++) {
        if (tcc_add_file(s, builtin_headers[i]) < 0) {
            fprintf(stderr, "TCC EVN: Failed to preload %s\n", builtin_headers[i]);
            return -1;
        }
    }

    /* ========== tcc_add_library() ========== 
     * 用途：链接标准 C 库
     * 行为：在 library_paths 中搜索 libc.a 或 libc.so 并链接
     * 必要性：提供标准库函数 (printf, malloc 等)，除非用户代码只调用我们注册的 API
     * 
     * [测试中] 暂时禁用，验证是否必需
     */
    // tcc_add_library(s, "c");`


    /* ========== tcc_register_builtin_symbols() ========== 
     * 用途：注册内置 API 符号（SQLite3 + zlib）到 TCC
     * 两步操作：
     *   1. tcc_add_symbol() - 注册函数地址（运行时链接）
     *   2. tcc_compile_string() - 预编译函数声明（编译时类型检查）
     * 效果：用户 C 脚本无需 #include 即可直接调用这些 API
     */
    if (tcc_register_builtin_symbols(s) < 0) {
        return -1;
    }

    return 0;
}
