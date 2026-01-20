# Git Master - Advanced Git Branch Management System

A fault-tolerant C program for managing Git branches, commits, and merges through an interactive command-line interface, with optional background daemon and GUI support.

## Features

### Core Git Operations
- **Branch Management**
  - Create new branches from any base branch
  - Switch between branches with uncommitted change warnings
  - List all local and remote branches
  - Delete branches with protection for main/master
  - Rename branches
  - View detailed branch information

- **Commit Management**
  - Stage all changes or specific files
  - Commit with messages
  - View uncommitted changes and diffs
  - Discard changes (with safety confirmations)
  - Stash and pop stash operations
  - View commit log

- **Merge Operations with Conflict Prevention**
  - **Preview merges** before executing (checks for conflicts)
  - **Automatic conflict detection** - prevents merges that would cause conflicts
  - Multiple merge strategies (default, no-ff, squash)
  - Abort ongoing merges
  - Clear warnings when conflicts are detected

- **Remote Operations**
  - Manage remotes (add, remove, list)
  - Fetch from remotes
  - Push to remotes (with or without upstream)
  - Pull from remotes
  - View sync status (ahead/behind)

### History & Restore (Advanced)
- View detailed commit history with formatting
- Show commit details and diffs
- List files changed in any commit
- **Restore files from previous commits**
- **Revert commits** (safe undo - creates new commit)
- **Reset to commits** (with soft/mixed/hard modes)
- **Cherry-pick commits** from other branches
- Compare any two commits
- View reflog (history of all HEAD changes)
- **Recover lost commits** from reflog

### Background Daemon Mode
- **Runs in background** monitoring your repositories
- **Auto-detects** repositories you're working on
- **Polls remotes** for new changes (configurable rate)
- **Desktop notifications** when remote has new commits
- **Conflict warnings** before operations
- Hot-reload configuration without restart

### Configuration System
- INI-style configuration file
- **Customizable keyboard shortcuts**
- **Add repositories** to monitor
- Set polling rate, notification preferences
- Enable/disable features
- Side-by-side diff settings

### Side-by-Side Diff Viewer
- **Colored side-by-side** diff display
- Line numbers for easy reference
- Syntax highlighting
- Adjustable terminal width
- Works with any diff (files, commits, branches)

### Optional GUI (raylib/raygui)
- Modern dark theme interface
- Quick action buttons
- Branch management panel
- Commit with visual feedback
- History browser with scrolling

## Fault Tolerance Features

1. **Pre-merge conflict checking** - Checks for conflicts BEFORE attempting a merge
2. **Automatic merge abort on conflicts** - If conflicts detected, merge is aborted automatically
3. **Uncommitted change warnings** - Warns before operations that might lose uncommitted work
4. **Protected branch warnings** - Extra confirmation for main/master branch operations
5. **Input validation** - Branch names and inputs are validated
6. **Graceful error handling** - All Git operations handle errors gracefully
7. **Memory safety** - All memory allocations are checked and freed properly

## Building

### Prerequisites

- GCC compiler (with C11 support)
- Git installed and in PATH
- POSIX-compatible system (Linux, macOS)
- Optional: libnotify for desktop notifications
- Optional: raylib for GUI

### Compile

```bash
# Build CLI version (default)
make

# Build with debug symbols
make debug

# Build with GUI support (requires raylib)
make gui

# Check dependencies
make check-deps

# Clean build artifacts
make clean
```

### Install

```bash
# Install CLI to /usr/local/bin
sudo make install

# Install CLI and GUI
sudo make install-gui

# Uninstall
sudo make uninstall
```

## Usage

### Interactive CLI Mode

```bash
# Run the program (from a Git repository)
./git_master

# Show help
./git_master --help

# Enable verbose output
./git_master --verbose

# Run in daemon mode (background)
./git_master --daemon
```

### Main Menu

```
╔══════════════════════════════════════════════════════════╗
║             GIT MASTER - Branch Management               ║
╚══════════════════════════════════════════════════════════╝

Repository Status:
  Path: /path/to/repo
  Current Branch: main
  Changes: Clean

=== Main Menu ===

  1. Branch Management
  2. Commit Management
  3. Merge Operations
  4. Remote & Push/Pull
  5. History & Restore
  6. View Status
  7. View Log
  0. Exit
```

## Configuration

Configuration is stored in `~/.config/git_master/.git_master.conf`:

