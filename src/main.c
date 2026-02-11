
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <libtcc.h>
#include <wpp/wpp.h>
#include <common.h>
#include "httpd.h"
#include "buildins.h"
#include "tcc_evn.h"

// 预配置的 TCCState（fork 后子进程继承）
TCCState *cgi_tcc_state = NULL;

#define PID_FILE ".pid"
#define URL_PATH "/hello.html"

ARGS_B(false, stop, 0, "stop", "Stop current running wpp");

// 保存主进程 PID，用于退出时删除 PID 文件
static pid_t g_main_pid = 0;
static pid_t check_running(int *out_port);
static int stop_pid(void);
static void clean_pid(void);

/**
 * @brief 初始化共享内存数据库（用于 SQTP 测试）
 */
void init_shared_memory_db(void) {
    sqlite3 *db = NULL;
    char *err_msg = NULL;
    int rc;
    
    printf("\n=== 初始化 SQTP 测试数据库 ===\n\n");
    
#if 0
    // 基于内存的数据库（注意它不能进程间共享
    //rc = sqlite3_open(":memory:", &db);
#else
    // 打开共享内存数据库（同一进程内共享；跨进程需使用磁盘文件数据库）
    // URI: file:shm?mode=memory&cache=shared
    // 使用 SQLITE_OPEN_URI 确保按 URI 解析，避免被当作文件名落盘
    rc = sqlite3_open_v2("file:shm?mode=memory&cache=shared", &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI,
        NULL
    );
#endif
    if (rc != SQLITE_OK) {
        fprintf(stderr, "❌ 无法打开共享内存数据库: %s\n", sqlite3_errmsg(db));
        return;
    }
    
    printf("✓ 共享内存数据库已创建 (URI: /shm)\n");
    
    // 创建测试表（如果已存在则跳过）
    const char *sql_create = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "email TEXT, "
        "age INTEGER, "
        "status TEXT DEFAULT 'active'"
        ");";
    
    rc = sqlite3_exec(db, sql_create, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "❌ 创建表失败: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return;
    }
    
    printf("✓ 表 'users' 已准备就绪\n");
    
    // 清空表并插入测试数据
    sqlite3_exec(db, "DELETE FROM users", NULL, NULL, NULL);
    
    // 插入测试数据
    const char *sql_insert = 
        "INSERT INTO users (name, email, age, status) VALUES "
        "('Alice', 'alice@example.com', 25, 'active'), "
        "('Bob', 'bob@example.com', 30, 'active'), "
        "('Charlie', 'charlie@example.com', 35, 'inactive'), "
        "('David', 'david@example.com', 28, 'active'), "
        "('Eve', 'eve@example.com', 32, 'active');";
    
    rc = sqlite3_exec(db, sql_insert, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "❌ 插入数据失败: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return;
    }
    
    printf("✓ 插入了 5 条测试记录\n");
    printf("\n数据库准备就绪，可通过 SQTP 协议访问:\n");
    printf("  SELECT /shm SQTP/1.0\n");
    printf("  TABLE: users\n");
    printf("\n");
    
    // 不关闭数据库，保持共享内存数据库存活
    // sqlite3_close(db);  // 注意：不关闭，因为是共享内存
}

int main(int argc, char *argv[]) {

    // 解析命令行参数（如果有的话）
    if (argc > 1) {

        ARGS_parse(argc, argv,
            &ARGS_DEF_stop,
            NULL);
        
        // 如果指定了 --stop 参数，停止当前运行的服务
        if (ARGS_stop.i64) {
            return stop_pid() == 0 ? 0 : 1;
        }
    }
    
    printf("WPP Server with SQTP Support\n");
    printf("=====================================\n");
    
    // 检查是否已有实例在运行
    int running_port = 0;
    pid_t running_pid = check_running(&running_port);
    if (running_pid < 0) return 1;  // 出错退出
    if (running_pid) {

        printf("✓ 服务器已在运行 (PID: %d, Port: %d)\n", running_pid, running_port);
        printf("  正在打开浏览器...\n");
        
        // 执行第一个成功启动的浏览器
        // + execlp 成功后，当前进程会被完全替换为浏览器程序，原代码不再执行，即不会再遍历后面项目
        //   只有失败时才会继续尝试下一个浏览器
        static const char *const azBrowserProg[] = {
#if defined(__DARWIN__) || defined(__APPLE__) || defined(__HAIKU__)
            "open"
#else
            "xdg-open", "gnome-open", "firefox", "google-chrome"
#endif
        };
        
        char url[256];
        snprintf(url, sizeof(url), "http://localhost:%d%s", running_port, URL_PATH);
        for (size_t i = 0; i < sizeof(azBrowserProg) / sizeof(azBrowserProg[0]); i++) {
            execlp(azBrowserProg[i], azBrowserProg[i], url, (char *) 0);
        }

        // 如果所有浏览器都启动失败
        return 1;
    }
    
    // 保存主进程 PID 并注册退出清理函数
    g_main_pid = getpid();
    atexit(clean_pid);
    
    // 初始化 buildins（由 main 触发）
    if (buildins_init() < 0) {
        fprintf(stderr, "❌ buildins 初始化失败\n");
        return 1;
    }

    // 初始化 TCC 环境（预加载 buildins，便于 fork 子进程复用）
    if (tcc_evn_init() < 0) {
        fprintf(stderr, "❌ TCC EVN 初始化失败\n");
        return 1;
    }

    // 预创建并配置 TCCState（fork 后子进程继承，避免重复初始化）
    cgi_tcc_state = tcc_new();
    if (!cgi_tcc_state) {
        fprintf(stderr, "❌ 无法创建 TCC State\n");
        return 1;
    }
    tcc_set_output_type(cgi_tcc_state, TCC_OUTPUT_MEMORY);
    if (tcc_configure(cgi_tcc_state) < 0) {
        fprintf(stderr, "❌ TCC 环境绑定失败\n");
        tcc_delete(cgi_tcc_state);
        return 1;
    }
    printf("✓ TCC CGI 环境已预配置（fork 后子进程继承）\n");

    // 初始化共享内存数据库（用于 SQTP 测试）
    init_shared_memory_db();
    
    printf("启动 HTTP/SQTP 服务器...\n");

    // 使用动态端口分配（传入 0, 0），httpd 会在获得端口后写入 PID 文件
    httpd_main(0, 0, true, true, URL_PATH, NULL, NULL, NULL, PID_FILE);

    // httpd_main 主进程进入监听循环不会返回，只有子进程会到达这里
    return 0;
}

