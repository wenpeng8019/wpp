//
// Created by 温朋 on 2026/2/9.
// SQTP (Structured Query Transfer Protocol) Implementation
//

#include "httpd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <ctype.h>
#include "../third_party/yyjson/src/yyjson.h"

// SQTP 内部使用的静态变量（从 httpd_sqtp 参数设置）
static char* zMethod = NULL;
static char* zScript = NULL;
static char* zProtocol = NULL;
static size_t* pnOut = NULL;
#define nOut (*pnOut)

// SQTP Header 结构
typedef struct {
    // DML headers
    char* table;                 // TABLE header (for DML)
    char* from;                  // FROM header (for SELECT)
    char** where_clauses;        // WHERE headers (可多个)
    int where_count;
    char** where_in_clauses;     // WHERE-IN headers (可多个)
    int where_in_count;
    char* columns;               // COLUMNS header
    char* order_by;              // ORDER-BY header
    char* group_by;              // GROUP-BY header
    char* having;                // HAVING header
    char* limit;                 // LIMIT header
    char* offset;                // OFFSET header
    char* join;                  // JOIN header
    char* content_type;          // Content-Type header
    char* content_length;        // Content-Length header
    char* view_format;           // X-SQTP-View-Format header
    char* on_conflict;           // ON-CONFLICT header (for INSERT)
    
    // DDL headers (for CREATE/DROP/ALTER)
    char* name;                  // NAME header (object name)
    char* type;                  // TYPE header (temporary, memory, etc)
    char* if_not_exists;         // IF-NOT-EXISTS header
    char* if_exists;             // IF-EXISTS header
    char* without_rowid;         // WITHOUT-ROWID header
    char* unique;                // UNIQUE header (for index/constraint)
    char** column_defs;          // COLUMN headers (可多个)
    int column_def_count;
    char* primary_key;           // PRIMARY-KEY header
    char* not_null;              // NOT-NULL header
    char** unique_constraints;   // UNIQUE constraint headers (可多个)
    int unique_constraint_count;
    char* autoinc;               // AUTOINC header
    char** foreign_keys;         // FOREIGN-KEY headers (可多个)
    int foreign_key_count;
    
    // TRIGGER headers
    char* timing;                // TIMING header (BEFORE/AFTER/INSTEAD OF)
    char* event;                 // EVENT header (INSERT/UPDATE/DELETE)
    char* for_each_row;          // FOR-EACH-ROW header
    char* when;                  // WHEN header (trigger condition)
    char* update_of;             // UPDATE-OF header (column list)
    char** actions;              // ACTION headers (可多个)
    int action_count;
    
    // ALTER headers
    char* action;                // ACTION header (for ALTER: RENAME-TABLE, ADD-COLUMN, etc.)
    char* new_name;              // NEW-NAME header (for RENAME operations)
} sqtp_headers_t;

// 前向声明
static void sqtp_parse_headers(sqtp_headers_t* headers);
static void sqtp_free_headers(sqtp_headers_t* headers);
static void sqtp_handle_select(sqtp_headers_t* headers);
static void sqtp_handle_insert(sqtp_headers_t* headers);
static void sqtp_handle_update(sqtp_headers_t* headers);
static void sqtp_handle_delete(sqtp_headers_t* headers);
static void sqtp_handle_upsert(sqtp_headers_t* headers);
static void sqtp_handle_reset(sqtp_headers_t* headers);
static void sqtp_handle_begin(sqtp_headers_t* headers);
static void sqtp_handle_commit(sqtp_headers_t* headers);
static void sqtp_handle_rollback(sqtp_headers_t* headers);
static void sqtp_handle_savepoint(sqtp_headers_t* headers);
static void sqtp_handle_create(sqtp_headers_t* headers);
static void sqtp_handle_drop(sqtp_headers_t* headers);
static void sqtp_handle_alter(sqtp_headers_t* headers);
static int sqtp_create_table(sqlite3* db, sqtp_headers_t* headers, char* sql, size_t sql_size);
static int sqtp_create_index(sqlite3* db, sqtp_headers_t* headers, char* sql, size_t sql_size);
static int sqtp_create_trigger(sqlite3* db, sqtp_headers_t* headers, char* sql, size_t sql_size);
static void sqtp_send_error(int code, const char* message);
static char* sqtp_read_body(const char* content_length_str);
static sqlite3* sqtp_open_database(const char* uri_path);
static bool sqtp_database_exists(const char* uri_path);

// 开始 SQTP 响应
static void sqtp_start_response(const char* status) {
    althttpd_printf("%s %s\r\n", zProtocol, status);
    althttpd_printf("X-SQTP-Protocol: SQTP/1.0\r\n");
}

// SQTP 主处理函数
void httpd_sqtp(char* method, char* script, char* protocol, size_t* out) {
    // 设置静态变量供内部函数使用
    zMethod = method;
    zScript = script;
    zProtocol = protocol;
    pnOut = out;
    
    sqtp_headers_t headers = {0};
    
    // 解析 SQTP 头部
    sqtp_parse_headers(&headers);
    
    // 获取实际的 SQL 操作（去掉 SQTP- 前缀）
    const char* sqlMethod = zMethod + 5;  // 跳过 "SQTP-" 前缀
    
    // 根据 METHOD 分发处理
    if (strcmp(sqlMethod, "SELECT") == 0) {
        sqtp_handle_select(&headers);
    }
    else if (strcmp(sqlMethod, "INSERT") == 0) {
        sqtp_handle_insert(&headers);
    }
    else if (strcmp(sqlMethod, "UPDATE") == 0) {
        sqtp_handle_update(&headers);
    }
    else if (strcmp(sqlMethod, "DELETE") == 0) {
        sqtp_handle_delete(&headers);
    }
    else if (strcmp(sqlMethod, "UPSERT") == 0) {
        sqtp_handle_upsert(&headers);
    }
    else if (strcmp(sqlMethod, "RESET") == 0) {
        sqtp_handle_reset(&headers);
    }
    else if (strcmp(sqlMethod, "BEGIN") == 0) {
        sqtp_handle_begin(&headers);
    }
    else if (strcmp(sqlMethod, "COMMIT") == 0) {
        sqtp_handle_commit(&headers);
    }
    else if (strcmp(sqlMethod, "ROLLBACK") == 0) {
        sqtp_handle_rollback(&headers);
    }
    else if (strcmp(sqlMethod, "SAVEPOINT") == 0) {
        sqtp_handle_savepoint(&headers);
    }
    else if (strcmp(sqlMethod, "CREATE") == 0) {
        sqtp_handle_create(&headers);
    }
    else if (strcmp(sqlMethod, "DROP") == 0) {
        sqtp_handle_drop(&headers);
    }
    else if (strcmp(sqlMethod, "ALTER") == 0) {
        sqtp_handle_alter(&headers);
    }
    else {
        sqtp_send_error(501, "Unsupported SQTP method");
    }
    
    // 释放资源
    sqtp_free_headers(&headers);
    MakeLogEntry(1, 0);
}