```ini
[daemon]
enabled = true
poll_rate_ms = 2000
auto_fetch = true
auto_detect_repos = true

[notifications]
enabled = true
timeout_ms = 5000
show_on_remote_changes = true
show_on_conflicts = true

[display]
use_colors = true
side_by_side_diff = true
diff_context_lines = 3
terminal_width = 120
show_line_numbers = true

[gui]
enabled = false
window_width = 1200
window_height = 800
theme = dark

[shortcuts]
ctrl+s = status
ctrl+a = stage_all
ctrl+c = commit
ctrl+p = push
ctrl+u = pull
ctrl+f = fetch
ctrl+b = branch_list
ctrl+n = branch_create
ctrl+m = merge
ctrl+l = log
ctrl+d = diff
ctrl+g = open_gui
ctrl+q = quit

[repos]
# Add repositories to monitor
# /path/to/repo = git@github.com:user/repo.git
```

## Merge Conflict Prevention

The key feature of Git Master is its **conflict-safe merging**:

1. When you select "Merge Branch", the program first performs a **dry-run check**
2. If conflicts would occur, you see a warning:

```
[WARNING] Merge would result in conflicts!
  CONFLICT (content): Merge conflict in file.txt
[ERROR] Merge blocked: conflicts detected
[INFO] Please resolve conflicts manually or use a different approach
```

3. The merge is **NOT executed** - your repository remains in a clean state

## Desktop Notifications

When the daemon is running, you'll receive notifications for:
- Remote repository has new commits
- Repository detected when you cd into it
- Merge conflicts detected
- Push/pull/commit operations completed

Requires `libnotify` on Linux:
```bash
# Ubuntu/Debian
sudo apt install libnotify4

# Arch Linux
sudo pacman -S libnotify

# Fedora
sudo dnf install libnotify
```

## File Structure

```
git_master/
├── git_master.h    # Main header with declarations
├── config.h        # Configuration structures
├── main.c          # Main entry point and menu system
├── utils.c         # Utility functions
├── branch.c        # Branch management
├── commit.c        # Commit and staging
├── merge.c         # Merge with conflict detection
├── remote.c        # Remote operations
├── history.c       # History and restore
├── config.c        # Configuration parsing
├── daemon.c        # Background daemon
├── diff_viewer.c   # Side-by-side diff
├── gui.c           # Optional GUI (raylib)
├── Makefile        # Build system
└── README.md       # This file
```

## API Reference

### Error Codes

```c
GM_SUCCESS              // Operation successful
GM_ERR_NOT_GIT_REPO     // Not a git repository
GM_ERR_BRANCH_EXISTS    // Branch already exists
GM_ERR_BRANCH_NOT_FOUND // Branch not found
GM_ERR_MERGE_CONFLICT   // Merge would cause conflicts
GM_ERR_UNCOMMITTED_CHANGES // Uncommitted changes exist
```

### Key Functions

```c
// Branch operations
gm_error_t create_branch(const char *name, const char *base);
gm_error_t switch_branch(const char *name);
gm_error_t delete_branch(const char *name, bool force);

// Commit operations
gm_error_t stage_all_changes(void);
gm_error_t commit_changes(const char *message);

// Merge with conflict detection
gm_error_t check_merge_conflicts(const char *source, bool *has_conflicts);
merge_result_t* merge_branch(const char *source, merge_strategy_t strategy);

// History operations
gm_error_t show_commit_history(int count, bool show_all);
gm_error_t restore_file_from_commit(const char *hash, const char *file);
gm_error_t revert_commit(const char *hash);

// Remote operations
gm_error_t push_branch(const char *remote, const char *branch, bool set_upstream);
gm_error_t pull_branch(const char *remote, const char *branch);

// Configuration
config_t* config_create(void);
gm_error_t config_load(config_t *config, const char *path);

// Daemon
daemon_state_t* daemon_init(config_t *config);
gm_error_t daemon_start(daemon_state_t *daemon);

// Diff viewer
void show_side_by_side_diff(const char *diff_text, display_settings_t *settings);
```

## Running as a Service

To run Git Master as a background service:

```bash
# Generate service script
make gen-daemon-script

# Start daemon
./git_master_service.sh start

# Stop daemon
./git_master_service.sh stop

# Restart daemon
./git_master_service.sh restart
```

## GUI Mode

The optional GUI requires raylib:

```bash
# Ubuntu/Debian
sudo apt install libraylib-dev

# Arch Linux
sudo pacman -S raylib

# Build with GUI
make gui

# Run GUI version
./git_master_gui
```

## License

MIT License

## Contributing

Contributions are welcome! Please ensure:

1. Code follows the existing style
2. All functions have proper error handling
3. Memory is properly managed (no leaks)
4. New features include appropriate user feedback
5. Update documentation for new features
