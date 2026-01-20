/**
 * history.c - Commit History and Restore Functions for Git Master
 * 
 * Contains functions for viewing commit history, showing commit details,
 * restoring previous commits, and reverting changes.
 */

#include "git_master.h"

/* ============================================================================
 * Commit Information Structure
 * ============================================================================ */

typedef struct {
    char hash[64];
    char short_hash[16];
    char author[256];
    char email[256];
    char date[64];
    char message[MAX_COMMIT_MSG];
    char full_message[MAX_COMMIT_MSG * 2];
    int files_changed;
    int insertions;
    int deletions;
} commit_info_t;

/* ============================================================================
 * Commit History Display Functions
 * ============================================================================ */

/**
 * Display commit history with pagination
 * 
 * @param count Number of commits to show (0 for default 20)
 * @param show_all Show all commits (no limit)
 * @return gm_error_t Error code
 */
gm_error_t show_commit_history(int count, bool show_all) {
    char cmd[MAX_COMMAND_LEN];
    
    if (show_all) {
        snprintf(cmd, sizeof(cmd), 
                 "log --pretty=format:'%%h|%%an|%%ar|%%s' --no-merges");
    } else {
        int limit = (count > 0) ? count : 20;
        snprintf(cmd, sizeof(cmd), 
                 "log --pretty=format:'%%h|%%an|%%ar|%%s' --no-merges -n %d", limit);
    }
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strstr(result->error, "does not have any commits") != NULL) {
            PRINT_INFO("No commits in this repository yet");
            free_cmd_result(result);
            return GM_SUCCESS;
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output == NULL || strlen(result->output) == 0) {
        PRINT_INFO("No commits found");
        free_cmd_result(result);
        return GM_SUCCESS;
    }
    
    printf("\n" COLOR_BOLD "Commit History:" COLOR_RESET "\n");
    printf(COLOR_CYAN "%-10s %-20s %-15s %s" COLOR_RESET "\n", 
           "Hash", "Author", "When", "Message");
    printf("─────────────────────────────────────────────────────────────────────────────\n");
    
    char *output_copy = safe_strdup(result->output);
    if (output_copy == NULL) {
        free_cmd_result(result);
        return GM_ERR_MEMORY_ALLOC;
    }
    
    int commit_num = 0;
    char *line = strtok(output_copy, "\n");
    
    while (line != NULL) {
        int part_count;
        char **parts = split_string(line, '|', &part_count);
        
        if (parts != NULL && part_count >= 4) {
            commit_num++;
            
            /* Truncate message if too long */
            char msg[50];
            strncpy(msg, parts[3], sizeof(msg) - 4);
            msg[sizeof(msg) - 4] = '\0';
            if (strlen(parts[3]) >= sizeof(msg) - 4) {
                strcat(msg, "...");
            }
            
            printf(COLOR_YELLOW "%-10s" COLOR_RESET " %-20.20s %-15s %s\n",
                   parts[0], parts[1], parts[2], msg);
        }
        
        if (parts != NULL) {
            free_string_array(parts, part_count);
        }
        
        line = strtok(NULL, "\n");
    }
    
    free(output_copy);
    free_cmd_result(result);
    
    printf("─────────────────────────────────────────────────────────────────────────────\n");
    printf("Total: %d commits shown\n\n", commit_num);
    
    return GM_SUCCESS;
}

/**
 * Show detailed information about a specific commit
 * 
 * @param commit_hash Commit hash (full or short)
 * @return gm_error_t Error code
 */