// 解析 SQTP 头部字段
static void sqtp_parse_headers(sqtp_headers_t* headers) {
    char zLine[10000];
    
    // 初始化 DML 数组
    headers->where_clauses = malloc(sizeof(char*) * 100);  // 最多 100 个 WHERE
    headers->where_in_clauses = malloc(sizeof(char*) * 100);
    headers->where_count = 0;
    headers->where_in_count = 0;
    
    // 初始化 DDL 数组
    headers->column_defs = malloc(sizeof(char*) * 100);  // 最多 100 个 COLUMN
    headers->column_def_count = 0;
    headers->unique_constraints = malloc(sizeof(char*) * 100);
    headers->unique_constraint_count = 0;
    headers->foreign_keys = malloc(sizeof(char*) * 100);
    headers->foreign_key_count = 0;
    headers->actions = malloc(sizeof(char*) * 100);  // 最多 100 个 ACTION
    headers->action_count = 0;
    
    // 读取头部字段
    while (althttpd_fgets(zLine, sizeof(zLine), stdin)) {
        char *zFieldName;
        char *zVal;
        
        zFieldName = GetFirstElement(zLine, &zVal);
        if (zFieldName == 0 || *zFieldName == 0) break;  // 空行表示头部结束
        RemoveNewline(zVal);
        
        if (strcasecmp(zFieldName, "TABLE:") == 0) {
            headers->table = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "FROM:") == 0) {
            headers->from = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "WHERE:") == 0) {
            headers->where_clauses[headers->where_count++] = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "WHERE-IN:") == 0) {
            headers->where_in_clauses[headers->where_in_count++] = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "COLUMNS:") == 0) {
            headers->columns = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "ORDER-BY:") == 0) {
            headers->order_by = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "GROUP-BY:") == 0) {
            headers->group_by = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "HAVING:") == 0) {
            headers->having = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "LIMIT:") == 0) {
            headers->limit = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "OFFSET:") == 0) {
            headers->offset = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "JOIN:") == 0) {
            headers->join = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "Content-Type:") == 0) {
            headers->content_type = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "Content-Length:") == 0) {
            headers->content_length = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "X-SQTP-View-Format:") == 0) {
            headers->view_format = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "ON-CONFLICT:") == 0) {
            headers->on_conflict = StrDup(zVal);
        }
        // DDL headers
        else if (strcasecmp(zFieldName, "NAME:") == 0) {
            headers->name = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "TYPE:") == 0) {
            headers->type = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "IF-NOT-EXISTS:") == 0) {
            headers->if_not_exists = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "IF-EXISTS:") == 0) {
            headers->if_exists = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "WITHOUT-ROWID:") == 0) {
            headers->without_rowid = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "UNIQUE:") == 0) {
            headers->unique = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "COLUMN:") == 0) {
            headers->column_defs[headers->column_def_count++] = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "PRIMARY-KEY:") == 0) {
            headers->primary_key = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "NOT-NULL:") == 0) {
            headers->not_null = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "UNIQUE-CONSTRAINT:") == 0) {
            headers->unique_constraints[headers->unique_constraint_count++] = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "AUTOINC:") == 0) {
            headers->autoinc = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "FOREIGN-KEY:") == 0) {
            headers->foreign_keys[headers->foreign_key_count++] = StrDup(zVal);
        }
        // TRIGGER headers
        else if (strcasecmp(zFieldName, "TIMING:") == 0) {
            headers->timing = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "EVENT:") == 0) {
            headers->event = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "FOR-EACH-ROW:") == 0) {
            headers->for_each_row = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "WHEN:") == 0) {
            headers->when = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "UPDATE-OF:") == 0) {
            headers->update_of = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "ACTION:") == 0) {
            // 检查是否已有单数 action（ALTER 用）
            if (!headers->action) {
                headers->action = StrDup(zVal);
            }
            // 同时也添加到 actions 数组（TRIGGER 用）
            headers->actions[headers->action_count++] = StrDup(zVal);
        }
        else if (strcasecmp(zFieldName, "NEW-NAME:") == 0) {
            headers->new_name = StrDup(zVal);
        }
    }
}

// 释放 headers 资源
static void sqtp_free_headers(sqtp_headers_t* headers) {
    if (headers->table) free(headers->table);
    if (headers->from) free(headers->from);
    if (headers->columns) free(headers->columns);
    if (headers->order_by) free(headers->order_by);
    if (headers->group_by) free(headers->group_by);
    if (headers->having) free(headers->having);
    if (headers->limit) free(headers->limit);
    if (headers->offset) free(headers->offset);
    if (headers->join) free(headers->join);
    if (headers->content_type) free(headers->content_type);
    if (headers->content_length) free(headers->content_length);
    if (headers->view_format) free(headers->view_format);
    if (headers->on_conflict) free(headers->on_conflict);
    
    // Free DDL fields
    if (headers->name) free(headers->name);
    if (headers->type) free(headers->type);
    if (headers->if_not_exists) free(headers->if_not_exists);
    if (headers->if_exists) free(headers->if_exists);
    if (headers->without_rowid) free(headers->without_rowid);
    if (headers->unique) free(headers->unique);
    if (headers->primary_key) free(headers->primary_key);
    if (headers->not_null) free(headers->not_null);
    if (headers->autoinc) free(headers->autoinc);
    
    for (int i = 0; i < headers->where_count; i++) {
        if (headers->where_clauses[i]) free(headers->where_clauses[i]);
    }
    if (headers->where_clauses) free(headers->where_clauses);
    
    for (int i = 0; i < headers->where_in_count; i++) {
        if (headers->where_in_clauses[i]) free(headers->where_in_clauses[i]);
    }
    if (headers->where_in_clauses) free(headers->where_in_clauses);
    
    for (int i = 0; i < headers->column_def_count; i++) {
        if (headers->column_defs[i]) free(headers->column_defs[i]);
    }
    if (headers->column_defs) free(headers->column_defs);
    
    for (int i = 0; i < headers->unique_constraint_count; i++) {
        if (headers->unique_constraints[i]) free(headers->unique_constraints[i]);
    }
    if (headers->unique_constraints) free(headers->unique_constraints);
    
    for (int i = 0; i < headers->foreign_key_count; i++) {
        if (headers->foreign_keys[i]) free(headers->foreign_keys[i]);
    }
    if (headers->foreign_keys) free(headers->foreign_keys);
    
    // Free TRIGGER fields
    if (headers->timing) free(headers->timing);
    if (headers->event) free(headers->event);
    if (headers->for_each_row) free(headers->for_each_row);
    if (headers->when) free(headers->when);
    if (headers->update_of) free(headers->update_of);
    
    for (int i = 0; i < headers->action_count; i++) {
        if (headers->actions[i]) free(headers->actions[i]);
    }
    if (headers->actions) free(headers->actions);
    
    // Free ALTER fields
    if (headers->action) free(headers->action);
    if (headers->new_name) free(headers->new_name);
}

// JSON 字符串转义函数 - 转义特殊字符以生成有效的 JSON
static void sqtp_json_escape(const char* input) {
    if (!input) {
        althttpd_printf("null");
        return;
    }
    
    althttpd_printf("\"");
    for (const char* p = input; *p; p++) {
        switch (*p) {
            case '"':  althttpd_printf("\\\""); break;
            case '\\': althttpd_printf("\\\\"); break;
            case '\b': althttpd_printf("\\b"); break;
            case '\f': althttpd_printf("\\f"); break;
            case '\n': althttpd_printf("\\n"); break;
            case '\r': althttpd_printf("\\r"); break;
            case '\t': althttpd_printf("\\t"); break;
            default:
                if ((unsigned char)*p < 32) {
                    // 控制字符转义为 \uXXXX
                    althttpd_printf("\\u%04x", (unsigned char)*p);
                } else {
                    althttpd_printf("%c", *p);
                }
        }
    }
    althttpd_printf("\"");
}

// 发送错误响应
static void sqtp_send_error(int code, const char* message) {
    char status[100];
    snprintf(status, sizeof(status), "%d %s", code, 
             code == 400 ? "Bad Request" :
             code == 404 ? "Not Found" :
             code == 500 ? "Internal Server Error" :
             code == 501 ? "Not Implemented" : "Error");
    
    sqtp_start_response(status);
    nOut += althttpd_printf(
        "Content-Type: application/json; charset=utf-8\r\n"
        "\r\n"
        "{\"error\":\"%s\",\"code\":%d}\n",
        message, code
    );
}

