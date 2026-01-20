/**
 * git_master.h - Git Branch Management System
 * 
 * A fault-tolerant C program for managing Git branches, commits, and merges.
 * 
 * Author: Git Master
 * License: MIT
 */

#ifndef GIT_MASTER_H
#define GIT_MASTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

/* ============================================================================
 * Constants and Configuration
 * ============================================================================ */

#define MAX_BRANCH_NAME     256
#define MAX_COMMIT_MSG      1024
#define MAX_PATH_LEN        4096
#define MAX_COMMAND_LEN     8192
#define MAX_OUTPUT_LEN      65536
#define MAX_BRANCHES        1024
#define MAX_REMOTES         64

/* Error codes */
typedef enum {
    GM_SUCCESS = 0,
    GM_ERR_NOT_GIT_REPO = -1,
    GM_ERR_BRANCH_EXISTS = -2,
    GM_ERR_BRANCH_NOT_FOUND = -3,
    GM_ERR_INVALID_BRANCH_NAME = -4,
    GM_ERR_UNCOMMITTED_CHANGES = -5,
    GM_ERR_MERGE_CONFLICT = -6,
    GM_ERR_COMMAND_FAILED = -7,
    GM_ERR_MEMORY_ALLOC = -8,
    GM_ERR_INVALID_INPUT = -9,
    GM_ERR_REMOTE_NOT_FOUND = -10,
    GM_ERR_PUSH_FAILED = -11,
    GM_ERR_PULL_FAILED = -12,
    GM_ERR_NO_COMMITS = -13,
    GM_ERR_CHECKOUT_FAILED = -14,
    GM_ERR_DELETE_CURRENT = -15,
    GM_ERR_PROTECTED_BRANCH = -16,
    GM_ERR_IO_ERROR = -17,
    GM_ERR_UNKNOWN = -99
} gm_error_t;

/* Branch status */
typedef enum {
    BRANCH_STATUS_CLEAN = 0,
    BRANCH_STATUS_MODIFIED = 1,
    BRANCH_STATUS_STAGED = 2,
    BRANCH_STATUS_AHEAD = 3,
    BRANCH_STATUS_BEHIND = 4,
    BRANCH_STATUS_DIVERGED = 5
} branch_status_t;

/* Merge strategy */
typedef enum {
    MERGE_STRATEGY_DEFAULT = 0,
    MERGE_STRATEGY_NO_FF = 1,
    MERGE_STRATEGY_SQUASH = 2,
    MERGE_STRATEGY_REBASE = 3
} merge_strategy_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/* Command execution result */
typedef struct {
    int exit_code;
    char *output;
    char *error;
    size_t output_len;
    size_t error_len;
} cmd_result_t;

/* Branch information */
typedef struct {
    char name[MAX_BRANCH_NAME];
    char remote[MAX_BRANCH_NAME];
    bool is_current;
    bool is_remote;
    bool has_upstream;
    int commits_ahead;
    int commits_behind;
    char last_commit_hash[64];
    char last_commit_msg[MAX_COMMIT_MSG];
    time_t last_commit_time;
} branch_info_t;

/* Repository status */
typedef struct {
    char repo_path[MAX_PATH_LEN];
    char current_branch[MAX_BRANCH_NAME];
    bool is_git_repo;
    bool has_uncommitted_changes;
    bool has_staged_changes;
    bool has_untracked_files;
    int modified_files_count;
    int staged_files_count;
    int untracked_files_count;
    branch_info_t *branches;
    int branch_count;
    char **remotes;
    int remote_count;
} repo_status_t;

/* Merge result */
typedef struct {
    bool success;
    bool has_conflicts;
    char **conflicting_files;
    int conflict_count;
    char merge_commit_hash[64];
    char error_message[MAX_COMMIT_MSG];
} merge_result_t;

/* Application state */
typedef struct {
    repo_status_t *repo;
    bool verbose;
    bool dry_run;
    char log_file[MAX_PATH_LEN];
    FILE *log_fp;
} app_state_t;

/* ============================================================================
 * Function Declarations - Utility Functions
 * ============================================================================ */

/* Command execution */
cmd_result_t* exec_command(const char *command);
cmd_result_t* exec_git_command(const char *git_args);
void free_cmd_result(cmd_result_t *result);

/* Error handling */
const char* gm_error_string(gm_error_t error);
void gm_log_error(app_state_t *state, const char *format, ...);
void gm_log_info(app_state_t *state, const char *format, ...);
void gm_log_debug(app_state_t *state, const char *format, ...);

/* String utilities */
char* trim_whitespace(char *str);
char* safe_strdup(const char *str);
bool is_valid_branch_name(const char *name);
char** split_string(const char *str, char delimiter, int *count);
void free_string_array(char **arr, int count);

