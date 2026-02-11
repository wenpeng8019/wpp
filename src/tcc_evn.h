/*
 * TinyCC Environment - Unified Integration
 */

#ifndef TCC_EVN_H
#define TCC_EVN_H

#include <libtcc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化 TCC 环境（由 main 触发）
 * 
 * 预先加载部分 buildins 资源，便于 fork 子进程直接复用。
 *
 * @return 0 成功，-1 失败
 */
int tcc_evn_init(void);

/**
 * 配置 TCC 编译环境
 * 
 * 职责：
 * 1. 设置 TCC 运行时库路径 (tcc_set_lib_path)
 * 2. 配置 include 路径 (tcc_add_include_path/tcc_add_sysinclude_path)
 * 3. 配置库搜索路径 (tcc_add_library_path)
 * 4. 链接标准 C 库 (tcc_add_library)
 * 5. 设置文件打开回调 (拦截文件访问，从 buildins 提供虚拟文件)
 * 6. 注册内置 API 符号 (SQLite3 + zlib)
 * 
 * @param s TinyCC 编译状态
 * @return 0 成功，-1 失败
 */
int tcc_configure(TCCState *s);

/**
 * 清理 TCC 环境
 */
void tcc_evn_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* TCC_EVN_H */
