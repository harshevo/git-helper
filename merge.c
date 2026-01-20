/**
 * merge.c - Merge Functions for Git Master
 * 
 * Contains functions for merging branches with comprehensive conflict
 * detection and resolution with fault tolerance.
 */

#include "git_master.h"

/* ============================================================================
 * Merge Conflict Detection
 * ============================================================================ */

/**
 * Check if merging a branch would cause conflicts (dry run)
 * 
 * @param source_branch Branch to merge from
 * @param has_conflicts Output: true if conflicts would occur
 * @return gm_error_t Error code
 */
gm_error_t check_merge_conflicts(const char *source_branch, bool *has_conflicts) {
    if (source_branch == NULL || has_conflicts == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    *has_conflicts = false;
    
    /* Check if source branch exists */
    if (!branch_exists(source_branch)) {
        PRINT_ERROR("Source branch '%s' does not exist", source_branch);
        return GM_ERR_BRANCH_NOT_FOUND;
    }
    
    /* Check for uncommitted changes first */
    repo_status_t *status = get_repo_status();
    if (status != NULL) {
        if (status->has_uncommitted_changes) {
            PRINT_WARNING("You have uncommitted changes");
            PRINT_INFO("Please commit or stash changes before merging");
            free_repo_status(status);
            return GM_ERR_UNCOMMITTED_CHANGES;
        }
        free_repo_status(status);
    }
    
    /* Get current branch for logging */
    char current_branch[MAX_BRANCH_NAME];
    get_current_branch(current_branch, sizeof(current_branch));
    
    /* Check if trying to merge into itself */
    if (strcmp(current_branch, source_branch) == 0) {
        PRINT_ERROR("Cannot merge branch into itself");
        return GM_ERR_INVALID_INPUT;
    }
    
    PRINT_INFO("Checking for potential merge conflicts...");
    
    /* Use merge-tree to check for conflicts (Git 2.38+) */
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "merge-tree --write-tree HEAD \"%s\"", source_branch);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result != NULL && result->exit_code == 0) {
        /* No conflicts - merge-tree succeeded */
        *has_conflicts = false;
        free_cmd_result(result);
        PRINT_SUCCESS("No conflicts detected - merge is safe to proceed");
        return GM_SUCCESS;
    }
    
    if (result != NULL) {
        /* Check if merge-tree found conflicts */
        if (result->output != NULL && strstr(result->output, "CONFLICT") != NULL) {
            *has_conflicts = true;
            PRINT_WARNING("Merge conflicts detected!");
            
            /* Try to extract conflict information */
            char *conflict_line = strstr(result->output, "CONFLICT");
            while (conflict_line != NULL) {
                char *eol = strchr(conflict_line, '\n');
                if (eol != NULL) {
                    *eol = '\0';
                }
                printf("  " COLOR_YELLOW "%s" COLOR_RESET "\n", conflict_line);
                if (eol != NULL) {
                    *eol = '\n';
                    conflict_line = strstr(eol + 1, "CONFLICT");
                } else {
                    break;
                }
            }
            
            free_cmd_result(result);
            return GM_ERR_MERGE_CONFLICT;
        }
        free_cmd_result(result);
    }
    
    /* Fallback: Try merge with --no-commit --no-ff and abort */
    snprintf(cmd, sizeof(cmd), "merge --no-commit --no-ff \"%s\"", source_branch);
    result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    bool merge_started = (result->exit_code == 0);
    bool conflict_detected = false;
    
    if (result->exit_code != 0) {
        /* Check for conflict indicators */
        if ((result->output != NULL && strstr(result->output, "CONFLICT") != NULL) ||
            (result->error != NULL && strstr(result->error, "CONFLICT") != NULL) ||
            (result->output != NULL && strstr(result->output, "Automatic merge failed") != NULL)) {
            conflict_detected = true;
        }
    }
    
    free_cmd_result(result);
    
    /* Abort the merge attempt */
    result = exec_git_command("merge --abort");
    if (result != NULL) {
        free_cmd_result(result);
    }
    
    if (conflict_detected) {
        *has_conflicts = true;
        PRINT_WARNING("Merge would result in conflicts!");
        PRINT_INFO("Please resolve conflicts manually before merging");
        return GM_ERR_MERGE_CONFLICT;
    }
    
    if (merge_started) {
        *has_conflicts = false;
        PRINT_SUCCESS("No conflicts detected - merge is safe to proceed");
    }
    
    return GM_SUCCESS;
}

