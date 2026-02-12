#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

int main() {
    // CGI 响应头（标准 CGI 格式）
    printf("Content-Type: text/html; charset=utf-8\r\n\r\n");
    
    // 获取当前时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buffer[26];
    strftime(time_buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    
    // HTML 输出
    printf("<!DOCTYPE html>\n");
    printf("<html lang='zh-CN'>\n");
    printf("<head>\n");
    printf("    <meta charset='utf-8'>\n");
    printf("    <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n");
    printf("    <title>Hello World - wpp C CGI</title>\n");
    printf("    <style>\n");
    printf("        * { margin: 0; padding: 0; box-sizing: border-box; }\n");
    printf("        body {\n");
    printf("            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;\n");
    printf("            background: #ffffff;\n");
    printf("            color: #1d1d1f;\n");
    printf("            min-height: 100vh;\n");
    printf("            padding: 80px 60px;\n");
    printf("            line-height: 1.6;\n");
    printf("        }\n");
    printf("        .container {\n");
    printf("            max-width: 1000px;\n");
    printf("            margin: 0 auto;\n");
    printf("        }\n");
    printf("        .header {\n");
    printf("            margin-bottom: 48px;\n");
    printf("            text-align: left;\n");
    printf("        }\n");
    printf("        .header h1 {\n");
    printf("            font-family: 'SF Mono', 'Monaco', 'Inconsolata', 'Fira Code', monospace;\n");
    printf("            font-size: 4rem;\n");
    printf("            font-weight: 600;\n");
    printf("            color: #1d1d1f;\n");
    printf("            margin-bottom: 20px;\n");
    printf("            background: linear-gradient(135deg, #f5f7fa 0%%, #c3cfe2 100%%);\n");
    printf("            padding: 20px 30px;\n");
    printf("            border-radius: 16px;\n");
    printf("            border: 1px solid #e1e5e9;\n");
    printf("            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.1);\n");
    printf("            display: inline-block;\n");
    printf("            position: relative;\n");
    printf("        }\n");
    printf("        .header h1::before {\n");
    printf("            content: '';\n");
    printf("            position: absolute;\n");
    printf("            top: 15px;\n");
    printf("            left: 15px;\n");
    printf("            width: 10px;\n");
    printf("            height: 10px;\n");
    printf("            background: #ff5f57;\n");
    printf("            border-radius: 50%%;\n");
    printf("            box-shadow: 16px 0 #ffbd2e, 32px 0 #28ca42;\n");
    printf("        }\n");
    printf("        .header .subtitle {\n");
    printf("            font-size: 1.5rem;\n");
    printf("            color: #6e6e73;\n");
    printf("            font-weight: 400;\n");
    printf("            margin-bottom: 20px;\n");
    printf("        }\n");
    printf("        .badge {\n");
    printf("            display: inline-block;\n");
    printf("            background: #f5f5f7;\n");
    printf("            color: #515154;\n");
    printf("            padding: 8px 16px;\n");
    printf("            border-radius: 20px;\n");
    printf("            font-size: 0.9em;\n");
    printf("            margin: 0 8px 8px 0;\n");
    printf("            font-weight: 500;\n");
    printf("        }\n");
    printf("        .content {\n");
    printf("            padding: 0;\n");
    printf("        }\n");
    printf("        .section {\n");
    printf("            margin-bottom: 40px;\n");
    printf("        }\n");
    printf("        .section h2 {\n");
    printf("            color: #333;\n");
    printf("            margin-bottom: 20px;\n");
    printf("            font-size: 1.5em;\n");
    printf("            border-bottom: 2px solid #f0f0f0;\n");
    printf("            padding-bottom: 10px;\n");
    printf("        }\n");
    printf("        .stats {\n");
    printf("            display: grid;\n");
    printf("            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));\n");
    printf("            gap: 20px;\n");
    printf("            margin-bottom: 30px;\n");
    printf("        }\n");
    printf("        .stat-card {\n");
    printf("            background: #f8f9fa;\n");
    printf("            padding: 20px;\n");
    printf("            border-radius: 12px;\n");
    printf("            text-align: center;\n");
    printf("        }\n");
    printf("        .stat-value {\n");
    printf("            font-size: 2em;\n");
    printf("            font-weight: bold;\n");
    printf("            color: #0071e3;\n");
    printf("        }\n");
    printf("        table {\n");
    printf("            width: 100%%;\n");
    printf("            border-collapse: collapse;\n");
    printf("            border-radius: 8px;\n");
    printf("            overflow: hidden;\n");
    printf("            box-shadow: 0 4px 6px rgba(0,0,0,0.1);\n");
    printf("        }\n");
    printf("        th, td {\n");
    printf("            padding: 12px 16px;\n");
    printf("            text-align: left;\n");
    printf("            border-bottom: 1px solid #e9ecef;\n");
    printf("        }\n");
    printf("        th {\n");
    printf("            background: #0071e3;\n");
    printf("            color: white;\n");
    printf("            font-weight: 600;\n");
    printf("        }\n");
    printf("        td {\n");
    printf("            background: white;\n");
    printf("        }\n");
    printf("        .missing {\n");
    printf("            background: #fff3cd !important;\n");
    printf("            color: #856404;\n");
    printf("        }\n");
    printf("        .env-value {\n");
    printf("            font-family: 'SF Mono', Monaco, monospace;\n");
    printf("            background: #f1f3f4;\n");
    printf("            padding: 4px 8px;\n");
    printf("            border-radius: 4px;\n");
    printf("            font-size: 0.9em;\n");
    printf("        }\n");
    printf("        .footer {\n");
    printf("            color: #86868b;\n");
    printf("            font-size: 0.875rem;\n");
    printf("            line-height: 1.5;\n");
    printf("            border-top: 1px solid #e8e8ed;\n");
    printf("            padding-top: 24px;\n");
    printf("            margin-top: 48px;\n");
    printf("            text-align: center;\n");
    printf("        }\n");
    printf("        .cwd {\n");
    printf("            background: #e9ecef;\n");
    printf("            padding: 12px 16px;\n");
    printf("            border-radius: 8px;\n");
    printf("            font-family: 'SF Mono', Monaco, monospace;\n");
    printf("            word-break: break-all;\n");
    printf("        }\n");
    printf("        @media (max-width: 768px) {\n");
    printf("            .header { padding: 20px; }\n");
    printf("            .header h1 { font-size: 2em; }\n");
    printf("            .content { padding: 20px; }\n");
    printf("            .stats { grid-template-columns: 1fr; }\n");
    printf("        }\n");
    printf("    </style>\n");
    printf("</head>\n");
    printf("<body>\n");
    printf("    <div class='container'>\n");
    printf("        <div class='header'>\n");
    printf("            <h1>👋 Hello World!</h1>\n");
    printf("            <div class='subtitle'>欢迎使用 wpp - 高性能 C 语言 Web 服务器</div>\n");
    printf("            <div>\n");
    printf("                <span class='badge'>TinyCC 即时编译</span>\n");
    printf("                <span class='badge'>零配置运行</span>\n");
    printf("                <span class='badge'>C 语言 CGI</span>\n");
    printf("            </div>\n");
    printf("        </div>\n");
    printf("        \n");
    printf("        <div class='content'>\n");
    
    // 获取系统统计信息
    const char *cgi_vars[] = {
        "GATEWAY_INTERFACE", "REQUEST_METHOD", "QUERY_STRING", "CONTENT_LENGTH",
        "CONTENT_TYPE", "REQUEST_URI", "SCRIPT_NAME", "SCRIPT_FILENAME", 
        "PATH_INFO", "SERVER_NAME", "SERVER_PORT", "SERVER_PROTOCOL",
        "SERVER_SOFTWARE", "REMOTE_ADDR", "REMOTE_USER", "AUTH_TYPE",
        "DOCUMENT_ROOT", "HTTP_HOST", "HTTP_USER_AGENT", "HTTP_ACCEPT",
        "HTTP_ACCEPT_ENCODING", "HTTP_REFERER", "HTTP_COOKIE", "HTTPS",
        "PATH", "HTTP_ACCEPT_LANGUAGE", "HTTP_CONNECTION", "HTTP_CACHE_CONTROL",
        "REMOTE_HOST", "HTTP_AUTHORIZATION", "REMOTE_IDENT"
    };
    
    int total_vars = sizeof(cgi_vars) / sizeof(cgi_vars[0]);
    int found_vars = 0;
    
    // 统计可用的环境变量
    for (int i = 0; i < total_vars; i++) {
        char *value = getenv(cgi_vars[i]);
        if (value && *value) {
            found_vars++;
        }
    }
    
    printf("            <div class='section'>\n");
    printf("                <h2>🚀 系统状态</h2>\n");
    printf("                <div class='stats'>\n");
    printf("                    <div class='stat-card'>\n");
    printf("                        <div class='stat-value'>%d</div>\n", found_vars);
    printf("                        <div>可用环境变量</div>\n");
    printf("                    </div>\n");
    printf("                    <div class='stat-card'>\n");
    printf("                        <div class='stat-value'>%d</div>\n", total_vars - found_vars);
    printf("                        <div>缺失变量</div>\n");
    printf("                    </div>\n");
    
    // 显示当前工作目录
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("                    <div class='stat-card'>\n");
        printf("                        <div class='stat-value'>📁</div>\n");
        printf("                        <div>程序正常运行</div>\n");
        printf("                    </div>\n");
    }
    
    printf("                    <div class='stat-card'>\n");
    printf("                        <div class='stat-value'>⏰</div>\n");
    printf("                        <div>%s</div>\n", time_buffer);
    printf("                    </div>\n");
    printf("                </div>\n");
    printf("            </div>\n");
    
    // 显示当前工作目录详情
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("            <div class='section'>\n");
        printf("                <h2>📂 运行环境</h2>\n");
        printf("                <div class='cwd'>%s</div>\n", cwd);
        printf("            </div>\n");
    }
    
    printf("            <div class='section'>\n");
    printf("                <h2>🔍 CGI 环境变量</h2>\n");
    printf("                <table>\n");
    printf("                    <tr><th>变量名</th><th>值</th></tr>\n");
    // 显示环境变量详情
    for (int i = 0; i < total_vars; i++) {
        char *value = getenv(cgi_vars[i]);
        if (value && *value) {
            printf("                    <tr>\n");
            printf("                        <td><strong>%s</strong></td>\n", cgi_vars[i]);
            printf("                        <td><span class='env-value'>%s</span></td>\n", value);
            printf("                    </tr>\n");
        } else {
            printf("                    <tr class='missing'>\n");
            printf("                        <td><strong>%s</strong></td>\n", cgi_vars[i]);
            printf("                        <td><em>(未设置)</em></td>\n");
            printf("                    </tr>\n");
        }
    }
    
    printf("                </table>\n");
    printf("            </div>\n");
    
    printf("        </div>\n");
    
    printf("        <div class='footer'>\n");
    printf("            <div>🛠️ 由 <strong>wpp</strong> 驱动 · TinyCC 编译</div>\n");
    printf("            <div>⚡ 零配置，高性能，C 语言 Web 开发</div>\n");
    printf("        </div>\n");
    printf("    </div>\n");
    printf("</body>\n");
    printf("</html>\n");
    
    return 0;
}