// 处理 SELECT 命令
static void sqtp_handle_select(sqtp_headers_t* headers) {
    sqlite3* db = NULL;
    sqlite3_stmt* stmt = NULL;
    char sql[10000];
    int rc;
    
    // 打开数据库
    db = sqtp_open_database(zScript);
    if (!db) {
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    // 确定表名：优先使用 FROM header，否则使用 TABLE header
    const char* table_name = headers->from ? headers->from : headers->table;
    
    // 检查必需的表名
    if (!table_name || strlen(table_name) == 0) {
        sqtp_send_error(400, "Missing table name (FROM or TABLE header)");
        sqlite3_close(db);
        return;
    }
    
    // 构造 SQL 查询
    snprintf(sql, sizeof(sql), "SELECT ");
    
    // COLUMNS
    if (headers->columns && strlen(headers->columns) > 0) {
        strncat(sql, headers->columns, sizeof(sql) - strlen(sql) - 1);
    } else {
        strncat(sql, "*", sizeof(sql) - strlen(sql) - 1);
    }
    
    // FROM
    strncat(sql, " FROM ", sizeof(sql) - strlen(sql) - 1);
    strncat(sql, table_name, sizeof(sql) - strlen(sql) - 1);
    
    // WHERE
    if (headers->where_count > 0) {
        strncat(sql, " WHERE ", sizeof(sql) - strlen(sql) - 1);
        for (int i = 0; i < headers->where_count; i++) {
            if (i > 0) strncat(sql, " AND ", sizeof(sql) - strlen(sql) - 1);
            strncat(sql, "(", sizeof(sql) - strlen(sql) - 1);
            strncat(sql, headers->where_clauses[i], sizeof(sql) - strlen(sql) - 1);
            strncat(sql, ")", sizeof(sql) - strlen(sql) - 1);
        }
    }
    
    // ORDER BY
    if (headers->order_by) {
        strncat(sql, " ORDER BY ", sizeof(sql) - strlen(sql) - 1);
        strncat(sql, headers->order_by, sizeof(sql) - strlen(sql) - 1);
    }
    
    // LIMIT
    if (headers->limit) {
        strncat(sql, " LIMIT ", sizeof(sql) - strlen(sql) - 1);
        strncat(sql, headers->limit, sizeof(sql) - strlen(sql) - 1);
    }
    
    // OFFSET
    if (headers->offset) {
        strncat(sql, " OFFSET ", sizeof(sql) - strlen(sql) - 1);
        strncat(sql, headers->offset, sizeof(sql) - strlen(sql) - 1);
    }
    
    // 准备 SQL 语句
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqtp_send_error(500, sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }
    
    // 发送响应头
    sqtp_start_response("200 OK");
    nOut += althttpd_printf("Content-Type: application/json; charset=utf-8\r\n\r\n");
    
    // 输出 JSON 数组开始
    nOut += althttpd_printf("[");
    
    int row_count = 0;
    int col_count = sqlite3_column_count(stmt);
    
    // 遍历结果
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (row_count > 0) nOut += althttpd_printf(",");
        nOut += althttpd_printf("\n  {");
        
        for (int i = 0; i < col_count; i++) {
            if (i > 0) nOut += althttpd_printf(",");
            
            const char* col_name = sqlite3_column_name(stmt, i);
            nOut += althttpd_printf("\"%s\":", col_name);
            
            int col_type = sqlite3_column_type(stmt, i);
            switch (col_type) {
                case SQLITE_INTEGER:
                    nOut += althttpd_printf("%lld", sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    nOut += althttpd_printf("%g", sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT:
                    sqtp_json_escape((const char*)sqlite3_column_text(stmt, i));
                    break;
                case SQLITE_NULL:
                    nOut += althttpd_printf("null");
                    break;
                default:
                    nOut += althttpd_printf("null");
            }
        }
        
        nOut += althttpd_printf("}");
        row_count++;
    }
    
    nOut += althttpd_printf("\n]\n");
    
    // 清理
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

// 读取请求 body
static char* sqtp_read_body(const char* content_length_str) {
    if (!content_length_str) return NULL;
    
    int content_length = atoi(content_length_str);
    if (content_length <= 0 || content_length > 10*1024*1024) {  // 最大 10MB
        return NULL;
    }
    
    char* body = malloc(content_length + 1);
    if (!body) return NULL;
    
    size_t total_read = 0;
    while (total_read < (size_t)content_length) {
        size_t remaining = content_length - total_read;
        size_t chunk = fread(body + total_read, 1, remaining, stdin);
        if (chunk == 0) break;
        total_read += chunk;
    }
    
    body[total_read] = '\0';
    return body;
}

// 打开数据库连接
static sqlite3* sqtp_open_database(const char* uri_path) {
    sqlite3* db = NULL;
    char db_path[512] = {0};
    bool auto_create = false;  // 是否自动创建数据库
    
    // URI 格式策略：
    // - "/" -> 共享内存数据库 (file:shm?mode=memory&cache=shared)
    // - "/.db" -> .db 文件（自动创建）
    // - 其他 -> 实际文件路径（必须存在，不自动创建）
    
    const char* path = uri_path;
    
    // 跳过开头的 '/'
    if (*path == '/') path++;
    
    // 特殊处理：根路径 -> 共享内存数据库
    if (*path == '\0' || strcmp(uri_path, "/") == 0) {
        strcpy(db_path, "file:shm?mode=memory&cache=shared");
        auto_create = true;
    }
    // 特殊处理：.db -> .db 文件（自动创建）
    else if (strcmp(path, ".db") == 0) {
        strcpy(db_path, ".db");
        auto_create = true;
    }
    // 普通路径：使用实际路径，不添加扩展名
    else {
        // 复制路径，去掉查询参数和 fragment
        const char* query = strchr(path, '?');
        const char* fragment = strchr(path, '#');
        const char* stop = path + strlen(path);
        if (query && query < stop) stop = query;
        if (fragment && fragment < stop) stop = fragment;
        
        size_t path_len = stop - path;
        if (path_len >= sizeof(db_path)) {
            path_len = sizeof(db_path) - 1;
        }
        
        strncpy(db_path, path, path_len);
        db_path[path_len] = '\0';
        
        // 检查文件是否存在（普通路径不自动创建）
        if (access(db_path, F_OK) != 0) {
            return NULL;  // 文件不存在，返回 NULL
        }
    }
    
    // 打开数据库
    int flags = SQLITE_OPEN_READWRITE;
    if (auto_create) {
        flags |= SQLITE_OPEN_CREATE;
    }
    if (strncmp(db_path, "file:", 5) == 0) {
        flags |= SQLITE_OPEN_URI;  // URI 格式需要此标志
    }
    
    int rc = sqlite3_open_v2(db_path, &db, flags, NULL);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    
    return db;
}

// 检查数据库路径是否存在（用于区分404和500错误）
static bool sqtp_database_exists(const char* uri_path) {
    const char* path = uri_path;
    
    // 跳过开头的 '/'
    if (*path == '/') path++;
    
    // 特殊路径总是"存在"（会自动创建或使用内存）
    // "/" -> 共享内存数据库
    // "/.db" -> .db 文件（自动创建）
    if (*path == '\0' || strcmp(uri_path, "/") == 0 || strcmp(path, ".db") == 0) {
        return true;
    }
    
    // 普通路径：提取实际文件路径
    char db_path[512] = {0};
    const char* query = strchr(path, '?');
    const char* fragment = strchr(path, '#');
    const char* stop = path + strlen(path);
    if (query && query < stop) stop = query;
    if (fragment && fragment < stop) stop = fragment;
    
    size_t path_len = stop - path;
    if (path_len >= sizeof(db_path)) {
        path_len = sizeof(db_path) - 1;
    }
    
    strncpy(db_path, path, path_len);
    db_path[path_len] = '\0';
    
    // 检查文件是否存在
    return access(db_path, F_OK) == 0;
}

// 处理 INSERT 命令
static void sqtp_handle_insert(sqtp_headers_t* headers) {
    sqlite3* db = NULL;
    char sql[10000];
    char* errmsg = NULL;
    int rc;
    
    // 检查必需的 headers
    if (!headers->table || strlen(headers->table) == 0) {
        sqtp_send_error(400, "Missing TABLE header");
        return;
    }
    
    if (!headers->columns || strlen(headers->columns) == 0) {
        sqtp_send_error(400, "Missing COLUMNS header");
        return;
    }
    
    // 读取 body
    char* body = sqtp_read_body(headers->content_length);
    if (!body) {
        sqtp_send_error(400, "Missing or invalid request body");
        return;
    }
    
    // 解析 JSON body
    yyjson_doc* doc = yyjson_read(body, strlen(body), 0);
    free(body);
    
    if (!doc) {
        sqtp_send_error(400, "Invalid JSON in request body");
        return;
    }
    
    yyjson_val* root = yyjson_doc_get_root(doc);
    
    // 打开数据库
    db = sqtp_open_database(zScript);
    if (!db) {
        yyjson_doc_free(doc);
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    // 判断是单行还是多行插入
    bool is_array_of_arrays = yyjson_is_arr(root) && yyjson_arr_size(root) > 0 &&
                               yyjson_is_arr(yyjson_arr_get_first(root));
    
    int rows_inserted = 0;
    
    // 开始事务
    sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
    
    if (is_array_of_arrays) {
        // 多行插入
        size_t idx, max;
        yyjson_val* row_val;
        yyjson_arr_foreach(root, idx, max, row_val) {
            // 构造 INSERT 语句
            snprintf(sql, sizeof(sql), "INSERT INTO %s (%s) VALUES (", 
                     headers->table, headers->columns);
            
            size_t col_idx, col_max;
            yyjson_val* val;
            yyjson_arr_foreach(row_val, col_idx, col_max, val) {
                if (col_idx > 0) strncat(sql, ", ", sizeof(sql) - strlen(sql) - 1);
                
                if (yyjson_is_str(val)) {
                    strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
                    strncat(sql, yyjson_get_str(val), sizeof(sql) - strlen(sql) - 1);
                    strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
                } else if (yyjson_is_num(val)) {
                    char num_buf[64];
                    if (yyjson_is_int(val)) {
                        snprintf(num_buf, sizeof(num_buf), "%lld", yyjson_get_sint(val));
                    } else {
                        snprintf(num_buf, sizeof(num_buf), "%g", yyjson_get_real(val));
                    }
                    strncat(sql, num_buf, sizeof(sql) - strlen(sql) - 1);
                } else if (yyjson_is_null(val)) {
                    strncat(sql, "NULL", sizeof(sql) - strlen(sql) - 1);
                } else if (yyjson_is_bool(val)) {
                    strncat(sql, yyjson_get_bool(val) ? "1" : "0", sizeof(sql) - strlen(sql) - 1);
                }
            }
            strncat(sql, ")", sizeof(sql) - strlen(sql) - 1);
            
            rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
            if (rc == SQLITE_OK) {
                rows_inserted++;
            }
        }
    } else if (yyjson_is_arr(root)) {
        // 单行插入（数组格式）
        snprintf(sql, sizeof(sql), "INSERT INTO %s (%s) VALUES (", 
                 headers->table, headers->columns);
        
        size_t idx, max;
        yyjson_val* val;
        yyjson_arr_foreach(root, idx, max, val) {
            if (idx > 0) strncat(sql, ", ", sizeof(sql) - strlen(sql) - 1);
            
            if (yyjson_is_str(val)) {
                strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
                strncat(sql, yyjson_get_str(val), sizeof(sql) - strlen(sql) - 1);
                strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
            } else if (yyjson_is_num(val)) {
                char num_buf[64];
                if (yyjson_is_int(val)) {
                    snprintf(num_buf, sizeof(num_buf), "%lld", yyjson_get_sint(val));
                } else {
                    snprintf(num_buf, sizeof(num_buf), "%g", yyjson_get_real(val));
                }
                strncat(sql, num_buf, sizeof(sql) - strlen(sql) - 1);
            } else if (yyjson_is_null(val)) {
                strncat(sql, "NULL", sizeof(sql) - strlen(sql) - 1);
            } else if (yyjson_is_bool(val)) {
                strncat(sql, yyjson_get_bool(val) ? "1" : "0", sizeof(sql) - strlen(sql) - 1);
            }
        }
        strncat(sql, ")", sizeof(sql) - strlen(sql) - 1);
        
        rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
        if (rc == SQLITE_OK) {
            rows_inserted = 1;
        }
    }
    
    // 提交事务
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    
    yyjson_doc_free(doc);
    
    // 发送响应
    if (rows_inserted > 0) {
        sqtp_start_response("201 Created");
        nOut += althttpd_printf("Content-Type: application/json; charset=utf-8\r\n");
        nOut += althttpd_printf("X-SQTP-Changes: %d\r\n", rows_inserted);
        nOut += althttpd_printf("\r\n{\"inserted\":%d}\n", rows_inserted);
    } else {
        sqtp_send_error(500, errmsg ? errmsg : "Insert failed");
    }
    
    if (errmsg) sqlite3_free(errmsg);
    sqlite3_close(db);
}

// 处理 UPDATE 命令
static void sqtp_handle_update(sqtp_headers_t* headers) {
    sqlite3* db = NULL;
    char sql[10000];
    char* errmsg = NULL;
    int rc;
    
    // 检查必需的 headers
    if (!headers->table || strlen(headers->table) == 0) {
        sqtp_send_error(400, "Missing TABLE header");
        return;
    }
    
    if (!headers->columns || strlen(headers->columns) == 0) {
        sqtp_send_error(400, "Missing COLUMNS header");
        return;
    }
    
    // UPDATE 必须有 WHERE 条件（防止误更新全表）
    if (headers->where_count == 0) {
        sqtp_send_error(400, "WHERE clause required for UPDATE (use WHERE: * for全表更新)");
        return;
    }
    
    // 读取 body
    char* body = sqtp_read_body(headers->content_length);
    if (!body) {
        sqtp_send_error(400, "Missing or invalid request body");
        return;
    }
    
    // 解析 JSON body
    yyjson_doc* doc = yyjson_read(body, strlen(body), 0);
    free(body);
    
    if (!doc) {
        sqtp_send_error(400, "Invalid JSON in request body");
        return;
    }
    
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        sqtp_send_error(400, "Request body must be a JSON array");
        return;
    }
    
    // 打开数据库
    db = sqtp_open_database(zScript);
    if (!db) {
        yyjson_doc_free(doc);
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    // 构造 UPDATE 语句
    snprintf(sql, sizeof(sql), "UPDATE %s SET ", headers->table);
    
    // 解析列名
    char columns_copy[1000];
    strncpy(columns_copy, headers->columns, sizeof(columns_copy) - 1);
    columns_copy[sizeof(columns_copy) - 1] = '\0';
    
    char* col_name = strtok(columns_copy, ",");
    size_t idx = 0;
    size_t max = yyjson_arr_size(root);
    
    while (col_name && idx < max) {
        // 去掉前后空格
        while (*col_name == ' ') col_name++;
        char* end = col_name + strlen(col_name) - 1;
        while (end > col_name && *end == ' ') *end-- = '\0';
        
        if (idx > 0) strncat(sql, ", ", sizeof(sql) - strlen(sql) - 1);
        
        strncat(sql, col_name, sizeof(sql) - strlen(sql) - 1);
        strncat(sql, " = ", sizeof(sql) - strlen(sql) - 1);
        
        yyjson_val* val = yyjson_arr_get(root, idx);
        if (yyjson_is_str(val)) {
            strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
            strncat(sql, yyjson_get_str(val), sizeof(sql) - strlen(sql) - 1);
            strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
        } else if (yyjson_is_num(val)) {
            char num_buf[64];
            if (yyjson_is_int(val)) {
                snprintf(num_buf, sizeof(num_buf), "%lld", yyjson_get_sint(val));
            } else {
                snprintf(num_buf, sizeof(num_buf), "%g", yyjson_get_real(val));
            }
            strncat(sql, num_buf, sizeof(sql) - strlen(sql) - 1);
        } else if (yyjson_is_null(val)) {
            strncat(sql, "NULL", sizeof(sql) - strlen(sql) - 1);
        } else if (yyjson_is_bool(val)) {
            strncat(sql, yyjson_get_bool(val) ? "1" : "0", sizeof(sql) - strlen(sql) - 1);
        }
        
        col_name = strtok(NULL, ",");
        idx++;
    }
    
    // WHERE clause
    if (headers->where_count > 0) {
        // 检查是否是全表更新
        if (strcmp(headers->where_clauses[0], "*") == 0) {
            // 全表更新，不添加 WHERE
        } else {
            strncat(sql, " WHERE ", sizeof(sql) - strlen(sql) - 1);
            for (int i = 0; i < headers->where_count; i++) {
                if (i > 0) strncat(sql, " AND ", sizeof(sql) - strlen(sql) - 1);
                strncat(sql, "(", sizeof(sql) - strlen(sql) - 1);
                strncat(sql, headers->where_clauses[i], sizeof(sql) - strlen(sql) - 1);
                strncat(sql, ")", sizeof(sql) - strlen(sql) - 1);
            }
        }
    }
    
    // 执行UPDATE
    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    yyjson_doc_free(doc);
    
    int changes = sqlite3_changes(db);
    
    if (rc == SQLITE_OK) {
        sqtp_start_response("200 OK");
        nOut += althttpd_printf("Content-Type: application/json; charset=utf-8\r\n");
        nOut += althttpd_printf("X-SQTP-Changes: %d\r\n", changes);
        nOut += althttpd_printf("\r\n{\"updated\":%d}\n", changes);
    } else {
        sqtp_send_error(500, errmsg ? errmsg : "Update failed");
    }
    
    if (errmsg) sqlite3_free(errmsg);
    sqlite3_close(db);
}

// 处理 DELETE 命令
static void sqtp_handle_delete(sqtp_headers_t* headers) {
    sqlite3* db = NULL;
    char sql[10000];
    char* errmsg = NULL;
    int rc;
    
    // 检查必需的 headers
    if (!headers->table || strlen(headers->table) == 0) {
        sqtp_send_error(400, "Missing TABLE header");
        return;
    }
    
    // DELETE 必须有 WHERE 条件（防止误删全表）
    if (headers->where_count == 0) {
        sqtp_send_error(400, "WHERE clause required for DELETE (use WHERE: * for全表删除)");
        return;
    }
    
    // 打开数据库
    db = sqtp_open_database(zScript);
    if (!db) {
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    // 构造 DELETE 语句
    snprintf(sql, sizeof(sql), "DELETE FROM %s", headers->table);
    
    // WHERE clause
    if (headers->where_count > 0) {
        // 检查是否是全表删除
        if (strcmp(headers->where_clauses[0], "*") == 0) {
            // 全表删除，不添加 WHERE
        } else {
            strncat(sql, " WHERE ", sizeof(sql) - strlen(sql) - 1);
            for (int i = 0; i < headers->where_count; i++) {
                if (i > 0) strncat(sql, " AND ", sizeof(sql) - strlen(sql) - 1);
                strncat(sql, "(", sizeof(sql) - strlen(sql) - 1);
                strncat(sql, headers->where_clauses[i], sizeof(sql) - strlen(sql) - 1);
                strncat(sql, ")", sizeof(sql) - strlen(sql) - 1);
            }
        }
    }
    
    // 执行DELETE
    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    int changes = sqlite3_changes(db);
    
    if (rc == SQLITE_OK) {
        sqtp_start_response("200 OK");
        nOut += althttpd_printf("Content-Type: application/json; charset=utf-8\r\n");
        nOut += althttpd_printf("X-SQTP-Changes: %d\r\n", changes);
        nOut += althttpd_printf("\r\n{\"deleted\":%d}\n", changes);
    } else {
        sqtp_send_error(500, errmsg ? errmsg : "Delete failed");
    }
    
    if (errmsg) sqlite3_free(errmsg);
    sqlite3_close(db);
}
// SQTP Additional Operations Implementation
// UPSERT, RESET, Transactions

// This file will be appended to http_sqtp.c

// 处理 UPSERT 命令
static void sqtp_handle_upsert(sqtp_headers_t* headers) {
    sqlite3* db = NULL;
    char sql[10000];
    char*errmsg = NULL;
    int rc;
    
    // 检查必需的 headers
    if (!headers->table || strlen(headers->table) == 0) {
        sqtp_send_error(400, "Missing TABLE header");
        return;
    }
    
    if (!headers->columns || strlen(headers->columns) == 0) {
        sqtp_send_error(400, "Missing COLUMNS header");
        return;
    }
    
    // 读取 body
    char* body = sqtp_read_body(headers->content_length);
    if (!body) {
        sqtp_send_error(400, "Missing or invalid request body");
        return;
    }
    
    // 解析 JSON body
    yyjson_doc* doc = yyjson_read(body, strlen(body), 0);
    free(body);
    
    if (!doc) {
        sqtp_send_error(400, "Invalid JSON in request body");
        return;
    }
    
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        sqtp_send_error(400, "Request body must be a JSON array");
        return;
    }
    
    // 打开数据库
    db = sqtp_open_database(zScript);
    if (!db) {
        yyjson_doc_free(doc);
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    // SQLite UPSERT: INSERT ... ON CONFLICT DO UPDATE
    // 需要表有 PRIMARY KEY 或 UNIQUE 约束
    snprintf(sql, sizeof(sql), "INSERT INTO %s (%s) VALUES (", 
             headers->table, headers->columns);
    
    size_t idx, max;
    yyjson_val* val;
    yyjson_arr_foreach(root, idx, max, val) {
        if (idx > 0) strncat(sql, ", ", sizeof(sql) - strlen(sql) - 1);
        
        if (yyjson_is_str(val)) {
            strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
            strncat(sql, yyjson_get_str(val), sizeof(sql) - strlen(sql) - 1);
            strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
        } else if (yyjson_is_num(val)) {
            char num_buf[64];
            if (yyjson_is_int(val)) {
                snprintf(num_buf, sizeof(num_buf), "%lld", yyjson_get_sint(val));
            } else {
                snprintf(num_buf, sizeof(num_buf), "%g", yyjson_get_real(val));
            }
            strncat(sql, num_buf, sizeof(sql) - strlen(sql) - 1);
        } else if (yyjson_is_null(val)) {
            strncat(sql, "NULL", sizeof(sql) - strlen(sql) - 1);
        } else if (yyjson_is_bool(val)) {
            strncat(sql, yyjson_get_bool(val) ? "1" : "0", sizeof(sql) - strlen(sql) - 1);
        }
    }
    strncat(sql, ") ON CONFLICT DO UPDATE SET ", sizeof(sql) - strlen(sql) - 1);
    
    // 构造 UPDATE SET 子句
    char columns_copy[1000];
    strncpy(columns_copy, headers->columns, sizeof(columns_copy) - 1);
    columns_copy[sizeof(columns_copy) - 1] = '\0';
    
    char* col_name = strtok(columns_copy, ",");
    int col_idx = 0;
    while (col_name) {
        // 去掉前后空格
        while (*col_name == ' ') col_name++;
        char* end = col_name + strlen(col_name) - 1;
        while (end > col_name && *end == ' ') *end-- = '\0';
        
        if (col_idx > 0) strncat(sql, ", ", sizeof(sql) - strlen(sql) - 1);
        
        strncat(sql, col_name, sizeof(sql) - strlen(sql) - 1);
        strncat(sql, " = excluded.", sizeof(sql) - strlen(sql) - 1);
        strncat(sql, col_name, sizeof(sql) - strlen(sql) - 1);
        
        col_name = strtok(NULL, ",");
        col_idx++;
    }
    
    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    yyjson_doc_free(doc);
    
    int changes = sqlite3_changes(db);
    
    if (rc == SQLITE_OK) {
        sqtp_start_response("200 OK");
        nOut += althttpd_printf("Content-Type: application/json; charset=utf-8\r\n");
        nOut += althttpd_printf("X-SQTP-Changes: %d\r\n", changes);
        nOut += althttpd_printf("\r\n{\"upserted\":%d}\n", changes);
    } else {
        sqtp_send_error(500, errmsg ? errmsg : "Upsert failed");
    }
    
    if (errmsg) sqlite3_free(errmsg);
    sqlite3_close(db);
}

// 处理 RESET 命令 (DELETE + INSERT)
static void sqtp_handle_reset(sqtp_headers_t* headers) {
    sqlite3* db = NULL;
    char sql[10000];
    char* errmsg = NULL;
    int rc;
    
    // 检查必需的 headers
    if (!headers->table || strlen(headers->table) == 0) {
        sqtp_send_error(400, "Missing TABLE header");
        return;
    }
    
    if (!headers->columns || strlen(headers->columns) == 0) {
        sqtp_send_error(400, "Missing COLUMNS header");
        return;
    }
    
    // 读取 body
    char* body = sqtp_read_body(headers->content_length);
    if (!body) {
        sqtp_send_error(400, "Missing or invalid request body");
        return;
    }
    
    // 解析 JSON body
    yyjson_doc* doc = yyjson_read(body, strlen(body), 0);
    free(body);
    
    if (!doc) {
        sqtp_send_error(400, "Invalid JSON in request body");
        return;
    }
    
    yyjson_val* root = yyjson_doc_get_root(doc);
    
    // 打开数据库
    db = sqtp_open_database(zScript);
    if (!db) {
        yyjson_doc_free(doc);
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    // 开始事务
    sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);
    
    // 第一步：DELETE（如果有 WHERE 条件）
    int deleted = 0;
    if (headers->where_count > 0) {
        snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE ", headers->table);
        for (int i = 0; i < headers->where_count; i++) {
            if (i > 0) strncat(sql, " AND ", sizeof(sql) - strlen(sql) - 1);
            strncat(sql, "(", sizeof(sql) - strlen(sql) - 1);
            strncat(sql, headers->where_clauses[i], sizeof(sql) - strlen(sql) - 1);
            strncat(sql, ")", sizeof(sql) - strlen(sql) - 1);
        }
        
        rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
        if (rc != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            yyjson_doc_free(doc);
            sqtp_send_error(500, errmsg ? errmsg : "Delete phase failed");
            if (errmsg) sqlite3_free(errmsg);
            sqlite3_close(db);
            return;
        }
        deleted = sqlite3_changes(db);
    }
    
    // 第二步：INSERT
    int inserted = 0;
    bool is_array_of_arrays = yyjson_is_arr(root) && yyjson_arr_size(root) > 0 &&
                               yyjson_is_arr(yyjson_arr_get_first(root));
    
    if (is_array_of_arrays) {
        // 批量插入
        size_t row_idx, row_max;
        yyjson_val* row_val;
        yyjson_arr_foreach(root, row_idx, row_max, row_val) {
            snprintf(sql, sizeof(sql), "INSERT INTO %s (%s) VALUES (", 
                     headers->table, headers->columns);
            
            size_t col_idx, col_max;
            yyjson_val* val;
            yyjson_arr_foreach(row_val, col_idx, col_max, val) {
                if (col_idx > 0) strncat(sql, ", ", sizeof(sql) - strlen(sql) - 1);
                
                if (yyjson_is_str(val)) {
                    strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
                    strncat(sql, yyjson_get_str(val), sizeof(sql) - strlen(sql) - 1);
                    strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
                } else if (yyjson_is_num(val)) {
                    char num_buf[64];
                    if (yyjson_is_int(val)) {
                        snprintf(num_buf, sizeof(num_buf), "%lld", yyjson_get_sint(val));
                    } else {
                        snprintf(num_buf, sizeof(num_buf), "%g", yyjson_get_real(val));
                    }
                    strncat(sql, num_buf, sizeof(sql) - strlen(sql) - 1);
                } else if (yyjson_is_null(val)) {
                    strncat(sql, "NULL", sizeof(sql) - strlen(sql) - 1);
                } else if (yyjson_is_bool(val)) {
                    strncat(sql, yyjson_get_bool(val) ? "1" : "0", sizeof(sql) - strlen(sql) - 1);
                }
            }
            strncat(sql, ")", sizeof(sql) - strlen(sql) - 1);
            
            rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
            if (rc == SQLITE_OK) inserted++;
        }
    } else if (yyjson_is_arr(root)) {
        // 单行插入
        snprintf(sql, sizeof(sql), "INSERT INTO %s (%s) VALUES (", 
                 headers->table, headers->columns);
        
        size_t idx, max;
        yyjson_val* val;
        yyjson_arr_foreach(root, idx, max, val) {
            if (idx > 0) strncat(sql, ", ", sizeof(sql) - strlen(sql) - 1);
            
            if (yyjson_is_str(val)) {
                strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
                strncat(sql, yyjson_get_str(val), sizeof(sql) - strlen(sql) - 1);
                strncat(sql, "'", sizeof(sql) - strlen(sql) - 1);
            } else if (yyjson_is_num(val)) {
                char num_buf[64];
                if (yyjson_is_int(val)) {
                    snprintf(num_buf, sizeof(num_buf), "%lld", yyjson_get_sint(val));
                } else {
                    snprintf(num_buf, sizeof(num_buf), "%g", yyjson_get_real(val));
                }
                strncat(sql, num_buf, sizeof(sql) - strlen(sql) - 1);
            } else if (yyjson_is_null(val)) {
                strncat(sql, "NULL", sizeof(sql) - strlen(sql) - 1);
            } else if (yyjson_is_bool(val)) {
                strncat(sql, yyjson_get_bool(val) ? "1" : "0", sizeof(sql) - strlen(sql) - 1);
            }
        }
        strncat(sql, ")", sizeof(sql) - strlen(sql) - 1);
        
        rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
        if (rc == SQLITE_OK) inserted = 1;
    }
    
    // 提交事务
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    
    yyjson_doc_free(doc);
    
    // 发送响应
    sqtp_start_response("200 OK");
    nOut += althttpd_printf("Content-Type: application/json; charset=utf-8\r\n");
    nOut += althttpd_printf("X-SQTP-Changes: %d\r\n", inserted);
    nOut += althttpd_printf("\r\n{\"deleted\":%d,\"inserted\":%d}\n", deleted, inserted);
    
    if (errmsg) sqlite3_free(errmsg);
    sqlite3_close(db);
}

// 处理 BEGIN 命令
static void sqtp_handle_begin(sqtp_headers_t* headers) {
    (void)headers;  // 未使用
    sqlite3* db = sqtp_open_database(zScript);
    if (!db) {
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    char* errmsg = NULL;
    int rc = sqlite3_exec(db, "BEGIN", NULL, NULL, &errmsg);
    
    if (rc == SQLITE_OK) {
        sqtp_start_response("200 OK");
        nOut += althttpd_printf("Content-Type: application/json; charset=utf-8\r\n\r\n");
        nOut += althttpd_printf("{\"status\":\"transaction started\"}\n");
    } else {
        sqtp_send_error(500, errmsg ? errmsg : "BEGIN failed");
    }
    
    if (errmsg) sqlite3_free(errmsg);
    sqlite3_close(db);
}

// 处理 COMMIT 命令
static void sqtp_handle_commit(sqtp_headers_t* headers) {
    (void)headers;  // 未使用
    sqlite3* db = sqtp_open_database(zScript);
    if (!db) {
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    char* errmsg = NULL;
    int rc = sqlite3_exec(db, "COMMIT", NULL, NULL, &errmsg);
    
    if (rc == SQLITE_OK) {
        sqtp_start_response("200 OK");
        nOut += althttpd_printf("Content-Type: application/json; charset=utf-8\r\n\r\n");
        nOut += althttpd_printf("{\"status\":\"transaction committed\"}\n");
    } else {
        sqtp_send_error(500, errmsg ? errmsg : "COMMIT failed");
    }
    
    if (errmsg) sqlite3_free(errmsg);
    sqlite3_close(db);
}

// 处理 ROLLBACK 命令
static void sqtp_handle_rollback(sqtp_headers_t* headers) {
    (void)headers;  // 未使用
    sqlite3* db = sqtp_open_database(zScript);
    if (!db) {
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    char* errmsg = NULL;
    int rc = sqlite3_exec(db, "ROLLBACK", NULL, NULL, &errmsg);
    
    if (rc == SQLITE_OK) {
        sqtp_start_response("200 OK");
        nOut += althttpd_printf("Content-Type: application/json; charset=utf-8\r\n\r\n");
        nOut += althttpd_printf("{\"status\":\"transaction rolled back\"}\n");
    } else {
        sqtp_send_error(500, errmsg ? errmsg : "ROLLBACK failed");
    }
    
    if (errmsg) sqlite3_free(errmsg);
    sqlite3_close(db);
}

// 处理 SAVEPOINT 命令
static void sqtp_handle_savepoint(sqtp_headers_t* headers) {
    // SAVEPOINT 需要一个名称（通过某个 header 传递，如 NAME）
    // 这里简化实现，暂时不支持
    (void)headers;
    sqtp_send_error(501, "SAVEPOINT not fully implemented - use NAME header for savepoint name");
}

// 处理 CREATE 操作（TABLE, INDEX, TRIGGER）
static void sqtp_handle_create(sqtp_headers_t* headers) {
    sqlite3* db = NULL;
    char* errmsg = NULL;
    char sql[10000];
    int rc;
    
    // 检查 NAME header（必须）
    if (!headers->name || strlen(headers->name) == 0) {
        sqtp_send_error(400, "NAME header is required for CREATE");
        return;
    }
    
    // 从 URI 路径最后的部分确定对象类型（/db/main/table, /db/main/index, /db/main/trigger）
    // 或者从 TYPE header 获取（如果存在）
    const char* object_type = NULL;
    
    // 优先从路径提取
    if (zScript) {
        const char* last_slash = strrchr(zScript, '/');
        if (last_slash && *(last_slash + 1)) {
            const char* path_type = last_slash + 1;
            // 检查是否包含查询参数或 fragment
            char* query_start = strchr(path_type, '?');
            char* fragment_start = strchr(path_type, '#');
            int type_len = strlen(path_type);
            if (query_start) type_len = query_start - path_type;
            if (fragment_start && (fragment_start - path_type) < type_len) {
                type_len = fragment_start - path_type;
            }
            
            // 检查是否是有效的对象类型
            if ((type_len == 5 && strncasecmp(path_type, "table", 5) == 0) ||
                (type_len == 5 && strncasecmp(path_type, "index", 5) == 0) ||
                (type_len == 7 && strncasecmp(path_type, "trigger", 7) == 0)) {
                object_type = path_type;  // 使用路径中的类型
            }
        }
    }
    
    // 如果路径中没有找到，从 TYPE header 获取
    if (!object_type && headers->type) {
        object_type = headers->type;
    }
    
    if (!object_type || strlen(object_type) == 0) {
        sqtp_send_error(400, "Object type (table, index, or trigger) must be specified in URI path (e.g., /db/main/table) or TYPE header");
        return;
    }
    
    // 打开数据库
    db = sqtp_open_database(zScript);
    if (!db) {
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    // 根据类型路由到相应的处理逻辑
    if (strncasecmp(object_type, "table", 5) == 0) {
        rc = sqtp_create_table(db, headers, sql, sizeof(sql));
    }
    else if (strncasecmp(object_type, "index", 5) == 0) {
        rc = sqtp_create_index(db, headers, sql, sizeof(sql));
    }
    else if (strncasecmp(object_type, "trigger", 7) == 0) {
        rc = sqtp_create_trigger(db, headers, sql, sizeof(sql));
    }
    else {
        sqlite3_close(db);
        sqtp_send_error(400, "Invalid object type - must be table, index, or trigger");
        return;
    }
    
    if (rc != 0) {
        sqlite3_close(db);
        return;  // 错误已经由子函数发送
    }
    
    // 执行 SQL
    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    
    if (rc != SQLITE_OK) {
        char error[1000];
        snprintf(error, sizeof(error), "SQLite error: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        sqlite3_close(db);
        sqtp_send_error(500, error);
        return;
    }
    
    sqlite3_close(db);
    
    // 发送成功响应
    // 如果有 IF-NOT-EXISTS，返回 200 OK；否则返回 201 Created
    const char* status = headers->if_not_exists ? "200 OK" : "201 Created";
    sqtp_start_response(status);
    nOut += althttpd_printf("Content-Type: application/json\r\n");
    nOut += althttpd_printf("\r\n");
    
    // 提取实际的对象类型（去掉路径和查询参数）
    char type_str[20] = {0};
    int i = 0;
    while (i < 19 && object_type[i] && object_type[i] != '?' && object_type[i] != '#') {
        type_str[i] = object_type[i];
        i++;
    }
    type_str[i] = '\0';
    
    nOut += althttpd_printf("{\"created\":1,\"type\":\"%s\",\"name\":\"%s\"}\n", 
                           type_str, headers->name);
}

// 生成 CREATE TABLE SQL
static int sqtp_create_table(sqlite3* db, sqtp_headers_t* headers, char* sql, size_t sql_size) {
    (void)db;
    int pos = 0;
    
    // CREATE [TEMPORARY] TABLE [IF NOT EXISTS] name
    pos += snprintf(sql + pos, sql_size - pos, "CREATE ");
    
    if (headers->type && strcasecmp(headers->type, "temporary") == 0) {
        pos += snprintf(sql + pos, sql_size - pos, "TEMPORARY ");
    }
    
    pos += snprintf(sql + pos, sql_size - pos, "TABLE ");
    
    if (headers->if_not_exists) {
        pos += snprintf(sql + pos, sql_size - pos, "IF NOT EXISTS ");
    }
    
    pos += snprintf(sql + pos, sql_size - pos, "%s (", headers->name);
    
    // 检查是否有 COLUMN headers
    if (headers->column_def_count == 0) {
        sqtp_send_error(400, "At least one COLUMN header is required for CREATE TABLE");
        return -1;
    }
    
    // 检查 PRIMARY KEY 是否为单列（用于 AUTOINCREMENT）
    int single_pk_col = -1;
    if (headers->primary_key) {
        // 检查是否包含逗号（多列主键）
        if (strchr(headers->primary_key, ',') == NULL) {
            // 单列主键 - 查找对应的列索引
            for (int i = 0; i < headers->column_def_count; i++) {
                // 提取列名（第一个空格或制表符之前的部分）
                char col_name[256];
                int j = 0;
                while (j < 255 && headers->column_defs[i][j] && 
                       headers->column_defs[i][j] != ' ' && 
                       headers->column_defs[i][j] != '\t') {
                    col_name[j] = headers->column_defs[i][j];
                    j++;
                }
                col_name[j] = '\0';
                
                if (strcasecmp(col_name, headers->primary_key) == 0) {
                    single_pk_col = i;
                    break;
                }
            }
        }
    }
    
    // 添加列定义
    for (int i = 0; i < headers->column_def_count; i++) {
        if (i > 0) {
            pos += snprintf(sql + pos, sql_size - pos, ", ");
        }
        
        pos += snprintf(sql + pos, sql_size - pos, "%s", headers->column_defs[i]);
        
        // 如果这是单列主键，添加 PRIMARY KEY [AUTOINCREMENT]
        if (i == single_pk_col) {
            pos += snprintf(sql + pos, sql_size - pos, " PRIMARY KEY");
            if (headers->autoinc && strcasecmp(headers->autoinc, "true") == 0) {
                pos += snprintf(sql + pos, sql_size - pos, " AUTOINCREMENT");
            }
        }
    }
    
    // 如果 PRIMARY KEY 是多列，添加表约束
    if (headers->primary_key && single_pk_col == -1) {
        pos += snprintf(sql + pos, sql_size - pos, ", PRIMARY KEY (%s)", headers->primary_key);
    }
    
    // 添加 UNIQUE 约束（多个）
    for (int i = 0; i < headers->unique_constraint_count; i++) {
        pos += snprintf(sql + pos, sql_size - pos, ", UNIQUE (%s)", headers->unique_constraints[i]);
    }
    
    // 添加 FOREIGN KEY 约束（多个）
    for (int i = 0; i < headers->foreign_key_count; i++) {
        pos += snprintf(sql + pos, sql_size - pos, ", FOREIGN KEY %s", headers->foreign_keys[i]);
    }
    
    pos += snprintf(sql + pos, sql_size - pos, ")");
    
    // 添加 WITHOUT ROWID
    if (headers->without_rowid && strcasecmp(headers->without_rowid, "true") == 0) {
        pos += snprintf(sql + pos, sql_size - pos, " WITHOUT ROWID");
    }
    
    return 0;
}

// 生成 CREATE INDEX SQL
static int sqtp_create_index(sqlite3* db, sqtp_headers_t* headers, char* sql, size_t sql_size) {
    (void)db;
    int pos = 0;
    
    // CREATE [UNIQUE] INDEX [IF NOT EXISTS] name ON table (columns)
    pos += snprintf(sql + pos, sql_size - pos, "CREATE ");
    
    if (headers->unique && strcasecmp(headers->unique, "true") == 0) {
        pos += snprintf(sql + pos, sql_size - pos, "UNIQUE ");
    }
    
    pos += snprintf(sql + pos, sql_size - pos, "INDEX ");
    
    if (headers->if_not_exists) {
        pos += snprintf(sql + pos, sql_size - pos, "IF NOT EXISTS ");
    }
    
    pos += snprintf(sql + pos, sql_size - pos, "%s ON ", headers->name);
    
    // TABLE header 指定目标表
    if (!headers->table || strlen(headers->table) == 0) {
        sqtp_send_error(400, "TABLE header is required for CREATE INDEX");
        return -1;
    }
    
    pos += snprintf(sql + pos, sql_size - pos, "%s (", headers->table);
    
    // COLUMN headers 指定索引列
    if (headers->column_def_count == 0) {
        sqtp_send_error(400, "At least one COLUMN header is required for CREATE INDEX");
        return -1;
    }
    
    for (int i = 0; i < headers->column_def_count; i++) {
        if (i > 0) {
            pos += snprintf(sql + pos, sql_size - pos, ", ");
        }
        pos += snprintf(sql + pos, sql_size - pos, "%s", headers->column_defs[i]);
    }
    
    pos += snprintf(sql + pos, sql_size - pos, ")");
    
    // WHERE 子句（用于部分索引）
    if (headers->where_count > 0) {
        pos += snprintf(sql + pos, sql_size - pos, " WHERE ");
        for (int i = 0; i < headers->where_count; i++) {
            if (i > 0) {
                pos += snprintf(sql + pos, sql_size - pos, " AND ");
            }
            pos += snprintf(sql + pos, sql_size - pos, "%s", headers->where_clauses[i]);
        }
    }
    
    return 0;
}

// 生成 CREATE TRIGGER SQL
static int sqtp_create_trigger(sqlite3* db, sqtp_headers_t* headers, char* sql, size_t sql_size) {
    (void)db;
    int pos = 0;
    
    // 检查必需字段
    if (!headers->table || strlen(headers->table) == 0) {
        sqtp_send_error(400, "TABLE header is required for CREATE TRIGGER");
        return -1;
    }
    
    if (!headers->timing || strlen(headers->timing) == 0) {
        sqtp_send_error(400, "TIMING header is required for CREATE TRIGGER (BEFORE/AFTER/INSTEAD OF)");
        return -1;
    }
    
    if (!headers->event || strlen(headers->event) == 0) {
        sqtp_send_error(400, "EVENT header is required for CREATE TRIGGER (INSERT/UPDATE/DELETE)");
        return -1;
    }
    
    if (headers->action_count == 0) {
        sqtp_send_error(400, "At least one ACTION header is required for CREATE TRIGGER");
        return -1;
    }
    
    // CREATE [TEMPORARY] TRIGGER [IF NOT EXISTS] name
    pos += snprintf(sql + pos, sql_size - pos, "CREATE ");
    
    if (headers->type && strcasecmp(headers->type, "temporary") == 0) {
        pos += snprintf(sql + pos, sql_size - pos, "TEMPORARY ");
    }
    
    pos += snprintf(sql + pos, sql_size - pos, "TRIGGER ");
    
    if (headers->if_not_exists) {
        pos += snprintf(sql + pos, sql_size - pos, "IF NOT EXISTS ");
    }
    
    pos += snprintf(sql + pos, sql_size - pos, "%s ", headers->name);
    
    // BEFORE/AFTER/INSTEAD OF
    pos += snprintf(sql + pos, sql_size - pos, "%s ", headers->timing);
    
    // INSERT/UPDATE/DELETE
    pos += snprintf(sql + pos, sql_size - pos, "%s ", headers->event);
    
    // UPDATE OF (column list) - 仅用于 UPDATE 事件
    if (headers->update_of && strlen(headers->update_of) > 0) {
        pos += snprintf(sql + pos, sql_size - pos, "OF %s ", headers->update_of);
    }
    
    // ON table
    pos += snprintf(sql + pos, sql_size - pos, "ON %s ", headers->table);
    
    // FOR EACH ROW (默认为 true)
    if (!headers->for_each_row || strcasecmp(headers->for_each_row, "true") == 0) {
        pos += snprintf(sql + pos, sql_size - pos, "FOR EACH ROW ");
    }
    
    // WHEN condition
    if (headers->when && strlen(headers->when) > 0) {
        pos += snprintf(sql + pos, sql_size - pos, "WHEN %s ", headers->when);
    }
    
    // BEGIN ... END block
    pos += snprintf(sql + pos, sql_size - pos, "BEGIN ");
    
    // 添加所有 ACTION 语句
    for (int i = 0; i < headers->action_count; i++) {
        pos += snprintf(sql + pos, sql_size - pos, "%s; ", headers->actions[i]);
    }
    
    pos += snprintf(sql + pos, sql_size - pos, "END");
    
    return 0;
}

// 处理 DROP 操作（TABLE, INDEX, TRIGGER）
static void sqtp_handle_drop(sqtp_headers_t* headers) {
    sqlite3* db = NULL;
    char* errmsg = NULL;
    char sql[1000];
    int rc;
    
    // 检查 NAME header（必须）
    if (!headers->name || strlen(headers->name) == 0) {
        sqtp_send_error(400, "NAME header is required for DROP");
        return;
    }
    
    // 从 URI 路径最后的部分确定对象类型
    const char* object_type = NULL;
    
    if (zScript) {
        const char* last_slash = strrchr(zScript, '/');
        if (last_slash && *(last_slash + 1)) {
            const char* path_type = last_slash + 1;
            // 检查是否包含查询参数或 fragment
            char* query_start = strchr(path_type, '?');
            char* fragment_start = strchr(path_type, '#');
            int type_len = strlen(path_type);
            if (query_start) type_len = query_start - path_type;
            if (fragment_start && (fragment_start - path_type) < type_len) {
                type_len = fragment_start - path_type;
            }
            
            // 检查是否是有效的对象类型
            if ((type_len == 5 && strncasecmp(path_type, "table", 5) == 0) ||
                (type_len == 5 && strncasecmp(path_type, "index", 5) == 0) ||
                (type_len == 7 && strncasecmp(path_type, "trigger", 7) == 0)) {
                object_type = path_type;
            }
        }
    }
    
    if (!object_type || strlen(object_type) == 0) {
        sqtp_send_error(400, "Object type (table, index, or trigger) must be specified in URI path (e.g., /db/main/table)");
        return;
    }
    
    // 打开数据库
    db = sqtp_open_database(zScript);
    if (!db) {
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    // 构造 DROP SQL
    int pos = 0;
    pos += snprintf(sql + pos, sizeof(sql) - pos, "DROP ");
    
    if (strncasecmp(object_type, "table", 5) == 0) {
        pos += snprintf(sql + pos, sizeof(sql) - pos, "TABLE ");
    }
    else if (strncasecmp(object_type, "index", 5) == 0) {
        pos += snprintf(sql + pos, sizeof(sql) - pos, "INDEX ");
    }
    else if (strncasecmp(object_type, "trigger", 7) == 0) {
        pos += snprintf(sql + pos, sizeof(sql) - pos, "TRIGGER ");
    }
    else {
        sqlite3_close(db);
        sqtp_send_error(400, "Invalid object type - must be table, index, or trigger");
        return;
    }
    
    // IF EXISTS
    if (headers->if_exists) {
        pos += snprintf(sql + pos, sizeof(sql) - pos, "IF EXISTS ");
    }
    
    // 对象名
    pos += snprintf(sql + pos, sizeof(sql) - pos, "%s", headers->name);
    
    // 执行 SQL
    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    
    if (rc != SQLITE_OK) {
        char error[1000];
        snprintf(error, sizeof(error), "SQLite error: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        sqlite3_close(db);
        sqtp_send_error(500, error);
        return;
    }
    
    sqlite3_close(db);
    
    // 发送成功响应
    sqtp_start_response("200 OK");
    nOut += althttpd_printf("Content-Type: application/json\r\n");
    nOut += althttpd_printf("\r\n");
    
    // 提取实际的对象类型（去掉路径和查询参数）
    char type_str[20] = {0};
    int i = 0;
    while (i < 19 && object_type[i] && object_type[i] != '?' && object_type[i] != '#') {
        type_str[i] = object_type[i];
        i++;
    }
    type_str[i] = '\0';
    
    nOut += althttpd_printf("{\"dropped\":1,\"type\":\"%s\",\"name\":\"%s\"}\n", 
                           type_str, headers->name);
}

// 处理 ALTER TABLE 操作
static void sqtp_handle_alter(sqtp_headers_t* headers) {
    sqlite3* db = NULL;
    char* errmsg = NULL;
    char sql[2000];
    int rc;
    
    // 检查 NAME header（必须）
    if (!headers->name || strlen(headers->name) == 0) {
        sqtp_send_error(400, "NAME header is required for ALTER TABLE");
        return;
    }
    
    // 检查 ACTION header（必须）
    if (!headers->action || strlen(headers->action) == 0) {
        sqtp_send_error(400, "ACTION header is required for ALTER TABLE");
        return;
    }
    
    // 打开数据库
    db = sqtp_open_database(zScript);
    if (!db) {
        if (!sqtp_database_exists(zScript)) {
            sqtp_send_error(404, "Database not found");
        } else {
            sqtp_send_error(500, "Failed to open database");
        }
        return;
    }
    
    // 根据 ACTION 类型构造不同的 SQL
    int pos = 0;
    
    if (strcasecmp(headers->action, "RENAME-TABLE") == 0) {
        // ALTER TABLE old_name RENAME TO new_name
        if (!headers->new_name || strlen(headers->new_name) == 0) {
            sqlite3_close(db);
            sqtp_send_error(400, "NEW-NAME header is required for RENAME-TABLE");
            return;
        }
        
        pos += snprintf(sql + pos, sizeof(sql) - pos, "ALTER TABLE %s RENAME TO %s", 
                       headers->name, headers->new_name);
    }
    else if (strcasecmp(headers->action, "ADD-COLUMN") == 0) {
        // ALTER TABLE name ADD COLUMN column_def
        if (headers->column_def_count == 0) {
            sqlite3_close(db);
            sqtp_send_error(400, "COLUMN header is required for ADD-COLUMN");
            return;
        }
        
        // SQLite 只支持添加一列
        pos += snprintf(sql + pos, sizeof(sql) - pos, "ALTER TABLE %s ADD COLUMN %s", 
                       headers->name, headers->column_defs[0]);
    }
    else if (strcasecmp(headers->action, "RENAME-COLUMN") == 0) {
        // ALTER TABLE name RENAME COLUMN old_name TO new_name (SQLite 3.25.0+)
        if (headers->column_def_count == 0) {
            sqlite3_close(db);
            sqtp_send_error(400, "COLUMN header is required for RENAME-COLUMN (old column name)");
            return;
        }
        
        if (!headers->new_name || strlen(headers->new_name) == 0) {
            sqlite3_close(db);
            sqtp_send_error(400, "NEW-NAME header is required for RENAME-COLUMN");
            return;
        }
        
        pos += snprintf(sql + pos, sizeof(sql) - pos, "ALTER TABLE %s RENAME COLUMN %s TO %s", 
                       headers->name, headers->column_defs[0], headers->new_name);
    }
    else if (strcasecmp(headers->action, "DROP-COLUMN") == 0) {
        // ALTER TABLE name DROP COLUMN column_name (SQLite 3.35.0+)
        if (headers->column_def_count == 0) {
            sqlite3_close(db);
            sqtp_send_error(400, "COLUMN header is required for DROP-COLUMN");
            return;
        }
        
        pos += snprintf(sql + pos, sizeof(sql) - pos, "ALTER TABLE %s DROP COLUMN %s", 
                       headers->name, headers->column_defs[0]);
    }
    else {
        sqlite3_close(db);
        sqtp_send_error(400, "Invalid ACTION - must be RENAME-TABLE, ADD-COLUMN, RENAME-COLUMN, or DROP-COLUMN");
        return;
    }
    
    // 执行 SQL
    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    
    if (rc != SQLITE_OK) {
        char error[1000];
        snprintf(error, sizeof(error), "SQLite error: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        sqlite3_close(db);
        sqtp_send_error(500, error);
        return;
    }
    
    sqlite3_close(db);
    
    // 发送成功响应
    sqtp_start_response("200 OK");
    nOut += althttpd_printf("Content-Type: application/json\r\n");
    nOut += althttpd_printf("\r\n");
    nOut += althttpd_printf("{\"altered\":1,\"table\":\"%s\",\"action\":\"%s\"}\n", 
                           headers->name, headers->action);
}

