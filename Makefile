# WPP Project Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -Ithird_party/tinycc/include -Ithird_party/tinycc
LDFLAGS = -Lthird_party/tinycc/build/compiler -ltcc -lm -ldl -lpthread

# Directories
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include
THIRD_PARTY = third_party

# Targets
TARGET = $(BUILD_DIR)/wpp

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "WPP Project Build System"
	@echo "Available targets:"
	@echo "  all     - Build the project (default)"
	@echo "  clean   - Remove build artifacts"
	@echo "  help    - Show this help message"
