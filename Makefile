# Makefile for Git Master - Git Branch Management System
# 
# Usage:
#   make              - Build the CLI program
#   make gui          - Build with GUI support (requires raylib)
#   make daemon       - Build daemon mode only
#   make debug        - Build with debug symbols
#   make clean        - Remove build artifacts
#   make install      - Install to /usr/local/bin
#   make test         - Run basic tests

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LDFLAGS = 
LIBS = -lpthread -ldl

# Debug flags
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2 -DNDEBUG

# GUI flags (optional)
GUI_CFLAGS = -DGUI_ENABLED
GUI_LIBS = -lraylib -lm -lrt

# Build directory
BUILD_DIR = build

# Source files - Core
CORE_SRCS = utils.c branch.c commit.c merge.c remote.c history.c
CORE_OBJS = $(addprefix $(BUILD_DIR)/,$(CORE_SRCS:.c=.o))

# Source files - Extended
EXT_SRCS = config.c daemon.c diff_viewer.c
EXT_OBJS = $(addprefix $(BUILD_DIR)/,$(EXT_SRCS:.c=.o))

# Source files - GUI (optional)
GUI_SRCS = gui.c
GUI_OBJS = $(addprefix $(BUILD_DIR)/,$(GUI_SRCS:.c=.o))

# All source files
CLI_SRCS = main.c $(CORE_SRCS) $(EXT_SRCS)
CLI_OBJS = $(addprefix $(BUILD_DIR)/,$(CLI_SRCS:.c=.o))

FULL_SRCS = $(CLI_SRCS) $(GUI_SRCS)
FULL_OBJS = $(addprefix $(BUILD_DIR)/,$(FULL_SRCS:.c=.o))

# Header files
DEPS = git_master.h config.h

# Target executables (in build directory)
TARGET = $(BUILD_DIR)/git_master
TARGET_GUI = $(BUILD_DIR)/git_master_gui
TARGET_DAEMON = $(BUILD_DIR)/git_master_daemon

# Installation directory
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
CONFDIR = $(HOME)/.config/git_master

# Default target: CLI release build
.PHONY: all
all: CFLAGS += $(RELEASE_FLAGS)
all: $(TARGET)

# Debug build
.PHONY: debug
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

# GUI build (requires raylib)
.PHONY: gui
gui: CFLAGS += $(RELEASE_FLAGS) $(GUI_CFLAGS)
gui: LIBS += $(GUI_LIBS)
gui: $(TARGET_GUI)

# GUI debug build
.PHONY: gui-debug
gui-debug: CFLAGS += $(DEBUG_FLAGS) $(GUI_CFLAGS)
gui-debug: LIBS += $(GUI_LIBS)
gui-debug: $(TARGET_GUI)

# Daemon-only build
.PHONY: daemon
daemon: CFLAGS += $(RELEASE_FLAGS)
daemon: $(BUILD_DIR)/daemon_main.o $(CORE_OBJS) $(EXT_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET_DAEMON) $^ $(LIBS)
	@echo "Built daemon: $(TARGET_DAEMON)"

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@echo "Created build directory: $(BUILD_DIR)/"

