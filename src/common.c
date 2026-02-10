//
// Created by 温朋 on 2026/1/24.
//
#include <common.h>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

const char* VAL_TYPE_NAME[VT_NUM] = {
        "VT_I8",   "VT_I16",  "VT_I32",  "VT_I64",
        "VT_U8",   "VT_U16",  "VT_U32",  "VT_U64",
        "VT_F32",  "VT_F64"
};

static log_cb                   g_cb_log = (log_cb)-1;              // 默认 (-1) stdout 输出
static bool                     g_tag_separate = false;

#ifdef printf
#undef printf
#endif

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief                       规范化目录路径（原地修改）
 *                              1. 替换 ~ 为 HOME 目录
 *                              2. 移除末尾的 / (根目录除外)
 *                              3. 合并连续的 //
 *                              4. 处理 /./ 为 /
 * @note                        仅当 ~ 展开时分配新内存
 *                              注意，这里分配的内存，其生命周期与进程一致，所以无需释放
 */
static char* normalize_dir_path(char* path) {
    if (!path || !*path) return path;

    char* result = path;

    // 处理 ~ 开头的路径（需要分配新内存）
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char* home = getenv("HOME");
        if (!home) home = "/tmp";  // fallback
        result = (char*)malloc(strlen(home) + strlen(path));  // len 已包含 ~，足够
        strcpy(result, home);
        if (path[1] != '\0') strcat(result, path + 1);
    }

    // 合并连续的 // 和处理 /./
    char *q = result, *p = result;
    while (*p) {
        if (*p == '/') {
            if (p[1] == '/') { ++p; continue; }                 // 跳过连续的 /
            if (p[1] == '.' && (p[2] == '/' || p[2] == '\0')) { // 跳过 /.
                p += 2; if (*p == '\0') break; continue;
            }
        }
        *q++ = *p++;
    }
    *q = '\0';

    // 移除末尾的 / (根目录除外)
    size_t len = q - result;
    while (len > 1 && result[--len] == '/') result[len] = '\0';

    return result;
}

int ARGS_ls_count(arg_var_st* var) {
    if (var->ls == NULL) return 0;
    return *(int*)&var->ls[-1];
}

static arg_def_st*  g_args_def = NULL;
static int          g_pos_count = 0;    // 位置参数数量
static bool         g_has_req = false;  // 是否有必选选项
static const char*  g_pos_desc = NULL;  // 位置参数描述（如 "<subcommand>"）
static const char*  g_usage_ex = NULL;  // 使用示例说明

void ARGS_usage(const char* pos_desc, const char* usage_ex) {
    g_pos_desc = pos_desc;
    g_usage_ex = usage_ex;
}

