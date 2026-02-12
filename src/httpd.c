
#include "httpd.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <time.h>
#include <sys/times.h>
#include <netdb.h>
#include <signal.h>
#include <dirent.h>
#include <assert.h>

#ifdef linux
#include <sys/sendfile.h>
#endif

/*
** Configure the server by setting the following macros and recompiling.
*/
#ifndef DEFAULT_PORT
#define DEFAULT_PORT "80"             /* Default TCP port for HTTP */
#endif
#ifndef MAX_CONTENT_LENGTH
#define MAX_CONTENT_LENGTH 250000000  /* Max length of HTTP request content */
#endif
#ifndef MAX_CPU
#define MAX_CPU 30                    /* Max CPU cycles in seconds */
#endif

#ifndef BANISH_TIME
#define BANISH_TIME 300               /* How long to banish for abuse (sec) */
#endif

#ifndef SERVER_SOFTWARE
#  define SERVER_SOFTWARE "wpp-httpd/1.0"
#endif
#ifndef SERVER_SOFTWARE_TLS
#  ifdef ENABLE_TLS
#    define SERVER_SOFTWARE_TLS SERVER_SOFTWARE ", with TLS support"
#  else
#    define SERVER_SOFTWARE_TLS SERVER_SOFTWARE
#  endif
#endif

/*
** Clock ID to use for clock_gettime(), for use in collecting the
** wall-clock processing time using clock_gettime() (which is
** signal-safe). We use clock_gettime(), rather than gmtime(), for
** measuring request wall-clock time because it's signal-safe.
** See discussion at:
** https://sqlite.org/althttpd/forumpost/4dc31619341ce947
*/
#ifdef _POSIX_MONOTONIC_CLOCK
#  define ALTHTTPD_CLOCK_ID CLOCK_MONOTONIC
#else
#  define ALTHTTPD_CLOCK_ID CLOCK_REALTIME
   /* noting that this can jump if the system time changes */
#endif

/*
** Althttpd normally only sends a \n, not a full \r\n, for line endings
** even on protocols that specifically require \r\n (HTTP, SMTP, etc.)
** This is in rebellion to the silliness that is \r\n.  Everybody
** understands a bare \n.  Fossil deliberately violates the spec in an
** attempt to promote change.
**
** If you prefer to follow the protocol exactly, compile with -DSEND_CR=1
**
** UPDATE:  Feedback from the internet suggests that the world is not yet
** ready for a CRLF-less web server.  Alas.  I'll make it so that CRLF
** is on by default, but you can still generate a CRLF-less webserver by
** recompiling with -DSEND_CR=0.
*/
#if !defined(SEND_CR) || SEND_CR + 0 >= 1
/* This case for strict compliance */
# define CRLF    "\r\n"
#else
/* Default: Send only \n */
# define CRLF    "\n"
#endif

/*
** We record most of the state information as global variables.  This
** saves having to pass information to subroutines as parameters, and
** makes the executable smaller...
*/

static char*                        default_path = "/bin:/usr/bin";  // 作为 CGI 变量: PATH。即包含常见的可执行文件目录

static int                          g_useHttps = 0;             // 使用的HTTPS模式：0=HTTP，1=外部 HTTPS（stunnel），2=内置TLS支持
                                                                // + 对于外部 HTTPS，其实这里并没做任何处理，也就是和 HTTP 模式完全一样，区别只是在名义上做了标识
                                                                //   即 CGI 变量 HTTPS 的值为 "on" 而不是 "off"，以及 HTTP_SCHEME 的值为 "https" 而不是 "http"

static const char*                  g_zRoot = NULL;             // 网站的根目录
static const char*                  g_zLogFile = NULL;          // 日志文件路径
static const char*                  g_zIPShunDir = NULL;        // 存放被封禁IP地址的目录
static int                          g_mxAge = 120;              // 缓存控制的最大时长（秒）
static int                          g_maxCpu = MAX_CPU;         // 每个进程允许的最大CPU时间（秒）
static int                          g_mxChild = 1000;           // 最大子进程数
static bool                         g_useTimeout = true;        // 是否开启超时限制机制
static bool                         g_enableSAB = false;        // 允许添加回复头以启用 SharedArrayBuffer


static char*                        g_zHttps = 0;               // 作为 CGI 变量：HTTPS
static char*                        g_zHttpScheme = "http";     // 作为 CGI 变量：HTTP_SCHEME
static char*                        g_zServerSoftware = NULL;   // 当前服务器软件名，作为 CGI 变量：SERVER_SOFTWARE，

static char*                        g_zRemoteAddr = NULL;       // 远程请求的IP地址，作为 CGI 变量：REMOTE_ADDR
static char*                        g_zHttpHost = NULL;         // 来自远程请求的主机名。作为 CGI 变量：HTTP_HOST，并可基于该值实现虚拟主机功能
                                                                // + 具体来说就是将 /$HTTP_HOST.website/ 作为站点根目录

static char*                        zHome = NULL;               // 当前请求的站点根目录，基于 HTTP_HOST 请求头（虚拟主机机制）计算得出

static char*                        zMethod = NULL;             // HTTP 协议的 Method 字段，通常是 "GET" 或 "POST"
static char*                        zProtocol = NULL;           /* The protocol being using by the browser */
                                                                // HTTP 协议的 Protocol 字段，通常是 "HTTP/1.0" 或 "HTTP/1.1"
static char*                        zRequestUri = NULL;         // 经过（Sanitized）消毒处理后的请求 URI
                                                                // + HTTP 协议: METHOD URI PROTOCOL，例如 "GET /index.html HTTP/1.1"
                                                                //   但用于请求的 URI 可能包含不安全的字符。而 zRequestUri 则是经过消毒处理后的 URI
                                                                //   即所有不安全的字符都被替换为 '_'，以防止跨站脚本攻击和其他恶意行为
                                                                // + 这里 zRequestUri 其实是通过 zScript + zQuerySuffix 计算得出的

static char*                        zScript = NULL;             // HTTP 请求的（经过消毒处理、且不包括后面 zQuerySuffix 部分的）原始 URI 对象
static char*                        zRealScript = NULL;         // 经过处理后和验证后的 zScript 字段，即确定存在的目标文件（支持用默认索引文件自动填补）
static char*                        zFile = NULL;               // zRealScript 对应的本地文件路径，即 /zHome/zRealScript
static int                          lenFile = 0;                // zFile 的长度
static char*                        zDir = NULL;                // zFile 中最后一个 '/' 之前的部分，即 zFile 的目录部分

static char*                        zPathInfo = NULL;           // zScript 去除 zRealScript 后剩余的部分（即额外路径信息、或者静态页面内的 #anchor）
                                                                // + 例如 URI "/index.html/extra/path/info"，则 zPathInfo 是 "/extra/path/info"
static char*                        zQuerySuffix = NULL;        // URI 中 '?' 及其后面的部分
static char*                        zQueryString = NULL;        // zQuerySuffix 去除 '?' 后的部分

static char*                        zPostData = NULL;           // 请求提交（上传）的 POST 数据
static size_t                       nPostData = 0;              // zPostData 的大小（字节数）

static char*                        zServerName = NULL;         // 服务器名称，即 http:// 后面的主机名部分
static char*                        zServerPort = NULL;         // 服务器端口号，即 http://host:port/ 中的 port 部分
static char*                        zRealPort = NULL;           // 服务器的实际 TCP 端口号（当程序作为独立服务进程运行时）
static char*                        zRealTlsPort = NULL;        // 服务器的实际 TLS TCP 端口号（当程序作为独立服务进程运行时）

static char*                        zAgent = NULL;              // HTTP 请求的 User-Agent 头字段，表示发起请求的浏览器类型
static char*                        zAccept = NULL;             // HTTP 请求的 Accept 头字段，表示客户端可接受的内容格式
static char*                        zAcceptEncoding = NULL;     // HTTP 请求的 Accept-Encoding 头字段，表示客户端可接受的内容编码方式。（gzip 或默认）
static char*                        zContentLength = NULL;      // HTTP 请求的 Content-Length 头字段，表示请求返回的数据长度（字节数）
static char*                        zContentType = NULL;        // HTTP 请求的 Content-Type 头字段，表示请求返回的数据内容类型
static char*                        zReferer = NULL;            // HTTP 请求的 Referer 头字段，表示发起请求的来源页面
static char*                        zCookie = NULL;             // HTTP 请求的 Cookie 头字段，表示随请求发送的 Cookie 信息

static char*                        zAuthType = NULL;           // HTTP 请求的 Authorization 头字段，表示授权类型（基本授权或摘要授权）
static char*                        zAuthArg = NULL;            // HTTP 请求的 Authorization 头字段的参数部分，表示授权凭据（如用户名和密码的 Base64 编码）
static char*                        zRemoteUser = NULL;         // CGI 变量：REMOTE_USER，由授权模块设置，表示经过授权的用户名

static char*                        zIfNoneMatch = NULL;        // HTTP 请求的 If-None-Match 头字段，表示客户端缓存的资源的 ETag 值
static char*                        zIfModifiedSince = NULL;    // HTTP 请求的 If-Modified-Since 头字段，表示客户端缓存的资源的最后修改时间
static ssize_t                      rangeStart = 0;             // HTTP 请求的 Range 头字段的起始字节位置（如果存在 Range 请求）
static ssize_t                      rangeEnd = 0;               // HTTP 请求的 Range 头字段的结束字节位置（如果存在 Range 请求）

static char*                        zScgi = NULL;               // 作为 CGI 变量：SCGI。表示当前请求是否为 SCGI 请求

static char                         zReplyStatus[4];            // HTTP Respond Code
static int                          statusSent = 0;             // 是否已经发送了 HTTP 状态行（如 "HTTP/1.1 200 OK"）。如果已经发送，则后续的 HTTP 回复头字段将被忽略，不会发送给客户端

static bool                         isCGI = false;              // 当前请求是否为 CGI 请求
static int                          isRobot = 0;                // 当前请求是否来自机器人（搜索引擎爬虫等）。2 表示确定是机器人; 1 表示确定不是机器人; 0 表示未知
static bool                         closeConnection = false;    // 表示当前 HTTP 连接（会话）是否需要（在 response 后）关闭

static struct timeval               beginTime;                  // HTTP 请求处理开始的时间，使用 gettimeofday() 获取
static struct timespec              tsBeginTime;                // HTTP 请求处理开始的时间，使用 clock_gettime() 获取。
                                                                // + 使用 ALTHTTPD_CLOCK_ID 定义的时钟获取的，可以是 CLOCK_MONOTONIC 或 CLOCK_REALTIME

static int                          nRequest = 0;               // 请求统计计数（仅在独立服务器模式下使用）
static size_t                       nIn = 0;                    // 上行数据量统计（字节数）。包括 HTTP 请求头和请求体的总字节数
static size_t                       nOut = 0;                   // 下行数据量统计（字节数）。包括 HTTP 回复头和回复体的总字节数

static int                          nTimeoutLine = 0;           // SetTimeout() 函数被调用时的行号，用于调试和日志记录，以便知道超时是在哪个代码位置设置的
static int                          isExiting = 0;              // 是否执行过 althttpd_exit() 函数。可以用来防止在退出过程中执行不必要的操作或输出日志。
static int                          debugFlag = 0;              /* True if being debugged */

//static struct timespec            tsSelf;
static struct rusage                priorSelf;                  /* Previously report SELF time */
static struct rusage                priorChild;                 /* Previously report CHILD time */
static char                         zExpLogFile[500] = {0};     //* %-扩展后的日志文件名（暂时好像没用到）


static int                          omitLog = 0;                /* Do not make logfile entries if true */
static int                          inSignalHandler = 0;        /* True if running a signal handler */

/* Forward reference */
static void Malfunction(int linenum, const char *zFormat, ...);

#ifdef ENABLE_TLS
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
typedef struct TlsServerConn {
    SSL *ssl;          /* The SSL codec */
    BIO *bio;          /* SSL BIO object */
    int iSocket;       /* The socket */
} TlsServerConn;

/*
** There can only be a single OpenSSL IO connection open at a time.
** State information about that IO is stored in the following
** global singleton:
*/
static struct TlsState {
    int isInit;             /* 0: uninit 1: init as client 2: init as server */
    SSL_CTX *ctx;
    const char *zCertFile;  /* --cert CLI arg */
    const char *zKeyFile;   /* --pkey CLI arg */
    TlsServerConn * sslCon;
} tlsState = {
    0,                      /* isInit */
    NULL,                   /* SSL_CTX *ctx */
    NULL,                   /* zCertFile */
    NULL,                   /* zKeyFile */
    NULL                    /* sslCon */
};

/*
** Read a single line of text from the client and stores it in zBuf
** (which must be at least nBuf bytes long). On error it
** calls Malfunction().
**
** If it reads anything, it returns zBuf.
*/
static char *tls_gets(void *pServerArg, char *zBuf, int nBuf) {
    int n = 0, err = 0;
    int i;
    TlsServerConn * const pServer = (TlsServerConn*)pServerArg;
    if( BIO_eof(pServer->bio) ) return 0;
    for (i=0; i<nBuf-1; i++) {
        n = SSL_read(pServer->ssl, &zBuf[i], 1);
        err = SSL_get_error(pServer->ssl, n);
        if( err!=0 ){
            Malfunction(525,"SSL read error.");
        } 
        else if( 0==n || zBuf[i]=='\n' ) break;
    }
    zBuf[i+1] = 0;
    return zBuf;
}

/*
** Reads up tp nBuf bytes of TLS-decoded bytes from the client and
** stores them in zBuf, which must be least nBuf bytes long.  Returns
** the number of bytes read. Fails fatally if nBuf is "too big" or if
** SSL_read() fails. Once pServerArg reaches EOF, this function simply
** returns 0 with no side effects.
*/
static size_t tls_read_server(void *pServerArg, void *zBuf, size_t nBuf) {
    
    TlsServerConn * const pServer = (TlsServerConn*)pServerArg;
    if (nBuf>0x7fffffff ) {
        Malfunction(526,"SSL read too big"); /* LOG: SSL read too big */
    }

    int err = 0; size_t rc = 0;
    while (0==err && nBuf!=rc && 0==BIO_eof(pServer->bio)) {

        const int n = SSL_read(pServer->ssl, (char*)zBuf + rc, (int)(nBuf - rc));
        if( n==0 ) break;
    
        err = SSL_get_error(pServer->ssl, n);
        if(0==err) rc += n;
        else Malfunction(527,"SSL read error."); /* LOG: SSL read error */
    }
    return rc;
}

/*
** Write cleartext bytes into the SSL server codec so that they can
** be encrypted and sent back to the client. On success, returns
** the number of bytes written, else returns a negative value.
*/
static int tls_write_server(void *pServerArg, void const *zBuf,  size_t nBuf){

    TlsServerConn * const pServer = (TlsServerConn*)pServerArg;
    if (nBuf<=0 ) return 0;
    if (nBuf>0x7fffffff) {
        Malfunction(528,"SSL write too big"); /* LOG: SSL write too big */
    }

    int n = SSL_write(pServer->ssl, zBuf, (int)nBuf);
    /* Do NOT call Malfunction() from here, as Malfunction()
    ** may output via this function. The current error handling
    ** is somewhat unsatisfactory, as it can lead to negative
    ** response length sizes in the althttpd log. */
    if( n<=0 ) return -SSL_get_error(pServer->ssl, n);

    return n;    
}
#endif /* ENABLE_TLS */

/*
** A printf() proxy which outputs either to stdout or the outbound TLS
** connection, depending on connection state. It uses a
** statically-sized buffer for TLS output and will fail (via
** Malfunction()) if it's passed too much data. In non-TLS mode it has
** no such limitation. The buffer is generously sized, in any case, to
** be able to handle all of the headers output by althttpd as of the
** time of this writing.
*/
#ifdef ENABLE_TLS
static int althttpd_vprintf(char const * fmt, va_list va){
    
    if (g_useHttps!=2 || NULL==tlsState.sslCon)
        return vprintf(fmt, va);
    
    char pfBuffer[10000];
    const int sz = vsnprintf(pfBuffer, sizeof(pfBuffer), fmt, va);
    if (sz<(int)sizeof(pfBuffer))
        return (int)tls_write_server(tlsState.sslCon, pfBuffer, sz);

    Malfunction(529/* LOG: Output buffer too small */, "Output buffer is too small. Wanted %d bytes.", sz);
    return 0;
}
#else
#define althttpd_vprintf vprintf
#endif

#ifdef ENABLE_TLS
int althttpd_printf(char const * fmt, ...){
  int rc;
  va_list va;
  va_start(va,fmt);
  rc = althttpd_vprintf(fmt, va);
  va_end(va);
  return rc;
}
static void *tls_new_server(int iSocket);
static void tls_close_server(void *pServerArg);
static void tls_atexit(void);
#else
#define althttpd_printf printf
#endif


/* forward references */
static int tls_init_conn(int iSocket);

static void tls_close_conn(void);

static void althttpd_fflush(FILE *f);

/*
** Flush stdout then exit() with the given result code.  This must
** always be used instead of exit() so that a corner case involving
** post-exit() signal handling via Timeout() can be accounted for.
*/
static void althttpd_exit(int iErrCode) {
    assert(isExiting == 0);
    isExiting = iErrCode ? iErrCode : 1;
    althttpd_fflush(stdout);
    tls_close_conn();
    exit(iErrCode);
}

/*
** Mapping between CGI variable names and values stored in
** global variables.
*/
static struct {
    char *zEnvName;
    char **pzEnvValue;
} cgienv[] = {
        {"CONTENT_LENGTH",         &zContentLength}, /* Must be first for SCGI */
        {"AUTH_TYPE",              &zAuthType},
        {"AUTH_CONTENT",           &zAuthArg},
        {"CONTENT_TYPE",           &zContentType},
        {"DOCUMENT_ROOT",          &zHome},
        {"HTTP_ACCEPT",            &zAccept},
        {"HTTP_ACCEPT_ENCODING",   &zAcceptEncoding},
        {"HTTP_COOKIE",            &zCookie},
        {"HTTP_HOST",              &g_zHttpHost},
        {"HTTP_IF_MODIFIED_SINCE", &zIfModifiedSince},
        {"HTTP_IF_NONE_MATCH",     &zIfNoneMatch},
        {"HTTP_REFERER",           &zReferer},
        {"HTTP_SCHEME",            &g_zHttpScheme},
        {"HTTP_USER_AGENT",        &zAgent},
        {"HTTPS",                  &g_zHttps},
        {"PATH",                   &default_path},
        {"PATH_INFO",              &zPathInfo},
        {"QUERY_STRING",           &zQueryString},
        {"REMOTE_ADDR",            &g_zRemoteAddr},
        {"REQUEST_METHOD",         &zMethod},
        {"REQUEST_URI",            &zRequestUri},
        {"REMOTE_USER",            &zRemoteUser},
        {"SCGI",                   &zScgi},
        {"SCRIPT_DIRECTORY",       &zDir},
        {"SCRIPT_FILENAME",        &zFile},
        {"SCRIPT_NAME",            &zRealScript},
        {"SERVER_NAME",            &zServerName},
        {"SERVER_PORT",            &zServerPort},
        {"SERVER_PROTOCOL",        &zProtocol},
        {"SERVER_SOFTWARE",        &g_zServerSoftware},
};

/*
** A structure for holding a single date and time.
*/
typedef struct DateTime DateTime;
struct DateTime {
    long long int iJD;  /* The julian day number times 86400000 */
    int Y, M, D;        /* Year, month, and day */
    int h, m;           /* Hour and minutes */
    double s;           /* Seconds */
};

/*
** Populates p based on the unix timestamp (seconds since 1970-01-01 00:00:00)
** stored in t.
*/
static void unixToDateTime(time_t t, DateTime *p) {
    int Z, A, B, C, D, E, X1;
    int day_s;
    p->iJD = (long long) (((double)t / 86400.0 + 2440587.5) * 86400000);
    Z = (int) ((p->iJD + 43200000) / 86400000);
    A = (int) ((Z - 1867216.25) / 36524.25);
    A = Z + 1 + A - (A / 4);
    B = A + 1524;
    C = (int) ((B - 122.1) / 365.25);
    D = (36525 * (C & 32767)) / 100;
    E = (int) ((B - D) / 30.6001);
    X1 = (int) (30.6001 * E);
    p->D = B - D - X1;
    p->M = E < 14 ? E - 1 : E - 13;
    p->Y = p->M > 2 ? C - 4716 : C - 4715;

    day_s = (int)(t % 86400);
    p->h = day_s / 3600;
    p->m = day_s % 3600 / 60;
    p->s = day_s % 60;
#if 0
                                                                                                                            fprintf(stderr,"day_s = %d, DateTime Y=%d M=%d D=%d h=%d m=%d s=%d\n",
    (int)day_s,
    p->Y, p->M, p->D,
    p->h, p->m, (int)p->s);
#endif
}

#ifdef COMBINED_LOG_FORMAT
                                                                                                                        /*
** Copies and converts _relevant_ fields of pD into pTm. Only those
** fields relevant for althttpd's own logging are handled and the rest
** are zeroed out, thus pTm is not necessarily suitable for all
** strftime() formats or libc time functions after calling this.
*/
static void DateTime_toTm(const DateTime *pD, struct tm *pTm){
    memset(pTm, 0, sizeof(*pTm));
    pTm->tm_isdst = -1;
    pTm->tm_sec = (int)pD->s;
    pTm->tm_min = pD->m;
    pTm->tm_hour = pD->h;
    pTm->tm_mday = pD->D;
    pTm->tm_mon = pD->M - 1;
    pTm->tm_year = pD->Y - 1900;
}
#endif

/*
** Convert a struct timeval into an integer number of microseconds
*/
static long long int tvms(struct timeval *p) {
    return ((long long int) p->tv_sec) * 1000000 + (long long int) p->tv_usec;
}

/*
** Convert a struct timespec into an integer number of microseconds
*/
static long long int tsms(struct timespec *p) {
    return ((long long int) p->tv_sec) * 1000000 + (long long int) (p->tv_nsec / 1000);
}

/*
**************************************************************************
** NOTE:  The following log_xxx() routines are doing work that fprintf()
** would normally do for us.  But we cannot use fprintf() because these
** routines are called from within an interrupt handler.
*/
/*
** Append text to z[] if there is space available in z[] to do so safely.
** zIn is zero-terminated text to be appended.
**
** If sufficient space exists in z[] to hold zIn[], then do the append
** and set *pzTail to point to the zero-terminator on z[] after zIn[] has
** been appended.  In other words, *pzTail is left pointing to the spot
** where the next append should begin.  A pointer to z[] is returned.
**
** If z[] is not large enough to hold all of zIn[] or if z is NULL, then
** leave z[] unchanged, return NULL, and set *pzTail to NULL.
**
** If zIn is NULL, then *pzTail is set to zEnd and z[] is returned
** unchanged.  This routine is essentially a no-op in that case.
*/
static char *log_str(
        char *z,              /* Append text from zIn[] into this buffer */
        const char *zEnd,     /* Last byte of available space for z[] */
        const char *zIn,      /* Text that should be appended to z[].  NOT NULL */
        char **pzTail         /* Write pointer to new zero-terminator in z[] */
) {
    const size_t n = z ? strlen(zIn) : 0;
    if (!z || zEnd <= z + n) {
        *pzTail = 0;
        return 0;
    }
    memcpy(z, zIn, n);
    *pzTail = z + n;
    return z;
}

