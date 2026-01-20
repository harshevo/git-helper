# Git Master - Git Branch Management System

A fault-tolerant C program for managing Git branches, commits, and merges through an interactive command-line interface.

## Features

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

## Fault Tolerance Features

1. **Pre-merge conflict checking** - The program checks for conflicts BEFORE attempting a merge
2. **Automatic merge abort on conflicts** - If conflicts are detected, the merge is aborted automatically
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

### Compile

```bash
# Build release version
make

# Build debug version (with symbols)
make debug

# Clean build artifacts
make clean
```

### Install

```bash
# Install to /usr/local/bin
sudo make install

# Uninstall
sudo make uninstall
```

## Usage

```bash
# Run the program (from a Git repository)
./git_master

# Show help
./git_master --help

# Enable verbose output
./git_master --verbose
```

### Interactive Menu

The program provides an interactive menu system:

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
  5. View Status
  6. View Log
  0. Exit
```

## Merge Conflict Prevention

The key feature of Git Master is its **conflict-safe merging**:

1. When you select "Merge Branch", the program first performs a **dry-run check**
2. If conflicts would occur, you see a warning like:

```
[WARNING] Merge would result in conflicts!
  CONFLICT (content): Merge conflict in file.txt
[ERROR] Merge blocked: conflicts detected
[INFO] Please resolve conflicts manually or use a different approach
```

3. The merge is **NOT executed** - your repository remains in a clean state
4. You can then:
   - Resolve conflicts manually in each branch first
   - Use a different merge strategy
   - Communicate with team members about the conflicting changes

## File Structure

```
git_master/
├── git_master.h    # Header file with declarations
├── main.c          # Main entry point and menu system
├── utils.c         # Utility functions (command execution, strings, memory)
├── branch.c        # Branch management functions
├── commit.c        # Commit and staging functions
├── merge.c         # Merge operations with conflict detection
├── remote.c        # Remote repository operations
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
// ... and more
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

// Remote operations
gm_error_t push_branch(const char *remote, const char *branch, bool set_upstream);
gm_error_t pull_branch(const char *remote, const char *branch);
```

## License

MIT License

## Contributing

Contributions are welcome! Please ensure:

1. Code follows the existing style
2. All functions have proper error handling
3. Memory is properly managed (no leaks)
4. New features include appropriate user feedback
