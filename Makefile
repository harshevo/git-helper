# Makefile for Git Master - Git Branch Management System
# 
# Usage:
#   make          - Build the program
#   make debug    - Build with debug symbols
#   make clean    - Remove build artifacts
#   make install  - Install to /usr/local/bin
#   make test     - Run basic tests

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -D_POSIX_C_SOURCE=200809L
LDFLAGS = 
LIBS = 

# Debug flags
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2 -DNDEBUG

# Source files
SRCS = main.c utils.c branch.c commit.c merge.c remote.c
OBJS = $(SRCS:.c=.o)
DEPS = git_master.h

# Target executable
TARGET = git_master

# Installation directory
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

# Default target: release build
.PHONY: all
all: CFLAGS += $(RELEASE_FLAGS)
all: $(TARGET)

# Debug build
.PHONY: debug
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	@echo ""
	@echo "Build complete: $(TARGET)"
	@echo "Run with: ./$(TARGET)"

# Compile source files
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
	rm -f *.log
	@echo "Cleaned build artifacts"

# Install the program
.PHONY: install
install: all
	@echo "Installing $(TARGET) to $(BINDIR)..."
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	@echo "Installation complete!"
	@echo "You can now run '$(TARGET)' from anywhere"

# Uninstall the program
.PHONY: uninstall
uninstall:
	@echo "Removing $(TARGET) from $(BINDIR)..."
	rm -f $(BINDIR)/$(TARGET)
	@echo "Uninstall complete"

# Run basic tests
.PHONY: test
test: $(TARGET)
	@echo "Running basic tests..."
	@echo ""
	@echo "Test 1: Help flag"
	./$(TARGET) --help
	@echo ""
	@echo "Test 2: Version flag"
	./$(TARGET) --version
	@echo ""
	@echo "All tests passed!"

# Check for memory leaks with valgrind (if available)
.PHONY: memcheck
memcheck: debug
	@echo "Running memory check with valgrind..."
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET) --help

# Static analysis (if cppcheck is available)
.PHONY: analyze
analyze:
	@echo "Running static analysis..."
	cppcheck --enable=all --std=c11 $(SRCS)

# Format code (if clang-format is available)
.PHONY: format
format:
	@echo "Formatting code..."
	clang-format -i $(SRCS) $(DEPS)

# Show help
.PHONY: help
help:
	@echo "Git Master - Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make          - Build release version"
	@echo "  make debug    - Build debug version"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make install  - Install to $(BINDIR)"
	@echo "  make uninstall- Remove from $(BINDIR)"
	@echo "  make test     - Run basic tests"
	@echo "  make memcheck - Check for memory leaks (requires valgrind)"
	@echo "  make analyze  - Static analysis (requires cppcheck)"
	@echo "  make format   - Format code (requires clang-format)"
	@echo "  make help     - Show this help"

# Dependencies
main.o: main.c git_master.h
utils.o: utils.c git_master.h
branch.o: branch.c git_master.h
commit.o: commit.c git_master.h
merge.o: merge.c git_master.h
remote.o: remote.c git_master.h