/*
** Works like log_str() but doubles all double-quote characters in the
** input. If z is NULL or zIn is too long for the input buffer,
** *zTail is set to 0 and 0 is returned.
**
** This is used to quote strings for output into the CSV log file.
*/
static char *log_escstr(
        char *z,            /* Append escaped text to this buffer */
        const char *zEnd,   /* Last available byte of z[] */
        const char *zIn,    /* Text to be appended.  NOT NULL. */
        char **pzTail       /* Write new zero-terminator of z[] here */
) {
    char *zOrig = z;
    char c = *zIn;
    if (!z || z >= zEnd) {
        goto too_short;
    }
    for (; c && z < zEnd; c = *++zIn) {
        if ('"' == c) {
            if (z >= zEnd - 1) {
                goto too_short;
            }
            *z++ = c;
        }
        *z++ = c;
    }
    *pzTail = z;
    return zOrig;
    too_short:
    *pzTail = 0;
    return 0;
}

/*
** Convert 64-bit signed integer "d" into text and append that text
** onto the end of buffer z[] if there is sufficient space in z[].
**
** Appends d to z, sets *zpTail to the byte after the new end of z, and
** returns z. If appending d would extend past zEnd then *zTail is set
** to 0 and 0 is returned, but z is not modified.  If z is NULL, *zTail
** is set to 0 but this is otherwise a no-op.
**
** minDigits must be at least 1 and d will be left-padded with 0's to
** a minimum of that length.
*/
static char *log_int(
        char *z,           /* Append text representation of d here */
        const char *zEnd,  /* Last available byte of space in z[] */
        long long int d,   /* 64-bit signed integer value to be converted */
        int minDigits,     /* Minimum number of digits to display */
        char **pzTail      /* OUT: The next zero-terminator in z[] */
) {
    char buf[128];
    int i = (int) sizeof(buf), n = 0;
    int neg = 0;

    assert(minDigits > 0 && minDigits <= 4);
    if (!z) {
        *pzTail = 0;
        return 0;
    } else if (d < 0) {
        neg = 1;
        d = -d;
    }
    for (; (minDigits-- > 0) || d > 0; d /= 10) {
        buf[--i] = (char)('0' + (d % 10));
        ++n;
    }
    if (neg) {
        buf[--i] = '-';
        ++n;
    }
    if (zEnd <= z + n) {
        return 0;
    }
    memcpy(z, &buf[i], n);
    *pzTail = z + n;
    return z;
}

#ifndef COMBINED_LOG_FORMAT

/*
** Append date/time pD (rendered as "YYYY-MM-DD hh:mm:ss") to z[] if
** space is available in z[].  Leave *pzTail pointing to the NULL
** terminator and return a pointer to z[].
**
** If insufficient space is available in z[] to hold the new decoded
** date/time, then set *pzTail to NULL and return a NULL pointer.
*/
static char *log_DateTime(
        char *z,                   /* Append YYYY-MM-DD hh:mm:ss to this buffer */
        const char *zEnd,           /* Last available byte in buffer z[] */
        const struct DateTime *pD,  /* The date/time to be appended */
        char **pzTail               /* OUT: zero terminator after appending */
) {
    char *zOut = z;
    if (z + 19 >= zEnd) {
        *pzTail = 0;
        return 0;
    }
#define aint(N, DIGITS, SUFFIX) log_int(zOut,zEnd,(int)(N),(DIGITS),&zOut); \
  if( SUFFIX ) log_str( zOut, zEnd, SUFFIX, &zOut )
    aint(pD->Y, 4, "-");
    aint(pD->M, 2, "-");
    aint(pD->D, 2, " ");
    aint(pD->h, 2, ":");
    aint(pD->m, 2, ":");
    aint(pD->s, 2, 0);
#undef aint
    *pzTail = zOut;
    return z;
}

#endif

/*
** Make an entry in the log file.  If the HTTP connection should be
** closed, then terminate this process.  Otherwise return.
**
** This routine might be called from a signal handler.  Make sure
** this routine does not invoke any subroutines that are not signal-safe
** if the global variable inSignalHandler is set.  Note that fprintf()
** is not signal-safe on all systems.
*/
void MakeLogEntry(int exitCode, int lineNum) {
    int logfd;
    if (zPostData) {
        if (inSignalHandler == 0) {
            free(zPostData);
        }
        zPostData = 0;
    }
    if (g_zLogFile && !omitLog) {
        struct rusage self, children;
        int waitStatus;
        const char *zRM = zRemoteUser ? zRemoteUser : "";
        const char *zFilename;

        if (zScript == 0) zScript = "";
        if (zRealScript == 0) zRealScript = "";
        if (g_zRemoteAddr == 0) g_zRemoteAddr = "";
        if (g_zHttpHost == 0) g_zHttpHost = "";
        if (zReferer == 0) zReferer = "";
        if (zAgent == 0) zAgent = "";
        if (zQuerySuffix == 0) zQuerySuffix = "";
        if (zExpLogFile[0] != 0) {
            zFilename = zExpLogFile;
        } else {
            zFilename = g_zLogFile;
        }
        waitpid(-1, &waitStatus, WNOHANG);
        if (inSignalHandler != 0) {
            /* getrusage() is not signal-safe, so fall back to 0. */
            self = priorSelf;
            children = priorChild;
        } else {
            getrusage(RUSAGE_SELF, &self);
            getrusage(RUSAGE_CHILDREN, &children);
        }
        if ((logfd = open(zFilename, O_WRONLY | O_CREAT | O_APPEND, 0640)) > 0) {
            time_t tNow;              /* current time */
            struct timespec tsNow;    /* current time (again!) */
            struct DateTime dt;       /* high-level tNow */
            char msgbuf[5000];        /* message buffer */
            const char *const zEnd = &msgbuf[0] + sizeof(msgbuf);
            char *zPos = &msgbuf[0]; /* current write pos */
            long long int t;          /* Elapse CGI time */
            size_t szRS;              /* size of zRealScript */
            size_t szOther;           /* Size of other components of (16) */

#define astr(STR) log_str( zPos, zEnd, (STR), &zPos )
#define acomma astr(",")
#define astr2(STR) astr( STR ); acomma
#define aint(N) log_int( zPos, zEnd, (N), 1, &zPos ); acomma

            clock_gettime(ALTHTTPD_CLOCK_ID, &tsNow);
            time(&tNow);
            unixToDateTime(tNow, &dt);
#ifdef COMBINED_LOG_FORMAT
                                                                                                                                    /* COMBINED_LOG_FORMAT is a log-file format used by some other
      ** web servers.  Support for COMBINED_LOG_FORMAT was added at some
      ** point in althttpd's history.  But there are no known current
      ** users.  COMBINED_LOG_FORMAT is deprecated
      */
      /* Potential TODO: eliminate use of strftime(). Default builds
      ** of althttpd do not use COMBINED_LOG_FORMAT, so this is a
      ** low-priority issue. */
      {
        struct tm vTm/* to remove once we have a strftime() substitute */;
        char zDate[200];
        DateTime_toTm(&dt, &vTm);
        strftime(zDate, sizeof(zDate), "%d/%b/%Y:%H:%M:%S %Z", &vTm);
        astr( g_zRemoteAddr );
        astr( " - - [" );
        astr( zDate );
        astr( "] \"" );
        astr( zMethod );
        astr( " " );
        astr( zScript );
        astr( " " );
        astr( zProtocol );
        astr( "\" " );
        astr( zReplyStatus );
        astr( " " );
        aint( nOut );
        astr( " \"" );
        astr( zReferer );
        astr( "\" \"" );
        astr( zAgent );
        astr( "\"\n" );
      }
#else
            /* Log record files:
      **  (1) Date and time
      **  (2) IP address
      **  (3) URL being accessed
      **  (4) Referer
      **  (5) Reply status
      **  (6) Bytes received
      **  (7) Bytes sent
      **  (8) Self user time
      **  (9) Self system time
      ** (10) Children user time
      ** (11) Children system time
      ** (12) Total wall-clock time
      ** (13) Request number for same TCP/IP connection
      ** (14) User agent
      ** (15) Remote user
      ** (16) Bytes of URL that correspond to the SCRIPT_NAME.  Or if
      **      SCRIPT_NAME contains components (ex: not-found.html) that are
      **      not part of URL, then this is a string of the form
      **      "N+OTHER" where N is the number of bytes of prefix taken from
      **      URL and OTHER is the text of the extension.
      ** (17) Line number in source file
      */
#define escstr(X) log_escstr( zPos, zEnd, X, &zPos )
            /* (1) */ log_DateTime(zPos, zEnd, &dt, &zPos);
            astr(",");
            /* (2) */ astr2(g_zRemoteAddr);
            /* (3) */ astr("\"");
            astr(g_zHttpScheme);
            astr("://");
            escstr(g_zHttpHost);
            escstr(zScript);
            escstr(zQuerySuffix);
            astr2("\"");
            /* (4) */ astr("\"");
            escstr(zReferer);
            astr2("\"");
            /* (5) */ astr2(zReplyStatus);
            /* (6) */ aint(nIn);
            /* (7) */ aint(nOut);
            /* (8) */ aint(tvms(&self.ru_utime) - tvms(&priorSelf.ru_utime));
            /* (9) */ aint(tvms(&self.ru_stime) - tvms(&priorSelf.ru_stime));
            t = tvms(&children.ru_utime) - tvms(&priorChild.ru_utime);
            if (isCGI) {
                if (t == 0) t = 1;
                isCGI = false;
            }
            /* (10) */ aint(t);
            /* (11) */ aint(tvms(&children.ru_stime) - tvms(&priorChild.ru_stime));
            /* (12) */ aint(tsms(&tsNow) - tsms(&tsBeginTime));
            /* (13) */ aint(nRequest);
            /* (14) */ astr("\"");
            escstr(zAgent);
            astr2("\"");
            /* (15) */ astr("\"");
            escstr(zRM);
            astr2("\"");
            szRS = strlen(zRealScript);
            szOther = strlen(g_zHttpScheme) + strlen(g_zHttpHost) + 3;
            if (zScript != 0 && strncmp(zRealScript, zScript, szRS) == 0) {
                /* (16) */ aint(szOther + szRS);
            } else {
                /* (16) */ astr("\"");
                log_int(zPos, zEnd, (int64_t)szOther, 1, &zPos);
                astr("+");
                escstr(zRealScript);
                astr2("\"");
            }
            if (lineNum == 0) lineNum = isRobot;
            isRobot = 0;
            /* (17) */ log_int(zPos, zEnd, lineNum, 1, &zPos);
#undef escstr
#ifdef ALTHTTPD_LOG_PID
                                                                                                                                    /* Appending of PID to the log is used only to assist in
      ** debugging of hanging althttpd processes:
      ** https://sqlite.org/althttpd/forumpost/4dc31619341ce947 */
      acomma;
      /* (18) */ log_int( zPos, zEnd, getpid(), 1, &zPos );
#endif /* ALTHTTPD_LOG_PID */
            astr("\n");
            priorSelf = self;
            priorChild = children;
#endif
            if (zPos != 0) {
                size_t iStart = 0;
                size_t toSend;
                size_t nSent;
                assert(zPos < zEnd - 1);
                assert(zPos > &msgbuf[0]);
                *zPos = 0;
                toSend = zPos - &msgbuf[0];
                while (1 /*exit-by-break*/ ) {
                    nSent = write(logfd, msgbuf + iStart, toSend);
                    if (nSent <= 0) break;
                    if (nSent >= toSend) break;
                    iStart += nSent;
                    toSend -= nSent;
                }
            }
            close(logfd);
            nIn = nOut = 0;
#undef astr
#undef astr2
#undef aint
#undef acomma
        }
    }
    if (closeConnection || inSignalHandler) {
        althttpd_exit(exitCode);
    }
    statusSent = 0;
}

/*
** Allocate memory safely
*/
static char *SafeMalloc(size_t size) {
    char *p;

    p = (char *) malloc(size);
    if (p == 0) {
        strcpy(zReplyStatus, "998");
        MakeLogEntry(1, 100);  /* LOG: Malloc() failed */
        althttpd_exit(1);
    }
    return p;
}

/* Forward reference */
static void BlockIPAddress(void);

static void ServiceUnavailable(int lineno);

/*
** Set the value of environment variable zVar to zValue.
*/
static void SetEnv(const char *zVar, const char *zValue) {
    char *z;
    size_t len;
    if (zValue == 0) zValue = "";
    /* Disable an attempted bashdoor attack */
    if (strncmp(zValue, "() {", 4) == 0) {
        BlockIPAddress();
        ServiceUnavailable(902); /* LOG: 902 bashdoor attack */
        zValue = "";
    }
    len = strlen(zVar) + strlen(zValue) + 2;
    z = SafeMalloc(len);
    sprintf(z, "%s=%s", zVar, zValue);
    putenv(z);
}

/*
** Remove the first space-delimited token from a string and return
** a pointer to it.  Add a NULL to the string to terminate the token.
** Make *zLeftOver point to the start of the next token.
*/
/*
** 解析输入字符串的第一个元素
*/
char *GetFirstElement(char *zInput, char **zLeftOver) {
    char *zResult = 0;
    if (zInput == 0) {
        if (zLeftOver) *zLeftOver = 0;
        return 0;
    }
    while (isspace(*(unsigned char *) zInput)) { zInput++; }
    zResult = zInput;
    while (*zInput && !isspace(*(unsigned char *) zInput)) { zInput++; }
    if (*zInput) {
        *zInput = 0;
        zInput++;
        while (isspace(*(unsigned char *) zInput)) { zInput++; }
    }
    if (zLeftOver) { *zLeftOver = zInput; }
    return zResult;
}

/*
** Make a copy of a string into memory obtained from malloc.
*/
/*
** 复制字符串
*/
char *StrDup(const char *zSrc) {
    char *zDest;
    size_t size;

    if (zSrc == 0) return 0;
    size = strlen(zSrc) + 1;
    zDest = (char *) SafeMalloc(size);
    strcpy(zDest, zSrc);
    return zDest;
}

static char *StrAppend(char *zPrior, const char *zSep, const char *zSrc) {
    char *zDest;
    size_t size;
    size_t n0, n1, n2;

    if (zSrc == 0) return 0;
    if (zPrior == 0) return StrDup(zSrc);
    n0 = strlen(zPrior);
    n1 = strlen(zSep);
    n2 = strlen(zSrc);
    size = n0 + n1 + n2 + 1;
    zDest = (char *) SafeMalloc(size);
    memcpy(zDest, zPrior, n0);
    free(zPrior);
    memcpy(&zDest[n0], zSep, n1);
    memcpy(&zDest[n0 + n1], zSrc, n2 + 1);
    return zDest;
}

// 通过 zString 和 zQueryString 构造 REQUEST_URI 值
// + REQUEST_URI 一般是 HTTP 请求第一行的第二个字段
//   但是我们可能对 SCRIPT_NAME 和/或 PATH_INFO 进行了一些清理
//   因此，在将 REQUEST_URI 发送到 CGI 或 SCGI 之前会重新计算它
// + 例如：
static void ComputeRequestUri(void) {
    zRequestUri = (zQueryString == NULL || !*zQueryString)
            ? zScript
            : StrAppend(zScript, "?", zQueryString);
}

/*
** Compare two ETag values. Return 0 if they match and non-zero if they differ.
**
** The one on the left might be a NULL pointer and it might be quoted.
*/
static int CompareEtags(const char *zA, const char *zB) {
    if (zA == 0) return 1;
    if (zA[0] == '"') {
        int lenB = (int) strlen(zB);
        if (strncmp(zA + 1, zB, lenB) == 0 && zA[lenB + 1] == '"') return 0;
    }
    return strcmp(zA, zB);
}

/*
** Break a line at the first \n or \r character seen.
*/
/*
** 移除字符串末尾的换行符
*/
void RemoveNewline(char *z) {
    if (z == 0) return;
    while (*z && *z != '\n' && *z != '\r') { z++; }
    *z = 0;
}

/* Render seconds since 1970 as an RFC822 date string.  Return
** a pointer to that string in a static buffer.
*/
static char *Rfc822Date(time_t t) {
    struct tm *tm;
    static char zDate[100];
    tm = gmtime(&t);
    strftime(zDate, sizeof(zDate), "%a, %d %b %Y %H:%M:%S GMT", tm);
    return zDate;
}

/*
** Print a date tag in the header.  The name of the tag is zTag.
** The date is determined from the unix timestamp given.
*/
static int DateTag(const char *zTag, time_t t) {
    return althttpd_printf("%s: %s" CRLF, zTag, Rfc822Date(t));
}

