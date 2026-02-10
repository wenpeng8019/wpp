# WPP - Web Programming Platform

> **Single-command web server: Any folder → Web app with SQLite+SQTP queries and C script CGI**

Transform any folder into a fully-featured web application with one command. No complex setup, no configuration files—just pure simplicity.

```bash
./wpp
# Server started on http://localhost:8080
# Your folder is now a web app! 🚀
```

## ✨ Core Features

### 🗄️ **SQTP Protocol - RESTful Database Queries**
Query SQLite databases directly via HTTP using SQL-like URLs:

```bash
# SELECT query
curl "http://localhost:8080/SELECT%20*%20FROM%20users%20WHERE%20status='active'"

# INSERT data
curl -X POST "http://localhost:8080/INSERT%20INTO%20users%20VALUES('Alice',25)"

# JSON response ready for JavaScript fetch()
```

**JavaScript Client:**
```javascript
// Use provided SQTP client library
fetch('/shm', {
    method: 'SELECT',
    headers: { 'TABLE': 'users', 'WHERE': 'age > 18' }
}).then(r => r.json()).then(data => console.log(data));
```

### 🔥 **C Script CGI - Runtime Compilation**
Write `.c` files as CGI scripts—they compile and run on-the-fly via TinyCC:

**hello.c:**
```c
#include <stdio.h>
int main() {
    printf("Content-Type: text/html\r\n\r\n");
    printf("<h1>Hello from C!</h1>");
    printf("<p>Query: %s</p>", getenv("QUERY_STRING"));
    return 0;
}
```

Access: `http://localhost:8080/hello.c?name=world` ✅ No compilation needed!

### ⚡ **One-Command Deployment**
```bash
# Build once
make

# Run anywhere
./build/wpp
# Opens browser automatically → Your web app is live!
```

## 🚀 Quick Start

### Prerequisites
- macOS / Linux
- CMake 3.10+
- C compiler (GCC/Clang)

### Build & Run
```bash
# 1. Clone & Build
git clone https://github.com/wenpeng8019/wpp.git
cd wpp
mkdir build && cd build
cmake .. && make

# 2. Start server (from project root)
cd ..
./build/wpp

# 3. Test C CGI
curl http://localhost:8080/hello.c

# 4. Test SQTP query
curl "http://localhost:8080/SELECT%20*%20FROM%20users"
```

### Create Your First C Script
```bash
cat > test.c << 'EOF'
#include <stdio.h>
#include <time.h>

int main() {
    printf("Content-Type: text/html\r\n\r\n");
    printf("<h1>Server Time</h1>");
    
    time_t now = time(NULL);
    printf("<p>%s</p>", ctime(&now));
    
    return 0;
}
EOF

curl http://localhost:8080/test.c
# ✨ C script executed instantly!
```

## 📖 Use Cases

### 1. **Rapid Prototyping**
Build data-driven web apps without backend frameworks:
```c
// api.c - Dynamic API endpoint
#include <stdio.h>
#include <sqlite3.h>

int main() {
    printf("Content-Type: application/json\r\n\r\n");
    
    sqlite3 *db;
    sqlite3_open("data.db", &db);
    // ... query and output JSON
    
    return 0;
}
```

### 2. **Database Web Interface**
Query SQLite databases via browser with SQTP JavaScript client:
```html
<script src="/lib/sqtp/sqtp.fetch.js"></script>
<script>
// Real-time database queries from frontend
sqtp.select('/shm', 'users', 'age > 18')
    .then(rows => renderTable(rows));
</script>
```

### 3. **Embedded Web UI**
Perfect for IoT devices, local tools, or embedded systems—single binary, no dependencies.

## 🏗️ Architecture

**Three-Layer Process Model:**
```
Main Process (Port 8080)
  └─ Request Handler (per connection)
      └─ CGI Subprocess (C script execution)
          ├─ Traditional CGI → execl(binary)
          └─ C Script → TinyCC compile + run
```
📚 Documentation

- [QUICKSTART.md](docs/QUICKSTART.md) - Detailed getting started guide
- [SQTP Protocol](docs/sqtp/) - Complete SQTP specification (8 operations)
- [Architecture](docs/design/ARCHITECTURE.md) - System design and internals
- [TinyCC Mechanism](third_party/TINYCC_MECHANISM.md) - How C script compilation works

## 🛠️ Advanced Configuration

### Custom Port & Root Directory
```bash
./build/wpp --port 8080 --root /path/to/webroot
```

### Multiple Instances
```bash
# Instance 1
./build/wpp --port 8080 &

# Instance 2  
./build/wpp --port 8081 &
```

### C Script CGI Environment
All standard CGI variables are available:
```c
getenv("REQUEST_METHOD");  // GET, POST
getenv("QUERY_STRING");     // URL parameters
getenv("REMOTE_ADDR");      // Client IP
getenv("HTTP_USER_AGENT");  // Browser info
// ... 30 CGI environment variables
```

## 🤝 Contributing

Contributions welcome! Areas of interest:
- 🌟 More SQTP operations (JOIN, GROUP BY)
- 🔒 HTTPS/TLS support
- 📦 Package managers (Homebrew, apt)
- 🧪 Test coverage
- 📝 Documentation improvements

## 📄 License

**GPL 3.0** - Compatible with all included components:

| Component | License | Usage |
|-----------|---------|-------|
| SQLite | Public Domain | Embedded database |
| TinyCC | LGPL 2.1 | Runtime C compiler |
| althttpd | Public Domain | HTTP server base |
| yyjson | MIT | JSON parser |

See [LICENSE](LICENSE), [NOTICE](NOTICE), and [LICENSES.md](LICENSES.md) for details.

## 🙏 Acknowledgments

Built on the shoulders of giants:
- **SQLite** - D. Richard Hipp and team
- **TinyCC** - Fabrice Bellard
- **althttpd** - D. Richard Hipp
- **yyjson** - YaoYuan

## 🎯 Project Status

**Version**: 0.1.0  
**Status**: Alpha (Production-ready core, active development)

**Completed:**
- ✅ HTTP server with fork-based concurrency
- ✅ SQTP protocol (8 operations)
- ✅ TinyCC C script CGI
- ✅ SQLite integration with shared memory
- ✅ JavaScript client libraries
- ✅ Process isolation and security model

**Roadmap:**
- 🚧 SQTP JOIN operations
- 🚧 WebSocket support
- 🚧 HTTPS/TLS
- 🚧 Performance optimizations
- 🚧 Extended platform support (Windows)

---

**Made with ❤️ for rapid web development**  
**Star ⭐ this repo if you find it useful!**

GitHub: [wenpeng8019/wpp](https://github.com/wenpeng8019/wpp)
- [ ] SQLite 集成和封装
- [ ] TinyCC 集成和封装
- [ ] althttpd 现代化改造
- [ ] 完整的示例应用
- [ ] 性能优化
- [ ] 文档完善

---

**License**: GPL 3.0 (compatible with LGPL 2.1 and Public Domain)  
**Version**: 0.1.0  
**Status**: Alpha / Development
