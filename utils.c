/**
 * utils.c - Utility Functions for Git Master
 * 
 * Contains command execution, error handling, string manipulation,
 * and memory management utilities with fault tolerance.
 */

#include "git_master.h"
#include <stdarg.h>
#include <ctype.h>
#include <signal.h>

/* ============================================================================
 * Command Execution Functions
 * ============================================================================ */

/**
 * Execute a shell command and capture its output
 * 
 * @param command The command to execute
 * @return cmd_result_t* Result structure (must be freed with free_cmd_result)
 */
cmd_result_t* exec_command(const char *command) {
    if (command == NULL || strlen(command) == 0) {
        return NULL;
    }

    cmd_result_t *result = (cmd_result_t*)safe_calloc(1, sizeof(cmd_result_t));
    if (result == NULL) {
        return NULL;
    }

    result->output = (char*)safe_calloc(MAX_OUTPUT_LEN, sizeof(char));
    result->error = (char*)safe_calloc(MAX_OUTPUT_LEN, sizeof(char));
    
    if (result->output == NULL || result->error == NULL) {
        free_cmd_result(result);
        return NULL;
    }

    /* Create pipes for stdout and stderr */
    int stdout_pipe[2];
    int stderr_pipe[2];
    
    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        free_cmd_result(result);
        return NULL;
    }

    pid_t pid = fork();
    
    if (pid == -1) {
        /* Fork failed */
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        free_cmd_result(result);
        return NULL;
    }
    
    if (pid == 0) {
        /* Child process */
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        
        /* Execute command through shell */
        execl("/bin/sh", "sh", "-c", command, (char*)NULL);
        
        /* If execl fails */
        _exit(127);
    }
    
    /* Parent process */
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    
    /* Read stdout */
    ssize_t bytes_read;
    size_t total_stdout = 0;
    size_t total_stderr = 0;
    char buffer[4096];
    
    while ((bytes_read = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        if (total_stdout + bytes_read < MAX_OUTPUT_LEN - 1) {
            memcpy(result->output + total_stdout, buffer, bytes_read);
            total_stdout += bytes_read;
        }
    }
    result->output[total_stdout] = '\0';
    result->output_len = total_stdout;
    
    /* Read stderr */
    while ((bytes_read = read(stderr_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        if (total_stderr + bytes_read < MAX_OUTPUT_LEN - 1) {
            memcpy(result->error + total_stderr, buffer, bytes_read);
            total_stderr += bytes_read;
        }
    }
    result->error[total_stderr] = '\0';
    result->error_len = total_stderr;
    
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);
    
    /* Wait for child process */
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        result->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result->exit_code = -WTERMSIG(status);
    } else {
        result->exit_code = -1;
    }
    
    return result;
}

/**
 * Execute a Git command with the "git" prefix
 * 
 * @param git_args Arguments to pass to git
 * @return cmd_result_t* Result structure (must be freed with free_cmd_result)
 */
cmd_result_t* exec_git_command(const char *git_args) {
    if (git_args == NULL || strlen(git_args) == 0) {
        return NULL;
    }

    char command[MAX_COMMAND_LEN];
    int written = snprintf(command, sizeof(command), "git %s", git_args);
    
    if (written < 0 || (size_t)written >= sizeof(command)) {
        return NULL;
    }
    
    return exec_command(command);
}

/**
 * Free a command result structure
 * 
 * @param result The result to free
 */
void free_cmd_result(cmd_result_t *result) {
    if (result == NULL) {
        return;
    }
    
    if (result->output != NULL) {
        free(result->output);
        result->output = NULL;
    }
    
    if (result->error != NULL) {
        free(result->error);
        result->error = NULL;
    }
    
    free(result);
}

/* ============================================================================
 * Error Handling Functions
 * ============================================================================ */

/**
 * Get human-readable error string for error code
 * 
 * @param error The error code
 * @return const char* Error description
 */
const char* gm_error_string(gm_error_t error) {
    switch (error) {
        case GM_SUCCESS:
            return "Success";
        case GM_ERR_NOT_GIT_REPO:
            return "Not a git repository";
        case GM_ERR_BRANCH_EXISTS:
            return "Branch already exists";
        case GM_ERR_BRANCH_NOT_FOUND:
            return "Branch not found";
        case GM_ERR_INVALID_BRANCH_NAME:
            return "Invalid branch name";
        case GM_ERR_UNCOMMITTED_CHANGES:
            return "Uncommitted changes exist";
        case GM_ERR_MERGE_CONFLICT:
            return "Merge conflict detected";
        case GM_ERR_COMMAND_FAILED:
            return "Git command failed";
        case GM_ERR_MEMORY_ALLOC:
            return "Memory allocation failed";
        case GM_ERR_INVALID_INPUT:
            return "Invalid input provided";
        case GM_ERR_REMOTE_NOT_FOUND:
            return "Remote repository not found";
        case GM_ERR_PUSH_FAILED:
            return "Push operation failed";
        case GM_ERR_PULL_FAILED:
            return "Pull operation failed";
        case GM_ERR_NO_COMMITS:
            return "No commits in repository";
        case GM_ERR_CHECKOUT_FAILED:
            return "Checkout failed";
        case GM_ERR_DELETE_CURRENT:
            return "Cannot delete current branch";
        case GM_ERR_PROTECTED_BRANCH:
            return "Cannot modify protected branch";
        case GM_ERR_IO_ERROR:
            return "I/O error occurred";
        case GM_ERR_UNKNOWN:
        default:
            return "Unknown error";
    }
}

