/**
 * commit.c - Commit Management Functions for Git Master
 * 
 * Contains functions for staging files, committing changes,
 * stashing, and managing the working tree with fault tolerance.
 */

#include "git_master.h"

/* ============================================================================
 * Staging Functions
 * ============================================================================ */

/**
 * Stage all changes (including new, modified, and deleted files)
 * 
 * @return gm_error_t Error code
 */
gm_error_t stage_all_changes(void) {
    cmd_result_t *result = exec_git_command("add -A");
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Failed to stage changes: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Staged all changes");
    
    return GM_SUCCESS;
}

/**
 * Stage a specific file
 * 
 * @param file_path Path to the file to stage
 * @return gm_error_t Error code
 */
gm_error_t stage_file(const char *file_path) {
    if (file_path == NULL || strlen(file_path) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Check if file exists or is being deleted */
    struct stat st;
    bool file_exists = (stat(file_path, &st) == 0);
    
    char cmd[MAX_COMMAND_LEN];
    
    if (file_exists) {
        snprintf(cmd, sizeof(cmd), "add \"%s\"", file_path);
    } else {
        /* File might be deleted, use update flag */
        snprintf(cmd, sizeof(cmd), "add -u \"%s\"", file_path);
    }
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            /* Check for specific errors */
            if (strstr(result->error, "did not match any files") != NULL) {
                PRINT_ERROR("File '%s' not found or not tracked", file_path);
                free_cmd_result(result);
                return GM_ERR_IO_ERROR;
            }
            PRINT_ERROR("Failed to stage file: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Staged file '%s'", file_path);
    
    return GM_SUCCESS;
}

/**
 * Unstage a file (remove from staging area but keep changes)
 * 
 * @param file_path Path to the file to unstage
 * @return gm_error_t Error code
 */
gm_error_t unstage_file(const char *file_path) {
    if (file_path == NULL || strlen(file_path) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "restore --staged \"%s\"", file_path);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        /* Try older reset syntax for compatibility */
        snprintf(cmd, sizeof(cmd), "reset HEAD \"%s\"", file_path);
        result = exec_git_command(cmd);
        
        if (result == NULL) {
            return GM_ERR_COMMAND_FAILED;
        }
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Failed to unstage file: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Unstaged file '%s'", file_path);
    
    return GM_SUCCESS;
}

/* ============================================================================
 * Commit Functions
 * ============================================================================ */

/**
 * Commit staged changes
 * 
 * @param message Commit message
 * @return gm_error_t Error code
 */
gm_error_t commit_changes(const char *message) {
    if (message == NULL || strlen(message) == 0) {
        PRINT_ERROR("Commit message cannot be empty");
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Check if there are staged changes */
    cmd_result_t *check = exec_git_command("diff --cached --quiet");
    if (check != NULL) {
        if (check->exit_code == 0) {
            /* No staged changes */
            PRINT_WARNING("No staged changes to commit");
            free_cmd_result(check);
            return GM_ERR_NO_COMMITS;
        }
        free_cmd_result(check);
    }
    
    /* Escape quotes in commit message */
    char escaped_msg[MAX_COMMIT_MSG * 2];
    size_t j = 0;
    for (size_t i = 0; message[i] != '\0' && j < sizeof(escaped_msg) - 2; i++) {
        if (message[i] == '"' || message[i] == '\\') {
            escaped_msg[j++] = '\\';
        }
        escaped_msg[j++] = message[i];
    }
    escaped_msg[j] = '\0';
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "commit -m \"%s\"", escaped_msg);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            /* Check for specific errors */
            if (strstr(result->error, "nothing to commit") != NULL ||
                strstr(result->output, "nothing to commit") != NULL) {
                PRINT_WARNING("Nothing to commit, working tree clean");
                free_cmd_result(result);
                return GM_ERR_NO_COMMITS;
            }
            
            if (strstr(result->error, "Please tell me who you are") != NULL) {
                PRINT_ERROR("Git user identity not configured");
                PRINT_INFO("Run: git config --global user.email \"you@example.com\"");
                PRINT_INFO("Run: git config --global user.name \"Your Name\"");
                free_cmd_result(result);
                return GM_ERR_COMMAND_FAILED;
            }
            
            PRINT_ERROR("Commit failed: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    /* Extract commit hash from output */
    char commit_hash[16] = "";
    if (result->output != NULL) {
        char *hash_start = strchr(result->output, '[');
        if (hash_start != NULL) {
            hash_start = strchr(hash_start, ' ');
            if (hash_start != NULL) {
                hash_start++;
                char *hash_end = strchr(hash_start, ']');
                if (hash_end != NULL) {
                    size_t len = (size_t)(hash_end - hash_start);
                    if (len < sizeof(commit_hash)) {
                        strncpy(commit_hash, hash_start, len);
                        commit_hash[len] = '\0';
                    }
                }
            }
        }
    }
    
    free_cmd_result(result);
    
    if (strlen(commit_hash) > 0) {
        PRINT_SUCCESS("Committed changes [%s]", commit_hash);
    } else {
        PRINT_SUCCESS("Committed changes");
    }
    
    return GM_SUCCESS;
}

/**
 * Amend the last commit with new message
 * 
 * @param new_message New commit message (NULL to keep existing)
 * @return gm_error_t Error code
 */
gm_error_t amend_commit(const char *new_message) {
    char cmd[MAX_COMMAND_LEN];
    
    if (new_message != NULL && strlen(new_message) > 0) {
        /* Escape quotes in commit message */
        char escaped_msg[MAX_COMMIT_MSG * 2];
        size_t j = 0;
        for (size_t i = 0; new_message[i] != '\0' && j < sizeof(escaped_msg) - 2; i++) {
            if (new_message[i] == '"' || new_message[i] == '\\') {
                escaped_msg[j++] = '\\';
            }
            escaped_msg[j++] = new_message[i];
        }
        escaped_msg[j] = '\0';
        
        snprintf(cmd, sizeof(cmd), "commit --amend -m \"%s\"", escaped_msg);
    } else {
        snprintf(cmd, sizeof(cmd), "commit --amend --no-edit");
    }
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Amend failed: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Amended last commit");
    
    return GM_SUCCESS;
}

/* ============================================================================
 * Working Tree Functions
 * ============================================================================ */

/**
 * Get list of uncommitted changes
 * 
 * @param files Output: array of file paths
 * @param count Output: number of files
 * @return gm_error_t Error code
 */
gm_error_t get_uncommitted_changes(char ***files, int *count) {
    if (files == NULL || count == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    *files = NULL;
    *count = 0;
    
    cmd_result_t *result = exec_git_command("status --porcelain");
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output == NULL || strlen(result->output) == 0) {
        free_cmd_result(result);
        return GM_SUCCESS; /* No changes */
    }
    
    /* Count lines */
    int line_count = 1;
    for (char *p = result->output; *p; p++) {
        if (*p == '\n') line_count++;
    }
    
    /* Allocate array */
    *files = (char**)safe_calloc(line_count + 1, sizeof(char*));
    if (*files == NULL) {
        free_cmd_result(result);
        return GM_ERR_MEMORY_ALLOC;
    }
    
    /* Parse lines */
    char *output_copy = safe_strdup(result->output);
    if (output_copy == NULL) {
        free(*files);
        *files = NULL;
        free_cmd_result(result);
        return GM_ERR_MEMORY_ALLOC;
    }
    
    int idx = 0;
    char *line = strtok(output_copy, "\n");
    
    while (line != NULL && idx < line_count) {
        if (strlen(line) > 3) {
            /* Skip status prefix (first 3 chars) */
            char *filename = line + 3;
            (*files)[idx] = safe_strdup(trim_whitespace(filename));
            if ((*files)[idx] != NULL) {
                idx++;
            }
        }
        line = strtok(NULL, "\n");
    }
    
    free(output_copy);
    free_cmd_result(result);
    
    (*files)[idx] = NULL;
    *count = idx;
    
    return GM_SUCCESS;
}

/**
 * Discard changes to a specific file
 * 
 * @param file_path Path to the file
 * @return gm_error_t Error code
 */
gm_error_t discard_changes(const char *file_path) {
    if (file_path == NULL || strlen(file_path) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    char cmd[MAX_COMMAND_LEN];
    
    /* First try restore command (Git 2.23+) */
    snprintf(cmd, sizeof(cmd), "restore \"%s\"", file_path);
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL || result->exit_code != 0) {
        if (result) free_cmd_result(result);
        
        /* Fallback to checkout for older Git */
        snprintf(cmd, sizeof(cmd), "checkout -- \"%s\"", file_path);
        result = exec_git_command(cmd);
        
        if (result == NULL) {
            return GM_ERR_COMMAND_FAILED;
        }
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Failed to discard changes: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Discarded changes to '%s'", file_path);
    
    return GM_SUCCESS;
}

/**
 * Discard all uncommitted changes
 * 
 * @return gm_error_t Error code
 */
gm_error_t discard_all_changes(void) {
    /* Reset staged changes */
    cmd_result_t *result = exec_git_command("reset HEAD");
    
    if (result != NULL) {
        if (result->exit_code != 0 && result->error != NULL) {
            /* Check if it's just because there are no commits yet */
            if (strstr(result->error, "does not have any commits") == NULL) {
                /* Real error */
                free_cmd_result(result);
                return GM_ERR_COMMAND_FAILED;
            }
        }
        free_cmd_result(result);
    }
    
    /* Restore working tree */
    result = exec_git_command("checkout -- .");
    
    if (result == NULL) {
        /* Try restore command for newer Git */
        result = exec_git_command("restore .");
        if (result == NULL) {
            return GM_ERR_COMMAND_FAILED;
        }
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Failed to discard changes: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    
    /* Clean untracked files (optional - be careful) */
    /* Not running git clean by default as it's destructive */
    
    PRINT_SUCCESS("Discarded all changes");
    
    return GM_SUCCESS;
}

/* ============================================================================
 * Stash Functions
 * ============================================================================ */

/**
 * Stash current changes
 * 
 * @param message Optional stash message
 * @return gm_error_t Error code
 */
gm_error_t stash_changes(const char *message) {
    char cmd[MAX_COMMAND_LEN];
    
    if (message != NULL && strlen(message) > 0) {
        /* Escape quotes in message */
        char escaped_msg[MAX_COMMIT_MSG * 2];
        size_t j = 0;
        for (size_t i = 0; message[i] != '\0' && j < sizeof(escaped_msg) - 2; i++) {
            if (message[i] == '"' || message[i] == '\\') {
                escaped_msg[j++] = '\\';
            }
            escaped_msg[j++] = message[i];
        }
        escaped_msg[j] = '\0';
        
        snprintf(cmd, sizeof(cmd), "stash push -m \"%s\"", escaped_msg);
    } else {
        snprintf(cmd, sizeof(cmd), "stash push");
    }
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Stash failed: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    /* Check if anything was stashed */
    if (result->output != NULL && strstr(result->output, "No local changes") != NULL) {
        PRINT_INFO("No local changes to stash");
        free_cmd_result(result);
        return GM_SUCCESS;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Stashed changes");
    
    return GM_SUCCESS;
}

/**
 * Pop the most recent stash
 * 
 * @return gm_error_t Error code
 */
gm_error_t pop_stash(void) {
    cmd_result_t *result = exec_git_command("stash pop");
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            /* Check for specific errors */
            if (strstr(result->error, "No stash entries found") != NULL) {
                PRINT_WARNING("No stash entries to pop");
                free_cmd_result(result);
                return GM_SUCCESS;
            }
            
            if (strstr(result->error, "CONFLICT") != NULL ||
                strstr(result->output, "CONFLICT") != NULL) {
                PRINT_WARNING("Stash pop resulted in conflicts");
                PRINT_INFO("Resolve conflicts and commit, or use 'git stash drop' to discard");
                free_cmd_result(result);
                return GM_ERR_MERGE_CONFLICT;
            }
            
            PRINT_ERROR("Stash pop failed: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Popped stash and applied changes");
    
    return GM_SUCCESS;
}

/**
 * List all stash entries
 * 
 * @return gm_error_t Error code
 */
gm_error_t list_stash(void) {
    cmd_result_t *result = exec_git_command("stash list");
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output != NULL && strlen(result->output) > 0) {
        printf("\n" COLOR_BOLD "Stash Entries:" COLOR_RESET "\n");
        printf("%s\n", result->output);
    } else {
        PRINT_INFO("No stash entries");
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Display status of working tree
 * 
 * @return gm_error_t Error code
 */
gm_error_t show_status(void) {
    cmd_result_t *result = exec_git_command("status");
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Status check failed: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output != NULL) {
        printf("\n%s\n", result->output);
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Display diff of uncommitted changes
 * 
 * @param staged Show staged changes instead of unstaged
 * @return gm_error_t Error code
 */
gm_error_t show_diff(bool staged) {
    const char *git_args = staged ? "diff --cached" : "diff";
    cmd_result_t *result = exec_git_command(git_args);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output != NULL && strlen(result->output) > 0) {
        printf("\n%s\n", result->output);
    } else {
        PRINT_INFO("No %s changes to show", staged ? "staged" : "unstaged");
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Display commit log
 * 
 * @param count Number of commits to show (0 for default)
 * @return gm_error_t Error code
 */
gm_error_t show_log(int count) {
    char cmd[MAX_COMMAND_LEN];
    
    if (count > 0) {
        snprintf(cmd, sizeof(cmd), "log --oneline -n %d", count);
    } else {
        snprintf(cmd, sizeof(cmd), "log --oneline -n 10");
    }
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            /* Check if no commits yet */
            if (strstr(result->error, "does not have any commits") != NULL) {
                PRINT_INFO("No commits yet in this repository");
                free_cmd_result(result);
                return GM_SUCCESS;
            }
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output != NULL && strlen(result->output) > 0) {
        printf("\n" COLOR_BOLD "Recent Commits:" COLOR_RESET "\n");
        printf("%s\n", result->output);
    } else {
        PRINT_INFO("No commits in log");
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}
