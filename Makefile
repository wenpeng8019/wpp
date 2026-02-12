# WPP Project Makefile

CC = gcc
# 默认 debug 模式（可断点调试）
CFLAGS = -Wall -Wextra -g -O0 -Iinclude -Isrc -Ithird_party/tinycc/include -Ithird_party/tinycc -Ithird_party/uthash/include -Ithird_party/sqlite -Ithird_party/yyjson/src -Ithird_party/zlib
# Release 模式优化选项
CFLAGS_RELEASE = -Wall -Wextra -O2 -Iinclude -Isrc -Ithird_party/tinycc/include -Ithird_party/tinycc -Ithird_party/uthash/include -Ithird_party/sqlite -Ithird_party/yyjson/src -Ithird_party/zlib
# 链接顺序：先 TCC，再其他库，最后系统库
LDFLAGS = -Lthird_party/tinycc/build/compiler -ltcc -lm -ldl -lpthread

# Directories
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include
THIRD_PARTY = third_party
BUILDINS_DIR = buildins

# BUILDINS 自动生成（脚本预构建）
BUILDINS_SCRIPT = tools/make_buildins.sh
BUILDINS_SOURCES = $(SRC_DIR)/buildins/sysroot.c
BUILDINS_HEADERS = $(SRC_DIR)/buildins/sysroot.h
BUILDINS_INPUT_FILES = $(shell find $(BUILDINS_DIR) -type f 2>/dev/null)

# Third-party source files
SQLITE_SRC = $(THIRD_PARTY)/sqlite/sqlite3.c
YYJSON_SRC = $(THIRD_PARTY)/yyjson/src/yyjson.c
ZLIB_SRCS = $(wildcard $(THIRD_PARTY)/zlib/*.c)

# Targets
TARGET = $(BUILD_DIR)/wpp

# Source files
# sysroot.c 编译为 sysroot.o（脚本生成的资源数据）
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS)) $(BUILD_DIR)/sysroot.o
THIRD_PARTY_OBJS = $(BUILD_DIR)/sqlite3.o $(BUILD_DIR)/yyjson.o $(patsubst $(THIRD_PARTY)/zlib/%.c,$(BUILD_DIR)/zlib_%.o,$(ZLIB_SRCS))

.PHONY: all clean buildins debug release stripped

# 默认目标：debug 版本（可断点调试）
all: debug

# Debug 版本（-g -O0，可断点调试）
debug: $(BUILD_DIR) buildins $(TARGET)

# Release 版本（-O2 优化）
release: CFLAGS = $(CFLAGS_RELEASE)
release: clean $(BUILD_DIR) buildins $(TARGET)
	@echo "✓ Release build complete: $(TARGET)"
	@ls -lh $(TARGET)

# Stripped 版本（-O2 优化 + strip 符号表）
stripped: CFLAGS = $(CFLAGS_RELEASE)
stripped: clean $(BUILD_DIR) buildins $(TARGET)
	@echo "Stripping debug symbols..."
	@strip $(TARGET)
	@echo "✓ Stripped build complete: $(TARGET)"
	@ls -lh $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# BUILDINS 资源自动生成：当 buildins/ 目录内容或脚本变化时重新生成
buildins: $(BUILDINS_SOURCES) $(BUILDINS_HEADERS)

$(BUILDINS_SOURCES) $(BUILDINS_HEADERS): $(BUILDINS_INPUT_FILES) $(BUILDINS_SCRIPT)
	@echo "Generating embedded BUILDINS resources..."
	@bash $(BUILDINS_SCRIPT)

$(TARGET): $(OBJS) $(THIRD_PARTY_OBJS)
	$(CC) $(OBJS) $(THIRD_PARTY_OBJS) -o $@ $(LDFLAGS)

# 脚本生成的 sysroot 资源数据
$(BUILD_DIR)/sysroot.o: $(BUILDINS_SOURCES) $(BUILDINS_HEADERS)
	$(CC) $(CFLAGS) -c $(BUILDINS_SOURCES) -o $@

# Main source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Third-party libraries
$(BUILD_DIR)/sqlite3.o: $(SQLITE_SRC)
	$(CC) $(CFLAGS) -DSQLITE_ENABLE_JSON1 -DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_RTREE -c $< -o $@

$(BUILD_DIR)/yyjson.o: $(YYJSON_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/zlib_%.o: $(THIRD_PARTY)/zlib/%.c
	$(CC) $(CFLAGS) -Wno-implicit-fallthrough -Wno-implicit-function-declaration -c $< -o $@

# clean 只删除构建产物，不删除脚本预构建的 sysroot.c/h
clean:
	rm -rf $(BUILD_DIR)

# 完全清理（包括预构建文件）
distclean: clean
	rm -f $(BUILDINS_SOURCES) $(BUILDINS_HEADERS)

help:
	@echo "WPP Project Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build debug version (default, -g -O0)"
	@echo "  debug     - Build debug version (same as all)"
	@echo "  release   - Build optimized version (-O2)"
	@echo "  stripped  - Build optimized and stripped version (-O2 + strip)"
	@echo "  buildins  - (Re)generate sysroot resources"
	@echo "  clean     - Remove build artifacts"
	@echo "  distclean - Remove everything including generated sysroot"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Build modes:"
	@echo "  debug    - Includes debug symbols, no optimization, for debugging"
	@echo "  release  - Optimized for performance (-O2)"
	@echo "  stripped - Optimized and stripped, smallest binary size"