/**
 * Log an error message
 */
void gm_log_error(app_state_t *state, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    fprintf(stderr, COLOR_RED "[ERROR] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, COLOR_RESET "\n");
    
    if (state != NULL && state->log_fp != NULL) {
        time_t now = time(NULL);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(state->log_fp, "[%s] ERROR: ", time_str);
        va_start(args, format);
        vfprintf(state->log_fp, format, args);
        fprintf(state->log_fp, "\n");
        fflush(state->log_fp);
    }
    
    va_end(args);
}

/**
 * Log an info message
 */
void gm_log_info(app_state_t *state, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    if (state == NULL || state->verbose) {
        printf(COLOR_CYAN "[INFO] ");
        vprintf(format, args);
        printf(COLOR_RESET "\n");
    }
    
    if (state != NULL && state->log_fp != NULL) {
        time_t now = time(NULL);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(state->log_fp, "[%s] INFO: ", time_str);
        va_start(args, format);
        vfprintf(state->log_fp, format, args);
        fprintf(state->log_fp, "\n");
        fflush(state->log_fp);
    }
    
    va_end(args);
}

/**
 * Log a debug message
 */
void gm_log_debug(app_state_t *state, const char *format, ...) {
    if (state == NULL || !state->verbose) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    printf(COLOR_MAGENTA "[DEBUG] ");
    vprintf(format, args);
    printf(COLOR_RESET "\n");
    
    if (state->log_fp != NULL) {
        time_t now = time(NULL);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(state->log_fp, "[%s] DEBUG: ", time_str);
        va_start(args, format);
        vfprintf(state->log_fp, format, args);
        fprintf(state->log_fp, "\n");
        fflush(state->log_fp);
    }
    
    va_end(args);
}

/* ============================================================================
 * String Utility Functions
 * ============================================================================ */

/**
 * Trim leading and trailing whitespace from a string (in-place)
 * 
 * @param str The string to trim
 * @return char* Pointer to the trimmed string
 */
char* trim_whitespace(char *str) {
    if (str == NULL) {
        return NULL;
    }
    
    /* Trim leading whitespace */
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    
    if (*start == '\0') {
        *str = '\0';
        return str;
    }
    
    /* Trim trailing whitespace */
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';
    
    /* Move trimmed string to beginning if needed */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    
    return str;
}

/**
 * Safely duplicate a string
 * 
 * @param str The string to duplicate
 * @return char* Newly allocated copy (must be freed)
 */
char* safe_strdup(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    
    size_t len = strlen(str) + 1;
    char *copy = (char*)safe_malloc(len);
    
    if (copy != NULL) {
        memcpy(copy, str, len);
    }
    
    return copy;
}

/**
 * Validate a branch name according to Git rules
 * 
 * @param name The branch name to validate
 * @return bool True if valid, false otherwise
 */
bool is_valid_branch_name(const char *name) {
    if (name == NULL || strlen(name) == 0) {
        return false;
    }
    
    size_t len = strlen(name);
    
    /* Cannot start with a dash or dot */
    if (name[0] == '-' || name[0] == '.') {
        return false;
    }
    
    /* Cannot end with a dot or slash */
    if (name[len - 1] == '.' || name[len - 1] == '/') {
        return false;
    }
    
    /* Cannot end with .lock */
    if (len >= 5 && strcmp(name + len - 5, ".lock") == 0) {
        return false;
    }
    
    /* Check for invalid characters and sequences */
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        
        /* Invalid characters */
        if (c == ' ' || c == '~' || c == '^' || c == ':' || c == '?' || 
            c == '*' || c == '[' || c == '\\' || (unsigned char)c < 32 || c == 127) {
            return false;
        }
        
        /* Cannot have consecutive dots */
        if (c == '.' && i > 0 && name[i - 1] == '.') {
            return false;
        }
        
        /* Cannot have @{ sequence */
        if (c == '@' && i + 1 < len && name[i + 1] == '{') {
            return false;
        }
    }
    
    /* Reserved names */
    if (strcmp(name, "HEAD") == 0 || strcmp(name, "@") == 0) {
        return false;
    }
    
    return true;
}

