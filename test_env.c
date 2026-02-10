#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
    // CGI 响应头（标准 CGI 格式，不包含 HTTP 状态行）
    printf("Content-Type: text/html; charset=utf-8\r\n\r\n");
    
    // HTML 输出
    printf("<html><head><meta charset='utf-8'><title>CGI 环境变量测试</title>");
    printf("<style>body{font-family:monospace;margin:20px;} table{border-collapse:collapse;width:100%%;} ");
    printf("th,td{border:1px solid #ddd;padding:8px;text-align:left;} th{background:#f2f2f2;}</style>");
    printf("</head><body>");
    printf("<h1>🔍 完整 CGI 环境变量测试</h1>");
    
    printf("<h2>标准 CGI 环境变量：</h2>");
    printf("<table><tr><th>变量名</th><th>值</th></tr>");
    
    // 定义所有标准 CGI 环境变量
    const char *cgi_vars[] = {
        "GATEWAY_INTERFACE",
        "REQUEST_METHOD",
        "QUERY_STRING",
        "CONTENT_LENGTH",
        "CONTENT_TYPE",
        "REQUEST_URI",
        "SCRIPT_NAME",
        "SCRIPT_FILENAME",
        "SCRIPT_DIRECTORY",
        "PATH_INFO",
        "SERVER_NAME",
        "SERVER_PORT",
        "SERVER_PROTOCOL",
        "SERVER_SOFTWARE",
        "REMOTE_ADDR",
        "REMOTE_USER",
        "AUTH_TYPE",
        "AUTH_CONTENT",
        "DOCUMENT_ROOT",
        "HTTP_HOST",
        "HTTP_USER_AGENT",
        "HTTP_ACCEPT",
        "HTTP_ACCEPT_ENCODING",
        "HTTP_REFERER",
        "HTTP_COOKIE",
        "HTTP_IF_MODIFIED_SINCE",
        "HTTP_IF_NONE_MATCH",
        "HTTP_SCHEME",
        "HTTPS",
        "PATH",
        "SCGI",
        NULL
    };
    
    int found_count = 0;
    int missing_count = 0;
    
    // 遍历并显示每个环境变量
    for (int i = 0; cgi_vars[i] != NULL; i++) {
        char *value = getenv(cgi_vars[i]);
        if (value && *value) {
            printf("<tr><td><b>%s</b></td><td>%s</td></tr>", cgi_vars[i], value);
            found_count++;
        } else {
            printf("<tr style='background:#fff3cd;'><td><b>%s</b></td><td><i>(未设置)</i></td></tr>", cgi_vars[i]);
            missing_count++;
        }
    }
    
    printf("</table>");
    
    // 统计信息
    printf("<h2>📊 统计：</h2>");
    printf("<p>✅ 已设置: <b>%d</b> 个</p>", found_count);
    printf("<p>❌ 未设置: <b>%d</b> 个</p>", missing_count);
    
    // 显示当前工作目录
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("<h2>📁 当前工作目录：</h2>");
        printf("<p><code>%s</code></p>", cwd);
    }
    
    printf("<hr><p style='color:#888;'>测试时间: ");
    system("date");
    printf("</p>");
    
    printf("</body></html>");
    
    return 0;
}