int ARGS_parse(int argc, char** argv, ...) {

    if (g_args_def) return 1;

    va_list args; va_start(args, argv);
    arg_def_st* def; int req_count = 0;  // 必选参数计数
    while ((def = va_arg(args, arg_def_st*)) != NULL) {
        def->next = g_args_def;
        g_args_def = def;
        if (def->req) req_count++;
    }
    va_end(args);
    g_has_req = (req_count > 0);

    int show_help = (argc == 1);
    g_pos_count = 0;
    char* pos_args[argc];  // 临时存储位置参数
    int w = 1;             // 写入位置（选项及其值前移到这里）

    for (int i = 1; i < argc; i++) { char* arg = argv[i];

        // 检查是否请求帮助
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            show_help = 1;
            break;
        }

        // -- 终止符：后续都是位置参数
        if (strcmp(arg, "--") == 0) {
            for (i++; i < argc; i++) {
                pos_args[g_pos_count++] = argv[i];
            }
            break;
        }

        // 长选项 --xxx
        if (arg[0] == '-' && arg[1] == '-') {
            for (def = g_args_def; def != NULL; def = def->next) {
                if (def->l && strcmp(def->l, arg + 2) == 0) break;
            }
            if (def == NULL) {
                fprintf(stderr, "Error: Unknown option '%s'\n", arg);
                exit(-1);
            }
            if (def->req) req_count--;

            argv[w++] = arg;  // 前移选项

            if (def->type == ARG_BOOL) {
                def->var->i64 = 1;
            } else if (def->type == ARG_LS) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: Option '%s' requires a value\n", arg);
                    exit(-1);
                }
                int count = 0;
                for (int k = i + 1; k < argc && argv[k][0] != '-'; k++) count++;
                def->var->ls = (const char**)&argv[w];
                *(int*)&argv[w - 1] = count;  // 重载选项位置为数量
                for (int k = 0; k < count; k++) {
                    argv[w++] = argv[++i];
                }
            } else {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: Option '%s' requires a value\n", arg);
                    exit(-1);
                }
                argv[w++] = argv[++i];  // 前移选项值
                switch (def->type) {
                    case ARG_INT:   def->var->i64 = strtoll(argv[i], NULL, 10); break;
                    case ARG_FLOAT: def->var->f64 = strtod(argv[i], NULL); break;
                    case ARG_STR:   def->var->str = argv[i]; break;
                    case ARG_DIR:   def->var->str = normalize_dir_path((char*)argv[i]); break;
                    default: break;
                }
            }
        }
            // 短选项 -x 或 -abc 组合形式
        else if (arg[0] == '-' && arg[1] != '\0') {
            argv[w++] = arg;  // 前移选项

            for (int j = 1; arg[j] != '\0'; j++) { char c = arg[j];

                for (def = g_args_def; def != NULL; def = def->next) {
                    if (def->s == c) break;
                }
                if (def == NULL) {
                    fprintf(stderr, "Error: Unknown option '-%c'\n", c);
                    exit(-1);
                }
                if (def->req) req_count--;

                if (def->type == ARG_BOOL) {
                    def->var->i64 = 1;
                } else if (def->type == ARG_LS) {
                    if (i + 1 >= argc) {
                        fprintf(stderr, "Error: Option '-%c' requires a value\n", c);
                        exit(-1);
                    }
                    int count = 0;
                    for (int k = i + 1; k < argc && argv[k][0] != '-'; k++) count++;
                    def->var->ls = (const char**)&argv[w];
                    *(int*)&argv[w - 1] = count;
                    for (int k = 0; k < count; k++) {
                        argv[w++] = argv[++i];
                    }
                    break;
                } else {
                    const char* value;
                    if (arg[j + 1] != '\0') {
                        value = &arg[j + 1];
                    } else if (i + 1 < argc) {
                        argv[w++] = argv[++i];
                        value = argv[i];
                    } else {
                        fprintf(stderr, "Error: Option '-%c' requires a value\n", c);
                        exit(-1);
                    }
                    switch (def->type) {
                        case ARG_INT:   def->var->i64 = strtoll(value, NULL, 10); break;
                        case ARG_FLOAT: def->var->f64 = strtod(value, NULL); break;
                        case ARG_STR:   def->var->str = value; break;
                        case ARG_DIR:   def->var->str = normalize_dir_path((char*)value); break;
                        default: break;
                    }
                    break;
                }
            }
        }
            // 位置参数（非选项参数）
        else {
            pos_args[g_pos_count++] = arg;
        }
    }

    // 将位置参数放到选项之后
    for (int i = 0; i < g_pos_count; i++) {
        argv[w + i] = pos_args[i];
    }

    // 检查必选参数是否都已提供
    if (req_count > 0 && !show_help) {
        fprintf(stderr, "Error: Missing required options:\n");
        for (def = g_args_def; def != NULL; def = def->next) {
            if (def->req) {
                // 检查是否已设置（对于不同类型检查不同）
                bool missing = false;
                switch (def->type) {
                    case ARG_STR:
                    case ARG_DIR:   missing = (def->var->str == NULL); break;
                    case ARG_LS:    missing = (def->var->ls == NULL); break;
                    default:        missing = false; break;  // INT/FLOAT/BOOL 无法判断是否设置
                }
                if (missing) {
                    fprintf(stderr, "  --%s (-%c)\n", def->l, def->s);
                }
            }
        }
        exit(-1);
    }

    // 反转链表顺序（因为构建时是倒序插入的）
    arg_def_st* prev = NULL; def = g_args_def;
    while (def) {
        arg_def_st* next = def->next;
        def->next = prev;
        prev = def;
        def = next;
    }
    g_args_def = prev;

    // 解析完成后，如果需要帮助则遍历链表打印
    if (show_help) {
        ARGS_print(argv[0]);
        exit(argc == 1 ? -1 : 0);
    }

    return g_pos_count;
}