/* Memory management */
void* safe_malloc(size_t size);
void* safe_realloc(void *ptr, size_t size);
void* safe_calloc(size_t nmemb, size_t size);

/* ============================================================================
 * Function Declarations - Repository Functions
 * ============================================================================ */

/* Repository initialization and status */
gm_error_t init_repository(const char *path);
gm_error_t check_git_repository(const char *path, bool *is_repo);
repo_status_t* get_repo_status(void);
void free_repo_status(repo_status_t *status);
gm_error_t refresh_repo_status(repo_status_t *status);

/* ============================================================================
 * Function Declarations - Branch Management
 * ============================================================================ */

/* Branch operations */
gm_error_t create_branch(const char *branch_name, const char *base_branch);
gm_error_t delete_branch(const char *branch_name, bool force);
gm_error_t switch_branch(const char *branch_name);
gm_error_t rename_branch(const char *old_name, const char *new_name);
gm_error_t list_branches(branch_info_t **branches, int *count, bool include_remote);
gm_error_t get_current_branch(char *branch_name, size_t max_len);
gm_error_t get_branch_info(const char *branch_name, branch_info_t *info);
bool branch_exists(const char *branch_name);

/* ============================================================================
 * Function Declarations - Commit Management
 * ============================================================================ */

/* Staging and committing */
gm_error_t stage_all_changes(void);
gm_error_t stage_file(const char *file_path);
gm_error_t unstage_file(const char *file_path);
gm_error_t commit_changes(const char *message);
gm_error_t amend_commit(const char *new_message);
gm_error_t get_uncommitted_changes(char ***files, int *count);
gm_error_t discard_changes(const char *file_path);
gm_error_t discard_all_changes(void);
gm_error_t stash_changes(const char *message);
gm_error_t pop_stash(void);

/* ============================================================================
 * Function Declarations - Merge Operations
 * ============================================================================ */

/* Merge functions with conflict detection */
gm_error_t check_merge_conflicts(const char *source_branch, bool *has_conflicts);
merge_result_t* merge_branch(const char *source_branch, merge_strategy_t strategy);
gm_error_t abort_merge(void);
gm_error_t get_conflicting_files(char ***files, int *count);
gm_error_t resolve_conflict(const char *file_path, const char *resolution);
void free_merge_result(merge_result_t *result);

/* ============================================================================
 * Function Declarations - Remote Operations
 * ============================================================================ */

/* Remote management */
gm_error_t list_remotes(char ***remotes, int *count);
gm_error_t add_remote(const char *name, const char *url);
gm_error_t remove_remote(const char *name);
gm_error_t fetch_remote(const char *remote_name);
gm_error_t fetch_all(void);

/* Push and pull */
gm_error_t push_branch(const char *remote, const char *branch, bool set_upstream);
gm_error_t push_with_force(const char *remote, const char *branch);
gm_error_t pull_branch(const char *remote, const char *branch);
gm_error_t set_upstream(const char *remote, const char *branch);

/* ============================================================================
 * Function Declarations - Application State
 * ============================================================================ */

/* Application lifecycle */
app_state_t* init_app_state(bool verbose, bool dry_run);
void cleanup_app_state(app_state_t *state);

/* ============================================================================
 * Function Declarations - User Interface
 * ============================================================================ */

/* Menu system */
void display_main_menu(void);
void display_branch_menu(void);
void display_commit_menu(void);
void display_merge_menu(void);
void display_remote_menu(void);
void display_repo_status(repo_status_t *status);

/* User input */
int get_menu_choice(int min, int max);
bool get_user_confirmation(const char *prompt);
char* get_user_input(const char *prompt, size_t max_len);

/* Colors for terminal output */
#define COLOR_RESET     "\033[0m"
#define COLOR_RED       "\033[31m"
#define COLOR_GREEN     "\033[32m"
#define COLOR_YELLOW    "\033[33m"
#define COLOR_BLUE      "\033[34m"
#define COLOR_MAGENTA   "\033[35m"
#define COLOR_CYAN      "\033[36m"
#define COLOR_BOLD      "\033[1m"

/* Print macros */
#define PRINT_ERROR(fmt, ...)   fprintf(stderr, COLOR_RED "[ERROR] " fmt COLOR_RESET "\n", ##__VA_ARGS__)
#define PRINT_SUCCESS(fmt, ...) printf(COLOR_GREEN "[SUCCESS] " fmt COLOR_RESET "\n", ##__VA_ARGS__)
#define PRINT_WARNING(fmt, ...) printf(COLOR_YELLOW "[WARNING] " fmt COLOR_RESET "\n", ##__VA_ARGS__)
#define PRINT_INFO(fmt, ...)    printf(COLOR_CYAN "[INFO] " fmt COLOR_RESET "\n", ##__VA_ARGS__)

#endif /* GIT_MASTER_H */