# Link the CLI executable
$(TARGET): $(BUILD_DIR) $(CLI_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(CLI_OBJS) $(LIBS)
	@echo ""
	@echo "════════════════════════════════════════════════════════════"
	@echo "Build complete: $(TARGET)"
	@echo "Run with: $(TARGET)"
	@echo "════════════════════════════════════════════════════════════"

# Link the GUI executable
$(TARGET_GUI): $(BUILD_DIR) $(FULL_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(FULL_OBJS) $(LIBS) $(GUI_LIBS)
	@echo ""
	@echo "════════════════════════════════════════════════════════════"
	@echo "Build complete: $(TARGET_GUI)"
	@echo "Run with: $(TARGET_GUI)"
	@echo "════════════════════════════════════════════════════════════"

# Compile source files to build directory
$(BUILD_DIR)/%.o: %.c $(DEPS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# GUI object needs special handling
$(BUILD_DIR)/gui.o: gui.c config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GUI_CFLAGS) -c $< -o $@

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f *.log
	@echo "Cleaned build directory and artifacts"

# Deep clean (including generated scripts)
.PHONY: distclean
distclean: clean
	rm -f git_master_service.sh
	@echo "Removed all generated files"

# Install the program
.PHONY: install
install: all
	@echo "Installing git_master to $(BINDIR)..."
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/git_master
	@echo ""
	@echo "Creating config directory at $(CONFDIR)..."
	install -d $(CONFDIR)
	@echo ""
	@echo "Installation complete!"
	@echo "You can now run 'git_master' from anywhere"
	@echo "Configuration will be stored in $(CONFDIR)"

# Install with GUI
.PHONY: install-gui
install-gui: gui install
	install -m 755 $(TARGET_GUI) $(BINDIR)/git_master_gui
	@echo "GUI version installed as git_master_gui"

# Uninstall the program
.PHONY: uninstall
uninstall:
	@echo "Removing git_master from $(BINDIR)..."
	rm -f $(BINDIR)/git_master
	rm -f $(BINDIR)/git_master_gui
	rm -f $(BINDIR)/git_master_daemon
	@echo "Uninstall complete"
	@echo "Note: Config at $(CONFDIR) was not removed"

# Run basic tests
.PHONY: test
test: $(TARGET)
	@echo "Running basic tests..."
	@echo ""
	@echo "Test 1: Help flag"
	$(TARGET) --help
	@echo ""
	@echo "Test 2: Version flag"
	$(TARGET) --version
	@echo ""
	@echo "All tests passed!"

# Check for memory leaks with valgrind (if available)
.PHONY: memcheck
memcheck: debug
	@echo "Running memory check with valgrind..."
	valgrind --leak-check=full --show-leak-kinds=all $(TARGET) --help

# Static analysis (if cppcheck is available)
.PHONY: analyze
analyze:
	@echo "Running static analysis..."
	cppcheck --enable=all --std=c11 --suppress=missingIncludeSystem $(FULL_SRCS)

# Format code (if clang-format is available)
.PHONY: format
format:
	@echo "Formatting code..."
	clang-format -i $(FULL_SRCS) $(DEPS)

# Check dependencies for GUI build
.PHONY: check-deps
check-deps:
	@echo "Checking dependencies..."
	@which pkg-config > /dev/null || (echo "pkg-config not found" && exit 1)
	@pkg-config --exists raylib && echo "raylib: OK" || echo "raylib: NOT FOUND (GUI will not work)"
	@ldconfig -p | grep -q libnotify && echo "libnotify: OK" || echo "libnotify: NOT FOUND (notifications disabled)"
	@echo ""
	@echo "To install dependencies:"
	@echo "  Ubuntu/Debian: sudo apt install libraylib-dev libnotify4"
	@echo "  Arch Linux: sudo pacman -S raylib libnotify"
	@echo "  Fedora: sudo dnf install raylib-devel libnotify"

# Generate a daemon wrapper script
.PHONY: gen-daemon-script
gen-daemon-script:
	@echo "#!/bin/bash" > git_master_service.sh
	@echo "# Git Master Daemon Service Script" >> git_master_service.sh
	@echo "" >> git_master_service.sh
	@echo 'PIDFILE="/tmp/git_master.pid"' >> git_master_service.sh
	@echo 'LOGFILE="$$HOME/.config/git_master/daemon.log"' >> git_master_service.sh
	@echo "" >> git_master_service.sh
	@echo 'start() {' >> git_master_service.sh
	@echo '    if [ -f "$$PIDFILE" ]; then' >> git_master_service.sh
	@echo '        echo "Daemon already running (PID: $$(cat $$PIDFILE))"' >> git_master_service.sh
	@echo '        exit 1' >> git_master_service.sh
	@echo '    fi' >> git_master_service.sh
	@echo '    echo "Starting Git Master daemon..."' >> git_master_service.sh
	@echo '    nohup $(BINDIR)/git_master --daemon > "$$LOGFILE" 2>&1 &' >> git_master_service.sh
	@echo '    echo $$! > "$$PIDFILE"' >> git_master_service.sh
	@echo '    echo "Daemon started (PID: $$!)"' >> git_master_service.sh
	@echo '}' >> git_master_service.sh
	@echo "" >> git_master_service.sh
	@echo 'stop() {' >> git_master_service.sh
	@echo '    if [ -f "$$PIDFILE" ]; then' >> git_master_service.sh
	@echo '        kill $$(cat "$$PIDFILE") 2>/dev/null' >> git_master_service.sh
	@echo '        rm -f "$$PIDFILE"' >> git_master_service.sh
	@echo '        echo "Daemon stopped"' >> git_master_service.sh
	@echo '    else' >> git_master_service.sh
	@echo '        echo "Daemon not running"' >> git_master_service.sh
	@echo '    fi' >> git_master_service.sh
	@echo '}' >> git_master_service.sh
	@echo "" >> git_master_service.sh
	@echo 'case "$$1" in' >> git_master_service.sh
	@echo '    start) start ;;' >> git_master_service.sh
	@echo '    stop) stop ;;' >> git_master_service.sh
	@echo '    restart) stop; start ;;' >> git_master_service.sh
	@echo '    *) echo "Usage: $$0 {start|stop|restart}" ;;' >> git_master_service.sh
	@echo 'esac' >> git_master_service.sh
	@chmod +x git_master_service.sh
	@echo "Generated git_master_service.sh"

# Show help
.PHONY: help
help:
	@echo "Git Master - Build System"
	@echo ""
	@echo "Build output directory: $(BUILD_DIR)/"
	@echo ""
	@echo "Available targets:"
	@echo "  make            - Build CLI version (default)"
	@echo "  make gui        - Build with raylib GUI"
	@echo "  make debug      - Build CLI with debug symbols"
	@echo "  make gui-debug  - Build GUI with debug symbols"
	@echo "  make daemon     - Build daemon mode only"
	@echo "  make clean      - Remove build directory"
	@echo "  make distclean  - Remove all generated files"
	@echo "  make install    - Install CLI to $(BINDIR)"
	@echo "  make install-gui- Install both CLI and GUI"
	@echo "  make uninstall  - Remove from $(BINDIR)"
	@echo "  make test       - Run basic tests"
	@echo "  make memcheck   - Check for memory leaks (requires valgrind)"
	@echo "  make analyze    - Static analysis (requires cppcheck)"
	@echo "  make check-deps - Check for optional dependencies"
	@echo "  make help       - Show this help"
	@echo ""
	@echo "Build artifacts:"
	@echo "  Executables:  $(BUILD_DIR)/git_master"
	@echo "                $(BUILD_DIR)/git_master_gui"
	@echo "  Objects:      $(BUILD_DIR)/*.o"
	@echo ""
	@echo "Configuration:"
	@echo "  Config file:  ~/.config/git_master/.git_master.conf"
	@echo "  Log file:     ./git_master.log"

# Dependencies
$(BUILD_DIR)/main.o: main.c git_master.h config.h | $(BUILD_DIR)
$(BUILD_DIR)/utils.o: utils.c git_master.h | $(BUILD_DIR)
$(BUILD_DIR)/branch.o: branch.c git_master.h | $(BUILD_DIR)
$(BUILD_DIR)/commit.o: commit.c git_master.h | $(BUILD_DIR)
$(BUILD_DIR)/merge.o: merge.c git_master.h | $(BUILD_DIR)
$(BUILD_DIR)/remote.o: remote.c git_master.h | $(BUILD_DIR)
$(BUILD_DIR)/history.o: history.c git_master.h | $(BUILD_DIR)
$(BUILD_DIR)/config.o: config.c config.h git_master.h | $(BUILD_DIR)
$(BUILD_DIR)/daemon.o: daemon.c config.h git_master.h | $(BUILD_DIR)
$(BUILD_DIR)/diff_viewer.o: diff_viewer.c git_master.h config.h | $(BUILD_DIR)
$(BUILD_DIR)/gui.o: gui.c config.h git_master.h | $(BUILD_DIR)