int ARGS_print(const char* arg0) {

    // 动态生成 Usage 信息
    const char* opt_str = g_args_def ? (g_has_req ? "OPTIONS" : "[OPTIONS]") : "";
    const char* arg_str = g_pos_desc ? g_pos_desc : (g_pos_count > 0 ? "ARGS..." : "");
    printf("Usage: %s%s%s%s%s\n", arg0,
           *arg_str ? " " : "", arg_str,
           *opt_str ? " " : "", opt_str);

    // 先输出位置参数/子命令说明
    if (g_usage_ex && *g_usage_ex) {
        putchar('\n');
        // 输出 usage_ex，遇到 $0 替换为程序名
        for (const char* p = g_usage_ex; *p; p++) {
            if (p[0] == '$' && p[1] == '0') {
                fputs(arg0, stdout);  // fputs 不会输出换行符，puts 才会
                p++;  // 跳过 '0'
            } else {
                putchar(*p);
            }
        }
    }

    // 再输出选项列表
    if (g_args_def) {

        // 先遍历计算最大长选项名长度
        int max_l_len = 0;
        for (arg_def_st* def = g_args_def; def != NULL; def = def->next) {
            int len = def->l ? (int)strlen(def->l) : 0;
            if (len > max_l_len) max_l_len = len;
        }

        printf("\nOptions:\n");
        for (arg_def_st* def = g_args_def; def != NULL; def = def->next) {
            const char* type_str = "";
            switch (def->type) {
                case ARG_INT:   type_str = "<int>"; break;
                case ARG_FLOAT: type_str = "<float>"; break;
                case ARG_BOOL:  type_str = ""; break;
                case ARG_STR:   type_str = "<string>"; break;
                case ARG_DIR:   type_str = "<dir>"; break;
                case ARG_LS:    type_str = "<list>"; break;
            }
            printf("  -%c, --%-*s  %-10s %s%s\n", def->s, max_l_len, def->l, type_str,
                   def->req ? "\033[31m[required]\033[0m " : "[optional] ", def->desc);
        }
    }

    putchar('\n');
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

void log_output(log_cb cb_log, bool tag_separate) {
    g_cb_log = cb_log;
    g_tag_separate = tag_separate;
}

void log_slot(log_level_e level, const char *tag, const char *fmt, va_list params) {

    static __thread char        g_line[LOG_LINE_MAX];
    static __thread int         g_logging = -1;
#ifndef LOG_FILE_DISABLED
    static void*                g_cache_head, *g_cache_rear;
    static uint32_t             g_line_count;
    static pthread_mutex_t      g_cache_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

    // 如果 fmt 为空，表示输出日志到缓存
    if (!tag) {

        // 开启缓存模式，并 rewind 到开头
        if (!fmt) {
            g_logging = 0;
            return;
        }
        if (g_logging >= LOG_LINE_MAX - 1) { // 缓存已满，忽略后续内容
            fprintf(stderr, "W: log buffer full, ignore log content\n");
            return;
        }
        if (g_logging < 0) g_logging = 0;
        g_logging += vsnprintf(g_line + g_logging, LOG_LINE_MAX - g_logging, fmt, params);
        return;
    }

    // 如果之前开启了缓存模式，且缓存内容为空，则关闭缓存模式，直接返回
    if (g_logging == 0) {
        g_logging = -1;
        return;
    }

    int n;
    if (g_logging > 0) {                    // 对于缓存模式的最终输出
        n = g_logging; g_logging = -1;      // 关闭缓存模式
        n += vsnprintf(g_line+n, sizeof(g_line)-n, fmt, params);
    } else if (g_tag_separate) {
        n = vsnprintf(g_line, sizeof(g_line), fmt, params);
    } else {
        n = snprintf(g_line, sizeof(g_line), "%s", tag);
        if (n < LOG_LINE_MAX - 1) {
            g_line[n++] = ' ';
            n += vsnprintf(g_line + n, LOG_LINE_MAX - n, fmt, params);
        }
    }

    if (n >= LOG_LINE_MAX) { n = LOG_LINE_MAX - 1;
        g_line[n] = 0;
    }

    // 对于标准输出
    if (g_cb_log == (log_cb)-1) {
        if (g_line[--n] == '\n') g_line[n] = 0; // 移除末尾换行符（避免重复添加）
        printf("%s\n", g_line);                 // 标准输出自动添加换行符
    } else if (g_cb_log) {
        g_cb_log(level, g_tag_separate ? tag : NULL, g_line, n);
    }

#ifndef LOG_FILE_DISABLED

    pthread_mutex_lock(&g_cache_mutex);

      static bool log_append = false;
      static char log_fn[256];
      if (!log_append) {
          for(int i=0;;) {
              sprintf(log_fn, "%s/%s%d.txt", LOG_OUTPUT_PATH, LOG_NAME, ++i);
              if (access(log_fn, F_OK) < 0) break;
          }
      }

      FILE* fp = fopen(log_fn, log_append?"a":"w");
      if (fp) {

          if (g_cache_head) {
              do {
                  void** item = (void**)g_cache_head;
                  fputs((char*)(item+1), fp);
                  g_cache_head = *item;
                  free(item);
              }
              while (g_cache_head);
              g_cache_rear = NULL;
              g_line_count = 0;
          }
          fprintf(fp, "%s %s\n", tag, g_line);
          fclose(fp);

          log_append = true;
      }
      else {

#if LOG_TAG_SEPARATE
          void** item = (void**)malloc(n + strlen(tag) + 3/* ' ' + \r\0 */ + sizeof(void*)); *item = NULL;
          sprintf((char*)(item+1), "%s %s\n", tag, g_line);
#else
          void** item = (void**)malloc(len + 2/* \r\0 */ + sizeof(void*)); *item = NULL;
          sprintf((char*)(item+1), "%s\n", g_line);
#endif
          if (g_cache_rear) *(void**)g_cache_rear = item;
          else g_cache_head = item;
          g_cache_rear = item;

          if (++g_line_count > 1000) {
              void* next = *(void**)g_cache_head;
              free(g_cache_head);
              g_cache_head = next;
          }
      }

      pthread_mutex_unlock(&g_cache_mutex);
#endif
}

///////////////////////////////////////////////////////////////////////////////
#pragma clang diagnostic pop