gm_error_t show_commit_details(const char *commit_hash) {
    if (commit_hash == NULL || strlen(commit_hash) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Verify the commit exists */
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "cat-file -t \"%s\"", commit_hash);
    
    cmd_result_t *result = exec_git_command(cmd);
    if (result == NULL || result->exit_code != 0) {
        if (result) free_cmd_result(result);
        PRINT_ERROR("Commit '%s' not found", commit_hash);
        return GM_ERR_INVALID_INPUT;
    }
    free_cmd_result(result);
    
    /* Get commit details */
    snprintf(cmd, sizeof(cmd), 
             "show --stat --format='"
             "Commit: %%H%%n"
             "Author: %%an <%%ae>%%n"
             "Date:   %%ad%%n"
             "%%n"
             "    %%s%%n"
             "%%n"
             "    %%b"
             "' \"%s\"", commit_hash);
    
    result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            PRINT_ERROR("Failed to get commit details: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    printf("\n" COLOR_BOLD "═══════════════════════════════════════════════════════════════════" COLOR_RESET "\n");
    if (result->output != NULL) {
        printf("%s\n", result->output);
    }
    printf(COLOR_BOLD "═══════════════════════════════════════════════════════════════════" COLOR_RESET "\n");
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Show the diff of a specific commit
 * 
 * @param commit_hash Commit hash
 * @return gm_error_t Error code
 */
gm_error_t show_commit_diff(const char *commit_hash) {
    if (commit_hash == NULL || strlen(commit_hash) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "show --format='' \"%s\"", commit_hash);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            PRINT_ERROR("Failed to get commit diff: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output != NULL && strlen(result->output) > 0) {
        printf("\n%s\n", result->output);
    } else {
        PRINT_INFO("No diff available (possibly an empty commit)");
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * List files changed in a specific commit
 * 
 * @param commit_hash Commit hash
 * @return gm_error_t Error code
 */
gm_error_t list_commit_files(const char *commit_hash) {
    if (commit_hash == NULL || strlen(commit_hash) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "show --name-status --format='' \"%s\"", commit_hash);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    printf("\n" COLOR_BOLD "Files changed in commit %s:" COLOR_RESET "\n\n", commit_hash);
    
    if (result->output != NULL && strlen(result->output) > 0) {
        /* Parse and colorize output */
        char *output_copy = safe_strdup(result->output);
        if (output_copy != NULL) {
            char *line = strtok(output_copy, "\n");
            while (line != NULL) {
                if (strlen(line) > 0) {
                    char status = line[0];
                    char *filename = line + 1;
                    while (*filename == '\t' || *filename == ' ') filename++;
                    
                    switch (status) {
                        case 'A':
                            printf(COLOR_GREEN "  + (added)    %s" COLOR_RESET "\n", filename);
                            break;
                        case 'M':
                            printf(COLOR_YELLOW "  ~ (modified) %s" COLOR_RESET "\n", filename);
                            break;
                        case 'D':
                            printf(COLOR_RED "  - (deleted)  %s" COLOR_RESET "\n", filename);
                            break;
                        case 'R':
                            printf(COLOR_CYAN "  > (renamed)  %s" COLOR_RESET "\n", filename);
                            break;
                        default:
                            printf("  %c %s\n", status, filename);
                    }
                }
                line = strtok(NULL, "\n");
            }
            free(output_copy);
        }
    } else {
        PRINT_INFO("No files changed");
    }
    
    printf("\n");
    free_cmd_result(result);
    return GM_SUCCESS;
}

/* ============================================================================
 * Commit Restore Functions
 * ============================================================================ */

/**
 * Restore a specific file from a previous commit
 * 
 * @param commit_hash Commit hash to restore from
 * @param file_path File path to restore
 * @return gm_error_t Error code
 */
gm_error_t restore_file_from_commit(const char *commit_hash, const char *file_path) {
    if (commit_hash == NULL || file_path == NULL ||
        strlen(commit_hash) == 0 || strlen(file_path) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    char cmd[MAX_COMMAND_LEN];
    
    /* First verify the file exists in that commit */
    snprintf(cmd, sizeof(cmd), "ls-tree -r \"%s\" -- \"%s\"", commit_hash, file_path);
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL || result->exit_code != 0 || 
        result->output == NULL || strlen(result->output) == 0) {
        if (result) free_cmd_result(result);
        PRINT_ERROR("File '%s' not found in commit '%s'", file_path, commit_hash);
        return GM_ERR_IO_ERROR;
    }
    free_cmd_result(result);
    
    /* Restore the file */
    snprintf(cmd, sizeof(cmd), "checkout \"%s\" -- \"%s\"", commit_hash, file_path);
    result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            PRINT_ERROR("Failed to restore file: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Restored '%s' from commit %s", file_path, commit_hash);
    PRINT_INFO("The file is now staged. Commit to save the change.");
    
    return GM_SUCCESS;
}

/**
 * Revert a commit (create a new commit that undoes the changes)
 * 
 * @param commit_hash Commit hash to revert
 * @return gm_error_t Error code
 */
gm_error_t revert_commit(const char *commit_hash) {
    if (commit_hash == NULL || strlen(commit_hash) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Check for uncommitted changes */
    repo_status_t *status = get_repo_status();
    if (status != NULL) {
        if (status->has_uncommitted_changes) {
            PRINT_ERROR("Cannot revert with uncommitted changes");
            PRINT_INFO("Please commit or stash your changes first");
            free_repo_status(status);
            return GM_ERR_UNCOMMITTED_CHANGES;
        }
        free_repo_status(status);
    }
    
    PRINT_INFO("Reverting commit %s...", commit_hash);
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "revert --no-edit \"%s\"", commit_hash);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL || result->output != NULL) {
            char *check = result->error ? result->error : result->output;
            
            if (strstr(check, "CONFLICT") != NULL) {
                PRINT_ERROR("Revert resulted in conflicts!");
                PRINT_INFO("Resolve conflicts and commit, or abort with: git revert --abort");
                free_cmd_result(result);
                return GM_ERR_MERGE_CONFLICT;
            }
            
            if (result->error != NULL) {
                PRINT_ERROR("Revert failed: %s", result->error);
            }
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Successfully reverted commit %s", commit_hash);
    PRINT_INFO("A new commit has been created that undoes the changes");
    
    return GM_SUCCESS;
}

/**
 * Reset to a specific commit (DANGEROUS - can lose work!)
 * 
 * @param commit_hash Commit hash to reset to
 * @param mode Reset mode: "soft", "mixed", or "hard"
 * @return gm_error_t Error code
 */
gm_error_t reset_to_commit(const char *commit_hash, const char *mode) {
    if (commit_hash == NULL || strlen(commit_hash) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    const char *reset_mode = "mixed"; /* Default */
    
    if (mode != NULL) {
        if (strcmp(mode, "soft") == 0 || strcmp(mode, "mixed") == 0 || 
            strcmp(mode, "hard") == 0) {
            reset_mode = mode;
        }
    }
    
    /* Extra warning for hard reset */
    if (strcmp(reset_mode, "hard") == 0) {
        repo_status_t *status = get_repo_status();
        if (status != NULL) {
            if (status->has_uncommitted_changes) {
                PRINT_WARNING("Hard reset will PERMANENTLY DELETE all uncommitted changes!");
                free_repo_status(status);
                /* Caller should confirm before calling this function */
            } else {
                free_repo_status(status);
            }
        }
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "reset --%s \"%s\"", reset_mode, commit_hash);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            PRINT_ERROR("Reset failed: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    
    switch (reset_mode[0]) {
        case 's': /* soft */
            PRINT_SUCCESS("Reset to %s (soft) - changes kept staged", commit_hash);
            break;
        case 'm': /* mixed */
            PRINT_SUCCESS("Reset to %s (mixed) - changes kept unstaged", commit_hash);
            break;
        case 'h': /* hard */
            PRINT_SUCCESS("Reset to %s (hard) - all changes discarded", commit_hash);
            break;
    }
    
    return GM_SUCCESS;
}

/**
 * Cherry-pick a commit (apply a specific commit to current branch)
 * 
 * @param commit_hash Commit hash to cherry-pick
 * @return gm_error_t Error code
 */
gm_error_t cherry_pick_commit(const char *commit_hash) {
    if (commit_hash == NULL || strlen(commit_hash) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Check for uncommitted changes */
    repo_status_t *status = get_repo_status();
    if (status != NULL) {
        if (status->has_uncommitted_changes) {
            PRINT_ERROR("Cannot cherry-pick with uncommitted changes");
            PRINT_INFO("Please commit or stash your changes first");
            free_repo_status(status);
            return GM_ERR_UNCOMMITTED_CHANGES;
        }
        free_repo_status(status);
    }
    
    PRINT_INFO("Cherry-picking commit %s...", commit_hash);
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "cherry-pick \"%s\"", commit_hash);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL || result->output != NULL) {
            char *check = result->error ? result->error : result->output;
            
            if (strstr(check, "CONFLICT") != NULL) {
                PRINT_ERROR("Cherry-pick resulted in conflicts!");
                PRINT_INFO("Resolve conflicts and run: git cherry-pick --continue");
                PRINT_INFO("Or abort with: git cherry-pick --abort");
                free_cmd_result(result);
                return GM_ERR_MERGE_CONFLICT;
            }
            
            if (result->error != NULL) {
                PRINT_ERROR("Cherry-pick failed: %s", result->error);
            }
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Successfully cherry-picked commit %s", commit_hash);
    
    return GM_SUCCESS;
}

/**
 * Compare two commits
 * 
 * @param commit1 First commit hash
 * @param commit2 Second commit hash
 * @return gm_error_t Error code
 */
gm_error_t compare_commits(const char *commit1, const char *commit2) {
    if (commit1 == NULL || commit2 == NULL ||
        strlen(commit1) == 0 || strlen(commit2) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "diff --stat \"%s\" \"%s\"", commit1, commit2);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            PRINT_ERROR("Failed to compare commits: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    printf("\n" COLOR_BOLD "Comparing %s..%s:" COLOR_RESET "\n\n", commit1, commit2);
    
    if (result->output != NULL && strlen(result->output) > 0) {
        printf("%s\n", result->output);
    } else {
        PRINT_INFO("No differences between commits");
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Show reflog (history of HEAD changes)
 * 
 * @param count Number of entries to show
 * @return gm_error_t Error code
 */
gm_error_t show_reflog(int count) {
    char cmd[MAX_COMMAND_LEN];
    int limit = (count > 0) ? count : 20;
    
    snprintf(cmd, sizeof(cmd), "reflog -n %d --format='%%h|%%gd|%%gs|%%ar'", limit);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    printf("\n" COLOR_BOLD "Reference Log (recent HEAD changes):" COLOR_RESET "\n");
    printf(COLOR_CYAN "%-10s %-15s %-30s %s" COLOR_RESET "\n", 
           "Hash", "Ref", "Action", "When");
    printf("─────────────────────────────────────────────────────────────────────────────\n");
    
    if (result->output != NULL && strlen(result->output) > 0) {
        char *output_copy = safe_strdup(result->output);
        if (output_copy != NULL) {
            char *line = strtok(output_copy, "\n");
            while (line != NULL) {
                int part_count;
                char **parts = split_string(line, '|', &part_count);
                
                if (parts != NULL && part_count >= 4) {
                    /* Truncate action if too long */
                    char action[32];
                    strncpy(action, parts[2], sizeof(action) - 1);
                    action[sizeof(action) - 1] = '\0';
                    
                    printf(COLOR_YELLOW "%-10s" COLOR_RESET " %-15s %-30s %s\n",
                           parts[0], parts[1], action, parts[3]);
                }
                
                if (parts != NULL) {
                    free_string_array(parts, part_count);
                }
                
                line = strtok(NULL, "\n");
            }
            free(output_copy);
        }
    }
    
    printf("─────────────────────────────────────────────────────────────────────────────\n");
    printf("\n" COLOR_CYAN "Tip:" COLOR_RESET " Use reflog hashes to recover lost commits\n\n");
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Recover a commit from reflog
 * 
 * @param reflog_ref Reflog reference (e.g., "HEAD@{2}" or commit hash)
 * @param branch_name Optional new branch name to create
 * @return gm_error_t Error code
 */
gm_error_t recover_from_reflog(const char *reflog_ref, const char *branch_name) {
    if (reflog_ref == NULL || strlen(reflog_ref) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    char cmd[MAX_COMMAND_LEN];
    
    if (branch_name != NULL && strlen(branch_name) > 0) {
        /* Create a new branch at the reflog point */
        snprintf(cmd, sizeof(cmd), "branch \"%s\" \"%s\"", branch_name, reflog_ref);
        
        cmd_result_t *result = exec_git_command(cmd);
        
        if (result == NULL) {
            return GM_ERR_COMMAND_FAILED;
        }
        
        if (result->exit_code != 0) {
            if (result->error != NULL) {
                PRINT_ERROR("Failed to create branch: %s", result->error);
            }
            free_cmd_result(result);
            return GM_ERR_COMMAND_FAILED;
        }
        
        free_cmd_result(result);
        PRINT_SUCCESS("Created branch '%s' at %s", branch_name, reflog_ref);
        PRINT_INFO("Use 'Switch Branch' to check out the recovered commits");
    } else {
        /* Reset current branch to reflog point */
        PRINT_WARNING("This will move HEAD to %s", reflog_ref);
        
        snprintf(cmd, sizeof(cmd), "reset --hard \"%s\"", reflog_ref);
        
        cmd_result_t *result = exec_git_command(cmd);
        
        if (result == NULL) {
            return GM_ERR_COMMAND_FAILED;
        }
        
        if (result->exit_code != 0) {
            if (result->error != NULL) {
                PRINT_ERROR("Failed to reset: %s", result->error);
            }
            free_cmd_result(result);
            return GM_ERR_COMMAND_FAILED;
        }
        
        free_cmd_result(result);
        PRINT_SUCCESS("Recovered to %s", reflog_ref);
    }
    
    return GM_SUCCESS;
}