/**
 * Get list of files with merge conflicts
 * 
 * @param files Output: array of conflicting file paths
 * @param count Output: number of files
 * @return gm_error_t Error code
 */
gm_error_t get_conflicting_files(char ***files, int *count) {
    if (files == NULL || count == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    *files = NULL;
    *count = 0;
    
    cmd_result_t *result = exec_git_command("diff --name-only --diff-filter=U");
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output == NULL || strlen(result->output) == 0) {
        free_cmd_result(result);
        return GM_SUCCESS; /* No conflicts */
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
        if (strlen(line) > 0) {
            (*files)[idx] = safe_strdup(trim_whitespace(line));
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

/* ============================================================================
 * Merge Operations
 * ============================================================================ */

/**
 * Merge a branch into the current branch
 * 
 * @param source_branch Branch to merge from
 * @param strategy Merge strategy to use
 * @return merge_result_t* Result structure (must be freed with free_merge_result)
 */
merge_result_t* merge_branch(const char *source_branch, merge_strategy_t strategy) {
    if (source_branch == NULL || strlen(source_branch) == 0) {
        return NULL;
    }
    
    merge_result_t *result = (merge_result_t*)safe_calloc(1, sizeof(merge_result_t));
    if (result == NULL) {
        return NULL;
    }
    
    result->success = false;
    result->has_conflicts = false;
    result->conflicting_files = NULL;
    result->conflict_count = 0;
    
    /* Check if source branch exists */
    if (!branch_exists(source_branch)) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "Source branch '%s' does not exist", source_branch);
        PRINT_ERROR("%s", result->error_message);
        return result;
    }
    
    /* Check for uncommitted changes */
    repo_status_t *status = get_repo_status();
    if (status != NULL) {
        if (status->has_uncommitted_changes) {
            snprintf(result->error_message, sizeof(result->error_message),
                     "Uncommitted changes exist. Please commit or stash before merging.");
            PRINT_ERROR("%s", result->error_message);
            free_repo_status(status);
            return result;
        }
        free_repo_status(status);
    }
    
    /* Get current branch */
    char current_branch[MAX_BRANCH_NAME];
    if (get_current_branch(current_branch, sizeof(current_branch)) != GM_SUCCESS) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to get current branch");
        return result;
    }
    
    /* Check merge into itself */
    if (strcmp(current_branch, source_branch) == 0) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "Cannot merge branch into itself");
        PRINT_ERROR("%s", result->error_message);
        return result;
    }
    
    PRINT_INFO("Merging '%s' into '%s'...", source_branch, current_branch);
    
    /* Pre-check for conflicts */
    bool would_conflict = false;
    gm_error_t check_err = check_merge_conflicts(source_branch, &would_conflict);
    
    if (check_err == GM_ERR_MERGE_CONFLICT || would_conflict) {
        result->has_conflicts = true;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Merge would result in conflicts. Cannot proceed automatically.");
        PRINT_ERROR("Merge blocked: conflicts detected");
        PRINT_INFO("Please resolve conflicts manually or use a different merge strategy");
        return result;
    }
    
    /* Build merge command based on strategy */
    char cmd[MAX_COMMAND_LEN];
    
    switch (strategy) {
        case MERGE_STRATEGY_NO_FF:
            snprintf(cmd, sizeof(cmd), "merge --no-ff \"%s\"", source_branch);
            break;
        case MERGE_STRATEGY_SQUASH:
            snprintf(cmd, sizeof(cmd), "merge --squash \"%s\"", source_branch);
            break;
        case MERGE_STRATEGY_REBASE:
            /* Rebase is handled differently */
            snprintf(cmd, sizeof(cmd), "rebase \"%s\"", source_branch);
            break;
        case MERGE_STRATEGY_DEFAULT:
        default:
            snprintf(cmd, sizeof(cmd), "merge \"%s\"", source_branch);
            break;
    }
    
    cmd_result_t *cmd_result = exec_git_command(cmd);
    
    if (cmd_result == NULL) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to execute merge command");
        return result;
    }
    
    if (cmd_result->exit_code != 0) {
        /* Check for conflicts */
        if ((cmd_result->output != NULL && strstr(cmd_result->output, "CONFLICT") != NULL) ||
            (cmd_result->error != NULL && strstr(cmd_result->error, "CONFLICT") != NULL)) {
            
            result->has_conflicts = true;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Merge resulted in conflicts");
            
            /* Get list of conflicting files */
            get_conflicting_files(&result->conflicting_files, &result->conflict_count);
            
            PRINT_ERROR("MERGE CONFLICT DETECTED!");
            PRINT_WARNING("The following files have conflicts:");
            
            for (int i = 0; i < result->conflict_count; i++) {
                printf("  " COLOR_RED "- %s" COLOR_RESET "\n", result->conflicting_files[i]);
            }
            
            PRINT_INFO("Aborting merge to prevent data corruption...");
            
            /* Abort the merge */
            cmd_result_t *abort_result = exec_git_command("merge --abort");
            if (abort_result != NULL) {
                free_cmd_result(abort_result);
            }
            
            free_cmd_result(cmd_result);
            return result;
        }
        
        /* Other error */
        snprintf(result->error_message, sizeof(result->error_message),
                 "Merge failed: %s", 
                 cmd_result->error != NULL ? cmd_result->error : "Unknown error");
        PRINT_ERROR("%s", result->error_message);
        free_cmd_result(cmd_result);
        return result;
    }
    
    /* Merge successful */
    result->success = true;
    
    /* Get merge commit hash */
    cmd_result_t *hash_result = exec_git_command("rev-parse HEAD");
    if (hash_result != NULL && hash_result->exit_code == 0 && hash_result->output != NULL) {
        strncpy(result->merge_commit_hash, trim_whitespace(hash_result->output),
                sizeof(result->merge_commit_hash) - 1);
    }
    if (hash_result != NULL) {
        free_cmd_result(hash_result);
    }
    
    /* For squash merge, we need to commit separately */
    if (strategy == MERGE_STRATEGY_SQUASH) {
        char commit_msg[MAX_COMMIT_MSG];
        snprintf(commit_msg, sizeof(commit_msg), "Squash merge of branch '%s'", source_branch);
        
        if (commit_changes(commit_msg) == GM_SUCCESS) {
            PRINT_SUCCESS("Squash merge of '%s' completed successfully", source_branch);
        } else {
            PRINT_WARNING("Squash merge staged. Please commit with your message.");
        }
    } else {
        PRINT_SUCCESS("Merged '%s' into '%s'", source_branch, current_branch);
    }
    
    if (strlen(result->merge_commit_hash) > 0) {
        PRINT_INFO("Merge commit: %.8s", result->merge_commit_hash);
    }
    
    free_cmd_result(cmd_result);
    return result;
}

