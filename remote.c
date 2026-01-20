/**
 * remote.c - Remote Operations for Git Master
 * 
 * Contains functions for managing remotes, pushing, pulling,
 * and fetching with fault tolerance.
 */

#include "git_master.h"

/* ============================================================================
 * Remote Management Functions
 * ============================================================================ */

/**
 * List all configured remotes
 * 
 * @param remotes Output: array of remote names
 * @param count Output: number of remotes
 * @return gm_error_t Error code
 */
gm_error_t list_remotes(char ***remotes, int *count) {
    if (remotes == NULL || count == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    *remotes = NULL;
    *count = 0;
    
    cmd_result_t *result = exec_git_command("remote");
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output == NULL || strlen(result->output) == 0) {
        free_cmd_result(result);
        return GM_SUCCESS; /* No remotes configured */
    }
    
    /* Count lines */
    int line_count = 1;
    for (char *p = result->output; *p; p++) {
        if (*p == '\n') line_count++;
    }
    
    /* Allocate array */
    *remotes = (char**)safe_calloc(line_count + 1, sizeof(char*));
    if (*remotes == NULL) {
        free_cmd_result(result);
        return GM_ERR_MEMORY_ALLOC;
    }
    
    /* Parse lines */
    char *output_copy = safe_strdup(result->output);
    if (output_copy == NULL) {
        free(*remotes);
        *remotes = NULL;
        free_cmd_result(result);
        return GM_ERR_MEMORY_ALLOC;
    }
    
    int idx = 0;
    char *line = strtok(output_copy, "\n");
    
    while (line != NULL && idx < line_count) {
        char *trimmed = trim_whitespace(line);
        if (strlen(trimmed) > 0) {
            (*remotes)[idx] = safe_strdup(trimmed);
            if ((*remotes)[idx] != NULL) {
                idx++;
            }
        }
        line = strtok(NULL, "\n");
    }
    
    free(output_copy);
    free_cmd_result(result);
    
    (*remotes)[idx] = NULL;
    *count = idx;
    
    return GM_SUCCESS;
}

/**
 * Check if a remote exists
 * 
 * @param name Remote name to check
 * @return bool True if remote exists
 */
bool remote_exists(const char *name) {
    if (name == NULL || strlen(name) == 0) {
        return false;
    }
    
    char **remotes = NULL;
    int count = 0;
    
    if (list_remotes(&remotes, &count) != GM_SUCCESS) {
        return false;
    }
    
    bool exists = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(remotes[i], name) == 0) {
            exists = true;
            break;
        }
    }
    
    free_string_array(remotes, count);
    return exists;
}

/**
 * Add a new remote
 * 
 * @param name Remote name
 * @param url Remote URL
 * @return gm_error_t Error code
 */