/**
 * @brief 退出时清理 PID 文件
 * 注意：httpd fork 出的所有子进程都会继承此 atexit 注册。
 * 子进程退出时也会调用此函数，但只有主进程才会删除 PID 文件。
 */
static void clean_pid(void) {
    if (getpid() == g_main_pid) {
        unlink(PID_FILE);
    }
}

/**
 * @brief 检查是否已有实例在运行
 * @param out_port 输出参数，返回运行中实例的端口号
 * @return 返回运行中实例的 PID (>0)，0 表示可以启动，-1 表示检查出错
 */
static pid_t check_running(int *out_port) {
    FILE* fp = fopen(PID_FILE, "r");
    if (!fp) {
        // PID 文件不存在，可以启动
        return 0;
    }
    
    // 读取 PID 和端口号（格式："PID:PORT"）
    pid_t pid = 0; int port = 0;
    int matched = fscanf(fp, "%d:%d", &pid, &port);
    fclose(fp);
    
    if (matched < 1 || pid <= 0) {
        // PID 文件损坏，删除并继续
        unlink(PID_FILE);
        return 0;
    }
    
    // 检查进程是否存在
    if (kill(pid, 0) == 0) {
        // 进程存在，返回其 PID 和端口号
        if (out_port && matched == 2) {
            *out_port = port;
        }
        return pid;
    }
    
    // 进程不存在，删除陈旧的 PID 文件
    if (errno == ESRCH) {
        printf("⚠️  删除陈旧的 PID 文件 (进程 %d 已不存在)\n", pid);
        unlink(PID_FILE);
        return 0;
    }
    
    // 其他错误，为安全起见拒绝启动
    fprintf(stderr, "❌ 无法检查进程 %d 状态: %s\n", pid, strerror(errno));
    return -1;
}

/**
 * @brief 读取 PID 文件并停止进程
 */
static int stop_pid(void) {
    FILE* fp = fopen(PID_FILE, "r");
    if (!fp) {
        fprintf(stderr, "❌ 找不到 PID 文件 %s，服务可能未运行\n", PID_FILE);
        return -1;
    }
    
    int pid = 0;
    int port = 0;
    int matched = fscanf(fp, "%d:%d", &pid, &port);
    fclose(fp);
    
    if (matched < 1 || pid <= 0) {
        fprintf(stderr, "❌ PID 文件格式错误\n");
        return -1;
    }
    
    // 检查进程是否存在
    if (kill(pid, 0) != 0) {
        fprintf(stderr, "⚠️  进程 %d 不存在，清理 PID 文件\n", pid);
        unlink(PID_FILE);
        return -1;
    }
    
    // 发送 SIGTERM 信号
    printf("正在停止服务进程 (PID: %d)...\n", pid);
    if (kill(pid, SIGTERM) != 0) {
        fprintf(stderr, "❌ 无法停止进程 %d: %s\n", pid, strerror(errno));
        return -1;
    }
    
    // 等待进程退出
    int retry = 0;
    while (retry < 50) {  // 最多等待 5 秒
        if (kill(pid, 0) != 0) {
            printf("✓ 服务已停止\n");
            unlink(PID_FILE);
            return 0;
        }
        usleep(100000);  // 100ms
        retry++;
    }
    
    // 如果还没退出，发送 SIGKILL
    fprintf(stderr, "⚠️  进程未响应，强制终止...\n");
    kill(pid, SIGKILL);
    unlink(PID_FILE);
    return 0;
}
