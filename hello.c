#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("Content-Type: text/html; charset=utf-8\r\n\r\n");
    
    char *method = getenv("REQUEST_METHOD");
    char *query = getenv("QUERY_STRING");
    char *remote = getenv("REMOTE_ADDR");
    
    printf("<!DOCTYPE html>\n");
    printf("<html><head><title>C CGI Demo</title></head>\n");
    printf("<body style='font-family: Arial; padding: 20px;'>\n");
    printf("<h1>✨ C CGI 工作中！</h1>\n");
    printf("<table border='1' cellpadding='10'>\n");
    printf("<tr><th>变量</th><th>值</th></tr>\n");
    printf("<tr><td><b>REQUEST_METHOD</b></td><td>%s</td></tr>\n", method ? method : "N/A");
    printf("<tr><td><b>QUERY_STRING</b></td><td>%s</td></tr>\n", query ? query : "(empty)");
    printf("<tr><td><b>REMOTE_ADDR</b></td><td>%s</td></tr>\n", remote ? remote : "N/A");
    printf("</table>\n");
    printf("<p>🎉 这是由 <b>TinyCC</b> 运行时编译的 C 脚本！</p>\n");
    printf("</body></html>\n");
    
    return 0;
}