gm_error_t add_remote(const char *name, const char *url) {
    if (name == NULL || url == NULL || strlen(name) == 0 || strlen(url) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Check if remote already exists */
    if (remote_exists(name)) {
        PRINT_ERROR("Remote '%s' already exists", name);
        return GM_ERR_INVALID_INPUT;
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "remote add \"%s\" \"%s\"", name, url);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Failed to add remote: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Added remote '%s' -> %s", name, url);
    
    return GM_SUCCESS;
}

/**
 * Remove a remote
 * 
 * @param name Remote name to remove
 * @return gm_error_t Error code
 */
gm_error_t remove_remote(const char *name) {
    if (name == NULL || strlen(name) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Check if remote exists */
    if (!remote_exists(name)) {
        PRINT_ERROR("Remote '%s' does not exist", name);
        return GM_ERR_REMOTE_NOT_FOUND;
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "remote remove \"%s\"", name);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Failed to remove remote: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Removed remote '%s'", name);
    
    return GM_SUCCESS;
}

/**
 * Get URL of a remote
 * 
 * @param name Remote name
 * @param url Output buffer for URL
 * @param max_len Maximum length of output buffer
 * @return gm_error_t Error code
 */
gm_error_t get_remote_url(const char *name, char *url, size_t max_len) {
    if (name == NULL || url == NULL || max_len == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "remote get-url \"%s\"", name);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        free_cmd_result(result);
        return GM_ERR_REMOTE_NOT_FOUND;
    }
    
    if (result->output != NULL) {
        strncpy(url, trim_whitespace(result->output), max_len - 1);
        url[max_len - 1] = '\0';
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Display remote information
 * 
 * @return gm_error_t Error code
 */
gm_error_t show_remotes(void) {
    cmd_result_t *result = exec_git_command("remote -v");
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output != NULL && strlen(result->output) > 0) {
        printf("\n" COLOR_BOLD "Configured Remotes:" COLOR_RESET "\n");
        printf("%s\n", result->output);
    } else {
        PRINT_INFO("No remotes configured");
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/* ============================================================================
 * Fetch Functions
 * ============================================================================ */

/**
 * Fetch from a specific remote
 * 
 * @param remote_name Name of remote to fetch from
 * @return gm_error_t Error code
 */
gm_error_t fetch_remote(const char *remote_name) {
    if (remote_name == NULL || strlen(remote_name) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Check if remote exists */
    if (!remote_exists(remote_name)) {
        PRINT_ERROR("Remote '%s' does not exist", remote_name);
        return GM_ERR_REMOTE_NOT_FOUND;
    }
    
    PRINT_INFO("Fetching from '%s'...", remote_name);
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "fetch \"%s\"", remote_name);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            /* Check for network/authentication errors */
            if (strstr(result->error, "Could not resolve") != NULL ||
                strstr(result->error, "unable to access") != NULL) {
                PRINT_ERROR("Network error: Unable to reach remote '%s'", remote_name);
            } else if (strstr(result->error, "Authentication") != NULL ||
                       strstr(result->error, "Permission denied") != NULL) {
                PRINT_ERROR("Authentication failed for remote '%s'", remote_name);
            } else {
                PRINT_ERROR("Fetch failed: %s", result->error);
            }
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Fetched from '%s'", remote_name);
    
    return GM_SUCCESS;
}

/**
 * Fetch from all remotes
 * 
 * @return gm_error_t Error code
 */
gm_error_t fetch_all(void) {
    PRINT_INFO("Fetching from all remotes...");
    
    cmd_result_t *result = exec_git_command("fetch --all");
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Fetch failed: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Fetched from all remotes");
    
    return GM_SUCCESS;
}

/* ============================================================================
 * Push Functions
 * ============================================================================ */

/**
 * Push a branch to a remote
 * 
 * @param remote Remote name (NULL for default 'origin')
 * @param branch Branch name (NULL for current branch)
 * @param set_upstream Set upstream tracking
 * @return gm_error_t Error code
 */
gm_error_t push_branch(const char *remote, const char *branch, bool set_upstream) {
    char remote_name[MAX_BRANCH_NAME] = "origin";
    char branch_name[MAX_BRANCH_NAME];
    
    /* Use provided remote or default to origin */
    if (remote != NULL && strlen(remote) > 0) {
        strncpy(remote_name, remote, sizeof(remote_name) - 1);
        remote_name[sizeof(remote_name) - 1] = '\0';
    }
    
    /* Use provided branch or current branch */
    if (branch != NULL && strlen(branch) > 0) {
        strncpy(branch_name, branch, sizeof(branch_name) - 1);
        branch_name[sizeof(branch_name) - 1] = '\0';
    } else {
        if (get_current_branch(branch_name, sizeof(branch_name)) != GM_SUCCESS) {
            return GM_ERR_COMMAND_FAILED;
        }
    }
    
    /* Check if remote exists */
    if (!remote_exists(remote_name)) {
        PRINT_ERROR("Remote '%s' does not exist", remote_name);
        PRINT_INFO("Use 'Add Remote' to configure a remote first");
        return GM_ERR_REMOTE_NOT_FOUND;
    }
    
    /* Check for uncommitted changes */
    repo_status_t *status = get_repo_status();
    if (status != NULL) {
        if (status->has_uncommitted_changes) {
            PRINT_WARNING("You have uncommitted changes");
            PRINT_INFO("Consider committing before pushing");
        }
        free_repo_status(status);
    }
    
    PRINT_INFO("Pushing '%s' to '%s'...", branch_name, remote_name);
    
    char cmd[MAX_COMMAND_LEN];
    if (set_upstream) {
        snprintf(cmd, sizeof(cmd), "push -u \"%s\" \"%s\"", remote_name, branch_name);
    } else {
        snprintf(cmd, sizeof(cmd), "push \"%s\" \"%s\"", remote_name, branch_name);
    }
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            /* Analyze error */
            if (strstr(result->error, "rejected") != NULL) {
                PRINT_ERROR("Push rejected - remote has newer changes");
                PRINT_INFO("Pull the latest changes first, or use force push (dangerous!)");
                free_cmd_result(result);
                return GM_ERR_PUSH_FAILED;
            }
            
            if (strstr(result->error, "Could not resolve") != NULL ||
                strstr(result->error, "unable to access") != NULL) {
                PRINT_ERROR("Network error: Unable to reach remote '%s'", remote_name);
                free_cmd_result(result);
                return GM_ERR_PUSH_FAILED;
            }
            
            if (strstr(result->error, "Authentication") != NULL ||
                strstr(result->error, "Permission denied") != NULL ||
                strstr(result->error, "denied") != NULL) {
                PRINT_ERROR("Authentication failed for remote '%s'", remote_name);
                PRINT_INFO("Check your credentials or SSH keys");
                free_cmd_result(result);
                return GM_ERR_PUSH_FAILED;
            }
            
            if (strstr(result->error, "no upstream branch") != NULL ||
                strstr(result->error, "has no upstream") != NULL) {
                PRINT_WARNING("Branch has no upstream tracking");
                PRINT_INFO("Use 'Push with Set Upstream' option");
                free_cmd_result(result);
                return GM_ERR_PUSH_FAILED;
            }
            
            PRINT_ERROR("Push failed: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_PUSH_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Pushed '%s' to '%s/%s'", branch_name, remote_name, branch_name);
    
    return GM_SUCCESS;
}

/**
 * Force push a branch to a remote
 * 
 * @param remote Remote name
 * @param branch Branch name
 * @return gm_error_t Error code
 */
gm_error_t push_with_force(const char *remote, const char *branch) {
    char remote_name[MAX_BRANCH_NAME] = "origin";
    char branch_name[MAX_BRANCH_NAME];
    
    if (remote != NULL && strlen(remote) > 0) {
        strncpy(remote_name, remote, sizeof(remote_name) - 1);
        remote_name[sizeof(remote_name) - 1] = '\0';
    }
    
    if (branch != NULL && strlen(branch) > 0) {
        strncpy(branch_name, branch, sizeof(branch_name) - 1);
        branch_name[sizeof(branch_name) - 1] = '\0';
    } else {
        if (get_current_branch(branch_name, sizeof(branch_name)) != GM_SUCCESS) {
            return GM_ERR_COMMAND_FAILED;
        }
    }
    
    /* Check if remote exists */
    if (!remote_exists(remote_name)) {
        PRINT_ERROR("Remote '%s' does not exist", remote_name);
        return GM_ERR_REMOTE_NOT_FOUND;
    }
    
    /* Warn about protected branches */
    if (strcmp(branch_name, "main") == 0 || strcmp(branch_name, "master") == 0) {
        PRINT_WARNING("Force pushing to protected branch '%s'!", branch_name);
        PRINT_WARNING("This can overwrite history and cause problems for other developers!");
    }
    
    PRINT_WARNING("Force pushing '%s' to '%s'...", branch_name, remote_name);
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "push --force-with-lease \"%s\" \"%s\"", remote_name, branch_name);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Force push failed: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_PUSH_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Force pushed '%s' to '%s/%s'", branch_name, remote_name, branch_name);
    
    return GM_SUCCESS;
}

/**
 * Set upstream tracking for a branch
 * 
 * @param remote Remote name
 * @param branch Branch name
 * @return gm_error_t Error code
 */
gm_error_t set_upstream(const char *remote, const char *branch) {
    if (remote == NULL || branch == NULL || 
        strlen(remote) == 0 || strlen(branch) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "branch --set-upstream-to=\"%s/%s\" \"%s\"", 
             remote, branch, branch);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            PRINT_ERROR("Failed to set upstream: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Set upstream for '%s' to '%s/%s'", branch, remote, branch);
    
    return GM_SUCCESS;
}

/* ============================================================================
 * Pull Functions
 * ============================================================================ */

/**
 * Pull changes from a remote
 * 
 * @param remote Remote name (NULL for default)
 * @param branch Branch name (NULL for current)
 * @return gm_error_t Error code
 */
gm_error_t pull_branch(const char *remote, const char *branch) {
    char remote_name[MAX_BRANCH_NAME] = "origin";
    char branch_name[MAX_BRANCH_NAME];
    
    if (remote != NULL && strlen(remote) > 0) {
        strncpy(remote_name, remote, sizeof(remote_name) - 1);
        remote_name[sizeof(remote_name) - 1] = '\0';
    }
    
    if (branch != NULL && strlen(branch) > 0) {
        strncpy(branch_name, branch, sizeof(branch_name) - 1);
        branch_name[sizeof(branch_name) - 1] = '\0';
    } else {
        if (get_current_branch(branch_name, sizeof(branch_name)) != GM_SUCCESS) {
            return GM_ERR_COMMAND_FAILED;
        }
    }
    
    /* Check for uncommitted changes */
    repo_status_t *status = get_repo_status();
    if (status != NULL) {
        if (status->has_uncommitted_changes) {
            PRINT_WARNING("You have uncommitted changes");
            PRINT_INFO("Consider stashing or committing before pulling");
            free_repo_status(status);
            return GM_ERR_UNCOMMITTED_CHANGES;
        }
        free_repo_status(status);
    }
    
    PRINT_INFO("Pulling '%s' from '%s'...", branch_name, remote_name);
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "pull \"%s\" \"%s\"", remote_name, branch_name);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL || result->output != NULL) {
            char *check_str = result->error != NULL ? result->error : result->output;
            
            /* Check for conflicts */
            if (strstr(check_str, "CONFLICT") != NULL ||
                strstr(check_str, "Automatic merge failed") != NULL) {
                PRINT_ERROR("Pull resulted in merge conflicts!");
                PRINT_INFO("Resolve conflicts and commit, or abort with 'git merge --abort'");
                free_cmd_result(result);
                return GM_ERR_MERGE_CONFLICT;
            }
            
            if (strstr(check_str, "Could not resolve") != NULL ||
                strstr(check_str, "unable to access") != NULL) {
                PRINT_ERROR("Network error: Unable to reach remote '%s'", remote_name);
                free_cmd_result(result);
                return GM_ERR_PULL_FAILED;
            }
            
            if (result->error != NULL) {
                PRINT_ERROR("Pull failed: %s", result->error);
            }
        }
        free_cmd_result(result);
        return GM_ERR_PULL_FAILED;
    }
    
    /* Check if anything was pulled */
    if (result->output != NULL) {
        if (strstr(result->output, "Already up to date") != NULL ||
            strstr(result->output, "Already up-to-date") != NULL) {
            PRINT_INFO("Already up to date");
        } else {
            PRINT_SUCCESS("Pulled latest changes from '%s/%s'", remote_name, branch_name);
        }
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Pull with rebase instead of merge
 * 
 * @param remote Remote name
 * @param branch Branch name
 * @return gm_error_t Error code
 */
gm_error_t pull_rebase(const char *remote, const char *branch) {
    char remote_name[MAX_BRANCH_NAME] = "origin";
    char branch_name[MAX_BRANCH_NAME];
    
    if (remote != NULL && strlen(remote) > 0) {
        strncpy(remote_name, remote, sizeof(remote_name) - 1);
        remote_name[sizeof(remote_name) - 1] = '\0';
    }
    
    if (branch != NULL && strlen(branch) > 0) {
        strncpy(branch_name, branch, sizeof(branch_name) - 1);
        branch_name[sizeof(branch_name) - 1] = '\0';
    } else {
        if (get_current_branch(branch_name, sizeof(branch_name)) != GM_SUCCESS) {
            return GM_ERR_COMMAND_FAILED;
        }
    }
    
    /* Check for uncommitted changes */
    repo_status_t *status = get_repo_status();
    if (status != NULL) {
        if (status->has_uncommitted_changes) {
            PRINT_WARNING("You have uncommitted changes");
            PRINT_INFO("Stash or commit changes before rebasing");
            free_repo_status(status);
            return GM_ERR_UNCOMMITTED_CHANGES;
        }
        free_repo_status(status);
    }
    
    PRINT_INFO("Pulling with rebase from '%s/%s'...", remote_name, branch_name);
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "pull --rebase \"%s\" \"%s\"", remote_name, branch_name);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL || result->output != NULL) {
            char *check_str = result->error != NULL ? result->error : result->output;
            
            if (strstr(check_str, "CONFLICT") != NULL) {
                PRINT_ERROR("Rebase resulted in conflicts!");
                PRINT_INFO("Resolve conflicts and run 'git rebase --continue'");
                PRINT_INFO("Or abort with 'git rebase --abort'");
                free_cmd_result(result);
                return GM_ERR_MERGE_CONFLICT;
            }
            
            if (result->error != NULL) {
                PRINT_ERROR("Pull rebase failed: %s", result->error);
            }
        }
        free_cmd_result(result);
        return GM_ERR_PULL_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Pulled and rebased from '%s/%s'", remote_name, branch_name);
    
    return GM_SUCCESS;
}

