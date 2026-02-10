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

// 前向声明
static void cgi_c_error_func(void *opaque, const char *msg);
static int cgi_c_read_file(const char* filename, char** content, size_t* size);

// TinyCC CGI 主处理函数
// 注意：此函数在 CGI 子子进程中运行，stdout 已重定向到管道
void httpd_cgi_c(char* method, char* script, char* protocol, size_t* out) {
    (void)method;    // 未使用的参数
    (void)protocol;  // 未使用的参数
    (void)out;       // 在子进程中无意义，输出通过管道
    
    // 读取 C 脚本文件
    char* source_code = NULL;
    size_t code_size = 0;
    
    if (cgi_c_read_file(script, &source_code, &code_size) != 0) {
        // CGI 错误输出：使用标准 CGI 头格式
        printf("Status: 404 Not Found\r\n");
        printf("Content-Type: text/plain\r\n\r\n");
        printf("Error: C script file not found: %s\n", script);
        exit(1);  // 非零退出码表示错误
    }
    
    // 创建 TCC 状态
    TCCState *s = tcc_new();
    if (!s) {
        free(source_code);
        printf("Status: 500 Internal Server Error\r\n");
        printf("Content-Type: text/plain\r\n\r\n");
        printf("Error: Failed to create TCC state\n");
        exit(1);
    }
    
    // 设置错误回调（错误信息输出到 stderr，父进程可捕获）
    tcc_set_error_func(s, NULL, cgi_c_error_func);
    
    // 设置 TCC 库路径（用于查找 libtcc1.a 等运行时文件）
    tcc_set_lib_path(s, "third_party/tinycc/build/runtime");
    
    // 设置输出类型为内存执行
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    
    // 添加 include 路径
    tcc_add_include_path(s, "third_party/tinycc/include");
    tcc_add_sysinclude_path(s, "/usr/include");
    
    // 添加系统库路径
    tcc_add_library_path(s, "/usr/lib");
    tcc_add_library_path(s, "/usr/local/lib");
    
    // 链接标准 C 库
    tcc_add_library(s, "c");
    
    // 编译 C 代码
    if (tcc_compile_string(s, source_code) < 0) {
        free(source_code);
        tcc_delete(s);
        printf("Status: 500 Internal Server Error\r\n");
        printf("Content-Type: text/plain\r\n\r\n");
        printf("Error: Failed to compile C script (see stderr for details)\n");
        exit(1);
    }
    
    free(source_code);
    
    // 刷新输出缓冲区
    fflush(stdout);
    fflush(stderr);
    
    // 使用 tcc_run 直接运行（它会自动 relocate）
    // 注意：
    // + C 脚本的 main() 函数输出的内容（printf 等）会到 stdout
    // + stdout 已被重定向到管道，父进程会读取并处理
    // + C 脚本应遵循 CGI 规范，输出 HTTP 头（如 Content-Type）+ 空行 + 内容
    char *argv[] = { script, NULL };
    int exit_code = tcc_run(s, 1, argv);
    
    // 清理
    tcc_delete(s);
    
    // 退出子进程，返回 C 脚本的退出码
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
