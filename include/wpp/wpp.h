/*
 * WPP Project - Main Header
 * Copyright (C) 2026 WPP Project Contributors
 * Licensed under GPL 3.0
 */

#ifndef WPP_H
#define WPP_H

#ifdef __cplusplus
extern "C" {
#endif

/* 版本信息 */
#define WPP_VERSION_MAJOR 0
#define WPP_VERSION_MINOR 1
#define WPP_VERSION_PATCH 0
#define WPP_VERSION "0.1.0"

/* 返回值定义 */
#define WPP_OK       0
#define WPP_ERROR   -1

/* SQLite 支持 */
#include <sqlite3.h>

/* 导出其他模块的头文件（待实现）*/
/* #include <wpp/compiler.h> */
/* #include <wpp/database.h> */
/* #include <wpp/server.h> */
/* #include <wpp/core.h> */

#ifdef __cplusplus
}
#endif

#endif /* WPP_H */