/**
 * Abort an in-progress merge
 * 
 * @return gm_error_t Error code
 */
gm_error_t abort_merge(void) {
    /* Check if a merge is in progress */
    cmd_result_t *check = exec_git_command("rev-parse -q --verify MERGE_HEAD");
    
    if (check == NULL || check->exit_code != 0) {
        if (check) free_cmd_result(check);
        PRINT_INFO("No merge in progress");
        return GM_SUCCESS;
    }
    free_cmd_result(check);
    
    cmd_result_t *result = exec_git_command("merge --abort");
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Failed to abort merge: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Merge aborted");
    
    return GM_SUCCESS;
}

/**
 * Free merge result structure
 * 
 * @param result The result to free
 */
void free_merge_result(merge_result_t *result) {
    if (result == NULL) {
        return;
    }
    
    if (result->conflicting_files != NULL) {
        free_string_array(result->conflicting_files, result->conflict_count);
        result->conflicting_files = NULL;
    }
    
    free(result);
}

/**
 * Show merge preview (what would be merged)
 * 
 * @param source_branch Branch to preview merge from
 * @return gm_error_t Error code
 */
gm_error_t preview_merge(const char *source_branch) {
    if (source_branch == NULL || strlen(source_branch) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Check if source branch exists */
    if (!branch_exists(source_branch)) {
        PRINT_ERROR("Branch '%s' does not exist", source_branch);
        return GM_ERR_BRANCH_NOT_FOUND;
    }
    
    /* Get current branch */
    char current_branch[MAX_BRANCH_NAME];
    get_current_branch(current_branch, sizeof(current_branch));
    
    printf("\n" COLOR_BOLD "Merge Preview: %s -> %s" COLOR_RESET "\n\n", 
           source_branch, current_branch);
    
    /* Show commits that would be merged */
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "log --oneline \"%s\"...\"%s\"", 
             current_branch, source_branch);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result != NULL && result->exit_code == 0 && result->output != NULL) {
        if (strlen(result->output) > 0) {
            printf(COLOR_CYAN "Commits to be merged:" COLOR_RESET "\n");
            printf("%s\n", result->output);
        } else {
            printf(COLOR_YELLOW "No new commits to merge (branches may already be in sync)" COLOR_RESET "\n");
        }
    }
    
    if (result != NULL) {
        free_cmd_result(result);
    }
    
    /* Show files that would be changed */
    snprintf(cmd, sizeof(cmd), "diff --stat \"%s\"...\"%s\"", 
             current_branch, source_branch);
    
    result = exec_git_command(cmd);
    
    if (result != NULL && result->exit_code == 0 && result->output != NULL) {
        if (strlen(result->output) > 0) {
            printf(COLOR_CYAN "\nFiles to be changed:" COLOR_RESET "\n");
            printf("%s\n", result->output);
        }
    }
    
    if (result != NULL) {
        free_cmd_result(result);
    }
    
    /* Check for potential conflicts */
    bool has_conflicts = false;
    check_merge_conflicts(source_branch, &has_conflicts);
    
    return GM_SUCCESS;
}