/**
 * Split a string by delimiter
 * 
 * @param str The string to split
 * @param delimiter The delimiter character
 * @param count Output: number of parts
 * @return char** Array of string parts (must be freed with free_string_array)
 */
char** split_string(const char *str, char delimiter, int *count) {
    if (str == NULL || count == NULL) {
        if (count) *count = 0;
        return NULL;
    }
    
    /* Count delimiters */
    int n = 1;
    const char *p = str;
    while (*p) {
        if (*p == delimiter) n++;
        p++;
    }
    
    char **result = (char**)safe_calloc(n + 1, sizeof(char*));
    if (result == NULL) {
        *count = 0;
        return NULL;
    }
    
    /* Make a working copy */
    char *copy = safe_strdup(str);
    if (copy == NULL) {
        free(result);
        *count = 0;
        return NULL;
    }
    
    int idx = 0;
    char *token = copy;
    char *next;
    
    while (token != NULL && idx < n) {
        next = strchr(token, delimiter);
        if (next != NULL) {
            *next = '\0';
            next++;
        }
        
        result[idx] = safe_strdup(token);
        if (result[idx] == NULL) {
            /* Cleanup on failure */
            for (int i = 0; i < idx; i++) {
                free(result[i]);
            }
            free(result);
            free(copy);
            *count = 0;
            return NULL;
        }
        
        idx++;
        token = next;
    }
    
    free(copy);
    result[idx] = NULL;
    *count = idx;
    
    return result;
}

/**
 * Free a string array created by split_string
 * 
 * @param arr The array to free
 * @param count Number of elements
 */
void free_string_array(char **arr, int count) {
    if (arr == NULL) {
        return;
    }
    
    for (int i = 0; i < count; i++) {
        if (arr[i] != NULL) {
            free(arr[i]);
        }
    }
    
    free(arr);
}

/* ============================================================================
 * Memory Management Functions
 * ============================================================================ */

/**
 * Safe malloc with NULL check
 * 
 * @param size Bytes to allocate
 * @return void* Allocated memory or NULL
 */
void* safe_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    void *ptr = malloc(size);
    
    if (ptr == NULL) {
        PRINT_ERROR("Memory allocation failed for %zu bytes", size);
    }
    
    return ptr;
}

/**
 * Safe realloc with NULL check
 * 
 * @param ptr Existing pointer
 * @param size New size
 * @return void* Reallocated memory or NULL
 */
void* safe_realloc(void *ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    void *new_ptr = realloc(ptr, size);
    
    if (new_ptr == NULL) {
        PRINT_ERROR("Memory reallocation failed for %zu bytes", size);
    }
    
    return new_ptr;
}

/**
 * Safe calloc with NULL check
 * 
 * @param nmemb Number of elements
 * @param size Size of each element
 * @return void* Allocated and zeroed memory or NULL
 */
void* safe_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    
    void *ptr = calloc(nmemb, size);
    
    if (ptr == NULL) {
        PRINT_ERROR("Memory allocation failed for %zu elements of %zu bytes", nmemb, size);
    }
    
    return ptr;
}

/* ============================================================================
 * Application State Functions
 * ============================================================================ */

/**
 * Initialize the application state
 * 
 * @param verbose Enable verbose logging
 * @param dry_run Enable dry run mode (no actual changes)
 * @return app_state_t* Application state (must be freed with cleanup_app_state)
 */
app_state_t* init_app_state(bool verbose, bool dry_run) {
    app_state_t *state = (app_state_t*)safe_calloc(1, sizeof(app_state_t));
    
    if (state == NULL) {
        return NULL;
    }
    
    state->verbose = verbose;
    state->dry_run = dry_run;
    state->repo = NULL;
    state->log_fp = NULL;
    
    /* Create log file */
    snprintf(state->log_file, sizeof(state->log_file), "git_master.log");
    state->log_fp = fopen(state->log_file, "a");
    
    if (state->log_fp != NULL) {
        time_t now = time(NULL);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(state->log_fp, "\n=== Git Master Session Started at %s ===\n", time_str);
        fflush(state->log_fp);
    }
    
    return state;
}

/**
 * Cleanup and free application state
 * 
 * @param state The state to cleanup
 */
void cleanup_app_state(app_state_t *state) {
    if (state == NULL) {
        return;
    }
    
    if (state->log_fp != NULL) {
        time_t now = time(NULL);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(state->log_fp, "=== Git Master Session Ended at %s ===\n\n", time_str);
        fclose(state->log_fp);
        state->log_fp = NULL;
    }
    
    if (state->repo != NULL) {
        free_repo_status(state->repo);
        state->repo = NULL;
    }
    
    free(state);
}