/*
** Parse an RFC822-formatted timestamp as we'd expect from HTTP and return
** a Unix epoch time. <= zero is returned on failure.
*/
time_t ParseRfc822Date(const char *zDate) {
    int mday, mon, year, yday, hour, min, sec;
    char zIgnore[4];
    char zMonth[4];
    static const char *const azMonths[] =
            {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (7 == sscanf(zDate, "%3[A-Za-z], %d %3[A-Za-z] %d %d:%d:%d", zIgnore,
                    &mday, zMonth, &year, &hour, &min, &sec)) {
        if (year > 1900) year -= 1900;
        for (mon = 0; mon < 12; mon++) {
            if (!strncmp(azMonths[mon], zMonth, 3)) {
                int nDay;
                int isLeapYr;
                static int priorDays[] =
                        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
                isLeapYr = year % 4 == 0 && (year % 100 != 0 || (year + 300) % 400 == 0);
                yday = priorDays[mon] + mday - 1;
                if (isLeapYr && mon > 1) yday++;
                nDay = (year - 70) * 365 + (year - 69) / 4 - year / 100 + (year + 300) / 400 + yday;
                return ((time_t) (nDay * 24 + hour) * 60 + min) * 60 + sec;
            }
        }
    }
    return 0;
}

/*
** Test procedure for ParseRfc822Date
*/
void TestParseRfc822Date(void) {
    time_t t1, t2;
    for (t1 = 0; t1 < 0x7fffffff; t1 += 127) {
        t2 = ParseRfc822Date(Rfc822Date(t1));
        assert(t1 == t2);
    }
}

/*
** Print the first line of a response followed by the server type.
*/
static void StartResponse(const char *zResultCode) {
    time_t now;
    time(&now);
    if (statusSent) return;
    nOut += althttpd_printf("%s %s" CRLF,
                            zProtocol ? zProtocol : "HTTP/1.1", zResultCode);
    strncpy(zReplyStatus, zResultCode, 3);
    zReplyStatus[3] = 0;
    if (zReplyStatus[0] >= '4') {
        closeConnection = true;
        // 错误响应（4xx, 5xx）不应该被缓存，避免浏览器缓存错误信息
        nOut += althttpd_printf("Cache-Control: no-cache, no-store, must-revalidate" CRLF);
        nOut += althttpd_printf("Pragma: no-cache" CRLF);  // HTTP/1.0 兼容
        nOut += althttpd_printf("Expires: 0" CRLF);        // HTTP/1.0 兼容
    }
    if (closeConnection) {
        nOut += althttpd_printf("Connection: close" CRLF);
    } else {
        nOut += althttpd_printf("Connection: keep-alive" CRLF);
    }
    nOut += DateTag("Date", now);
    statusSent = 1;
}

/*
** Check all of the files in the g_zIPShunDir directory.  Unlink any
** files in that directory that have expired.
**
** This routine might be slow if there are a lot of blocker files.
** So it only runs when we are not in a hurry, such as prior to sending
** a 404 Not Found reply.
*/
static void UnlinkExpiredIPBlockers(void) {
    DIR *pDir;
    struct dirent *pFile;
    size_t nIPShunDir;
    time_t now;
    char zFilename[2000];

    if (g_zIPShunDir == 0) return;
    if (g_zIPShunDir[0] != '/') return;
    nIPShunDir = strlen(g_zIPShunDir);
    while (nIPShunDir > 0 && g_zIPShunDir[nIPShunDir - 1] == '/') nIPShunDir--;
    if (nIPShunDir > sizeof(zFilename) - 100) return;
    memcpy(zFilename, g_zIPShunDir, nIPShunDir);
    zFilename[nIPShunDir] = 0;
    pDir = opendir(zFilename);
    if (pDir == 0) return;
    zFilename[nIPShunDir] = '/';
    time(&now);
    while ((pFile = readdir(pDir)) != 0) {
        size_t nFile = strlen(pFile->d_name);
        int rc;
        struct stat statbuf;
        if (nIPShunDir + nFile >= sizeof(zFilename) - 2) continue;
        if (strstr(pFile->d_name, "..")) continue;
        memcpy(zFilename + nIPShunDir + 1, pFile->d_name, nFile + 1);
        memset(&statbuf, 0, sizeof(statbuf));
        rc = stat(zFilename, &statbuf);
        if (rc) continue;
        if (!S_ISREG(statbuf.st_mode)) continue;
        if (statbuf.st_size == 0) continue;
        if (statbuf.st_size * 5 * BANISH_TIME + statbuf.st_mtime > now) continue;
        unlink(zFilename);
    }
    closedir(pDir);
}

/* Return true if the request URI contained in zScript[] seems like a
** hack attempt.
*/
static int LikelyHackAttempt(void) {
    static const char *azHackUris[] = {
            "/../",
            "/./",
            "_SELECT_",
            "_select_",
            "_sleep_",
            "_OR_",
            "_AND_",
            "/etc/passwd",
            "/bin/sh",
            "/.git/",
            "/swagger.yaml",
            "/phpThumb.php",
            "/.htpasswd",
            "/.passwd",
            "/tomcat/manager/status/",
            "/WEB-INF/jboss-web.xml",
            "/phpMyAdmin/setup/index.php",
            "/examples/feed-viewer/feed-proxy.php",
    };
    unsigned int i;
    if (zScript == 0) return 0;
    if (zScript[0] == 0) return 0;
    if (zScript[0] != '/') return 1;
    for (i = 0; i < sizeof(azHackUris) / sizeof(azHackUris[0]); i++) {
        if (strstr(zScript, azHackUris[i]) != 0) return 1;
    }
    return 0;
}

/*
** An abusive HTTP request has been submitted by the IP address g_zRemoteAddr.
** Block future requests coming from this IP address.
**
** This only happens if the g_zIPShunDir variable is set, which is only set
** by the --ipshun command-line option.  Without that setting, this routine
** is a no-op.
**
** If g_zIPShunDir is a valid directory, then this routine uses g_zRemoteAddr
** as the name of a file within that directory.  Cases:
**
** +  The file already exists and is not an empty file.  This will be the
**    case if the same IP was recently blocked, but the block has expired,
**    and yet the expiration was not so long ago that the blocking file has
**    been unlinked.  In this case, add one character to the file, which
**    will update its mtime (causing it to be active again) and increase
**    its expiration timeout.
**
** +  The file exists and is empty.  This happens if the administrator
**    uses "touch" to create the file.  An empty blocking file indicates
**    a permanent block.  Do nothing.
**
** +  The file does not exist.  Create it anew and make it one byte in size.
**
** The UnlinkExpiredIPBlockers() routine will run from time to time to
** unlink expired blocker files.  If the DisallowedRemoteAddr() routine finds
** an expired blocker file corresponding to g_zRemoteAddr, it might unlink
** that one blocker file if the file has been expired for long enough.
*/
static void BlockIPAddress(void) {
    size_t nIPShunDir;
    size_t nRemoteAddr;
    int rc;
    struct stat statbuf;
    char zFullname[1000];

    if (g_zIPShunDir == 0) return;
    if (g_zRemoteAddr == 0) return;
    if (g_zRemoteAddr[0] == 0) return;

    /* If we reach this point, it means that a suspicious request was
  ** received and we want to activate IP blocking on the remote
  ** address.
  */
    nIPShunDir = strlen(g_zIPShunDir);
    while (nIPShunDir > 0 && g_zIPShunDir[nIPShunDir - 1] == '/') nIPShunDir--;
    nRemoteAddr = strlen(g_zRemoteAddr);
    if (nIPShunDir + nRemoteAddr + 2 >= sizeof(zFullname)) {
        Malfunction(914, /* LOG: buffer overflow */
                    "buffer overflow");
    }
    memcpy(zFullname, g_zIPShunDir, nIPShunDir);
    zFullname[nIPShunDir] = '/';
    memcpy(zFullname + nIPShunDir + 1, g_zRemoteAddr, nRemoteAddr + 1);
    rc = stat(zFullname, &statbuf);
    if (rc != 0 || statbuf.st_size > 0) {
        FILE *lock = fopen(zFullname, "a");
        if (lock) {
            fputc('X', lock);
            fclose(lock);
        }
    }
}

/*
** Send a service-unavailable reply.
*/
static void ServiceUnavailable(int lineno) {
    StartResponse("503 Service Unavailable");
    nOut += althttpd_printf(
            "Content-type: text/plain; charset=utf-8" CRLF
            CRLF
            "Service to IP address %s temporarily blocked due to abuse\n",
            g_zRemoteAddr
    );
    closeConnection = true;
    MakeLogEntry(0, lineno);
    althttpd_exit(0);
}

/*
** Tell the client that there is no such document
*/
static void NotFound(int lineno) {
    if (LikelyHackAttempt()) {
        BlockIPAddress();
        ServiceUnavailable(lineno);
    }
    UnlinkExpiredIPBlockers();
    StartResponse("404 Not Found");
    nOut += althttpd_printf(
            "Content-type: text/html; charset=utf-8" CRLF
            CRLF
            "<html><head><title lineno=\"%d\">Not Found</title></head>\n"
            "<body><h1>Document Not Found</h1>\n"
            "The document %s is not available on this server\n"
            "</body></html>\n", lineno, zScript);
    MakeLogEntry(0, lineno);
    althttpd_exit(0);
}

/*
** Tell the client that they are not welcomed here.
*/
static void Forbidden(int lineno) {
    StartResponse("403 Forbidden");
    nOut += althttpd_printf(
            "Content-type: text/plain; charset=utf-8" CRLF
            CRLF
            "Access denied\n"
    );
    closeConnection = true;
    MakeLogEntry(0, lineno);
    althttpd_exit(0);
}

/*
** Tell the client that authorization is required to access the
** document.
*/
static void NotAuthorized(const char *zRealm) {
    StartResponse("401 Authorization Required");
    nOut += althttpd_printf(
            "WWW-Authenticate: Basic realm=\"%s\"" CRLF
            "Content-type: text/html; charset=utf-8" CRLF
            CRLF
            "<head><title>Not Authorized</title></head>\n"
            "<body><h1>401 Not Authorized</h1>\n"
            "A login and password are required for this document\n"
            "</body>\n", zRealm);
    MakeLogEntry(0, 110);  /* LOG: Not authorized */
}

/*
** Tell the client that there is an error in the script.
*/
static void CgiError(void) {
    StartResponse("500 Error");
    nOut += althttpd_printf(
            "Content-type: text/html; charset=utf-8" CRLF
            CRLF
            "<head><title>CGI Program Error</title></head>\n"
            "<body><h1>CGI Program Error</h1>\n"
            "The CGI program %s generated an error\n"
            "</body>\n", zScript);
    MakeLogEntry(0, 120);  /* LOG: CGI Error */
    althttpd_exit(0);
}

/*
** Set the timeout in seconds.  0 means no-timeout.
*/
static void SetTimeout(int nSec, int lineNum) {
    if (g_useTimeout) {
        nTimeoutLine = lineNum;
        alarm(nSec);
    }
}

/*
** This is called if we timeout or catch some other kind of signal.
** Log an error code which is 900+iSig and then quit.
**
** If called after althttpd_exit(), this is a no-op.
*/
static void Timeout(int iSig) {
    if (debugFlag == 0 && isExiting == 0) {
        if (zScript && zScript[0]) {
            char zBuf[10];
            zBuf[0] = '9';
            zBuf[1] = (char)('0' + (iSig / 10) % 10);
            zBuf[2] = (char)('0' + iSig % 10);
            zBuf[3] = 0;
            strcpy(zReplyStatus, zBuf);
            ++inSignalHandler;
            switch (iSig) {
                case SIGALRM:
                    MakeLogEntry(0, nTimeoutLine);
                    break;
                case SIGSEGV:
                    MakeLogEntry(0, 131);  /* LOG: SIGSEGV */
                    break;
                case SIGPIPE:
                    MakeLogEntry(0, 132);  /* LOG: SIGPIPE */
                    break;
                case SIGXCPU:
                    MakeLogEntry(0, 133);  /* LOG: SIGXCPU */
                    break;
                default:
                    MakeLogEntry(0, 139);  /* LOG: Unknown signal */
                    break;
            }
            --inSignalHandler;
        }
        althttpd_exit(0);
    }
}

/*
** Tell the client that there is an error in the script.
*/
static void CgiScriptWritable(void) {
    StartResponse("500 CGI Configuration Error");
    nOut += althttpd_printf(
            "Content-type: text/plain; charset=utf-8" CRLF
            CRLF
            "The CGI program %s is writable by users other than its owner.\n",
            zRealScript);
    MakeLogEntry(0, 140);  /* LOG: CGI script is writable */
    althttpd_exit(0);
}

/*
** Tell the client that the server malfunctioned.
*/
void Malfunction(int linenum, const char *zFormat, ...) {
    va_list ap;
    va_start(ap, zFormat);
    StartResponse("500 Server Malfunction");
    nOut += althttpd_printf(
            "Content-type: text/plain; charset=utf-8" CRLF
            CRLF
            "Web server malfunctioned; error number %d\n\n", linenum);
    if (zFormat) {
        nOut += althttpd_vprintf(zFormat, ap);
        althttpd_printf("\n");
        nOut++;
    }
    va_end(ap);
    MakeLogEntry(0, linenum);
    althttpd_exit(0);
}

/*
** This routine is called only by the CGI subprocess while it is
** starting up.  The subprocess has encountered a problem that prevents
** it from running the CGI program.  Report that error back up to the
** parent, then exit.
**
** This is similar to Malfunction() but it works on the CGI subprocess
** side.
*/
void CgiStartFailure(int fd, int linenum, const char *zFormat, ...) { (void) linenum;
    va_list ap; va_start(ap, zFormat);
    char zErr[5000];
    ssize_t sent = write(fd, "status: 500\r\n\r\n", 15);
    if (zFormat) {
        vsnprintf(zErr, sizeof(zErr) - 1, zFormat, ap);
        zErr[sizeof(zErr) - 1] = 0;
    }
    else memcpy(zErr, "CGI failed", 11);
    sent += write(fd, zErr, strlen(zErr));
    va_end(ap);
    althttpd_exit(1);
}


/*
** Do a server redirect to the document specified.  The document
** name not contain scheme or network location or the query string.
** It will be just the path.
*/
static void Redirect(const char *zPath, int iStatus, int finish, int lineno) {
    switch (iStatus) {
        case 301:
            StartResponse("301 Permanent Redirect");
            break;
        case 308:
            StartResponse("308 Permanent Redirect");
            break;
        default:
            StartResponse("302 Temporary Redirect");
            break;
    }
    if (zServerPort == 0 || zServerPort[0] == 0 || strcmp(zServerPort, "80") == 0) {
        nOut += althttpd_printf("Location: %s://%s%s%s" CRLF,
                                g_zHttpScheme, zServerName, zPath, zQuerySuffix);
    } else {
        nOut += althttpd_printf("Location: %s://%s:%s%s%s" CRLF,
                                g_zHttpScheme, zServerName, zServerPort, zPath, zQuerySuffix);
    }
    if (finish) {
        nOut += althttpd_printf("Content-length: 0" CRLF CRLF);
        MakeLogEntry(0, lineno);
    }
    fflush(stdout);
}

/*
** This function treats its input as a base-64 string and returns the
** decoded value of that string.  Characters of input that are not
** valid base-64 characters (such as spaces and newlines) are ignored.
*/
static void Decode64(char *z64) {
    char *zData;
    int n64;
    int i, j;
    int a, b, c, d;
    static int isInit = 0;
    static int trans[128];
    static unsigned char zBase[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    if (!isInit) {
        for (i = 0; i < 128; i++) { trans[i] = 0; }
        for (i = 0; zBase[i]; i++) { trans[zBase[i] & 0x7f] = i; }
        isInit = 1;
    }
    n64 = (int)strlen(z64);
    while (n64 > 0 && z64[n64 - 1] == '=') n64--;
    zData = z64;
    for (i = j = 0; i + 3 < n64; i += 4) {
        a = trans[z64[i] & 0x7f];
        b = trans[z64[i + 1] & 0x7f];
        c = trans[z64[i + 2] & 0x7f];
        d = trans[z64[i + 3] & 0x7f];
        zData[j++] = (char)(((a << 2) & 0xfc) | ((b >> 4) & 0x03));
        zData[j++] = (char)(((b << 4) & 0xf0) | ((c >> 2) & 0x0f));
        zData[j++] = (char)(((c << 6) & 0xc0) | (d & 0x3f));
    }
    if (i + 2 < n64) {
        a = trans[z64[i] & 0x7f];
        b = trans[z64[i + 1] & 0x7f];
        c = trans[z64[i + 2] & 0x7f];
        zData[j++] = (char)(((a << 2) & 0xfc) | ((b >> 4) & 0x03));
        zData[j++] = (char)(((b << 4) & 0xf0) | ((c >> 2) & 0x0f));
    } else if (i + 1 < n64) {
        a = trans[z64[i] & 0x7f];
        b = trans[z64[i + 1] & 0x7f];
        zData[j++] = (char)(((a << 2) & 0xfc) | ((b >> 4) & 0x03));
    }
    zData[j] = 0;
}

#ifdef ENABLE_TLS
/* This is a self-signed cert in the PEM format that can be used when
** no other certs are available.
**
** NB: Use of this self-signed cert is wildly insecure.  Use for testing
** purposes only.
*/
static const char sslSelfCert[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIDMTCCAhkCFGrDmuJkkzWERP/ITBvzwwI2lv0TMA0GCSqGSIb3DQEBCwUAMFQx\n"
"CzAJBgNVBAYTAlVTMQswCQYDVQQIDAJOQzESMBAGA1UEBwwJQ2hhcmxvdHRlMRMw\n"
"EQYDVQQKDApGb3NzaWwtU0NNMQ8wDQYDVQQDDAZGb3NzaWwwIBcNMjExMjI3MTEz\n"
"MTU2WhgPMjEyMTEyMjcxMTMxNTZaMFQxCzAJBgNVBAYTAlVTMQswCQYDVQQIDAJO\n"
"QzESMBAGA1UEBwwJQ2hhcmxvdHRlMRMwEQYDVQQKDApGb3NzaWwtU0NNMQ8wDQYD\n"
"VQQDDAZGb3NzaWwwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCCbTU2\n"
"6GRQHQqLq7vyZ0OxpAxmgfAKCxt6eIz+jBi2ZM/CB5vVXWVh2+SkSiWEA3UZiUqX\n"
"xZlzmS/CglZdiwLLDJML8B4OiV72oivFH/vJ7+cbvh1dTxnYiHuww7GfQngPrLfe\n"
"fiIYPDk1GTUJHBQ7Ue477F7F8vKuHdVgwktF/JDM6M60aSqlo2D/oysirrb+dlur\n"
"Tlv0rjsYOfq6bLAajoL3qi/vek6DNssoywbge4PfbTgS9g7Gcgncbcet5pvaS12J\n"
"avhFcd4JU4Ity49Hl9S/C2MfZ1tE53xVggRwKz4FPj65M5uymTdcxtjKXtCxIE1k\n"
"KxJxXQh7rIYjm+RTAgMBAAEwDQYJKoZIhvcNAQELBQADggEBAFkdtpqcybAzJN8G\n"
"+ONuUm5sXNbWta7JGvm8l0BTSBcCUtJA3hn16iJqXA9KmLnaF2denC4EYk+KlVU1\n"
"QXxskPJ4jB8A5B05jMijYv0nzCxKhviI8CR7GLEEGKzeg9pbW0+O3vaVehoZtdFX\n"
"z3SsCssr9QjCLiApQxMzW1Iv3od2JXeHBwfVMFrWA1VCEUCRs8OSW/VOqDPJLVEi\n"
"G6wxc4kN9dLK+5S29q3nzl24/qzXoF8P9Re5KBCbrwaHgy+OEEceq5jkmfGFxXjw\n"
"pvVCNry5uAhH5NqbXZampUWqiWtM4eTaIPo7Y2mDA1uWhuWtO6F9PsnFJlQHCnwy\n"
"s/TsrXk=\n"
"-----END CERTIFICATE-----\n";

/* This is the private-key corresponding to the cert above
*/
static const char sslSelfPKey[] =
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQCCbTU26GRQHQqL\n"
"q7vyZ0OxpAxmgfAKCxt6eIz+jBi2ZM/CB5vVXWVh2+SkSiWEA3UZiUqXxZlzmS/C\n"
"glZdiwLLDJML8B4OiV72oivFH/vJ7+cbvh1dTxnYiHuww7GfQngPrLfefiIYPDk1\n"
"GTUJHBQ7Ue477F7F8vKuHdVgwktF/JDM6M60aSqlo2D/oysirrb+dlurTlv0rjsY\n"
"Ofq6bLAajoL3qi/vek6DNssoywbge4PfbTgS9g7Gcgncbcet5pvaS12JavhFcd4J\n"
"U4Ity49Hl9S/C2MfZ1tE53xVggRwKz4FPj65M5uymTdcxtjKXtCxIE1kKxJxXQh7\n"
"rIYjm+RTAgMBAAECggEANfTH1vc8yIe7HRzmm9lsf8jF+II4s2705y2H5qY+cvYx\n"
"nKtZJGOG1X0KkYy7CGoFv5K0cSUl3lS5FVamM/yWIzoIex/Sz2C1EIL2aI5as6ez\n"
"jB6SN0/J+XI8+Vt7186/rHxfdIPpxuzjHbxX3HTpScETNWcLrghbrPxakbTPPxwt\n"
"+x7QlPmmkFNuMfvkzToFf9NdwL++44TeBPOpvD/Lrw+eyqdth9RJPq9cM96plh9V\n"
"HuRqeD8+QNafaXBdSQs3FJK/cDK/vWGKZWIfFVSDbDhwYljkXGijreFjtXQfkkpF\n"
"rl1J87/H9Ee7z8fTD2YXQHl+0/rghAVtac3u54dpQQKBgQC2XG3OEeMrOp9dNkUd\n"
"F8VffUg0ecwG+9L3LCe7U71K0kPmXjV6xNnuYcNQu84kptc5vI8wD23p29LaxdNc\n"
"9m0lcw06/YYBOPkNphcHkINYZTvVJF10mL3isymzMaTtwDkZUkOjL1B+MTiFT/qp\n"
"ARKrTYGJ4HxY7+tUkI5pUmg4PQKBgQC3GA4d1Rz3Pb/RRpcsZgWknKsKhoN36mSn\n"
"xFJ3wPBvVv2B1ltTMzh/+the0ty6clzMrvoLERzRcheDsNrc/j/TUVG8sVdBYJwX\n"
"tMZyFW4NVMOErT/1ukh6jBqIMBo6NJL3EV/AKj0yniksgKOr0/AAduAccnGST8Jd\n"
"SHOdjwvHzwKBgGZBq/zqgNTDuYseHGE07CMgcDWkumiMGv8ozlq3mSR0hUiPOTPP\n"
"YFjQjyIdPXnF6FfiyPPtIvgIoNK2LVAqiod+XUPf152l4dnqcW13dn9BvOxGyPTR\n"
"lWCikFaAFviOWjY9r9m4dU1dslDmySqthFd0TZgPvgps9ivkJ0cdw30NAoGAMC/E\n"
"h1VvKiK2OP27C5ROJ+STn1GHiCfIFd81VQ8SODtMvL8NifgRBp2eFFaqgOdYRQZI\n"
"CGGYlAbS6XXCJCdF5Peh62dA75PdgN+y2pOJQzjrvB9cle9Q4++7i9wdCvSLOTr5\n"
"WDnFoWy+qVexu6crovOmR9ZWzYrwPFy1EOJ010ECgYBl7Q+jmjOSqsVwhFZ0U7LG\n"
"diN+vXhWfn1wfOWd8u79oaqU/Oy7xyKW2p3H5z2KFrBM/vib53Lh4EwFZjcX+jVG\n"
"krAmbL+M/hP7z3TD2UbESAzR/c6l7FU45xN84Lsz5npkR8H/uAHuqLgb9e430Mjx\n"
"YNMwdb8rChHHChNZu6zuxw==\n"
"-----END PRIVATE KEY-----\n";

/*
** Read a PEM certificate from memory and push it into an SSL_CTX.
** Return the number of errors.
*/
static int sslctx_use_cert_from_mem(
  SSL_CTX *ctx,
  const char *pData,
  int nData
){
  BIO *in;
  int rc = 1;
  X509 *x = 0;
  X509 *cert = 0;

  in = BIO_new_mem_buf(pData, nData);
  if( in==0 ) goto end_of_ucfm;
  x = X509_new();
  if( x==0 ) goto end_of_ucfm;
  cert = PEM_read_bio_X509(in, &x, 0, 0);
  if( cert==0 ) goto end_of_ucfm;
  rc = SSL_CTX_use_certificate(ctx, x)<=0;
end_of_ucfm:
  X509_free(x);
  BIO_free(in);
  return rc;
}

/*
** Read a PEM private key from memory and add it to an SSL_CTX.
** Return the number of errors.
*/
static int sslctx_use_pkey_from_mem(
  SSL_CTX *ctx,
  const char *pData,
  int nData
){
  int rc = 1;
  BIO *in;
  EVP_PKEY *pkey = 0;

  in = BIO_new_mem_buf(pData, nData);
  if( in==0 ) goto end_of_upkfm;
  pkey = PEM_read_bio_PrivateKey(in, 0, 0, 0);
  if( pkey==0 ) goto end_of_upkfm;
  rc = SSL_CTX_use_PrivateKey(ctx, pkey)<=0;
  EVP_PKEY_free(pkey);
end_of_upkfm:
  BIO_free(in);
  return rc;
}

/*
** Initialize the SSL library so that it is able to handle
** server-side connections.  Invokes Malfunction() if there are
** any problems (so does not return on error).
**
** If zKeyFile and zCertFile are not NULL, then they are the names
** of disk files that hold the certificate and private-key for the
** server.  If zCertFile is not NULL but zKeyFile is NULL, then
** zCertFile is assumed to be a concatenation of the certificate and
** the private-key in the PEM format.
**
** If zCertFile is "unsafe-builtin" then a built-in self-signed cert
** is used and zKeyFile is ignored.
**
** Error messages may contain the paths to the given files, but this
** function is called before the server starts listening for requests,
** so those will never be sent to clients.
*/
static void ssl_init_server(const char *zCertFile,
                            const char *zKeyFile){
  if( tlsState.isInit==0 ){
    const int useSelfSigned = zCertFile
      && 0==strcmp("unsafe-builtin", zCertFile);
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    tlsState.ctx = SSL_CTX_new(SSLv23_server_method());
    if( tlsState.ctx==0 ){
      ERR_print_errors_fp(stderr);
      Malfunction(501,   /* LOG: Error initializing the SSL Server */
           "Error initializing the SSL server");
    }
#if 0  /* Don't be overly restrictive.  Some people still use older browsers */
    SSL_CTX_set_min_proto_version(tlsState.ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(tlsState.ctx, TLS1_3_VERSION);
    SSL_CTX_set_cipher_list(tlsState.ctx,
       "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384");
    SSL_CTX_set_options(tlsState.ctx,
       SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
#endif
    if( !useSelfSigned && zCertFile && zCertFile[0] ){
      if( SSL_CTX_use_certificate_chain_file(tlsState.ctx,
                                             zCertFile)!=1 ){
        ERR_print_errors_fp(stderr);
        Malfunction(502,  /* LOG: Error loading CERT file */
           "Error loading CERT file \"%s\"", zCertFile);
      }
      if( zKeyFile==0 ) zKeyFile = zCertFile;
      if( SSL_CTX_use_PrivateKey_file(tlsState.ctx, zKeyFile,
                                      SSL_FILETYPE_PEM)<=0 ){
        ERR_print_errors_fp(stderr);
        Malfunction(503,  /* LOG: Error loading private key file */
            "Error loading PRIVATE KEY from file \"%s\"",
            zKeyFile);
      }
    }else if( useSelfSigned ){
      if(sslctx_use_cert_from_mem(tlsState.ctx, sslSelfCert, -1)
         || sslctx_use_pkey_from_mem(tlsState.ctx, sslSelfPKey, -1) ){
        Malfunction(504,  /* LOG: Error loading self-signed cert */
           "Error loading self-signed CERT");
      }
    }else{
      Malfunction(505,"No certificate TLS specified"); /* LOG: No cert */
    }
    if( !SSL_CTX_check_private_key(tlsState.ctx) ){
      Malfunction(506,  /* LOG: private key does not match cert */
           "PRIVATE KEY \"%s\" does not match CERT \"%s\"",
           zKeyFile, zCertFile);
    }
    SSL_CTX_set_mode(tlsState.ctx, SSL_MODE_AUTO_RETRY);
    tlsState.isInit = 2;
  }else{
    assert( tlsState.isInit==2 );
  }
}
#endif /*ENABLE_TLS*/

// 用 SHA256 哈希算法对输入字符串进行哈希计算，并将结果以十六进制字符串的形式返回。
// 返回的字符串存储在静态内存中，每次调用该函数都会覆盖之前的结果。
static char *HashAuthArg(const char *z) {
#ifdef ENABLE_TLS
    EVP_MD_CTX *ctx = NULL;
    int ok = 0;
    int i,j;
    unsigned int n = 0;
    unsigned char digest[32];
    static char h[70];

    ctx = EVP_MD_CTX_new();
    if (!ctx) goto done;
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) !=1) goto done;
    if (EVP_DigestUpdate(ctx, z, strlen(z)) != 1 ) goto done;
    if (EVP_DigestFinal_ex(ctx, digest, &n) != 1 ) goto done;
    ok = 1;
    i = j = 0;
    while( i<64 ){
        unsigned char c = digest[j++];
        h[i++] = "0123456789abcdef"[c>>4];
        h[i++] = "0123456789abcdef"[c&15];
    }
    h[i] = 0;

done:
    EVP_MD_CTX_free(ctx);
    return ok ? h : 0;
#else
    /* Does not work without OpenSSL */
    (void) z;
    return 0;
#endif /* ENABLE_TLS */
}

// 检查基本认证的凭据是否符合 zAuthFile 文件中的要求。返回 1 表示授权成功，返回 0 表示授权失败。
/* + 文件格式说明：
 *   - 空行和以 '#' 开头的行会被忽略。
 *   - "http-redirect" 强制在非 HTTPS 环境下重定向到 HTTPS。
 *   - "https-only" 禁止在 HTTP 环境下访问。
 *   - "user NAME LOGIN:PASSWORD" 检查是否提供了 LOGIN:PASSWORD 的认证凭据，
 *      如果提供了则将 REMOTE_USER 设置为 NAME。LOGIN:PASSWORD 也可以是原始文本的 SHA256 哈希值（如果编译时启用了 ENABLE_TLS）。使用 "--auth-hash" 选项来计算该哈希值。
 *   - "realm TEXT" 设置认证领域为 TEXT。
 *   - "anyone" 允许任何人访问文件，绕过认证。这在与 "http-redirect" 结合使用时非常有用。
 */
static int CheckBasicAuthorization(const char *zAuthFile) {
    FILE *in;
    char *zRealm = "unknown realm";
    char *zLoginPswd;
    char *zName;
    const char *zHash = 0;
    char zLine[2000];

    in = fopen(zAuthFile, "rb");
    if (in == 0) {
        NotFound(150);  /* LOG: Cannot open -auth file */
        return 0;
    }
    if (zAuthArg) Decode64(zAuthArg);
    while (fgets(zLine, sizeof(zLine), in)) {
        char *zFieldName;
        char *zVal;

        zFieldName = GetFirstElement(zLine, &zVal);
        if (zFieldName == 0 || *zFieldName == 0) continue;
        if (zFieldName[0] == '#') continue;
        RemoveNewline(zVal);
        if (strcmp(zFieldName, "realm") == 0) {
            zRealm = StrDup(zVal);
        } else if (strcmp(zFieldName, "user") == 0) {
            if (zAuthArg == 0) continue;
            zName = GetFirstElement(zVal, &zVal);
            zLoginPswd = GetFirstElement(zVal, &zVal);
            if (zLoginPswd == 0) continue;
            if (zAuthArg) {
                if (strcmp(zAuthArg, zLoginPswd) == 0) {
                    zRemoteUser = StrDup(zName);
                    fclose(in);
                    return 1;
                }
                if (zHash == 0) zHash = HashAuthArg(zAuthArg);
                if (zHash && strcmp(zHash, zLoginPswd) == 0) {
                    zRemoteUser = StrDup(zName);
                    fclose(in);
                    return 1;
                }
            }
        } else if (strcmp(zFieldName, "https-only") == 0) {
            if (!g_useHttps) {
                NotFound(160);  /* LOG:  http request on https-only page */
                fclose(in);
                return 0;
            }
        } else if (strcmp(zFieldName, "http-redirect") == 0) {
            if (!g_useHttps) {
                g_zHttpScheme = "https";
                Redirect(zScript, 301, 1, 170); /* LOG: -auth redirect */
                fclose(in);
                return 0;
            }
        } else if (strcmp(zFieldName, "anyone") == 0) {
            fclose(in);
            return 1;
        } else {
            NotFound(180);  /* LOG:  malformed entry in -auth file */
            fclose(in);
            return 0;
        }
    }
    fclose(in);
    NotAuthorized(zRealm);
    return 0;
}

// 文件扩展名与 mime-type 的映射表的 Item 结构
typedef struct MimeTypeDef {
    const char *zSuffix;       /* The file suffix */
    unsigned char size;        /* Length of the suffix */
    unsigned char flags;       /* See the MTF_xxx flags macros */
    const char *zMimetype;     /* The corresponding mimetype */
} MimeTypeDef;

// mimetype 的 flags 字段的标志位，
// 必须与 GetMimeType() 中的值匹配，因为 GetMimeType() 为了节省空间而避免使用宏
#define MTF_NOCGI      0x1   /* Never treat as CGI even if executable */
#define MTF_NOCHARSET  0x2   /* Elide charset=... from Content-Type */

// 根据文件名猜测 mime-type
const MimeTypeDef *GetMimeType(const char *zName, int nName) {
    const char *z;
    int i;
    int first, last;
    int len;
    char zSuffix[20];

    // 根据文件后缀猜测 mime-type 的表，按照后缀字母顺序排序，以便二分查找
    static const MimeTypeDef aMime[] = {
            {"ai",      2, 0x00, "application/postscript"},
            {"aif",     3, 0x02, "audio/x-aiff"},
            {"aifc",    4, 0x02, "audio/x-aiff"},
            {"aiff",    4, 0x02, "audio/x-aiff"},
            {"arj",     3, 0x02, "application/x-arj-compressed"},
            {"asc",     3, 0x00, "text/plain"},
            {"asf",     3, 0x02, "video/x-ms-asf"},
            {"asx",     3, 0x02, "video/x-ms-asx"},
            {"au",      2, 0x02, "audio/ulaw"},
            {"avi",     3, 0x02, "video/x-msvideo"},
            {"bat",     3, 0x02, "application/x-msdos-program"},
            {"bcpio",   5, 0x02, "application/x-bcpio"},
            {"bin",     3, 0x02, "application/octet-stream"},
            {"br",      2, 0x02, "application/x-brotli"},
            {"c",       1, 0x00, "text/plain"},
            {"cc",      2, 0x00, "text/plain"},
            {"ccad",    4, 0x02, "application/clariscad"},
            {"cdf",     3, 0x02, "application/x-netcdf"},
            {"class",   5, 0x02, "application/octet-stream"},
            {"cod",     3, 0x02, "application/vnd.rim.cod"},
            {"com",     3, 0x02, "application/x-msdos-program"},
            {"cpio",    4, 0x02, "application/x-cpio"},
            {"cpt",     3, 0x02, "application/mac-compactpro"},
            {"csh",     3, 0x00, "application/x-csh"},
            {"css",     3, 0x00, "text/css"},
            {"dcr",     3, 0x02, "application/x-director"},
            {"deb",     3, 0x02, "application/x-debian-package"},
            {"dir",     3, 0x02, "application/x-director"},
            {"dl",      2, 0x02, "video/dl"},
            {"dms",     3, 0x02, "application/octet-stream"},
            {"doc",     3, 0x02, "application/msword"},
            {"drw",     3, 0x02, "application/drafting"},
            {"dvi",     3, 0x02, "application/x-dvi"},
            {"dwg",     3, 0x02, "application/acad"},
            {"dxf",     3, 0x02, "application/dxf"},
            {"dxr",     3, 0x02, "application/x-director"},
            {"eps",     3, 0x00, "application/postscript"},
            {"etx",     3, 0x00, "text/x-setext"},
            {"exe",     3, 0x02, "application/octet-stream"},
            {"ez",      2, 0x00, "application/andrew-inset"},
            {"f",       1, 0x00, "text/plain"},
            {"f90",     3, 0x00, "text/plain"},
            {"fli",     3, 0x02, "video/fli"},
            {"flv",     3, 0x02, "video/flv"},
            {"gif",     3, 0x02, "image/gif"},
            {"gl",      2, 0x02, "video/gl"},
            {"gtar",    4, 0x02, "application/x-gtar"},
            {"gz",      2, 0x02, "application/x-gzip"},
            {"hdf",     3, 0x02, "application/x-hdf"},
            {"hh",      2, 0x00, "text/plain"},
            {"hqx",     3, 0x00, "application/mac-binhex40"},
            {"h",       1, 0x00, "text/plain"},
            {"htm",     3, 0x00, "text/html"},
            {"html",    4, 0x00, "text/html"},
            {"ice",     3, 0x00, "x-conference/x-cooltalk"},
            {"ief",     3, 0x02, "image/ief"},
            {"iges",    4, 0x00, "model/iges"},
            {"igs",     3, 0x00, "model/iges"},
            {"ips",     3, 0x00, "application/x-ipscript"},
            {"ipx",     3, 0x00, "application/x-ipix"},
            {"jad",     3, 0x00, "text/vnd.sun.j2me.app-descriptor"},
            {"jar",     3, 0x02, "application/java-archive"},
            {"jp2",     3, 0x02, "image/jp2"},
            {"jpe",     3, 0x02, "image/jpeg"},
            {"jpeg",    4, 0x02, "image/jpeg"},
            {"jpg",     3, 0x02, "image/jpeg"},
            {"js",      2, 0x00, "text/javascript"},
            /* application/javascript is commonly used for JS, but the
              ** HTML spec says text/javascript is correct:
              ** https://html.spec.whatwg.org/multipage/scripting.html
              ** #scriptingLanguages:javascript-mime-type */
            {"json",    4, 0x00, "application/json"},
            {"kar",     3, 0x02, "audio/midi"},
            {"latex",   5, 0x00, "application/x-latex"},
            {"lha",     3, 0x02, "application/octet-stream"},
            {"lsp",     3, 0x00, "application/x-lisp"},
            {"lzh",     3, 0x02, "application/octet-stream"},
            {"m",       1, 0x00, "text/plain"},
            {"m3u",     3, 0x02, "audio/x-mpegurl"},
            {"man",     3, 0x00, "application/x-troff-man"},
            {"md",      2, 0x00, "text/plain"},
            {"me",      2, 0x00, "application/x-troff-me"},
            {"mesh",    4, 0x00, "model/mesh"},
            {"mid",     3, 0x02, "audio/midi"},
            {"midi",    4, 0x02, "audio/midi"},
            {"mif",     3, 0x00, "application/x-mif"},
            {"mime",    4, 0x00, "www/mime"},
            {"mjs",     3, 0x00, "text/javascript" /*ES6 module*/   },
            {"movie",   5, 0x00, "video/x-sgi-movie"},
            {"mov",     3, 0x02, "video/quicktime"},
            {"mp2",     3, 0x02, "audio/mpeg"},
            {"mp3",     3, 0x02, "audio/mpeg"},
            {"mpeg",    4, 0x02, "video/mpeg"},
            {"mpe",     3, 0x02, "video/mpeg"},
            {"mpga",    4, 0x02, "audio/mpeg"},
            {"mpg",     3, 0x02, "video/mpeg"},
            {"ms",      2, 0x00, "application/x-troff-ms"},
            {"msh",     3, 0x00, "model/mesh"},
            {"nc",      2, 0x00, "application/x-netcdf"},
            {"oda",     3, 0x02, "application/oda"},
            {"ogg",     3, 0x02, "application/ogg"},
            {"ogm",     3, 0x02, "application/ogg"},
            {"pbm",     3, 0x00, "image/x-portable-bitmap"},
            {"pdb",     3, 0x00, "chemical/x-pdb"},
            {"pdf",     3, 0x02, "application/pdf"},
            {"pgm",     3, 0x00, "image/x-portable-graymap"},
            {"pgn",     3, 0x00, "application/x-chess-pgn"},
            {"pgp",     3, 0x00, "application/pgp"},
            {"pl",      2, 0x00, "application/x-perl"},
            {"pm",      2, 0x00, "application/x-perl"},
            {"png",     3, 0x02, "image/png"},
            {"pnm",     3, 0x02, "image/x-portable-anymap"},
            {"pot",     3, 0x02, "application/mspowerpoint"},
            {"ppm",     3, 0x02, "image/x-portable-pixmap"},
            {"pps",     3, 0x02, "application/mspowerpoint"},
            {"ppt",     3, 0x02, "application/mspowerpoint"},
            {"ppz",     3, 0x02, "application/mspowerpoint"},
            {"pre",     3, 0x02, "application/x-freelance"},
            {"prt",     3, 0x00, "application/pro_eng"},
            {"ps",      2, 0x00, "application/postscript"},
            {"qt",      2, 0x02, "video/quicktime"},
            {"ra",      2, 0x02, "audio/x-realaudio"},
            {"ram",     3, 0x02, "audio/x-pn-realaudio"},
            {"rar",     3, 0x02, "application/x-rar-compressed"},
            {"ras",     3, 0x00, "image/cmu-raster"},
            {"rgb",     3, 0x00, "image/x-rgb"},
            {"rm",      2, 0x00, "audio/x-pn-realaudio"},
            {"roff",    4, 0x00, "application/x-troff"},
            {"rpm",     3, 0x02, "audio/x-pn-realaudio-plugin"},
            {"rtf",     3, 0x00, "text/rtf"},
            {"rtx",     3, 0x00, "text/richtext"},
            {"scm",     3, 0x02, "application/x-lotusscreencam"},
            {"set",     3, 0x00, "application/set"},
            {"sgml",    4, 0x00, "text/sgml"},
            {"sgm",     3, 0x00, "text/sgml"},
            {"sh",      2, 0x00, "application/x-sh"},
            {"shar",    4, 0x00, "application/x-shar"},
            {"silo",    4, 0x00, "model/mesh"},
            {"sit",     3, 0x02, "application/x-stuffit"},
            {"skd",     3, 0x00, "application/x-koan"},
            {"skm",     3, 0x00, "application/x-koan"},
            {"skp",     3, 0x00, "application/x-koan"},
            {"skt",     3, 0x00, "application/x-koan"},
            {"smi",     3, 0x00, "application/smil"},
            {"smil",    4, 0x00, "application/smil"},
            {"snd",     3, 0x02, "audio/basic"},
            {"sol",     3, 0x00, "application/solids"},
            {"spl",     3, 0x00, "application/x-futuresplash"},
            {"sql",     3, 0x00, "application/sql"},
            {"src",     3, 0x00, "application/x-wais-source"},
            {"step",    4, 0x00, "application/STEP"},
            {"stl",     3, 0x00, "application/SLA"},
            {"stp",     3, 0x00, "application/STEP"},
            {"sv4cpio", 7, 0x00, "application/x-sv4cpio"},
            {"sv4crc",  6, 0x00, "application/x-sv4crc"},
            {"svg",     3, 0x00, "image/svg+xml"},
            {"swf",     3, 0x02, "application/x-shockwave-flash"},
            {"t",       1, 0x00, "application/x-troff"},
            {"tar",     3, 0x02, "application/x-tar"},
            {"tcl",     3, 0x00, "application/x-tcl"},
            {"tex",     3, 0x00, "application/x-tex"},
            {"texi",    4, 0x00, "application/x-texinfo"},
            {"texinfo", 7, 0x00, "application/x-texinfo"},
            {"tgz",     3, 0x02, "application/x-tar-gz"},
            {"tiff",    4, 0x00, "image/tiff"},
            {"tif",     3, 0x00, "image/tiff"},
            {"tr",      2, 0x00, "application/x-troff"},
            {"tsi",     3, 0x02, "audio/TSP-audio"},
            {"tsp",     3, 0x00, "application/dsptype"},
            {"tsv",     3, 0x00, "text/tab-separated-values"},
            {"txt",     3, 0x00, "text/plain"},
            {"unv",     3, 0x00, "application/i-deas"},
            {"ustar",   5, 0x00, "application/x-ustar"},
            {"vcd",     3, 0x00, "application/x-cdlink"},
            {"vda",     3, 0x00, "application/vda"},
            {"viv",     3, 0x02, "video/vnd.vivo"},
            {"vivo",    4, 0x02, "video/vnd.vivo"},
            {"vrml",    4, 0x00, "model/vrml"},
            {"vsix",    4, 0x00, "application/vsix"},
            {"wasm",    4, 0x03, "application/wasm"},
            {"wav",     3, 0x02, "audio/x-wav"},
            {"wax",     3, 0x02, "audio/x-ms-wax"},
            {"wiki",    4, 0x00, "application/x-fossil-wiki"},
            {"wma",     3, 0x02, "audio/x-ms-wma"},
            {"wmv",     3, 0x02, "video/x-ms-wmv"},
            {"wmx",     3, 0x02, "video/x-ms-wmx"},
            {"woff",    4, 0x02, "font/woff"},
            {"woff2",   5, 0x02, "font/woff2"},
            {"wrl",     3, 0x00, "model/vrml"},
            {"wvx",     3, 0x02, "video/x-ms-wvx"},
            {"xbm",     3, 0x00, "image/x-xbitmap"},
            {"xhtml",   5, 0x00, "application/xhtml+xml"},
            {"xlc",     3, 0x02, "application/vnd.ms-excel"},
            {"xll",     3, 0x02, "application/vnd.ms-excel"},
            {"xlm",     3, 0x02, "application/vnd.ms-excel"},
            {"xls",     3, 0x02, "application/vnd.ms-excel"},
            {"xlw",     3, 0x02, "application/vnd.ms-excel"},
            {"xml",     3, 0x00, "text/xml"},
            {"xpm",     3, 0x00, "image/x-xpixmap"},
            {"xsl",     3, 0x00, "text/xml"},
            {"xslt",    4, 0x00, "text/xml"},
            {"xwd",     3, 0x00, "image/x-xwindowdump"},
            {"xyz",     3, 0x00, "chemical/x-pdb"},
            {"zip",     3, 0x02, "application/zip"},
    };

    for (i = nName - 1; i > 0 && zName[i] != '.'; i--) {}
    z = &zName[i + 1];
    len = nName - i;
    if (len < (int) sizeof(zSuffix) - 1) {
        strcpy(zSuffix, z);
        for (i = 0; zSuffix[i]; i++) zSuffix[i] = (char)tolower(zSuffix[i]);
        first = 0;
        last = sizeof(aMime) / sizeof(aMime[0]);
        while (first <= last) {
            int c;
            i = (first + last) / 2;
            c = strcmp(zSuffix, aMime[i].zSuffix);
            if (c == 0) return &aMime[i];
            if (c < 0) {
                last = i - 1;
            } else {
                first = i + 1;
            }
        }
    }
    return 0;
}

// 下面的表格包含了所有允许在 URL 中查询参数和片段之前的部分中使用的字符的
// 允许的字符包括：0-9a-zA-Z,-./:_~
// 不允许的字符包括：!"#$%&'()*+;<=>?@[\]^`{|}
static const char allowedInName[] = {
/*  x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xa  xb  xc  xd  xe  xf */
/* 0x */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 1x */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 2x */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
/* 3x */   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
/* 4x */   0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 5x */   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
/* 6x */   0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 7x */   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0,
/* 8x */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 9x */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* Ax */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* Bx */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* Cx */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* Dx */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* Ex */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* Fx */   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// 移除输入字符串 z[] 中的所有不允许的字符。将任何不允许的字符转换为 "_"。
/* + 注意，三个字符序列 "%XX"（其中 X 是任何字节）被转换为单个 "_" 字符。
 | 返回转换的字符数。一个 "%XX" -> "_" 转换计为一个字符。
*/
static int sanitizeString(char *z) {
    int nChange = 0;
    while (*z) {
        if (!allowedInName[*(unsigned char *) z]) {
            char cNew = '_';
            if (*z == '%' && z[1] != 0 && z[2] != 0) {
                int i;
                if (z[1] == '2') {
                    if (z[2] == 'e' || z[2] == 'E') cNew = '.';
                    if (z[2] == 'f' || z[2] == 'F') cNew = '/';
                }
                for (i = 3; (z[i - 2] = z[i]) != 0; i++) {}
            }
            *z = cNew;
            nChange++;
        }
        z++;
    }
    return nChange;
}

// 计算字符串中 "/" 字符的数量
static int countSlashes(const char *z) {
    int n = 0;
    while (*z) if (*(z++) == '/') n++;
    return n;
}

#ifdef ENABLE_TLS
                                                                                                                        /*
** Create a new server-side codec.  The argument is the socket's file
** descriptor from which the codec reads and writes. The returned
** memory must eventually be passed to tls_close_server().
*/
static void *tls_new_server(int iSocket){
      TlsServerConn *pServer = malloc(sizeof(*pServer));
      BIO *b = pServer ? BIO_new_socket(iSocket, 0) : NULL;
      if( NULL==b ){
          Malfunction(507,"Cannot allocate TlsServerConn."); /* LOG: TlsServerConn */
      }
      assert(NULL!=tlsState.ctx);
      pServer->ssl = SSL_new(tlsState.ctx);
      pServer->bio = b;
      pServer->iSocket = iSocket;
      SSL_set_bio(pServer->ssl, b, b);
      SSL_accept(pServer->ssl);
      return (void*)pServer;
}

/*
** Close a server-side code previously returned from tls_new_server().
*/
static void tls_close_server(void *pServerArg) {
    TlsServerConn *pServer = (TlsServerConn*)pServerArg;
    SSL_free(pServer->ssl);
    memset(pServer, 0, sizeof(TlsServerConn));
    free(pServer);
}

  /*
  ** Shutting down TLS can lead to spurious hung processes on some
  ** platforms/builds.  See the long discussion on this at:
  ** https://sqlite.org/althttpd/forumpost/4dc31619341ce947
  */
static void tls_atexit(void){
    if( inSignalHandler==0 && tlsState.sslCon!=0 ){
        tls_close_server(tlsState.sslCon);
        tlsState.sslCon = NULL;
    }
}
#endif /* ENABLE_TLS */

// 执行和 fgets() 一样的功能
/* + 从输入流 in 中读取一行文本到 s[] 中，确保 s[] 以 null 结尾。s[] 的大小为 size 字节，因此最多读取 size-1 字节。
 | 成功时，返回指向 s[] 的指针，输入结束时返回 NULL
 | 对于内置 TLS 模式，最后一个参数被忽略，并改为从 TLS 连接中读取
*/
/*
** 从输入流读取一行
*/
char *althttpd_fgets(char *s, int size, FILE *in) {

    if (g_useHttps != 2) return fgets(s, size, in);

#ifdef ENABLE_TLS
    assert(NULL!=tlsState.sslCon);
  return tls_gets(tlsState.sslCon, s, size);
#else
    Malfunction(508, "SSL not available"); /* LOG: SSL not available */
    return NULL;
#endif
}

// 行和 fread() 一样的功能
/* + 对于内置 TLS 模式，会使用 libssl 来读取数据（在这种情况下，最后一个参数被忽略）
 |  注意目标缓冲区必须至少为 (sz*nmemb) 字节。
*/
static size_t althttpd_fread(void *tgt, size_t sz, size_t nmemb, FILE *in) {

    if (g_useHttps != 2) return fread(tgt, sz, nmemb, in);

#ifdef ENABLE_TLS
    assert(NULL!=tlsState.sslCon);
    return tls_read_server(tlsState.sslCon, tgt, sz*nmemb);
#else
    Malfunction(509, "SSL not available"); /* LOG: SSL not available */
    return 0;
#endif
}

// 执行和 fwrite() 一样的功能
// + 对于内置 TLS 模式，会使用 libssl 来写入数据（在这种情况下，最后一个参数被忽略）
static size_t althttpd_fwrite(
        void const *src,          /* Buffer containing content to write */
        size_t sz,                /* Size of each element in the buffer */
        size_t nmemb,             /* Number of elements to write */
        FILE *out                 /* Write on this stream */
) {

    if (g_useHttps != 2) return fwrite(src, sz, nmemb, out);

#ifdef ENABLE_TLS
    assert(NULL!=tlsState.sslCon);
    return tls_write_server(tlsState.sslCon, src, sz*nmemb);
#else
    Malfunction(510, "SSL not available"); /* LOG: SSL not available */
    return 0;
#endif
}

// 执行和 fflush() 一样的功能
// + 对于内置 TLS 模式，执行空操作
static void althttpd_fflush(FILE *f) {

    if (g_useHttps != 2) fflush(f);
}

/*
** Transfer nXfer bytes from in to out, after first discarding
** nSkip bytes from in.  Increment the nOut global variable
** according to the number of bytes transferred.
**
** When running in built-in TLS mode the 2nd argument is ignored and
** output is instead sent via the TLS connection.
*/
static void xferBytes(FILE *in, FILE *out, ssize_t nXfer, ssize_t nSkip) {

    size_t n;
    size_t got;
    char zBuf[16384];
    while (nSkip > 0) {
        n = nSkip;
        if (n > sizeof(zBuf)) n = sizeof(zBuf);
        got = fread(zBuf, 1, n, in);
        if (got == 0) break;
        nSkip -= (ssize_t)got;
    }
    while (nXfer > 0) {
        n = nXfer;
        if (n > sizeof(zBuf)) n = sizeof(zBuf);
        got = fread(zBuf, 1, n, in);
        if (got == 0) break;
        althttpd_fwrite(zBuf, got, 1, out);
        nOut += got;
        nXfer -= (ssize_t)got;
    }
}

/*
** Send a file from buildins (virtual file system) as the reply.
** Supports direct gzip delivery if client accepts gzip encoding.
**
** Return 1 to omit making a log entry for the reply.
*/
static int SendBuildins(
        const char *csFile,           /* Virtual file path (for logging/MIME) */
        int iLenFile,                 /* Length of the csFile name in bytes */
        buildin_file_info_st *info    /* buildins file info */
) {
    const char *csContentType;
    time_t t = time(NULL);  // 使用当前时间（buildins 没有修改时间）
    char zETag[100];
    const MimeTypeDef *pMimeType;
    int bAddCharset = 1;
    const char *zEncoding = 0;
    size_t content_length = 0;
    const uint8_t *content_data = NULL;
    void *decompressed_data = NULL;

    // 获取 MIME 类型
    pMimeType = GetMimeType(csFile, iLenFile);
    csContentType = pMimeType
                   ? pMimeType->zMimetype : "application/octet-stream";
    if (pMimeType && (MTF_NOCHARSET & pMimeType->flags)) {
        bAddCharset = 0;
    }

    // 生成 ETag（基于文件路径哈希和原始大小）
    sprintf(zETag, "b%xz%x", info->id, info->orig_sz);

    // 检查 ETag 缓存
    if (CompareEtags(zIfNoneMatch, zETag) == 0
        || (zIfModifiedSince != 0
            && (t = ParseRfc822Date(zIfModifiedSince)) > 0
            && t >= time(NULL))  // buildins 文件视为从不修改
    ) {
        StartResponse("304 Not Modified");
        nOut += DateTag("Last-Modified", time(NULL));
        nOut += althttpd_printf("Cache-Control: max-age=%d" CRLF, g_mxAge);
        nOut += althttpd_printf("ETag: \"%s\"" CRLF CRLF, zETag);
        fflush(stdout);
        MakeLogEntry(0, 470);  /* LOG: ETag Cache Hit */
        return 1;
    }

    // 决定是否发送压缩数据
    if (rangeEnd <= 0 && zAcceptEncoding && strstr(zAcceptEncoding, "gzip") != 0) {
        // 客户端支持 gzip，直接发送压缩数据
        zEncoding = "gzip";
        content_data = info->comp;
        content_length = info->comp_sz;
    } else {
        // 客户端不支持 gzip 或请求了 Range，需要解压
        decompressed_data = buildins_decompressed(info);
        if (!decompressed_data) {
            NotFound(481); /* LOG: buildins decompression failed */
        }
        content_data = (const uint8_t *)decompressed_data;
        content_length = info->orig_sz;
    }

    // 处理 Range请求
    if (rangeEnd > 0 && rangeStart < content_length) {
        StartResponse("206 Partial Content");
        if (rangeEnd >= content_length) {
            rangeEnd = content_length - 1;
        }
        nOut += althttpd_printf("Content-Range: bytes %lld-%lld/%zu" CRLF,
                                (long long) rangeStart, (long long) rangeEnd,
                                content_length);
        content_length = rangeEnd + 1 - rangeStart;
        content_data += rangeStart;
    } else {
        StartResponse("200 OK");
        rangeStart = 0;
    }

    // 发送 HTTP 头部
    nOut += DateTag("Last-Modified", time(NULL));
    if (g_enableSAB) {
        nOut += althttpd_printf("Cross-Origin-Opener-Policy: same-origin" CRLF);
        nOut += althttpd_printf("Cross-Origin-Embedder-Policy: require-corp" CRLF);
    }
    nOut += althttpd_printf("Cache-Control: max-age=%d" CRLF, g_mxAge);
    nOut += althttpd_printf("ETag: \"%s\"" CRLF, zETag);
    nOut += althttpd_printf("Content-type: %s%s" CRLF, csContentType,
                            bAddCharset ? "; charset=utf-8" : "");
    if (zEncoding) {
        nOut += althttpd_printf("Content-encoding: %s" CRLF, zEncoding);
    }
    nOut += althttpd_printf("Content-length: %zu" CRLF CRLF, content_length);
    fflush(stdout);

    // HEAD 请求只发送头部
    if (strcmp(zMethod, "HEAD") == 0) {
        MakeLogEntry(0, 2); /* LOG: Normal HEAD reply */
        fflush(stdout);
        return 1;
    }

    // 发送数据
    althttpd_fwrite(content_data, 1, content_length, stdout);
    nOut += content_length;

    return 0;
}

/*
** Send the text of the file named by csFile as the reply.  Use the
** suffix on the end of the csFile name to determine the mimetype.
**
** Return 1 to omit making a log entry for the reply.
*/
static int SendFile(
        const char *csFile,      /* Name of the file to send */
        int iLenFile,            /* Length of the zFile name in bytes */
        struct stat *pStat      /* Result of a stat() against zFile */
) {
    const char *csContentType;
    time_t t;
    FILE *in;
    size_t szFilename;
    char zETag[100];
    const MimeTypeDef *pMimeType;
    int bAddCharset = 1;
    const char *zEncoding = 0;
    struct stat statbuf;
    char zGzFilename[2000];

    pMimeType = GetMimeType(csFile, iLenFile);
    csContentType = pMimeType
                   ? pMimeType->zMimetype : "application/octet-stream";
    if (pMimeType && (MTF_NOCHARSET & pMimeType->flags)) {
        bAddCharset = 0;
    }
    if (zPostData) {
        free(zPostData);
        zPostData = 0;
    }
    sprintf(zETag, "m%xs%x", (int) pStat->st_mtime, (int) pStat->st_size);
    if (CompareEtags(zIfNoneMatch, zETag) == 0
        || (zIfModifiedSince != 0
            && (t = ParseRfc822Date(zIfModifiedSince)) > 0
            && t >= pStat->st_mtime)
            ) {
        StartResponse("304 Not Modified");
        nOut += DateTag("Last-Modified", pStat->st_mtime);
        nOut += althttpd_printf("Cache-Control: max-age=%d" CRLF, g_mxAge);
        nOut += althttpd_printf("ETag: \"%s\"" CRLF CRLF, zETag);
        fflush(stdout);
        MakeLogEntry(0, 470);  /* LOG: ETag Cache Hit */
        return 1;
    }
    if (rangeEnd <= 0
        && zAcceptEncoding) {
        szFilename = strlen(csFile);
        if (szFilename < sizeof(zGzFilename) - 10) {
            memcpy(zGzFilename, csFile, szFilename);
            if (strstr(zAcceptEncoding, "br") != 0) {
                memcpy(zGzFilename + szFilename, ".br", 4);
                if (access(zGzFilename, R_OK) == 0) {
                    memset(&statbuf, 0, sizeof(statbuf));
                    if (stat(zGzFilename, &statbuf) == 0) {
                        zEncoding = "br";
                        csFile = zGzFilename;
                        pStat = &statbuf;
                    }
                }
            }
            if (!zEncoding && strstr(zAcceptEncoding, "gzip") != 0) {
                memcpy(zGzFilename + szFilename, ".gz", 4);
                if (access(zGzFilename, R_OK) == 0) {
                    memset(&statbuf, 0, sizeof(statbuf));
                    if (stat(zGzFilename, &statbuf) == 0) {
                        zEncoding = "gzip";
                        csFile = zGzFilename;
                        pStat = &statbuf;
                    }
                }
            }
        }
    }
    in = fopen(csFile, "rb");
    if (in == 0) NotFound(480); /* LOG: fopen() failed for static content */
    if (rangeEnd > 0 && rangeStart < pStat->st_size) {
        StartResponse("206 Partial Content");
        if (rangeEnd >= pStat->st_size) {
            rangeEnd = pStat->st_size - 1;
        }
        nOut += althttpd_printf("Content-Range: bytes %lld-%lld/%llu" CRLF,
                                (long long) rangeStart, (long long) rangeEnd,
                                (unsigned long long) pStat->st_size);
        pStat->st_size = rangeEnd + 1 - rangeStart;
    } else {
        StartResponse("200 OK");
        rangeStart = 0;
    }
    nOut += DateTag("Last-Modified", pStat->st_mtime);
    if (g_enableSAB) {
        /* The following two HTTP reply headers are required if javascript
    ** is to make use of SharedArrayBuffer */
        nOut += althttpd_printf("Cross-Origin-Opener-Policy: same-origin" CRLF);
        nOut += althttpd_printf("Cross-Origin-Embedder-Policy: require-corp" CRLF);
    }
    nOut += althttpd_printf("Cache-Control: max-age=%d" CRLF, g_mxAge);
    nOut += althttpd_printf("ETag: \"%s\"" CRLF, zETag);
    nOut += althttpd_printf("Content-type: %s%s" CRLF, csContentType,
                            bAddCharset ? "; charset=utf-8" : "");
    if (zEncoding) {
        nOut += althttpd_printf("Content-encoding: %s" CRLF, zEncoding);
    }
    nOut += althttpd_printf("Content-length: %llu" CRLF CRLF,
                            (unsigned long long) pStat->st_size);
    fflush(stdout);
    if (strcmp(zMethod, "HEAD") == 0) {
        MakeLogEntry(0, 2); /* LOG: Normal HEAD reply */
        fclose(in);
        fflush(stdout);
        return 1;
    }
#ifdef linux
    if( 2!=g_useHttps && (unsigned long long)pStat->st_size < (unsigned long long)0x7ffff000 /*max sendfile() size*/) {
        off_t offset = rangeStart;
        nOut += sendfile(fileno(stdout), fileno(in), &offset, pStat->st_size);
    } else
#endif
    {
        xferBytes(in, stdout, pStat->st_size, rangeStart);
    }
    fclose(in);
    return 0;
}

/*
** Streams all contents from in to out. If in TLS mode, the
** output stream is ignored and the output instead goes
** to the TLS channel.
*/
static void stream_file(FILE *const in, FILE *const out) {
    enum {
        STREAMBUF_SIZE = 1024 * 4
    };
    char streamBuf[STREAMBUF_SIZE];
    size_t n;
    while ((n = fread(streamBuf, 1, sizeof(STREAMBUF_SIZE), in))) {
        althttpd_fwrite(streamBuf, 1, n, out);
    }
}

/*
** A CGI or SCGI script has run and is sending its reply back across
** the channel "in".  Process this reply into an appropriate HTTP reply.
** Close the "in" channel when done.
**
** If isNPH is true, the input is assumed to be from a
** non-parsed-header CGI and is passed on as-is to stdout or the TLS
** layer, depending on the connection state.
*/
static void CgiHandleReply(FILE *in, int isNPH) {
    int seenContentLength = 0;   /* True if Content-length: header seen */
    int contentLength = 0;       /* The content length */
    size_t nRes = 0;             /* Bytes of payload */
    size_t nMalloc = 0;          /* Bytes of space allocated to aRes */
    char *aRes = 0;              /* Payload */
    int c;                       /* Next character from in */
    char *z;                     /* Pointer to something inside of zLine */
    int iStatus = 0;             /* Reply status code */
    int seenReply = 0;           /* True if any reply text has been seen */
    char zLine[1000];            /* One line of reply from the CGI script */

    /* Set a 1-hour timeout, so that we can implement Hanging-GET or
  ** long-poll style CGIs.  The RLIMIT_CPU will serve as a safety
  ** to help prevent a run-away CGI */
    SetTimeout(60 * 60, 800); /* LOG: CGI Handler timeout */

    if (isNPH) {
        /*
    ** Non-parsed-header output: simply pipe it out as-is. We
    ** need to go through this routine, instead of simply exec()'ing,
    ** in order to go through the TLS output channel.
    */
        stream_file(in, stdout);
        fclose(in);
        return;
    }

    while (fgets(zLine, sizeof(zLine), in) && !isspace((unsigned char) zLine[0])) {
        seenReply = 1;
        if (strncasecmp(zLine, "Location:", 9) == 0) {
            StartResponse("302 Redirect");
            RemoveNewline(zLine);
            z = &zLine[10];
            while (isspace(*(unsigned char *) z)) { z++; }
            nOut += althttpd_printf("Location: %s" CRLF, z);
            rangeEnd = 0;
        } else if (strncasecmp(zLine, "Status:", 7) == 0) {
            int i;
            for (i = 7; isspace((unsigned char) zLine[i]); i++) {}
            strncpy(zReplyStatus, &zLine[i], 3);
            zReplyStatus[3] = 0;
            iStatus = (int)strtol(zReplyStatus, NULL, 10);
            if (rangeEnd == 0 || (iStatus != 200 && iStatus != 206)) {
                if (iStatus == 418) {
                    /* If a CGI returns a status code of 418 ("I'm a teapot", rfc2324)
          ** that is a signal from the CGI to althttpd that the request was
          ** abuse - for example an attempted SQL injection attack or
          ** similar. */
                    BlockIPAddress();
                    ServiceUnavailable(903);  /* LOG: CGI reports abuse */
                }
                nOut += althttpd_printf("%s %s", zProtocol, &zLine[i]);
                rangeEnd = 0;
                statusSent = 1;
            }
        } else if (strncasecmp(zLine, "Content-length:", 15) == 0) {
            seenContentLength = 1;
            contentLength = (int)strtol(zLine + 15, NULL, 10);
        } else if (strncasecmp(zLine, "X-Robot:", 8) == 0) {
            isRobot = (strtol(&zLine[8], NULL, 10) != 0) + 1;
        } else {
            size_t nLine = strlen(zLine);
            if (nRes + nLine >= nMalloc) {
                nMalloc += nMalloc + nLine * 2;
                aRes = realloc(aRes, nMalloc + 1);
                if (aRes == 0) {
                    Malfunction(600, /* LOG: OOM */
                                "Out of memory: %llu bytes",
                                (long long unsigned int) nMalloc);
                }
            }
            memcpy(aRes + nRes, zLine, nLine);
            nRes += nLine;
        }
    }
    if (!seenReply) CgiError();

    /* Copy everything else thru without change or analysis.
  */
    if (rangeEnd > 0 && seenContentLength && rangeStart < contentLength) {
        StartResponse("206 Partial Content");
        if (rangeEnd >= contentLength) {
            rangeEnd = contentLength - 1;
        }
        nOut += althttpd_printf("Content-Range: bytes %lld-%lld/%llu" CRLF,
                                (long long) rangeStart, (long long) rangeEnd,
                                (unsigned long long) contentLength);
        contentLength = (int)(rangeEnd + 1 - rangeStart);
    } else {
        StartResponse("200 OK");
    }
    if (nRes > 0) {
        aRes[nRes] = 0;
        althttpd_fwrite(aRes, nRes, 1, stdout);
        nOut += nRes;
        nRes = 0;
    }
    if (iStatus == 304) {
        nOut += althttpd_printf(CRLF CRLF);
    } else if (seenContentLength) {
        nOut += althttpd_printf("Content-length: %d" CRLF CRLF, contentLength);
        xferBytes(in, stdout, contentLength, rangeStart);
    } else {
        while ((c = getc(in)) != EOF) {
            if (nRes >= nMalloc) {
                nMalloc = nMalloc * 2 + 1000;
                aRes = realloc(aRes, nMalloc + 1);
                if (aRes == 0) {
                    Malfunction(610, /* LOG: OOM */
                                "Out of memory: %llu bytes",
                                (long long unsigned int) nMalloc);
                }
            }
            aRes[nRes++] = (char)c;
        }
        if (nRes) {
            aRes[nRes] = 0;
            nOut += althttpd_printf("Content-length: %d" CRLF CRLF, (int) nRes);
            nOut += althttpd_fwrite(aRes, nRes, 1, stdout);
        } else {
            nOut += althttpd_printf("Content-length: 0" CRLF CRLF);
        }
    }
    free(aRes);
    fclose(in);
}

// 发送 SCGI 请求到 csFile 标识的主机，并处理主机的回复。
static void SendScgiRequest(const char *csFile, const char *scScript) {
    FILE *in;
    FILE *s;
    char *z;
    char *zHost;
    char *zPort = 0;
    char *zRelight = 0;
    char *zFallback = 0;
    int rc;
    int iSocket = -1;
    struct addrinfo hints;
    struct addrinfo *ai = 0;
    struct addrinfo *p;
    char *zHdr;
    size_t nHdr = 0;
    size_t nHdrAlloc;
    int i;
    char zLine[1000];
    char zExtra[1000];
    in = fopen(csFile, "rb");
    if (in == 0) {
        Malfunction(700, "cannot open \"%s\"\n", csFile); /* LOG: cannot open file */
    }
    if (fgets(zLine, sizeof(zLine) - 1, in) == 0) {
        Malfunction(701, "cannot read \"%s\"\n", csFile); /* LOG: cannot read file */
    }
    if (strncmp(zLine, "SCGI ", 5) != 0) {
        Malfunction(702, /* LOG: bad SCGI spec */
                    "misformatted SCGI spec \"%s\"\n", csFile);
    }
    z = zLine + 5;
    zHost = GetFirstElement(z, &z);
    zPort = GetFirstElement(z, 0);
    if (zHost == 0 || zHost[0] == 0 || zPort == 0 || zPort[0] == 0) {
        Malfunction(703, /* LOG: bad SCGI spec (2) */
                    "misformatted SCGI spec \"%s\"\n", csFile);
    }
    while (fgets(zExtra, sizeof(zExtra) - 1, in)) {
        char *zCmd = GetFirstElement(zExtra, &z);
        if (zCmd == 0) continue;
        if (zCmd[0] == '#') continue;
        RemoveNewline(z);
        if (strcmp(zCmd, "relight:") == 0) {
            free(zRelight);
            zRelight = StrDup(z);
            continue;
        }
        if (strcmp(zCmd, "fallback:") == 0) {
            free(zFallback);
            zFallback = StrDup(z);
            continue;
        }
        Malfunction(704, /* LOG: Unrecognized line in SCGI spec */
                    "unrecognized line in SCGI spec: \"%s %s\"\n",
                    zCmd, z ? z : "");
    }
    fclose(in);
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    rc = getaddrinfo(zHost, zPort, &hints, &ai);
    if (rc) {
        Malfunction(705, /* LOG: Cannot resolve SCGI server name */
                    "cannot resolve SCGI server name %s:%s\n%s\n",
                    zHost, zPort, gai_strerror(rc));
    }
    while (1) {  /* Exit via break */
        for (p = ai; p; p = p->ai_next) {
            iSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (iSocket < 0) continue;
            if (connect(iSocket, p->ai_addr, p->ai_addrlen) >= 0) break;
            close(iSocket);
        }
        if (iSocket < 0 || (s = fdopen(iSocket, "r+")) == 0) {
            if (iSocket >= 0) close(iSocket);
            if (zRelight) {
                rc = system(zRelight);
                if (rc) {
                    Malfunction(721, /* LOG: SCGI relight failed */
                                "Relight failed with %d: \"%s\"\n",
                                rc, zRelight);
                }
                free(zRelight);
                zRelight = 0;
                sleep(1);
                continue;
            }
            if (zFallback) {
                struct stat statbuf;
                int rc;
                memset(&statbuf, 0, sizeof(statbuf));
                if (chdir(zDir)) {
                    char zBuf[1000];
                    Malfunction(720, /* LOG: chdir() failed */
                                "cannot chdir to [%s] from [%s]",
                                zDir, getcwd(zBuf, 999));
                }
                rc = stat(zFallback, &statbuf);
                if (rc == 0 && S_ISREG(statbuf.st_mode) && access(zFallback, R_OK) == 0) {
                    closeConnection = true;
                    rc = SendFile(zFallback, (int) strlen(zFallback), &statbuf);
                    free(zFallback);
                    althttpd_exit(0);
                } else {
                    Malfunction(706, /* LOG: bad SCGI fallback */
                                "bad fallback file: \"%s\"\n", zFallback);
                }
            }
            Malfunction(707, /* LOG: Cannot open socket to SCGI */
                        "cannot open socket to SCGI server %s\n",
                        scScript);
        }
        break;
    }

    nHdrAlloc = 0;
    zHdr = 0;
    if (zContentLength == 0) zContentLength = "0";
    ComputeRequestUri();
    zScgi = "1";
    for (i = 0; i < (int) (sizeof(cgienv) / sizeof(cgienv[0])); i++) {
        int n1, n2;
        if (cgienv[i].pzEnvValue[0] == 0) continue;
        n1 = (int) strlen(cgienv[i].zEnvName);
        n2 = (int) strlen(*cgienv[i].pzEnvValue);
        if (n1 + n2 + 2 + nHdr >= nHdrAlloc) {
            nHdrAlloc = nHdr + n1 + n2 + 1000;
            zHdr = realloc(zHdr, nHdrAlloc);
            if (zHdr == 0) {
                Malfunction(708, "out of memory"); /* LOG: OOM */
            }
        }
        memcpy(zHdr + nHdr, cgienv[i].zEnvName, n1);
        nHdr += n1;
        zHdr[nHdr++] = 0;
        memcpy(zHdr + nHdr, *cgienv[i].pzEnvValue, n2);
        nHdr += n2;
        zHdr[nHdr++] = 0;
    }
    zScgi = NULL;
    fprintf(s, "%d:", (int) nHdr);
    fwrite(zHdr, 1, nHdr, s);
    fprintf(s, ",");
    free(zHdr);
    if (nPostData > 0) {
        size_t wrote = 0;
        while (wrote < nPostData) {
            size_t n = fwrite(zPostData + wrote, 1, nPostData - wrote, s);
            if (n <= 0) break;
            wrote += n;
        }
        free(zPostData);
        zPostData = 0;
        nPostData = 0;
    }
    fflush(s);
    CgiHandleReply(s, 0);
}

// 检查 g_zRemoteAddr 是否被禁止访问。返回 1 表示被禁止，返回 0 表示允许访问。
/* + 无效的 g_zRemoteAddr 认定条件
 |   - g_zIPShunDir 为 NULL 或空字符串
 |   - g_zIPShunDir 是一个目录
 |   - g_zIPShunDir 中没有与 g_zRemoteAddr 完全匹配的文件
 |   - N==0 或文件的 mtime 距今超过 N*BANISH_TIME 秒
 | + N>0 且文件的 mtime 距今超过 N*5*BANISH_TIME 秒（默认 25 分钟/字节），则删除该文件
 | 文件的大小 N 决定了禁令的持续时间。0 字节的文件表示永久禁令。否则，禁令持续时间为 BANISH_TIME 秒乘以文件大小（字节）。
 |
*/
static int DisallowedRemoteAddr(void) {
    char zFullname[1000];
    size_t nIPShunDir;
    size_t nRemoteAddr;
    int rc;
    struct stat statbuf;
    time_t now;

    if (g_zIPShunDir == 0) return 0;
    if (g_zRemoteAddr == 0) return 0;
    if (g_zIPShunDir[0] != '/') {
        Malfunction(910, /* LOG: argument to --ipshun should be absolute path */
                    "The --ipshun directory should have an absolute path");
    }
    nIPShunDir = strlen(g_zIPShunDir);
    while (nIPShunDir > 0 && g_zIPShunDir[nIPShunDir - 1] == '/') nIPShunDir--;
    nRemoteAddr = strlen(g_zRemoteAddr);
    if (nIPShunDir + nRemoteAddr + 2 >= sizeof(zFullname)) {
        Malfunction(912, /* LOG: RemoteAddr filename too big */
                    "RemoteAddr filename too big");
    }
    if (g_zRemoteAddr[0] == 0
        || g_zRemoteAddr[0] == '.'
        || strchr(g_zRemoteAddr, '/') != 0
            ) {
        Malfunction(913, /* LOG: RemoteAddr contains suspicious characters */
                    "RemoteAddr contains suspicious characters");
    }
    memcpy(zFullname, g_zIPShunDir, nIPShunDir);
    zFullname[nIPShunDir] = '/';
    memcpy(zFullname + nIPShunDir + 1, g_zRemoteAddr, nRemoteAddr + 1);
    memset(&statbuf, 0, sizeof(statbuf));
    rc = stat(zFullname, &statbuf);
    if (rc) return 0;  /* No such file, hence no restrictions */
    if (statbuf.st_size == 0) return 1;  /* Permanently banned */
    time(&now);
    if (statbuf.st_size * BANISH_TIME + statbuf.st_mtime >= now) {
        return 1;  /* Currently under a ban */
    }
    if (statbuf.st_size * 5 * BANISH_TIME + statbuf.st_mtime < now) {
        unlink(zFullname);
    }
    return 0;
}

// 如果使用内置 TLS 模式（g_useHttps == 2），则初始化 SSL I/O 状态并返回 1；否则不执行任何操作并返回 0。
static int tls_init_conn(int iSocket) {
#ifdef ENABLE_TLS
    if (2==g_useHttps) {
        /*assert(NULL==tlsState.sslCon);*/
        if( NULL==tlsState.sslCon ) {
            tlsState.sslCon = (TlsServerConn *)tls_new_server(iSocket);
            if( NULL==tlsState.sslCon ) {
                Malfunction(512/* LOG: TLS context */, "Could not instantiate TLS context.");
            }
            atexit(tls_atexit);
        }
        return 1;
    }
#else
    if (0 == iSocket) {/*unused arg*/}
#endif
    return 0;
}

static void tls_close_conn(void) {
#ifdef ENABLE_TLS
    if( tlsState.sslCon ){
        tls_close_server(tlsState.sslCon);
        tlsState.sslCon = NULL;
    }
#endif
}

/**
 * @brief                           处理单个 HTTP 请求。这是 althttpd 的核心请求处理函数
 * @param forceClose                强制关闭连接标志
 *                                  = 0: 允许保持连接打开，支持处理同一连接上的后续请求（HTTP Keep-Alive）
 *                                       函数可能会根据请求内容或错误情况主动选择关闭连接
 *                                  = 1: 当前请求完成后必须关闭套接字，不处理后续请求
 *                                       通常在达到最大请求数限制时使用（main 中循环 100 次后的第 101 次）
 * @param socketId                  套接字文件描述符
 *                                  = 0: 通过 xinetd/inetd 等外部服务启动，使用标准输入/输出（stdin/stdout）
 *                                  > 0: 独立服务器模式，http_server() 通过 accept() 返回的套接字 ID
 *                                       仅在内置 TLS 模式下使用，用于初始化 TLS 连接
 * @return
 *                                  - 如果需要关闭连接（forceClose=1 或遇到错误或非 Keep-Alive 请求）：
 *                                    调用 exit() 终止进程，函数不返回
 *                                  - 如果允许保持连接打开（forceClose=0 且为 Keep-Alive）：
 *                                    正常返回，调用者可以继续在同一连接上调用此函数处理下一个请求
 *
 * @note                            此函数不是线程安全的，使用全局变量存储请求状态，
 *                                  依赖"一个进程处理一个连接"的模型。
 */

void ProcessOneRequest(int forceClose, int socketId) {

    int i, j, j0;
    char *z;                  /* 用于解析字符串 */
    struct stat statbuf;      /* 要检索的文件的信息 */
    FILE *in;                 /* 用于从 CGI 脚本读取 */
#ifdef LOG_HEADER
    FILE *hdrLog = 0;         /* 完整头部内容的日志文件 */
#endif
    char zLine[10000];        /* 输入行或构造名称的缓冲区 */
    const MimeTypeDef *pMimeType = 0; /* URI 的 MIME 类型 */
    size_t sz = 0;
    struct tm vTm;            /* zExpLogFile 的时间戳 */

    // ---------------------------

    // 记录请求开始的精确时间（用于性能统计）
    clock_gettime(ALTHTTPD_CLOCK_ID, &tsBeginTime);
    gettimeofday(&beginTime, 0);
    
    // 如果配置了日志文件，则根据当前时间格式化日志文件名（支持 strftime 格式）
    if (g_zLogFile) {
        assert(beginTime.tv_sec > 0);
        gmtime_r(&beginTime.tv_sec, &vTm);
        sz = strftime(zExpLogFile, sizeof(zExpLogFile), g_zLogFile, &vTm);
    }
    // 如果日志文件名无效或格式化失败
    if (sz == 0 || sz >= sizeof(zExpLogFile) - 2) zExpLogFile[0] = 0;

    // 设置超时保护（必须在一定时间内看到初始头部，防止慢速攻击）
    // > 首次请求：10 秒（给客户端更多准备时间）
    // > 后续请求：5 秒（Keep-Alive 连接，保持短超时避免子进程占用过久）
    if (g_useTimeout) {
        if (nRequest > 0)
            SetTimeout(5, 801);         /* 日志：请求头超时（后续请求） */
        else SetTimeout(10, 802);       /* 日志：请求头超时（首次请求） */
    }

    // 切换当前工作目录到 HTTP 文件系统的根目录（后续的文件访问都基于此根目录进行）
    if (chdir(*g_zRoot ? g_zRoot : "/") != 0) {
        char zBuf[1000];
        Malfunction(190 /* LOG: chdir() failed */, "cannot chdir to [%s] from [%s]",
                    g_zRoot, getcwd(zBuf, sizeof(zBuf) - 1));
    }

    nRequest++;                         // 请求计数器递增
    tls_init_conn(socketId);            // 初始化 TLS 连接（如果启用）

    // ---------------------------

    // 读取 HTTP 请求协议的第一行：METHOD /path/to/resource HTTP/1.1
    // + 这里会临时禁止日志输出（避免记录空请求）
    omitLog = 1;
    if (althttpd_fgets(zLine, sizeof(zLine), stdin) == 0)
        althttpd_exit(0);               // 如果读取失败（连接已关闭或超时），直接退出
    omitLog = 0;

    nIn += (i = (int)strlen(zLine));    // 跳过第一行的长度，同时统计上行数据的数据大小

    // 解析 HTTP 请求的第一行
    zMethod = StrDup(GetFirstElement(zLine, &z));           // HTTP 方法：GET、POST、HEAD
    zRealScript = zScript = StrDup(GetFirstElement(z, &z)); // 请求的 URI 路径
    zProtocol = StrDup(GetFirstElement(z, &z));             // 协议版本：HTTP/1.0 或 HTTP/1.1
    fprintf(stderr, "[httpd] HTTP Request: %s %s %s\n", zMethod, zScript, zProtocol);

    // 如果请求协议无效，返回 400 Bad Request 错误
    // > 必须以 "HTTP/" 开头
    // > 总长度必须是 8 个字符（例如 "HTTP/1.1"）
    // > 请求行不能过长（防止缓冲区溢出攻击）
    // 检查协议：支持 HTTP 和 SQTP
    if (zProtocol == NULL || i > 9990) { zProtocol = NULL;

        if (i <= 9990) {
            StartResponse("400 Bad Request");
            nOut += althttpd_printf(
                    "Content-type: text/plain; charset=utf-8" CRLF
                    CRLF
                    "This server does not understand the requested protocol\n"
            );
            MakeLogEntry(0, 200); /* 日志：HTTP 头中的协议错误 */
        } else {
            StartResponse("414 URI Too Long");
            nOut += althttpd_printf(
                    "Content-type: text/plain; charset=utf-8" CRLF
                    CRLF
                    "URI too long\n"
            );
            MakeLogEntry(0, 201); /* 日志：URI 过长 */
        }
        althttpd_exit(0);
    }

    // HTTP 协议检查
    if (strncmp(zProtocol, "HTTP/", 5) != 0 || strlen(zProtocol) != 8) {
        StartResponse("400 Bad Request");
        nOut += althttpd_printf(
                "Content-type: text/plain; charset=utf-8" CRLF
                CRLF
                "This server does not understand the requested protocol\n"
        );
        MakeLogEntry(0, 200); /* 日志：HTTP 头中的协议错误 */
        althttpd_exit(0);
    }

    if (*zScript != '/') NotFound(210);                     // 空的 URI 请求
    while (zScript[1] == '/') zScript++; zRealScript++;     // 规范化 URI：移除多余的前导斜杠（例如 "//path" -> "/path"）

    // 确定执行完后是否关闭连接
    // > 调用者明确要求关闭连接（例如达到最大连续请求数）
    // > HTTP/1.0 或更早版本不支持 Keep-Alive，必须关闭连接
    if (forceClose ||
        zProtocol[5] < '1' || zProtocol[7] < '1')
        closeConnection = true;

    // SQTP 协议拦截：如果 METHOD 以 SQTP- 开头，交给 httpd_sqtp 处理
    if (strncmp(zMethod, "SQTP-", 5) == 0) {
        httpd_sqtp(zMethod, zScript, zProtocol, &nOut);  // 交给 SQTP 处理函数，传递必要参数
        althttpd_exit(0);
    }

    // 如果 HTTP METHOD 不是 GET、POST 或 HEAD，则返回 501 Not Implemented 错误
    // + 该服务器仅支持 HEAD/GET/POST 三种 METHOD，即不支持 PUT、DELETE、PATCH 等其他 HTTP 方法
    if (strcmp(zMethod, "GET") != 0 && strcmp(zMethod, "POST") != 0
        && strcmp(zMethod, "HEAD") != 0) {
        StartResponse("501 Not Implemented");
        nOut += althttpd_printf(
                "Content-type: text/plain; charset=utf-8" CRLF
                CRLF
                "The %s method is not implemented on this server.\n",
                zMethod);
        MakeLogEntry(0, 220); /* 日志：未知的请求方法 */
        althttpd_exit(0);
    }

    // 如果启用了 LOG_HEADER 功能，并且请求 URI 中包含 "FullHeaderLog" 字符串
    // : 将完整的 HTTP 头部内容记录到一个专门的日志文件中（文件名为 g_zLogFile-hdr）
    // + 该段代码用于调试目的
#ifdef LOG_HEADER
    /* If there is a log file (if g_zLogFile!=0) and if the pathname in
    * the first line of the http request contains the magic string
    * "FullHeaderLog" then write the complete header text into the
    * file %s(g_zLogFile)-hdr.  Overwrite the file.  This is for protocol
    * debugging only and is only enabled if althttpd is compiled with
    * the -DLOG_HEADER=1 option
    */
    if (g_zLogFile
        && strstr(zScript,"FullHeaderLog")!=0
        && strlen(g_zLogFile)<sizeof(zLine)-50) {

        sprintf(zLine, "%s-hdr", g_zLogFile);
        hdrLog = fopen(zLine, "wb");
    }
#endif

    // ---------------------------
    // 解析 HTTP header 字段

    // 初始化头部字段变量（清空上次请求的数据）
    zCookie = 0;              // Cookie 信息
    zAuthType = 0;            // 认证类型（Basic/Digest）
    zRemoteUser = 0;          // 认证后的远程用户名
    zReferer = 0;             // 引用页（从哪个页面跳转来的）
    zIfNoneMatch = 0;         // ETag 缓存验证
    zIfModifiedSince = 0;     // 时间缓存验证
    zContentLength = 0;       // POST 数据长度
    rangeEnd = 0;             // Range 请求的结束位置
    
    // 循环读取 HTTP header 每一行，直到遇到空行（即 \n\n 表示头部结束）
    // + 这里只解析了部分常见的 HTTP 头部字段，其他字段会被忽略
    while (althttpd_fgets(zLine, sizeof(zLine), stdin)) {
        char *zFieldName;  // 头部字段名（冒号前的部分）
        char *zVal;        // 头部字段值（冒号后的部分）

#ifdef LOG_HEADER
        // 如果启用了头部日志记录，将原始头部写入日志
        if( hdrLog ) fprintf(hdrLog, "%s", zLine);
#endif
        nIn += strlen(zLine);                                   // 统计上行数据的大小（包括头部字段）
        
        zFieldName = GetFirstElement(zLine, &zVal);
        if (zFieldName == 0 || *zFieldName == 0) break;         // 空行表示头部结束
        RemoveNewline(zVal);                                    // 移除字段值末尾的换行符

        if (strcasecmp(zFieldName, "User-Agent:") == 0) zAgent = StrDup(zVal);
        else if (strcasecmp(zFieldName, "Accept:") == 0) zAccept = StrDup(zVal);
        else if (strcasecmp(zFieldName, "Accept-Encoding:") == 0) zAcceptEncoding = StrDup(zVal);
        else if (strcasecmp(zFieldName, "Content-length:") == 0) zContentLength = StrDup(zVal);
        else if (strcasecmp(zFieldName, "Content-type:") == 0) zContentType = StrDup(zVal);
        else if (strcasecmp(zFieldName, "Referer:") == 0) {

            zReferer = StrDup(zVal);
            if (strstr(zVal, "devids.net/") != 0) {
                zReferer = "devids.net.smut";
                Forbidden(230); /* LOG: Referrer is devids.net */
            }
        }
        else if (strcasecmp(zFieldName, "Cookie:") == 0) zCookie = StrAppend(zCookie, "; ", zVal);
        else if (strcasecmp(zFieldName, "Connection:") == 0) {

            if (strcasecmp(zVal, "close") == 0)
                closeConnection = true;
            else if (!forceClose && strcasecmp(zVal, "keep-alive") == 0)
                closeConnection = false;
        }
        else if (strcasecmp(zFieldName, "Host:") == 0) {

            // 安全检查：Host 头不能包含危险字符
            if (sanitizeString(zVal)) Forbidden(240);  /* 日志：HOST 参数中的非法内容 */

            g_zHttpHost = StrDup(zVal);                           // 保存完整的 Host 值
            zServerPort = zServerName = StrDup(g_zHttpHost);

            // 解析 Host 头，分离主机名和端口号
            // + 格式：hostname:port 或 [IPv6]:port
            int inSquare = 0; char c;
            while (zServerPort && (c = *zServerPort) != 0
                   && (c != ':' || inSquare)) {                 // 注意：IPv6 地址中的冒号不算分隔符
                if (c == '[') inSquare = 1;                     // 进入 IPv6 地址
                if (c == ']') inSquare = 0;                     // 离开 IPv6 地址
                zServerPort++;
            }
            
            // 如果找到了端口号，分离它
            if (zServerPort && *zServerPort) {
                *zServerPort = 0;
                zServerPort++;
            }

            // 如果有实际端口（独立模式），使用实际端口覆盖
            if (zRealPort) zServerPort = StrDup(zRealPort);

        }
        else if (strcasecmp(zFieldName, "Authorization:") == 0) zAuthType = GetFirstElement(StrDup(zVal), &zAuthArg);
        else if (strcasecmp(zFieldName, "If-None-Match:") == 0) zIfNoneMatch = StrDup(zVal);
        else if (strcasecmp(zFieldName, "If-Modified-Since:") == 0)  zIfModifiedSince = StrDup(zVal);
        else if (strcasecmp(zFieldName, "Range:") == 0 && strcmp(zMethod, "GET") == 0) {

            int x1 = 0, x2 = 0;
            int n = sscanf(zVal, "bytes=%d-%d", &x1, &x2);
            if (n == 2 && x1 >= 0 && x2 >= x1) {
                rangeStart = x1;
                rangeEnd = x2;
            } else if (n == 1 && x1 > 0) {
                rangeStart = x1;
                rangeEnd = 0x7fffffff;
            }
        }
    }
#ifdef LOG_HEADER
    if( hdrLog ) fclose(hdrLog);
#endif

    // ---------------------------
    // 1. Agent

    // 通过检查 User-Agent 头部，禁止某些已知的恶意爬虫或攻击工具访问服务器
    if (zAgent) {
        const char *azDisallow[] = {
                "Thinkbot/",
                "GPTBot/",
                "ClaudeBot/",
                "Amazonbot",
                "Windows 9",
                "Download Master",
                "Ezooms/",
                "DotBot",
                "HTTrace",
                "AhrefsBot",
                "MicroMessenger",
                "OPPO A33 Build",
                "SemrushBot",
                "MegaIndex.ru",
                "MJ12bot",
                "Chrome/0.A.B.C",
                "Neevabot/",
                "BLEXBot/",
                "Scrapy/",
                "Synapse",
        };
        size_t ii;
        for (ii = 0; ii < sizeof(azDisallow) / sizeof(azDisallow[0]); ii++) {
            if (strstr(zAgent, azDisallow[ii]) != 0) {
                Forbidden(250);  /* LOG: Disallowed user agent */
            }
        }

#if 0
         /* Spider attack from 2019-04-24 */
        if (strcmp(zAgent, "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/50.0.2661.102 Safari/537.36")==0) {
            Forbidden(251);  /* LOG: Disallowed user agent (20190424) */
        }
#endif
    }
#if 0
    if (zReferer) {
        static const char *azDisallow[] = {
          "skidrowcrack.com",
          "hoshiyuugi.tistory.com",
          "skidrowgames.net",
        };
        for (int i=0; i<sizeof(azDisallow)/sizeof(azDisallow[0]); i++) {
            if (strstr(zReferer, azDisallow[i])!=0) NotFound(260);  /* LOG: Disallowed referrer */
        }
    }
#endif

    // ---------------------------
    // 2. Server

    // 如果 Host 头部没有提供服务器名称或端口号，尝试使用系统调用获取服务器主机名
    if (zServerName == 0) {
        zServerName = SafeMalloc(100);
        gethostname(zServerName, 100);
    }
    if (zServerPort == 0 || *zServerPort == 0) {
        zServerPort = zRealPort ? zRealPort : DEFAULT_PORT;
    }

    // ---------------------------
    // 3. Query String & zScript

    // 解析 URI，分离出查询字符串（如果存在）
    for (z = zScript; *z && *z != '?'; z++) {}
    if (*z == '?') {
        zQuerySuffix = StrDup(z); *z = 0;
#if 0
        if ((zReferer==0 || zReferer[0]==0)
            && strncmp(zQuerySuffix+1, "w=%2", 4)==0) {

            NotFound(261);  /* LOG: Spider attack */
        }
#endif
#if 0
        /* Chinese spider attack against the SQLite Forum.  2024-07-25 */
        if ((zReferer==0 || zReferer[0]==0)
            && strstr(zScript,"forum/timeline")!=0
            && strstr(&zQuerySuffix[1],"y=a")!=0
            && (strstr(&zQuerySuffix[1],"a=20")!=0 ||
                strstr(&zQuerySuffix[1],"b=20")!=0 ||
                strstr(&zQuerySuffix[1],"&c=")!=0 ||
                (zQuerySuffix[1]=='c' && zQuerySuffix[2]=='='))) {

            Forbidden(262);  /* LOG: Chinese forum timeline spider */
        }
#endif
    } else zQuerySuffix = "";
    zQueryString = *zQuerySuffix ? &zQuerySuffix[1] : zQuerySuffix;

    // ---------------------------
    // 4. POST 数据上传

    // 如果是 POST 请求且有 Content-Length 头，读取请求体数据
    if (zMethod[0] == 'P' && zContentLength != 0) {

        size_t len = strtol(zContentLength, NULL, 10);          // 获取要读取的数据长度
        
        // 安全检查：防止过大的 POST 数据导致内存耗尽
        if (len > MAX_CONTENT_LENGTH) {
            StartResponse("500 Request too large");
            nOut += althttpd_printf(
                    "Content-type: text/plain; charset=utf-8" CRLF
                    CRLF
                    "Too much POST data\n"
            );
            MakeLogEntry(0, 270); /* 日志：请求过大 */
            althttpd_exit(0);
        }
        
        rangeEnd = 0;                                           // POST 请求不支持 Range
        zPostData = SafeMalloc(len + 1);                        // 分配内存来存储 POST 数据
        
        // 设置超时：15秒基础 + 每2KB数据增加1秒（防止慢速POST攻击）
        SetTimeout(15 + (int)len / 2000, 803/* 日志：POST 数据超时 */);
        
        // 从 stdin 读取 POST 数据
        nPostData = althttpd_fread(zPostData, 1, len, stdin);
        nIn += nPostData;                                       // 统计上行数据的大小（包括 POST 数据）
    }

    // ---------------------------

    // 设置总体超时限制：整个请求处理不能超过 30 秒
    SetTimeout(30, 804/* 日志：解码 HTTP 请求超时 */);

    // ---------------------------
    // 5. zScript & 安全处理

    // URI 安全化：将脚本名中的所有异常字符转换为 "_" 这是对各种攻击的防御，特别是 XSS（跨站脚本）攻击
    // + 例如：将 "<script>" 转换为 "_script_"
    sanitizeString(zScript);

    // IP 黑名单检查：如果请求来源 IP 已被屏蔽，拒绝处理
    // + 屏蔽机制通过 --ipshun 选项启用，用于防止滥用和攻击
    if (g_zIPShunDir && DisallowedRemoteAddr())
        ServiceUnavailable(901/* 日志：被禁止的远程 IP 地址 */);

    // 路径安全检查
    // + 不允许 "/." 或 "/-" 出现在路径中
    // 目的：
    // 1. 防止目录遍历攻击（例如 "/../../../etc/passwd"）
    // 2. 允许创建对 Web 服务器不可见的文件/目录（名称以 "-" 或 "." 开头）
    //    例如：配置文件 ".config"、认证文件 "-auth"
    //
    // 例外：允许 "/.well-known/" 前缀（RFC-5785 标准）
    //       这是为了支持 Let's Encrypt 等证书验证服务
    for (z = zScript; *z; z++) {
        if (*z == '/' && (z[1] == '.' || z[1] == '-')) {
            // 检查是否是 /.well-known/ 路径（且不是 /..）
            if (strncmp(zScript, "/.well-known/", 13) == 0 && (z[1] != '.' || z[2] != '.')) {
                // 例外：允许 /.well-known/ 路径中的 "/." 和 "/-" 但仍然不允许 "/..".
                continue;
            }
            NotFound(300/* 日志：路径元素以 "." 或 "-" 开头 */);
        }
    }

    // ---------------------------
    // 6. host 虚拟主机 & home 目录确定

    // 确定虚拟主机目录
    // + 根据 HTTP Host 头确定文件系统根目录（实现虚拟主机）
    // 处理步骤：
    // 1. 从 HTTP_HOST 中去除端口号（例如 "example.com:8080" -> "example.com"）
    // 2. 将所有字符转换为小写
    // 3. 将非字母数字字符（包括 "."）转换为 "_"
    // 4. 添加 ".website" 后缀，在文件系统中查找对应目录
    //    例如："www.sqlite.org" -> "www_sqlite_org.website"
    // 5. 如果找不到，回退到 "default.website" 目录
    if (zScript[0] != '/') NotFound(310); /* LOG: URI does not start with "/" */
    if (strlen(g_zRoot) + 40 >= sizeof(zLine)) NotFound(320); /* LOG: URI too long */
    if (g_zHttpHost == 0 || g_zHttpHost[0] == 0) NotFound(330);  /* LOG: Missing HOST: parameter */

    if (strlen(g_zHttpHost) + strlen(g_zRoot) + 10 >= sizeof(zLine)) NotFound(340);  /* LOG: HOST parameter too long */

    sprintf(zLine, "%s/%s", g_zRoot, g_zHttpHost);
    for (i = (int)strlen(g_zRoot) + 1; zLine[i] && zLine[i] != ':'; i++) {

        unsigned char c = (unsigned char) zLine[i];
        if (!isalnum(c)) {

            // 如果 Host 头以 "." 结尾（例如 "example.com."），则允许最后一个 "." 但不转换为 "_"
            // + 这是为了兼容某些客户端的行为
            if (c == '.' && (zLine[i + 1] == 0 || zLine[i + 1] == ':')) break;

            zLine[i] = '_';
        }
        else if (isupper(c)) zLine[i] = (char)tolower(c);
    }
    strcpy(&zLine[i], ".website");

    if (stat(zLine, &statbuf) || !S_ISDIR(statbuf.st_mode)) {
        sprintf(zLine, "%s/default.website", g_zRoot);
        if (stat(zLine, &statbuf) || !S_ISDIR(statbuf.st_mode)) {
            sprintf(zLine, "%s", g_zRoot);
        }
    }

    // 将找到的目录路径保存到 zHome
    zHome = StrDup(zLine);
    if (chdir(zHome) != 0) {
        char zBuf[1000];
        Malfunction(360/* LOG: chdir() failed */, "cannot chdir to [%s] from [%s]", zHome, getcwd(zBuf, 999));
    }

    // ---------------------------
    // 7. 定位目标文件

    // 根据 URI 路径在文件系统中定位目标文件
    // 处理逻辑：
    i = 0; j = j0 = (int) strlen(zLine);  // j0 记录根目录长度

    // 解析处理 script 信息
    // 1. 以 '/' 为分隔符，逐段遍历处理 URI 路径，找到一个存在的文件
    // 2. 如果逐级遍历过程过程中，某段路径不存在，则尝试寻找 "not-found.html" 错误页，如果没有找到则返回 404 错误
    // 3. 如果整个 URI 指向一个目录，则尝试寻找目录下是否存在默认索引文件（index.html、index.cgi 等），如果没有找到则返回 404 错误
    while (zScript[i]) {

        // 遍历 URI 路径中（以 '/' 分隔）的一段，并复制到 zLine
        while (zScript[i] && (i == 0 || zScript[i] != '/')) { // 找到下一个 '/'
            zLine[j++] = zScript[i++];
        }
        zLine[j] = 0;
        /* fprintf(stderr, "searching [%s]...\n", zLine); */

        // 检查路径是否存在（优先 buildins 内存查找，性能更好）
        buildin_file_info_st *buildin = buildins_find(&zLine[j0]);
        
        // 如果 buildin 不存在，检查文件系统
        if (!buildin && stat(zLine, &statbuf) != 0) {
            // 路径既不在 buildins 也不在文件系统，尝试寻找 "not-found.html" 错误页

            int stillSearching = 1;
            while (stillSearching && i > 0 && j > j0) {

                while (j > j0 && zLine[j - 1] != '/') { j--; }  // 回溯到上一级目录
                strcpy(&zLine[j - 1], "/not-found.html");       // 尝试不同层级的 not-found.html
                
                // 优先检查 buildins
                buildin = buildins_find(&zLine[j0]);
                if (buildin && !buildins_is_dir(buildin)) {
                    zRealScript = StrDup(&zLine[j0]);
                    // buildins 文件不需要 access() 检查，直接重定向
                    Redirect(zRealScript, 302, 1, 370/* 日志：重定向到 not-found */);
                    return;
                }
                
                // buildins 中没有，检查文件系统
                if (stat(zLine, &statbuf) == 0
                    && S_ISREG(statbuf.st_mode)
                    && access(zLine, R_OK) == 0) {

                    zRealScript = StrDup(&zLine[j0]);
                    if (access(zLine, X_OK) != 0) {
                        Redirect(zRealScript, 302, 1, 370/* 日志：重定向到 not-found */);
                        return;
                    }
                    stillSearching = 0;
                    break;
                }

                --j;  // 继续向上回溯
            }

            if (stillSearching) NotFound(380/* 日志：URI 未找到 */);
            break;
        }

        // 如果找到的是文件（buildin 文件或文件系统文件）
        if ((buildin && !buildins_is_dir(buildin)) || 
            (!buildin && S_ISREG(statbuf.st_mode))) {

            // 文件系统文件需要检查可读性
            if (!buildin && access(zLine, R_OK)) NotFound(390/* 日志：文件不可读 */);

            // 找到并保存实际文件路径
            zRealScript = StrDup(&zLine[j0]);
            break;
        }

        // 如果 script 结束（同时意味着整个 URI 指向一个目录）
        if (zScript[i] == 0 || zScript[i + 1] == 0) {

            // 按优先级尝试各种索引文件
            static const char *azIndex[] = {
                "/home", "/index", "/index.html", "/index.cgi", "/not-found.html"
            };

            // 遍历所有可能的索引文件名
            unsigned n = sizeof(azIndex) / sizeof(azIndex[0]), jj,
                     k = j > 0 && zLine[j - 1] == '/' ? j - 1 : j;
            for (jj = 0; jj < n; jj++) {
                strcpy(&zLine[k], azIndex[jj]);
                
                // 优先检查 buildins（内存查找，性能更好）
                buildin = buildins_find(&zLine[j0]);
                if (buildin && !buildins_is_dir(buildin)) {
                    // 在 buildins 中找到了文件（不是目录）
                    fprintf(stderr, "[httpd] Index file '%s' found in buildins\n", &zLine[j0]);
                    break;
                }
                
                // buildins 中没有，检查文件系统
                if (stat(zLine, &statbuf) == 0) {
                    if (!S_ISREG(statbuf.st_mode)) continue;    // 不是普通文件
                    if (access(zLine, R_OK)) continue;          // 不可读
                    break;  // 找到了文件系统中的索引文件
                }
            }
            
            if (jj >= n) NotFound(400/* 日志：URI 是目录但没有 index.html */);

            // 保存（带有索引文件的）实际文件路径
            zRealScript = StrDup(&zLine[j0]);

            // 如果请求的 URL 不是以 "/" 结尾，但由于我们添加了 index.html，所以需要重定向
            // + 否则所有相对 URL 都会不正确，除非 URL 只有域名没有其他路径（例如 "http://sqlite.org"）则不需要重定向
            if (zScript[i] == 0 && strcmp(zScript, "/") != 0) {
                Redirect(zRealScript, 301, 1, 410 /* 日志：重定向以添加尾随 / */);
                return;
            }

            break;
        }

        zLine[j++] = zScript[i++];      // 继续处理下一段路径
    }
    zPathInfo = StrDup(&zScript[i]);    // 额外的路径信息（PATH_INFO），也就是脚本文件路径之后的部分，
                                        // + 例如 /cgi-bin/script.cgi/extra/path/info 中的 "/extra/path/info"

    zFile = StrDup(zLine);              // 实际本地文件的完整路径，即 /zHome/zRealScript
    lenFile = (int)strlen(zFile);       // 文件名长度

    // 提取文件所在目录
    zDir = StrDup(zFile);
    for (i = (int)strlen(zDir) - 1; i > 0 && zDir[i] != '/'; i--) {}  // 从后往前查找最后一个 '/'
    if (i == 0) strcpy(zDir, "/");      // 文件在根目录
    else zDir[i] = 0;                   // 截断，只保留目录部分

    // 授权检查（HTTP Basic/Digest 认证）
    sprintf(zLine, "%s/-auth", zDir);   // 在目录中查找 -auth 文件
    if (access(zLine, R_OK) == 0 && !CheckBasicAuthorization(zLine)) {
        tls_close_conn();  // 认证失败，关闭连接
        return;
    }

    // ---------------------------
    // 根据文件类型采取相应行动
    // ---------------------------

    // 检查是否是 buildins 文件（只查找一次，后续复用）
    buildin_file_info_st *buildin_file = buildins_find(zRealScript);
    bool is_buildin_c_cgi = false;  // 标记是否为 buildin C 脚本
    char *temp_cgi_path = NULL;
    
    if (buildin_file && !buildins_is_dir(buildin_file)) {
        // buildins 文件：检查是否是 CGI（基于扩展名判断）
        if (lenFile > 2 && strcmp(&zFile[lenFile - 2], ".c") == 0) {
            // C 脚本：直接在 httpd_cgi_c 中处理，不需要临时文件
            is_buildin_c_cgi = true;
        } else if (lenFile > 4 && (
                   strcmp(&zFile[lenFile - 4], ".cgi") == 0 ||
                   strcmp(&zFile[lenFile - 3], ".py") == 0 ||
                   strcmp(&zFile[lenFile - 3], ".sh") == 0)) {
            // 其他 CGI 脚本：仍然需要提取到临时文件
            fprintf(stderr, "[httpd] Extracting buildin CGI to temp file: %s\n", zRealScript);
            int temp_cgi_fd = buildins_to_tmp_fd(buildin_file, &temp_cgi_path);
            if (temp_cgi_fd < 0) {
                Malfunction(807, "Failed to extract buildin CGI: %s", zRealScript);
            }
            close(temp_cgi_fd); // 关闭 fd，后续会重新打开
            
            // 添加执行权限
            chmod(temp_cgi_path, 0700);
            
            // 更新 zFile 指向临时文件
            free(zFile);
            zFile = temp_cgi_path;
            lenFile = (int)strlen(zFile);
            
            // 重新 stat 临时文件
            if (stat(zFile, &statbuf) != 0) {
                Malfunction(808, "Failed to stat temp CGI file: %s", zFile);
            }
        }
    }

    // 对 CGI 脚本的处理和执行
    int isCScript = (lenFile > 2 && strcmp(&zFile[lenFile - 2], ".c") == 0);        // 扩展 c 脚本类型
    
    if (isCScript || 
        ((statbuf.st_mode & 0100) == 0100 && access(zFile, X_OK) == 0
        && (!(pMimeType = GetMimeType(zFile, lenFile)) || (pMimeType->flags & MTF_NOCGI) == 0))) {

        // 安全检查：所有 CGI 脚本（包括 C 脚本和传统 CGI）不能被其他用户写
        // 注意：这里检查的是其他用户的写权限（0022），而不是所有用户的写权限（0222）
        // + 0022 = 组写权限(0020) + 其他用户写权限(0002)
        // + 即如果 CGI 脚本被其所有者以外的人可写，则中止执行
        // + 这对 C 脚本同样重要：恶意用户可修改 .c 文件，服务器会编译执行被篡改的代码
        // + 例外：buildins 中的文件是编译进可执行文件的，可以信任，不需要权限检查
        if (!is_buildin_c_cgi && !buildin_file && (statbuf.st_mode & 0022))
            CgiScriptWritable();        // 返回错误并退出

        // 计算 CGI 脚本的基本文件名（不包含路径）
        for (i = (int)strlen(zFile) - 1; i >= 0 && zFile[i] != '/'; i--) {}

        // 不带目录前缀的文件名
        char *zBaseFilename = &zFile[i + 1];

        // fork 一个子进程来运行 CGI 脚本，这样即使 CGI 崩溃也不会影响对请求的响应
        
        // 创建用于与子 CGI 进程通信的管道
        int px[2];      /* CGI stdout → 请求处理进程，接收 CGI 的输出 */
        if (pipe(px)) Malfunction(440/* 日志：pipe() 失败 */, "Unable to create a pipe for the CGI program");
        int py[2];      /* 请求处理进程 → CGI stdin，发送 POST 数据 */
        if (pipe(py)) Malfunction(441/* 日志：pipe() 失败 */, "Unable to create a pipe for the CGI program");

        // 创建运行 CGI 的子子进程
        fflush(stdout);  // fork 之前刷新缓冲区，避免子进程重复输出缓存中的数据
        if (fork() == 0) {
            // 以下代码在 CGI 子子进程中运行

            // 设置 CGI → 请求处理进程的管道（重定向 stdout）
            close(1);                               // 关闭标准输出
            if (dup(px[1]) < 0) {                   // 将 stdout 重定向到管道写端
                CgiStartFailure(px[1], 442/* 日志：dup() 失败 */, "CGI cannot dup() file descriptor 1");
            }

            // 设置请求处理进程 → CGI 的管道（重定向 stdin）
            close(0);                               // 关闭标准输入
            if (dup(py[0]) < 0) {                   // 将 stdin 重定向到管道读端
                CgiStartFailure(px[1], 444/* 日志：dup() 失败 */, "CGI cannot dup() file descriptor 0");
            }

            // [2026-02-11] 注释掉 fd 批量关闭逻辑
            // 原设计：关闭所有 fd≥3 的文件描述符，防止泄漏到 CGI 子进程
            // 现在问题：buildins 虚拟文件系统需要保留这些 fd（fd=3-12），供 TCC 运行时访问
            // 泄漏检查：
            //   ✅ CGI 子进程退出时，OS 自动关闭所有打开的 fd（进程资源回收机制）
            //   ✅ 不会造成系统级 fd 泄漏
            //   ⚠️  管道 fd (px[0], px[1], py[0], py[1]) 会继承到 CGI，但因为：
            //      - px[0], py[1] 已在请求处理进程中关闭
            //      - px[1], py[0] 已被 dup 到 stdout/stdin（会在进程退出时关闭）
            //      - 即使有额外副本，进程退出时也会自动清理
            // 设计权衡：支持 buildins 机制 > 手动管理少量 fd 副本
            // for (i = 3; close(i) == 0; i++) {}

            // 移动到 CGI 程序所在的目录
            if (chdir(zDir)) {
                char zBuf[1000];
                CgiStartFailure(1, 445/* 日志：chdir() 失败 */, "CGI cannot chdir to [%s] from [%s]",
                                zDir, getcwd(zBuf, 999));
            }

            // 设置 CGI 环境变量
            ComputeRequestUri();                    // 计算 REQUEST_URI
            putenv("GATEWAY_INTERFACE=CGI/1.0");    // 设置 CGI 版本
            for (i = 0; i < (int) (sizeof(cgienv) / sizeof(cgienv[0])); i++) {

                if (*cgienv[i].pzEnvValue) SetEnv(cgienv[i].zEnvName, *cgienv[i].pzEnvValue);
            }

            // 根据类型执行不同的 CGI
            if (isCScript) {
                // C 脚本：使用 TinyCC 运行时编译并执行
                // + 优势：无需预编译，修改即生效，适合快速开发
                // + 劣势：每次请求都要编译，性能略低于预编译 CGI
                // + 注意：这里在子子进程中执行，crash 不会影响请求处理进程
                httpd_cgi_c(zMethod, zFile, zProtocol, &nOut, is_buildin_c_cgi ? buildin_file : NULL);
                exit(0);  // C 脚本执行完毕，退出 CGI 子子进程
            } else {
                // 传统 CGI：execl() 替换进程映像执行二进制文件
                execl(zBaseFilename, zBaseFilename, (char *) 0);
                CgiStartFailure(1, 446/* 日志：CGI exec() 失败 */,
                                "CGI program \"%s\" could not be started.", zScript);
            }

            /* NOT REACHED */
        }

        // 以下代码在请求处理进程（父进程）中运行
        isCGI = true;                      // 标记当前请求为 CGI

        close(px[1]);                   // 关闭管道写端（只读取）
        in = fdopen(px[0], "rb");       // 打开管道读端为文件流

        // 设置 althttpd 到 CGI 的管道用于发送 POST 数据（如果有）

        close(py[0]);                   // 关闭管道读端（只写入）
        if (nPostData > 0) {

            // 循环写入 POST 数据，直到全部发送
            ssize_t wrote = 0, n;
            while ((ssize_t) nPostData > wrote) {
                n = write(py[1], zPostData + wrote, nPostData - wrote);
                if (n <= 0) break;  // 写入错误
                wrote += n;
            }
        }

        // 释放 POST 数据缓冲区
        if (zPostData) {
            free(zPostData);
            zPostData = 0;
            nPostData = 0;
        }
        close(py[1]);                   // 关闭管道写入端，告知 CGI 数据已发送完毕

        // 等待 CGI 程序响应并处理该响应
        if (in == 0) {
            CgiError();                                                 // 管道打开失败
        } else {
            CgiHandleReply(in, strncmp(zBaseFilename, "nph-", 4) == 0); // 处理 CGI 的响应输出
        }
    }

    // 如果文件以 ".scgi" 结尾，进入 SCGI 处理流程
    else if (lenFile > 5 && strcmp(&zFile[lenFile - 5], ".scgi") == 0) {

        // 任何以 ".scgi" 结尾的文件都被假定为包含如下格式的文本：SCGI hostname port
        // + 打开一个 TCP/IP 连接到该主机并发送 SCGI 请求
        SendScgiRequest(zFile, zScript);
    }

    // 如果访问的是静态文件，但 URI 中包含了文件名之后的额外路径信息（即 PATH_INFO 部分），则返回 404 错误
    // + 前面已经完成了 CGI 和 SCGI 的处理，所以这里被认为是静态文件请求
    //   cgi 允许额外路径信息，但静态文件不允许
    // ! 这里不能直接判断 zPathInfo 是否为空，因为静态页面也可以存在 PATH_INFO 信息，即页面内锚点（即 #anchor 部分）
    //   但锚点 #anchor 肯定不会包含斜杠 '/'，所以这里通过比较斜杠数量来判断
    else if (countSlashes(zRealScript) != countSlashes(zScript)) {

        NotFound(460/* 日志：静态文件名之后的额外 URI 内容 */);
    }
    // 对于普通静态文件请求，默认执行文件发送处理
    else {

        // 设置超时：30秒基础 + 每 2KB 数据 1 秒
        SetTimeout(30 + (int)statbuf.st_size / 2000, 805/* 日志：发送静态文件超时 */);

        // 检查是否是 buildins 文件（复用前面的查找结果，到这里不可能是目录）
        if (buildin_file) {
            // 从 buildins 发送（支持 gzip 直传）
            fprintf(stderr, "[httpd] Sending file from buildins: %s\n", zRealScript);
            if (SendBuildins(zRealScript, strlen(zRealScript), buildin_file)) {
                // 清理临时 CGI 文件（如果存在）
                if (temp_cgi_path) {
                    unlink(temp_cgi_path);
                    free(temp_cgi_path);
                }
                return;
            }
            // SendBuildins 失败，继续尝试文件系统（不太可能发生）
        }

        // 从文件系统发送
        fprintf(stderr, "[httpd] Sending file from filesystem: %s\n", zFile);
        if (SendFile(zFile, lenFile, &statbuf)) {
            // 清理 buildins 临时CGI 文件
            if (temp_cgi_path) {
                unlink(temp_cgi_path);
                free(temp_cgi_path);
            }
            return;
        }
    }

    althttpd_fflush(stdout);
    MakeLogEntry(0, 0);  /* LOG: Normal reply */
    omitLog = 1;
    
    // 清理 buildins 临时 CGI 文件
    if (temp_cgi_path) {
        unlink(temp_cgi_path);
        free(temp_cgi_path);
    }
}

// 启动浏览器，打开指定 zPage, 即 URL http://localhost:iPort/zPath
static void launch_web_browser(const char *zPath, int iPort) {

    char zUrl[2000];
    static const char *const azBrowserProg[] = {
#if defined(__DARWIN__) || defined(__APPLE__) || defined(__HAIKU__)
        "open"
#else
        "xdg-open", "gnome-open", "firefox", "google-chrome"
#endif
    };

    if (strlen(zPath) <= sizeof(zUrl) - 1000) {
        while (zPath[0] == '/') zPath++;
        sprintf(zUrl, "http://localhost:%d/%s", iPort, zPath);
        for (size_t i = 0; i < sizeof(azBrowserProg) / sizeof(azBrowserProg[0]); i++) {
            execlp(azBrowserProg[i], azBrowserProg[i], zUrl, (char *) 0);
        }
    }
    althttpd_exit(1);
}

/*
** All possible forms of an IP address.  Needed to work around GCC strict
** aliasing rules.
*/
typedef union {
    struct sockaddr sa;              /* Abstract superclass */
    struct sockaddr_in sa4;          /* IPv4 */
    struct sockaddr_in6 sa6;         /* IPv6 */
    struct sockaddr_storage sas;     /* Should be the maximum of the above 3 */
} address;


// HTTP 监听服务守护进程，监听端口 zPort
/* + 当监听到连接时，fork 出一个子进程来处理连接请求，父进程继续监听
 |   当子进程成功启动时，返回 0；如果无法建立监听套接字，则返回非零值
 |   当 accept() 到一个连接时，套接字 ID 被写入最后一个参数
*/
int http_server(
        int mnPort, int mxPort,   /* Range of TCP ports to try */
        int tlsPort,              /* Additional TCP port for TLS connections */
        int bLocalhost,           /* Listen on loopback sockets only */
        const char *zPage,        /* Launch web browser on this document */
        int *httpConnection,      /* Socket over which HTTP request arrives */
        const char *zPidFile      /* PID file path (nullable) */
) {
    int listener = -1;           /* The server socket */
    int listenTLS = -1;          /* Server socket for separate TLS */
    int mxListener;              /* Maximum of listener and listenTLS */
    int connection;              /* A socket for each individual connection */
    fd_set readfds;              /* Set of file descriptors for select() */
    socklen_t lenaddr;           /* Length of the inaddr structure */
    int child;                   /* PID of the child process */
    int nchildren = 0;           /* Number of child processes */
    struct timeval delay;        /* How long to wait inside select() */
    int opt = 1;                 /* setsockopt flag */

    //---------------------------------------

    // 尝试在 mnPort 到 mxPort 的范围内建立监听套接字（找到第一个可用的端口）

    struct sockaddr_in6 inaddr;  /* The socket address */
    int iPort = mnPort;
    while (iPort <= mxPort) {

        memset(&inaddr, 0, sizeof(inaddr));
        inaddr.sin6_family = AF_INET6;
        inaddr.sin6_addr = bLocalhost ? in6addr_loopback : in6addr_any;
        inaddr.sin6_port = htons(iPort);
        listener = socket(AF_INET6, SOCK_STREAM, 0);
        if (listener < 0) {
            iPort++;
            continue;
        }

        // 如果无法正常终止，至少允许套接字被重用
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (bind(listener, (struct sockaddr *) &inaddr, sizeof(inaddr)) < 0) {
            close(listener);
            iPort++;
            continue;
        }

        break;
    }
    if (iPort > mxPort) {
        if (mnPort == mxPort)
            fprintf(stderr, "unable to open listening socket on port %d\n", mnPort);
        else fprintf(stderr, "unable to open listening socket on any port in the range %d..%d\n", mnPort, mxPort);
        althttpd_exit(1);
    }
    if (iPort > mxPort) return 1;

    zRealPort = StrDup("            ");
    snprintf(zRealPort, 10, "%d", iPort);

    //---------------------------------------

    // 启动对套接字的监听，准备接收连接请求
    listen(listener, 100);
    mxListener = listener;

#ifdef ENABLE_TLS
    if (tlsPort && g_useHttps>=2) {
        memset(&inaddr, 0, sizeof(inaddr));
        inaddr.sin6_family = AF_INET6;
        if (bLocalhost) {
            inaddr.sin6_addr = in6addr_loopback;
        } else{
            inaddr.sin6_addr = in6addr_any;
        }
        inaddr.sin6_port = htons(tlsPort);
        listenTLS = socket(AF_INET6, SOCK_STREAM, 0);
        if( listenTLS<0 ){
            fprintf(stderr,"unable to open listening socket on port %d\n", tlsPort);
            althttpd_exit(1);
        }

        /* if we can't terminate nicely, at least allow the socket to be reused */
        setsockopt(listenTLS,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
        if (bind(listenTLS, (struct sockaddr*)&inaddr, sizeof(inaddr))<0) {
            fprintf(stderr,"unable to open listening socket on port %d\n", tlsPort);
            althttpd_exit(1);
        }
        zRealTlsPort = StrDup("            ");
        snprintf(zRealTlsPort, 10, "%d", tlsPort);
        listen(listenTLS,100);
        printf("Listening for TLS-encrypted HTTPS requests on TCP port %d\n", tlsPort);
        if( tlsPort>mxListener ) mxListener = tlsPort;
    }
#endif

    printf("Listening for %s requests on TCP port %d\n",
           (g_useHttps && tlsPort == 0) ? "TLS-encrypted HTTPS" : "HTTP", iPort);

    // 写入 PID 文件（如果指定了路径）
    if (zPidFile && zPidFile[0]) {
        FILE* fp = fopen(zPidFile, "w");
        if (fp) {
            fprintf(fp, "%d:%d\n", getpid(), iPort);
            fflush(fp);
            fsync(fileno(fp));
            fclose(fp);
        }
    }

    //---------------------------------------

    // 下面 fork 之前刷新缓冲区，避免子进程重复输出缓存中的数据
    // + 同样，下面主进程对 stdout 的输出也需要立即 flush
    fflush(stdout); 

    // 如果需要启动 web 浏览器来打开指定的页面
    if (zPage) {

        child = fork();                     // fork 出一个子进程来执行该任务

        // 对于子进程
        if (child == 0) {

            launch_web_browser(zPage, iPort);

            /* NOT REACHED */
            althttpd_exit(1);
        }
        // 对于父进程，且 fork 成功了
        else if (child > 0) nchildren++;     // 记录子进程数量
    }

    for(;;) {

        int onTLSSocket;    /* True if inbound connection on --tls-port */

        // 使用 select() 来等待监听套接字上的连接请求
        FD_ZERO(&readfds);
        assert(listener >= 0);
        FD_SET(listener, &readfds);
        if (listenTLS > 0) {
            FD_SET(listenTLS, &readfds);
        }
        delay.tv_sec = 0; delay.tv_usec = 100000;
        select(mxListener + 1, &readfds, 0, 0, &delay);

        // 如果监听套接字端口被置位
        onTLSSocket = listenTLS > 0 && FD_ISSET(listenTLS, &readfds);
        if (onTLSSocket || FD_ISSET(listener, &readfds)) {

            // accept() 连接
            lenaddr = sizeof(inaddr);
            connection = accept(onTLSSocket ? listenTLS : listener, (struct sockaddr *) &inaddr, &lenaddr);
            if (connection >= 0) {

                // 如果达到了允许的最大子进程数，则等待一个旧的子进程结束后再创建一个新的子进程来处理连接请求
                int status;  /* Required argument to wait() */
                while (nchildren >= g_mxChild && (child = wait(&status)) >= 0) {
                    nchildren--;
                    /* printf("process %d ends; %d/%d\n",child,nchildren,g_mxChild); fflush(stdout); */
                }

                // 创建请求处理子进程（fork 模型说明）
                // > 进程架构：
                //   · 主进程（当前）：监听端口，accept() 连接，fork 子进程，管理子进程生命周期
                //   · 请求处理子进程：处理单个客户端连接的 HTTP 请求（最多 101 个 Keep-Alive 请求）
                //   · CGI 子子进程（可选）：传统 CGI 通过再次 fork 创建，用于管道通信和进程隔离
                // 
                // > 为什么主进程要 fork 子进程处理请求（而不是直接在主进程处理）：
                //   1. 并发处理：主进程继续监听新连接，多个子进程同时处理不同客户端
                //   2. 故障隔离：请求处理崩溃只影响该子进程，主服务器继续运行
                //   3. 资源清理：子进程退出时系统自动回收所有资源（socket、内存、文件描述符）
                //   4. 安全边界：子进程可设置不同的用户权限、工作目录，限制安全风险
                //   5. 简化编程：每个子进程处理完整请求生命周期，无需复杂的状态机和异步 I/O
                //
                // fork 出一个子进程来处理连接请求
                // + fork 会为子进程复制一份文件描述符表，即增加 fd 的引用计数
                child = fork();
                if (child == 0) {
                    // 以下代码在请求处理子进程中运行
                    // > 职责：处理单个客户端连接的所有 HTTP 请求（最多 101 个 Keep-Alive 请求）
                    // > 生命周期：处理完请求后退出，主进程通过 wait() 回收

                    int nErr = 0, fd;

                    close(0);               // 关闭 stdin
                    fd = dup(connection);   // 将 connection 映射到 stdin 上，即以后可以通过 stdin 来读取连接套接字的数据
                    if (fd != 0) nErr++;

                    close(1);               // 关闭 stdout
                    fd = dup(connection);   // 将 connection 映射到 stdout 上，即以后可以通过 stdout 来写入数据到连接套接字
                    if (fd != 1) nErr++;

                    *httpConnection = fd;   // 将连接套接字的 fd 通过 httpConnection 参数返回给调用者

                    close(connection);      // 关闭 connection（在子进程中的引用），以后使用上面两个新 dup 的引用

                    if (onTLSSocket) {
                        zRealPort = zRealTlsPort;
                    } else if (listenTLS > 0) {
                        g_useHttps = 0;
                        g_zHttps = 0;
                        g_zHttpScheme = "http";
                    }

                    // 同样需要关闭监听套接字 fd（在子进程中的引用）
                    close(listener);
                    if (listenTLS > 0) close(listenTLS);
                    return nErr;
                }

                // 以下代码在主进程中运行，继续监听新连接

                // child < 0 表示 fork 失败，即无法创建子进程来处理连接请求，此时父进程继续监听，但不增加子进程计数
                if (child > 0) nchildren++;

                // 关闭连接套接字 fd（在父进程中的引用）
                close(connection);
                /* printf("subprocess %d starts; %d/%d\n",child,nchildren,g_mxChild); fflush(stdout); */
            }
        }

        // 使用 waitpid (非阻塞的) 回收已经结束的子进程，避免僵尸进程的产生
        // > 第一个参数 0 (pid) 等待任何 进程组ID (Process Group ID) 与当前进程相同的子进程
        //   这里就是等待 “由我 fork 出来的子进程”
        // > 第二个参数 0 (status) 是用来返回子进程退出状态（比如它是正常退出还是被信号杀死的，推出码是多少）的指针。
        //   NULL 意味着父进程 “不在乎它是怎么死的”。父进程只想把它从系统进程表中删除，不关心具体的返回值
        // > WNOHANG: Wait No Hang（等待但不挂起/非阻塞）
        // 函数返回 0 说明没有已经结束的子进程了
        while ((child = waitpid(0, NULL, WNOHANG)) > 0) {
            nchildren--;
            /* printf("process %d ends; %d/%d\n",child,nchildren,g_mxChild); fflush(stdout); */
        }
    }
    /* NOT REACHED */
    althttpd_exit(1);
}

int httpd_main(uint16_t mnPort, uint16_t mxPort,
               bool loopback,
               bool jail,
               const char *csStartPage,
               const char* user,
               const char* root,
               http_params_st* pParams, http_tls_st* pTls,
               const char* pid_file) {

    uid_t runUid;                   // 最终运行 http 服务的用户身份 id
    gid_t runGid;                   // 最终运行 http 服务的用户身份所属的组 id
    int runUserSet = 0;             // 标记是通过什么方式指定的运行所使用的用户身份
                                    // 0: 未指定; 1: 通过 -user 参数主动设置; 2: 根据 -root 目录的所有者自动设置

    if (user) {

        struct passwd *pwd = getpwnam(user);
        if (pwd == NULL) Malfunction(511, "no such user: %s", user);

        runUid = pwd->pw_uid;
        runGid = pwd->pw_gid;
        runUserSet = 1;             // 标记是通过命令参数主动设置的
    }

    if (!mnPort) {
        mnPort = 8080;
        mxPort = 8100;
    }
    else if (!mxPort) {
        mxPort = mnPort;
    }

    if (pParams) {
        g_zRoot = root;
        g_zLogFile = pParams->csLogFile;
        g_zIPShunDir = pParams->csIPShunDir;
        g_zHttpHost = (char*)pParams->csDefaultHost;
        g_mxAge = (int)pParams->iMxAge;
        g_maxCpu = (int)pParams->iMaxCpu;
        g_mxChild = (int)pParams->iMxChild;
        g_enableSAB = pParams->bEnableSAB;
        g_useTimeout = pParams->bUseTimeout;
    }

    // 如果没有指定根目录，则使用当前目录作为根目录，并且以用户权限运行
    if (g_zRoot == NULL) g_zRoot = ".";

    int tlsPort;
    if (pTls) {
        if (pTls == (void*)-1) {
            g_useHttps = 1/* 外置TLS支持 */;
            g_zHttpScheme = "https";
            g_zHttps = "on";
            g_zRemoteAddr = getenv("REMOTE_HOST");
            if (zRealPort == 0) zRealPort = "443";
        }
#ifdef ENABLE_TLS
        else {
            tlsState.zCertFile = pTls->certFile;
            tlsState.zKeyFile = tlsState.zKeyFile ? tlsState.zKeyFile : pTls->certFile;
            tlsPort = pTls->tlsPort;
            g_useHttps = 2/* 内置TLS支持 */;
            g_zHttpScheme = "https";
            g_zHttps = "on";
        }
#endif
    }

    int httpConnection = 0;         // 远程（入站）连接的 socket fd

    //---------------------------------

    // 根据需求，设置超时机制
    if (g_useTimeout) {
        signal(SIGALRM, Timeout);
        signal(SIGSEGV, Timeout);
        signal(SIGPIPE, Timeout);
        signal(SIGXCPU, Timeout);
    }

#if ENABLE_TLS
    /* If the users says --tls-port only, and omits --port, that means they want only TLS.  So adjust the arguments accordingly. */
    if ( mnPort<=0 && tlsPort>0 ) {
        mnPort = mxPort = tlsPort;
        tlsPort = 0;
        if( tlsState.zCertFile==0 ) {
            Malfunction(516/* LOG: No cert specified */, "No -cert specified; cannot use TLS");
        }
    }

    /* We "need" to read the cert before chroot'ing to allow that the
    ** cert is stored in space outside of the --root and not readable by
    ** the --user.
    */
    if ( g_useHttps>=2 && tlsPort ) {
        /* If separate --port and --tls-port options are provided but the
        ** --cert file does not exist, then fall back to ordinary non-TLS
        ** operation on --port.  No complaints.  This enables the same althttpd
        ** command to bring up a non-TLS-only server before a cert is obtained,
        ** and a dual non-TLS/TLS server after the cert is present.
        */
        const char *zFN = tlsState.zCertFile;
        if (zFN==0 || (strcmp(zFN, "unsafe-builtin") != 0 && access(zFN,R_OK)))
            g_useHttps = 0;
    }
    if (g_useHttps>=2) {
        ssl_init_server(tlsState.zCertFile, tlsState.zKeyFile);
    }
    else if( tlsPort ) tlsPort = 0;
#endif

    // 初始化该服务器软件标识名字符串，用于 CGI 的 SERVER_SOFTWARE 信息
    g_zServerSoftware = g_useHttps == 2 ? SERVER_SOFTWARE_TLS : SERVER_SOFTWARE;


    // 将工作目录切换到（HTTP 文件系统的）根目录
    if (chdir(g_zRoot) != 0) {
        Malfunction(517/* LOG: chdir() failed */, "cannot change to directory [%s]", g_zRoot);
    }

    // 如果没有指定用来运行 http 处理的用户身份（且当前是以 root 身份运行的程序）
    if (runUserSet == 0 && getuid() == 0) {

        // 尝试以当前目录的所有者身份来运行 http 请求
        struct stat stat_buf;
        if (stat(".", &stat_buf) == 0) {
            runUid = stat_buf.st_uid;
            runGid = stat_buf.st_gid;
            runUserSet = 2;             // 标记是通过当前目录的所有者身份来自动设置的
        }
    }
    // 验证当前程序运行身份不能是 root 用户
    if (getuid() == 0 && (runUid == 0 || runGid == 0)) {
        static const char *zErrMsg[] = {
                /*0*/ ". Fix by adding \"-user NAME\" or chown the -root directory?",
                /*1*/ " obtained from -user. Maybe pick a different user?",
                /*2*/ " based on the owner of the -root directory. Fix by running chown on that directory so that neither value is 0"
        };
        Malfunction(518, "Cannot run as root, but I have uid=%d, gid=%d%s",
                    (int) runUid, (int) runGid, zErrMsg[runUserSet]);
        return 1;
    }

    // 根据需求，进入沙箱模式
    if (jail && getuid() == 0) {

        // 将当前目录作为根目录（即进入 jail 沙箱）
        if (chroot(".") < 0)
            Malfunction(519/* LOG: chroot() failed */, "unable to create chroot jail");

        else g_zRoot = "";
    }

    // 根据请求，启动 HTTP 服务
    // + 这里主进程进入 http_server() 后，会变为一个监听 HTTP 连接的守护进程，接收 HTTP 请求后会 fork 出子进程来处理请求
    //   也就是说，从 http_server() 返回的，就是 fork 出来的一个用于处理 HTTP 请求的子进程
    // ! 该操作需要在进入沙箱后执行，这样 fork 出来的子进程就会继承沙箱环境
    // ! 注意，这里涉及端口绑定，所以需要在下面降权之前执行
    if (mnPort > 0 && mnPort <= mxPort
        && http_server(mnPort, mxPort, tlsPort, loopback, csStartPage, &httpConnection, pid_file)) {

        Malfunction(520/* LOG: server startup failed */, "failed to start server");
    }

    // 设置 CPU 限制
    // + 该操作需要针对 fork 出的子进程进行设置
    // ! 注意，该操作需要 root 权限，所以需要在下面降权之前执行
#ifdef RLIMIT_CPU
    if (g_maxCpu > 0) {
        struct rlimit rlim;
        rlim.rlim_cur = g_maxCpu;
        rlim.rlim_max = g_maxCpu;
        setrlimit(RLIMIT_CPU, &rlim);
    }
#endif

    // 如果指定了用来运行 http 处理的用户身份
    // + 此时相当于放弃了当前的 root 权限。目的是增加沙箱模式的安全性，避免利用 root 权限来越狱
    if (runUserSet) {

        // 调整当前进程的用户身份为指定的 runUid 和 runGid
        if (setgid(runGid))
            Malfunction(521/* LOG: setgid() failed */, "cannot set group-id to %d", (int) runGid);
        if (setuid(runUid))
            Malfunction(522/* LOG: setuid() failed */, "cannot set user-id to %d", (int) runUid);
    }
    // 验证当前程序运行身份不能是 root 用户
    if (getuid() == 0) {
        Malfunction(524/* LOG: cannot run as root */, "cannot run as root");
    }

    //---------------------------------

    // 获取请求来源的IP地址
    if (g_zRemoteAddr == 0) {

        address remoteAddr;
        socklen_t size = sizeof(remoteAddr);
        char zHost[NI_MAXHOST];
        if (getpeername(0, &remoteAddr.sa, &size) >= 0) {
            getnameinfo(&remoteAddr.sa, size, zHost, sizeof(zHost), 0, 0, NI_NUMERICHOST);
            g_zRemoteAddr = StrDup(zHost);
        }
    }
    if (g_zRemoteAddr != 0
        && strncmp(g_zRemoteAddr, "::ffff:", 7) == 0
        && strchr(g_zRemoteAddr + 7, ':') == 0
        && strchr(g_zRemoteAddr + 7, '.') != 0 ) {

        g_zRemoteAddr += 7;
    }

    // 连续执行 100 + 1 个 HTTP 请求
    // + 也就是在 Keep-Alive 模式下，可以连续持续处理（最多 100 + 1 次）
    //   如果不是 Keep-Alive，或者 connection 被远程 closed，会直接 exit 进程，即不会返回到这里来
    //   最最多 100 次的限制，是为了避免 Keep-Alive 模式下，某个连接持续占用服务器资源过久，导致发生拒绝服务攻击（DoS）
    for (int i = 0; i < 100; i++)
        ProcessOneRequest(0, httpConnection);
    ProcessOneRequest(1, httpConnection);

    tls_close_conn();
    althttpd_exit(0);
    return 0;
}

#if 0
                                                                                                                        /* Copy/paste the following text into SQLite to generate the xref
** table that describes all error codes.
*/
BEGIN;
CREATE TABLE IF NOT EXISTS xref(lineno INTEGER PRIMARY KEY, desc TEXT);
DELETE FROM xref;
INSERT INTO xref VALUES(0,'Normal reply');
INSERT INTO xref VALUES(2,'Normal HEAD reply');
INSERT INTO xref VALUES(100,'Malloc() failed');
INSERT INTO xref VALUES(110,'Not authorized');
INSERT INTO xref VALUES(120,'CGI Error');
INSERT INTO xref VALUES(131,'SIGSEGV');
INSERT INTO xref VALUES(132,'SIGPIPE');
INSERT INTO xref VALUES(133,'SIGXCPU');
INSERT INTO xref VALUES(139,'Unknown signal');
INSERT INTO xref VALUES(140,'CGI script is writable');
INSERT INTO xref VALUES(150,'Cannot open -auth file');
INSERT INTO xref VALUES(160,' http request on https-only page');
INSERT INTO xref VALUES(170,'-auth redirect');
INSERT INTO xref VALUES(180,' malformed entry in -auth file');
INSERT INTO xref VALUES(190,'chdir() failed');
INSERT INTO xref VALUES(200,'bad protocol in HTTP header');
INSERT INTO xref VALUES(201,'URI too long');
INSERT INTO xref VALUES(210,'Empty request URI');
INSERT INTO xref VALUES(220,'Unknown request method');
INSERT INTO xref VALUES(230,'Referrer is devids.net');
INSERT INTO xref VALUES(240,'Illegal content in HOST: parameter');
INSERT INTO xref VALUES(250,'Disallowed user agent');
INSERT INTO xref VALUES(251,'Disallowed user agent (20190424)');
INSERT INTO xref VALUES(260,'Disallowed referrer');
INSERT INTO xref VALUES(270,'Request too large');
INSERT INTO xref VALUES(300,'Path element begins with "." or "-"');
INSERT INTO xref VALUES(310,'URI does not start with "/"');
INSERT INTO xref VALUES(320,'URI too long');
INSERT INTO xref VALUES(330,'Missing HOST: parameter');
INSERT INTO xref VALUES(340,'HOST parameter too long');
INSERT INTO xref VALUES(350,'*.website permissions');
INSERT INTO xref VALUES(360,'chdir() failed');
INSERT INTO xref VALUES(370,'redirect to not-found');
INSERT INTO xref VALUES(380,'URI not found');
INSERT INTO xref VALUES(390,'File not readable');
INSERT INTO xref VALUES(400,'URI is a directory w/o index.html');
INSERT INTO xref VALUES(410,'redirect to add trailing /');
INSERT INTO xref VALUES(440,'pipe() failed');
INSERT INTO xref VALUES(441,'pipe() failed');
INSERT INTO xref VALUES(442,'dup() failed');
INSERT INTO xref VALUES(444,'dup() failed');
INSERT INTO xref VALUES(445,'chdir() failed');
INSERT INTO xref VALUES(460,'Excess URI content past static file name');
INSERT INTO xref VALUES(470,'ETag Cache Hit');
INSERT INTO xref VALUES(480,'fopen() failed for static content');
INSERT INTO xref VALUES(501,'Error initializing the SSL Server');
INSERT INTO xref VALUES(502,'Error loading CERT file');
INSERT INTO xref VALUES(503,'Error loading private key file');
INSERT INTO xref VALUES(504,'Error loading self-signed cert');
INSERT INTO xref VALUES(505,'No cert');
INSERT INTO xref VALUES(506,'private key does not match cert');
INSERT INTO xref VALUES(507,'TlsServerConn');
INSERT INTO xref VALUES(508,'SSL not available');
INSERT INTO xref VALUES(509,'SSL not available');
INSERT INTO xref VALUES(510,'SSL not available');
INSERT INTO xref VALUES(512,'TLS context');
INSERT INTO xref VALUES(513,'unknown IP protocol');
INSERT INTO xref VALUES(514,'cannot open --input file');
INSERT INTO xref VALUES(515,'unknown command-line argument on launch');
INSERT INTO xref VALUES(516,'--root argument missing');
INSERT INTO xref VALUES(517,'chdir() failed');
INSERT INTO xref VALUES(519,'chroot() failed');
INSERT INTO xref VALUES(520,'server startup failed');
INSERT INTO xref VALUES(521,'setgid() failed');
INSERT INTO xref VALUES(522,'setuid() failed');
INSERT INTO xref VALUES(523,'unknown user');
INSERT INTO xref VALUES(524,'cannot run as root');
INSERT INTO xref VALUES(526,'SSL read too big');
INSERT INTO xref VALUES(527,'SSL read error');
INSERT INTO xref VALUES(528,'SSL write too big');
INSERT INTO xref VALUES(529,'Output buffer too small');
INSERT INTO xref VALUES(600,'OOM');
INSERT INTO xref VALUES(610,'OOM');
INSERT INTO xref VALUES(700,'cannot open file');
INSERT INTO xref VALUES(701,'cannot read file');
INSERT INTO xref VALUES(702,'bad SCGI spec');
INSERT INTO xref VALUES(703,'bad SCGI spec (2)');
INSERT INTO xref VALUES(704,'Unrecognized line in SCGI spec');
INSERT INTO xref VALUES(705,'Cannot resolve SCGI server name');
INSERT INTO xref VALUES(706,'bad SCGI fallback');
INSERT INTO xref VALUES(707,'Cannot open socket to SCGI');
INSERT INTO xref VALUES(708,'OOM');
INSERT INTO xref VALUES(720,'chdir() failed');
INSERT INTO xref VALUES(721,'SCGI relight failed');
INSERT INTO xref VALUES(800,'CGI Handler timeout');
INSERT INTO xref VALUES(801,'Timeout request header (1+)');
INSERT INTO xref VALUES(802,'Timeout request header (0)');
INSERT INTO xref VALUES(803,'Timeout POST data');
INSERT INTO xref VALUES(804,'Timeout decode HTTP request');
INSERT INTO xref VALUES(805,'Timeout send static file');
INSERT INTO xref VALUES(806,'Timeout startup');
INSERT INTO xref VALUES(901,'Prohibited remote IP address');
INSERT INTO xref VALUES(902,'Bashdoor attack');
INSERT INTO xref VALUES(903,'CGI reports abusive request');
#endif /* SQL */