/**
 * Show sync status with remote
 * 
 * @return gm_error_t Error code
 */
gm_error_t show_sync_status(void) {
    char current_branch[MAX_BRANCH_NAME];
    
    if (get_current_branch(current_branch, sizeof(current_branch)) != GM_SUCCESS) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    printf("\n" COLOR_BOLD "Sync Status for branch '%s':" COLOR_RESET "\n\n", current_branch);
    
    /* Get upstream info */
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "rev-parse --abbrev-ref \"%s@{upstream}\"", current_branch);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL || result->exit_code != 0) {
        if (result) free_cmd_result(result);
        PRINT_INFO("No upstream branch configured");
        PRINT_INFO("Use 'Push with Set Upstream' to configure tracking");
        return GM_SUCCESS;
    }
    
    char upstream[MAX_BRANCH_NAME];
    strncpy(upstream, trim_whitespace(result->output), sizeof(upstream) - 1);
    upstream[sizeof(upstream) - 1] = '\0';
    free_cmd_result(result);
    
    printf("Upstream: " COLOR_CYAN "%s" COLOR_RESET "\n", upstream);
    
    /* Get ahead/behind count */
    snprintf(cmd, sizeof(cmd), "rev-list --left-right --count \"%s\"...\"%s@{upstream}\"", 
             current_branch, current_branch);
    
    result = exec_git_command(cmd);
    
    if (result != NULL && result->exit_code == 0 && result->output != NULL) {
        int ahead = 0, behind = 0;
        if (sscanf(result->output, "%d\t%d", &ahead, &behind) == 2) {
            if (ahead == 0 && behind == 0) {
                printf("Status: " COLOR_GREEN "Up to date" COLOR_RESET "\n");
            } else {
                if (ahead > 0) {
                    printf("  " COLOR_YELLOW "%d commit(s) ahead" COLOR_RESET " - ready to push\n", ahead);
                }
                if (behind > 0) {
                    printf("  " COLOR_RED "%d commit(s) behind" COLOR_RESET " - need to pull\n", behind);
                }
            }
        }
    }
    
    if (result != NULL) {
        free_cmd_result(result);
    }
    
    printf("\n");
    return GM_SUCCESS;
}
