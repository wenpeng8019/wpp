//
// Created by 温朋 on 2026/2/10.
// TinyCC CGI Handler - Execute C scripts as CGI
//
// 设计说明：
// + 本函数在 CGI 子子进程中运行，stdout/stderr 已被重定向到管道
// + 父进程（请求处理进程）通过管道读取输出，由 CgiHandleReply() 处理
// + 输出格式遵循标准 CGI 规范：可选的 CGI 头 + 空行 + 内容
// + C 脚本的 printf() 输出会通过管道传给父进程，最终发送给客户端

#include "httpd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libtcc.h>
#include "tcc_evn.h"
#include "buildins.h"

// 前向声明
static void cgi_c_error_func(void *opaque, const char *msg);
static int cgi_c_read_file(const char* filename, char** content, size_t* size);

// 使用 main() 中预配置的 TCCState（fork 后继承）
extern TCCState *cgi_tcc_state;

// TinyCC CGI 主处理函数
// 注意：此函数在 CGI 子子进程中运行，stdout 已重定向到管道
void httpd_cgi_c(char* method, char* script, char* protocol, size_t* out, buildin_file_info_st* buildin_info) {
    (void)method;    // 未使用的参数
    (void)protocol;  // 未使用的参数
    (void)out;       // 在子进程中无意义，输出通过管道
    
    char* source_code = NULL;
    size_t code_size = 0;
    int need_free_source = 0;
    
    // 根据是否为 buildin 文件选择不同的源代码获取方式
    if (buildin_info) {
        // 从 buildins 解压获取源代码
        source_code = (char*)buildins_decompressed(buildin_info);
        if (!source_code) {
            printf("Status: 500 Internal Server Error\r\n");
            printf("Content-Type: text/plain\r\n\r\n");
            printf("Error: Failed to decompress buildin C script: %s\n", script);
            exit(1);
        }
        
        // 特殊处理空文件（buildins_decompressed 返回 (void*)1）
        if (source_code == (void*)1) {
            printf("Status: 500 Internal Server Error\r\n");
            printf("Content-Type: text/plain\r\n\r\n");
            printf("Error: C script file is empty: %s\n", script);
            exit(1);
        }
        
        code_size = buildin_info->orig_sz;
        // 注意：buildins 解压的内存由 buildins 模块管理，不需要 free
        need_free_source = 0;
    } else {
        // 从文件系统读取 C 脚本文件
        if (cgi_c_read_file(script, &source_code, &code_size) != 0) {
            printf("Status: 404 Not Found\r\n");
            printf("Content-Type: text/plain\r\n\r\n");
            printf("Error: C script file not found: %s\n", script);
            exit(1);
        }
        need_free_source = 1;
    }
    
    // 使用预配置的 TCCState（由 fork 继承，无需重新创建）
    // 设计说明：
    // + 主进程预先配置 cgi_tcc_state（路径、符号、预编译头文件等）
    // + fork 后子进程继承这个 State 的完整副本（写时复制）
    // + 每个子进程使用自己的副本，互不干扰，无状态污染风险
    // + 避免每次请求都重新初始化，提升性能
    TCCState *s = cgi_tcc_state;
    if (!s) {
        if (need_free_source) free(source_code);
        printf("Status: 500 Internal Server Error\r\n");
        printf("Content-Type: text/plain\r\n\r\n");
        printf("Error: TCC state not initialized\n");
        exit(1);
    }
    
    // 设置错误回调（错误信息输出到 stderr，父进程可捕获）
    tcc_set_error_func(s, NULL, cgi_c_error_func);
    
    // 编译 C 代码
    if (tcc_compile_string(s, source_code) < 0) {
        if (need_free_source) free(source_code);
        // 注意：在子进程中 tcc_delete 不影响父进程（fork 后内存独立）
        tcc_delete(s);
        printf("Status: 500 Internal Server Error\r\n");
        printf("Content-Type: text/plain\r\n\r\n");
        printf("Error: Failed to compile C script (see stderr for details)\n");
        exit(1);
    }
    
    if (need_free_source) free(source_code);
    
    // 刷新输出缓冲区
    fflush(stdout);
    fflush(stderr);
    
    // 使用 tcc_run 直接运行（它会自动 relocate）
    // 注意：
    // + C 脚本的 main() 函数输出的内容（printf 等）会到 stdout
    // + stdout 已被重定向到管道，父进程会读取并处理
    // + C 脚本应遵循 CGI 规范，输出 HTTP 头（如 Content-Type）+ 空行 + 内容
    // + fork 的子进程有独立的 TCCState 副本，tcc_run() 的状态修改不影响其他子进程
    char *argv[] = { script, NULL };
    int exit_code = tcc_run(s, 1, argv);
    
    // 子进程退出（系统自动回收资源，无需手动清理 TCCState）
    exit(exit_code);
}

// TCC 错误回调函数
static void cgi_c_error_func(void *opaque, const char *msg) {
    (void)opaque;
    fprintf(stderr, "TCC Error: %s\n", msg);
}

// 读取文件内容
static int cgi_c_read_file(const char* filename, char** content, size_t* size) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return -1;
    }
    
    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(fp);
        return -1;
    }
    
    // 分配内存
    *content = malloc(file_size + 1);
    if (!*content) {
        fclose(fp);
        return -1;
    }
    
    // 读取文件
    size_t read_size = fread(*content, 1, file_size, fp);
    (*content)[read_size] = '\0';
    *size = read_size;
    
    fclose(fp);
    return 0;
}
