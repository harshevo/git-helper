/**
 * branch.c - Branch Management Functions for Git Master
 * 
 * Contains functions for creating, deleting, switching, listing,
 * and managing Git branches with fault tolerance.
 */

#include "git_master.h"

/* ============================================================================
 * Repository Status Functions
 * ============================================================================ */

/**
 * Check if the current directory is a Git repository
 * 
 * @param path Path to check (NULL for current directory)
 * @param is_repo Output: true if it's a git repo
 * @return gm_error_t Error code
 */
gm_error_t check_git_repository(const char *path, bool *is_repo) {
    if (is_repo == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    *is_repo = false;
    
    cmd_result_t *result;
    
    if (path != NULL && strlen(path) > 0) {
        char cmd[MAX_COMMAND_LEN];
        snprintf(cmd, sizeof(cmd), "-C \"%s\" rev-parse --is-inside-work-tree", path);
        result = exec_git_command(cmd);
    } else {
        result = exec_git_command("rev-parse --is-inside-work-tree");
    }
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code == 0 && result->output != NULL) {
        char *trimmed = trim_whitespace(result->output);
        *is_repo = (strcmp(trimmed, "true") == 0);
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Initialize a new Git repository
 * 
 * @param path Path where to initialize (NULL for current directory)
 * @return gm_error_t Error code
 */
gm_error_t init_repository(const char *path) {
    cmd_result_t *result;
    
    if (path != NULL && strlen(path) > 0) {
        char cmd[MAX_COMMAND_LEN];
        snprintf(cmd, sizeof(cmd), "init \"%s\"", path);
        result = exec_git_command(cmd);
    } else {
        result = exec_git_command("init");
    }
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    gm_error_t err = (result->exit_code == 0) ? GM_SUCCESS : GM_ERR_COMMAND_FAILED;
    free_cmd_result(result);
    
    return err;
}

/**
 * Get the current branch name
 * 
 * @param branch_name Output buffer for branch name
 * @param max_len Maximum length of output buffer
 * @return gm_error_t Error code
 */
gm_error_t get_current_branch(char *branch_name, size_t max_len) {
    if (branch_name == NULL || max_len == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    cmd_result_t *result = exec_git_command("rev-parse --abbrev-ref HEAD");
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        /* Check if it's a fresh repo with no commits */
        if (result->error != NULL && strstr(result->error, "HEAD") != NULL) {
            free_cmd_result(result);
            
            /* Try to get branch from symbolic-ref */
            result = exec_git_command("symbolic-ref --short HEAD");
            if (result == NULL || result->exit_code != 0) {
                if (result) free_cmd_result(result);
                strncpy(branch_name, "main", max_len - 1);
                branch_name[max_len - 1] = '\0';
                return GM_SUCCESS;
            }
        } else {
            free_cmd_result(result);
            return GM_ERR_COMMAND_FAILED;
        }
    }
    
    if (result->output != NULL && strlen(result->output) > 0) {
        char *trimmed = trim_whitespace(result->output);
        strncpy(branch_name, trimmed, max_len - 1);
        branch_name[max_len - 1] = '\0';
    } else {
        strncpy(branch_name, "unknown", max_len - 1);
        branch_name[max_len - 1] = '\0';
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Get complete repository status
 * 
 * @return repo_status_t* Repository status (must be freed with free_repo_status)
 */
repo_status_t* get_repo_status(void) {
    repo_status_t *status = (repo_status_t*)safe_calloc(1, sizeof(repo_status_t));
    
    if (status == NULL) {
        return NULL;
    }
    
    /* Check if this is a git repository */
    gm_error_t err = check_git_repository(NULL, &status->is_git_repo);
    if (err != GM_SUCCESS || !status->is_git_repo) {
        return status;
    }
    
    /* Get current working directory */
    if (getcwd(status->repo_path, sizeof(status->repo_path)) == NULL) {
        strncpy(status->repo_path, ".", sizeof(status->repo_path) - 1);
    }
    
    /* Get current branch */
    get_current_branch(status->current_branch, sizeof(status->current_branch));
    
    /* Check for uncommitted changes */
    cmd_result_t *result = exec_git_command("status --porcelain");
    if (result != NULL && result->exit_code == 0 && result->output != NULL) {
        status->has_uncommitted_changes = (strlen(result->output) > 0);
        
        /* Count modified, staged, and untracked files */
        char *line = strtok(result->output, "\n");
        while (line != NULL) {
            if (strlen(line) >= 2) {
                char index_status = line[0];
                char worktree_status = line[1];
                
                if (index_status != ' ' && index_status != '?') {
                    status->staged_files_count++;
                    status->has_staged_changes = true;
                }
                
                if (worktree_status != ' ' && worktree_status != '?') {
                    status->modified_files_count++;
                }
                
                if (index_status == '?' && worktree_status == '?') {
                    status->untracked_files_count++;
                    status->has_untracked_files = true;
                }
            }
            line = strtok(NULL, "\n");
        }
        free_cmd_result(result);
    } else if (result != NULL) {
        free_cmd_result(result);
    }
    
    return status;
}

/**
 * Refresh repository status
 * 
 * @param status Existing status structure to update
 * @return gm_error_t Error code
 */
gm_error_t refresh_repo_status(repo_status_t *status) {
    if (status == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Reset counters */
    status->has_uncommitted_changes = false;
    status->has_staged_changes = false;
    status->has_untracked_files = false;
    status->modified_files_count = 0;
    status->staged_files_count = 0;
    status->untracked_files_count = 0;
    
    /* Free existing branches if any */
    if (status->branches != NULL) {
        free(status->branches);
        status->branches = NULL;
        status->branch_count = 0;
    }
    
    /* Free existing remotes if any */
    if (status->remotes != NULL) {
        free_string_array(status->remotes, status->remote_count);
        status->remotes = NULL;
        status->remote_count = 0;
    }
    
    /* Refresh using get_repo_status logic */
    repo_status_t *new_status = get_repo_status();
    if (new_status != NULL) {
        /* Copy relevant fields */
        status->is_git_repo = new_status->is_git_repo;
        status->has_uncommitted_changes = new_status->has_uncommitted_changes;
        status->has_staged_changes = new_status->has_staged_changes;
        status->has_untracked_files = new_status->has_untracked_files;
        status->modified_files_count = new_status->modified_files_count;
        status->staged_files_count = new_status->staged_files_count;
        status->untracked_files_count = new_status->untracked_files_count;
        strncpy(status->current_branch, new_status->current_branch, sizeof(status->current_branch));
        
        free_repo_status(new_status);
    }
    
    return GM_SUCCESS;
}

/**
 * Free repository status structure
 * 
 * @param status The status to free
 */
void free_repo_status(repo_status_t *status) {
    if (status == NULL) {
        return;
    }
    
    if (status->branches != NULL) {
        free(status->branches);
        status->branches = NULL;
    }
    
    if (status->remotes != NULL) {
        free_string_array(status->remotes, status->remote_count);
        status->remotes = NULL;
    }
    
    free(status);
}

/* ============================================================================
 * Branch Operations
 * ============================================================================ */

/**
 * Check if a branch exists
 * 
 * @param branch_name Name of the branch to check
 * @return bool True if branch exists
 */
bool branch_exists(const char *branch_name) {
    if (branch_name == NULL || strlen(branch_name) == 0) {
        return false;
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "show-ref --verify --quiet refs/heads/%s", branch_name);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return false;
    }
    
    bool exists = (result->exit_code == 0);
    free_cmd_result(result);
    
    return exists;
}

/**
 * Create a new branch
 * 
 * @param branch_name Name of the new branch
 * @param base_branch Base branch to create from (NULL for current branch)
 * @return gm_error_t Error code
 */
gm_error_t create_branch(const char *branch_name, const char *base_branch) {
    if (branch_name == NULL || strlen(branch_name) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Validate branch name */
    if (!is_valid_branch_name(branch_name)) {
        PRINT_ERROR("Invalid branch name: '%s'", branch_name);
        return GM_ERR_INVALID_BRANCH_NAME;
    }
    
    /* Check if branch already exists */
    if (branch_exists(branch_name)) {
        PRINT_ERROR("Branch '%s' already exists", branch_name);
        return GM_ERR_BRANCH_EXISTS;
    }
    
    char cmd[MAX_COMMAND_LEN];
    
    if (base_branch != NULL && strlen(base_branch) > 0) {
        /* Verify base branch exists */
        if (!branch_exists(base_branch)) {
            PRINT_ERROR("Base branch '%s' does not exist", base_branch);
            return GM_ERR_BRANCH_NOT_FOUND;
        }
        snprintf(cmd, sizeof(cmd), "branch \"%s\" \"%s\"", branch_name, base_branch);
    } else {
        snprintf(cmd, sizeof(cmd), "branch \"%s\"", branch_name);
    }
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL && strlen(result->error) > 0) {
            PRINT_ERROR("Failed to create branch: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Created branch '%s'", branch_name);
    
    return GM_SUCCESS;
}

/**
 * Delete a branch
 * 
 * @param branch_name Name of the branch to delete
 * @param force Force delete even if not fully merged
 * @return gm_error_t Error code
 */
gm_error_t delete_branch(const char *branch_name, bool force) {
    if (branch_name == NULL || strlen(branch_name) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Check if branch exists */
    if (!branch_exists(branch_name)) {
        PRINT_ERROR("Branch '%s' does not exist", branch_name);
        return GM_ERR_BRANCH_NOT_FOUND;
    }
    
    /* Check if trying to delete current branch */
    char current_branch[MAX_BRANCH_NAME];
    if (get_current_branch(current_branch, sizeof(current_branch)) == GM_SUCCESS) {
        if (strcmp(branch_name, current_branch) == 0) {
            PRINT_ERROR("Cannot delete the current branch '%s'", branch_name);
            return GM_ERR_DELETE_CURRENT;
        }
    }
    
    /* Protect main branches (configurable warning) */
    if (strcmp(branch_name, "main") == 0 || strcmp(branch_name, "master") == 0) {
        PRINT_WARNING("Attempting to delete protected branch '%s'", branch_name);
        if (!force) {
            PRINT_ERROR("Use force delete for protected branches");
            return GM_ERR_PROTECTED_BRANCH;
        }
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "branch %s \"%s\"", force ? "-D" : "-d", branch_name);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            /* Check for unmerged changes warning */
            if (strstr(result->error, "not fully merged") != NULL) {
                PRINT_WARNING("Branch '%s' is not fully merged", branch_name);
                PRINT_INFO("Use force delete (-D) to delete anyway");
            } else {
                PRINT_ERROR("Failed to delete branch: %s", result->error);
            }
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Deleted branch '%s'", branch_name);
    
    return GM_SUCCESS;
}

/**
 * Switch to a different branch
 * 
 * @param branch_name Name of the branch to switch to
 * @return gm_error_t Error code
 */
gm_error_t switch_branch(const char *branch_name) {
    if (branch_name == NULL || strlen(branch_name) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Check if branch exists */
    if (!branch_exists(branch_name)) {
        PRINT_ERROR("Branch '%s' does not exist", branch_name);
        return GM_ERR_BRANCH_NOT_FOUND;
    }
    
    /* Check for uncommitted changes that might be lost */
    repo_status_t *status = get_repo_status();
    if (status != NULL) {
        if (status->has_uncommitted_changes) {
            PRINT_WARNING("You have uncommitted changes");
            PRINT_INFO("Consider committing or stashing before switching branches");
        }
        free_repo_status(status);
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "checkout \"%s\"", branch_name);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            /* Check for specific errors */
            if (strstr(result->error, "uncommitted changes") != NULL ||
                strstr(result->error, "would be overwritten") != NULL) {
                PRINT_ERROR("Cannot switch: uncommitted changes would be lost");
                PRINT_INFO("Commit or stash your changes first");
                free_cmd_result(result);
                return GM_ERR_UNCOMMITTED_CHANGES;
            }
            PRINT_ERROR("Failed to switch branch: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_CHECKOUT_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Switched to branch '%s'", branch_name);
    
    return GM_SUCCESS;
}

/**
 * Rename a branch
 * 
 * @param old_name Current name of the branch
 * @param new_name New name for the branch
 * @return gm_error_t Error code
 */
gm_error_t rename_branch(const char *old_name, const char *new_name) {
    if (old_name == NULL || new_name == NULL || 
        strlen(old_name) == 0 || strlen(new_name) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    /* Validate new branch name */
    if (!is_valid_branch_name(new_name)) {
        PRINT_ERROR("Invalid branch name: '%s'", new_name);
        return GM_ERR_INVALID_BRANCH_NAME;
    }
    
    /* Check if old branch exists */
    if (!branch_exists(old_name)) {
        PRINT_ERROR("Branch '%s' does not exist", old_name);
        return GM_ERR_BRANCH_NOT_FOUND;
    }
    
    /* Check if new name already exists */
    if (branch_exists(new_name)) {
        PRINT_ERROR("Branch '%s' already exists", new_name);
        return GM_ERR_BRANCH_EXISTS;
    }
    
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "branch -m \"%s\" \"%s\"", old_name, new_name);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            PRINT_ERROR("Failed to rename branch: %s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    free_cmd_result(result);
    PRINT_SUCCESS("Renamed branch '%s' to '%s'", old_name, new_name);
    
    return GM_SUCCESS;
}

/**
 * List all branches
 * 
 * @param branches Output: array of branch info structures
 * @param count Output: number of branches
 * @param include_remote Include remote branches
 * @return gm_error_t Error code
 */
gm_error_t list_branches(branch_info_t **branches, int *count, bool include_remote) {
    if (branches == NULL || count == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    *branches = NULL;
    *count = 0;
    
    /* Get branch list */
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "branch %s --format='%%(refname:short)|%%(upstream:short)|%%(HEAD)'",
             include_remote ? "-a" : "");
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output == NULL || strlen(result->output) == 0) {
        free_cmd_result(result);
        return GM_SUCCESS; /* No branches yet */
    }
    
    /* Count lines */
    int line_count = 1;
    char *output_copy = safe_strdup(result->output);
    if (output_copy == NULL) {
        free_cmd_result(result);
        return GM_ERR_MEMORY_ALLOC;
    }
    
    for (char *p = output_copy; *p; p++) {
        if (*p == '\n') line_count++;
    }
    
    /* Allocate branch array */
    *branches = (branch_info_t*)safe_calloc(line_count, sizeof(branch_info_t));
    if (*branches == NULL) {
        free(output_copy);
        free_cmd_result(result);
        return GM_ERR_MEMORY_ALLOC;
    }
    
    /* Parse each line */
    int idx = 0;
    char *line = strtok(output_copy, "\n");
    
    while (line != NULL && idx < line_count) {
        if (strlen(line) > 0) {
            /* Parse format: name|upstream|HEAD */
            int part_count;
            char **parts = split_string(line, '|', &part_count);
            
            if (parts != NULL && part_count >= 1) {
                strncpy((*branches)[idx].name, trim_whitespace(parts[0]), 
                        sizeof((*branches)[idx].name) - 1);
                
                if (part_count >= 2 && strlen(parts[1]) > 0) {
                    strncpy((*branches)[idx].remote, parts[1], 
                            sizeof((*branches)[idx].remote) - 1);
                    (*branches)[idx].has_upstream = true;
                }
                
                if (part_count >= 3) {
                    (*branches)[idx].is_current = (strcmp(parts[2], "*") == 0);
                }
                
                /* Check if it's a remote branch */
                (*branches)[idx].is_remote = (strncmp((*branches)[idx].name, 
                                                       "remotes/", 8) == 0);
                
                idx++;
            }
            
            if (parts != NULL) {
                free_string_array(parts, part_count);
            }
        }
        line = strtok(NULL, "\n");
    }
    
    free(output_copy);
    free_cmd_result(result);
    *count = idx;
    
    return GM_SUCCESS;
}

/**
 * Get detailed information about a specific branch
 * 
 * @param branch_name Name of the branch
 * @param info Output: branch information structure
 * @return gm_error_t Error code
 */
gm_error_t get_branch_info(const char *branch_name, branch_info_t *info) {
    if (branch_name == NULL || info == NULL || strlen(branch_name) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    memset(info, 0, sizeof(branch_info_t));
    strncpy(info->name, branch_name, sizeof(info->name) - 1);
    
    /* Check if branch exists */
    if (!branch_exists(branch_name)) {
        return GM_ERR_BRANCH_NOT_FOUND;
    }
    
    /* Get last commit info */
    char cmd[MAX_COMMAND_LEN];
    snprintf(cmd, sizeof(cmd), "log -1 --format='%%H|%%s|%%at' \"%s\"", branch_name);
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result != NULL && result->exit_code == 0 && result->output != NULL) {
        int part_count;
        char **parts = split_string(trim_whitespace(result->output), '|', &part_count);
        
        if (parts != NULL && part_count >= 3) {
            strncpy(info->last_commit_hash, parts[0], sizeof(info->last_commit_hash) - 1);
            strncpy(info->last_commit_msg, parts[1], sizeof(info->last_commit_msg) - 1);
            info->last_commit_time = (time_t)atol(parts[2]);
        }
        
        if (parts != NULL) {
            free_string_array(parts, part_count);
        }
    }
    
    if (result != NULL) {
        free_cmd_result(result);
    }
    
    /* Check if it's the current branch */
    char current[MAX_BRANCH_NAME];
    if (get_current_branch(current, sizeof(current)) == GM_SUCCESS) {
        info->is_current = (strcmp(branch_name, current) == 0);
    }
    
    /* Get upstream tracking info */
    snprintf(cmd, sizeof(cmd), "rev-parse --abbrev-ref \"%s@{upstream}\"", branch_name);
    result = exec_git_command(cmd);
    
    if (result != NULL && result->exit_code == 0 && result->output != NULL) {
        strncpy(info->remote, trim_whitespace(result->output), sizeof(info->remote) - 1);
        info->has_upstream = true;
        free_cmd_result(result);
        
        /* Get ahead/behind count */
        snprintf(cmd, sizeof(cmd), "rev-list --left-right --count \"%s\"...\"%s@{upstream}\"", 
                 branch_name, branch_name);
        result = exec_git_command(cmd);
        
        if (result != NULL && result->exit_code == 0 && result->output != NULL) {
            int ahead = 0, behind = 0;
            if (sscanf(result->output, "%d\t%d", &ahead, &behind) == 2) {
                info->commits_ahead = ahead;
                info->commits_behind = behind;
            }
        }
    }
    
    if (result != NULL) {
        free_cmd_result(result);
    }
    
    return GM_SUCCESS;
}