/**
 * Check if a merge is in progress
 * 
 * @return bool True if merge is in progress
 */
bool is_merge_in_progress(void) {
    cmd_result_t *result = exec_git_command("rev-parse -q --verify MERGE_HEAD");
    
    if (result == NULL) {
        return false;
    }
    
    bool in_progress = (result->exit_code == 0);
    free_cmd_result(result);
    
    return in_progress;
}

/**
 * Continue a merge after resolving conflicts
 * 
 * @param message Commit message for merge
 * @return gm_error_t Error code
 */
gm_error_t continue_merge(const char *message) {
    if (!is_merge_in_progress()) {
        PRINT_ERROR("No merge in progress");
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Check if there are still unresolved conflicts */
    char **files = NULL;
    int count = 0;
    
    get_conflicting_files(&files, &count);
    
    if (count > 0) {
        PRINT_ERROR("Cannot continue merge - unresolved conflicts exist:");
        for (int i = 0; i < count; i++) {
            printf("  " COLOR_RED "- %s" COLOR_RESET "\n", files[i]);
        }
        free_string_array(files, count);
        return GM_ERR_MERGE_CONFLICT;
    }
    
    if (files != NULL) {
        free_string_array(files, count);
    }
    
    /* Stage all changes */
    stage_all_changes();
    
    /* Commit the merge */
    char cmd[MAX_COMMAND_LEN];
    
    if (message != NULL && strlen(message) > 0) {
        char escaped_msg[MAX_COMMIT_MSG * 2];
        size_t j = 0;
        for (size_t i = 0; message[i] != '\0' && j < sizeof(escaped_msg) - 2; i++) {
            if (message[i] == '"' || message[i] == '\\') {
                escaped_msg[j++] = '\\';
            }
            escaped_msg[j++] = message[i];
        }
        escaped_msg[j] = '\0';
        
        snprintf(cmd, sizeof(cmd), "commit -m \"%s\"", escaped_msg);
    } else {
        snprintf(cmd, sizeof(cmd), "commit --no-edit");
    }
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            PRINT_ERROR("Failed to complete merge: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Merge completed successfully");
    
    return GM_SUCCESS;
}
