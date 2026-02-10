//
// Created by 温朋 on 2026/2/8.
//

#ifndef ALTHTTPD_H
#define ALTHTTPD_H

// ENABLE_TLS 由 CMake 定义，不在此处定义
// #define ENABLE_TLS

#include <stdint.h>
#include <stdbool.h>
#include <sys/unistd.h>
#include <stdio.h>

typedef struct http_tls {
    const char*                 certFile;                   // TLS 证书文件路径
    const char*                 keyFile;                    // TLS 私钥文件路径
    int                         tlsPort;                    // TLS 监听端口
} http_tls_st;

typedef struct http_params {

    const char*                 csRoot;                     // 网站的根目录
    const char*                 csLogFile;                  // 日志文件路径
    const char*                 csIPShunDir;                // 存放被封禁IP地址的目录
    const char*                 csDefaultHost;              // 默认主机名
    uint32_t                    iMxAge;                     // 缓存控制的最大时长（秒），默认 120s
    uint32_t                    iMaxCpu;                    // 每个进程允许的最大CPU时间（秒），默认 30s
    uint32_t                    iMxChild;                   // 最大子进程数，默认 1000
    bool                        bEnableSAB;                 // 允许添加回复头以启用 SharedArrayBuffer，默认 false
    bool                        bUseTimeout;                // 是否开启超时限制机制，默认 true

} http_params_st;

// SQTP 需要的工具函数
char* GetFirstElement(char *zInput, char **zLeftOver);
char* StrDup(const char *zSrc);
void RemoveNewline(char *z);
char* althttpd_fgets(char *zBuf, int nBuf, FILE *in);
int althttpd_printf(const char *zFormat, ...);
void MakeLogEntry(int completed, int lineno);

// SQTP 处理函数（参数传递）
void httpd_sqtp(char* method, char* script, char* protocol, size_t* out);

// TinyCC CGI 处理函数
void httpd_cgi_c(char* method, char* script, char* protocol, size_t* out);

/**
 * @brief HTTP 服务器主函数
 * 
 * 注意：httpd_main 会 fork 多个子进程（浏览器启动进程、HTTP 请求处理进程）。
 * 因此信号处理器与 atexit 注册会被继承，需要调用方自行注意副作用。
 */
int httpd_main(uint16_t mnPort, uint16_t mxPort,            // 监听的 TCP 端口范围。设置该选项意味着该程序将开启监听服务模式
               bool loopback,                               // 是否只（bind）监听 loop-back 端口，即 localhost ip 的端口（默认监听所有 ip 地址）
               bool jail,                                   // 是否使用 change-root jail（沙盒）机制，默认 true
               const char *csStartPage/* nullable */,
               const char* user/* nullable */,              // 指定的用来运行 http 处理的用户身份
               http_params_st* pParams/* nullable */,
               http_tls_st* pTls/* nullable */,
               const char* pid_file/* nullable */);

#endif //ALTHTTPD_H
