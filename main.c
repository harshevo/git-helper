/**
 * main.c - Git Master: Git Branch Management System
 * 
 * A fault-tolerant C program for managing Git branches, commits, and merges.
 * Main entry point with interactive menu system.
 * 
 * Author: Git Master
 * License: MIT
 */

#include "git_master.h"
#include "config.h"
#include <signal.h>
#include <termios.h>
#include <fcntl.h>

/* Forward declarations for daemon functions */
extern daemon_state_t* daemon_init(config_t *config);
extern gm_error_t daemon_start(daemon_state_t *daemon);
extern gm_error_t daemon_stop(daemon_state_t *daemon);
extern void daemon_cleanup(daemon_state_t *daemon);
extern bool daemon_is_running(daemon_state_t *daemon);

/* Forward declarations for functions in other files */
extern gm_error_t show_status(void);
extern gm_error_t show_diff(bool staged);
extern gm_error_t show_log(int count);
extern gm_error_t list_stash(void);
extern gm_error_t show_remotes(void);
extern gm_error_t show_sync_status(void);
extern gm_error_t preview_merge(const char *source_branch);
extern bool is_merge_in_progress(void);
extern bool remote_exists(const char *name);

/* Global application state */
static app_state_t *g_app_state = NULL;
static volatile sig_atomic_t g_running = 1;

/* ============================================================================
 * Signal Handlers
 * ============================================================================ */

/**
 * Handle interrupt signal (Ctrl+C)
 */
void signal_handler(int sig) {
    (void)sig; /* Unused */
    g_running = 0;
    printf("\n" COLOR_YELLOW "Interrupt received. Exiting..." COLOR_RESET "\n");
}

/* ============================================================================
 * User Interface Functions
 * ============================================================================ */

/**
 * Clear the screen
 */
void clear_screen(void) {
    printf("\033[2J\033[H");
}

/**
 * Wait for user to press Enter
 */
void wait_for_enter(void) {
    printf("\n" COLOR_CYAN "Press Enter to continue..." COLOR_RESET);
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/**
 * Get user input with prompt
 * 
 * @param prompt The prompt to display
 * @param max_len Maximum length of input
 * @return char* User input (must be freed)
 */
char* get_user_input(const char *prompt, size_t max_len) {
    if (prompt == NULL || max_len == 0) {
        return NULL;
    }
    
    printf("%s", prompt);
    fflush(stdout);
    
    char *buffer = (char*)safe_malloc(max_len + 1);
    if (buffer == NULL) {
        return NULL;
    }
    
    if (fgets(buffer, (int)max_len, stdin) == NULL) {
        free(buffer);
        return NULL;
    }
    
    /* Remove trailing newline */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    
    return buffer;
}

/**
 * Get yes/no confirmation from user
 * 
 * @param prompt The prompt to display
 * @return bool True for yes, false for no
 */
bool get_user_confirmation(const char *prompt) {
    printf("%s " COLOR_YELLOW "(y/n): " COLOR_RESET, prompt);
    fflush(stdout);
    
    char input[16];
    if (fgets(input, sizeof(input), stdin) == NULL) {
        return false;
    }
    
    char c = input[0];
    return (c == 'y' || c == 'Y');
}

/**
 * Get menu choice from user
 * 
 * @param min Minimum valid choice
 * @param max Maximum valid choice
 * @return int User's choice, or -1 for invalid
 */
int get_menu_choice(int min, int max) {
    printf("\n" COLOR_BOLD "Enter choice [%d-%d]: " COLOR_RESET, min, max);
    fflush(stdout);
    
    char input[32];
    if (fgets(input, sizeof(input), stdin) == NULL) {
        return -1;
    }
    
    int choice;
    if (sscanf(input, "%d", &choice) != 1) {
        return -1;
    }
    
    if (choice < min || choice > max) {
        return -1;
    }
    
    return choice;
}

/**
 * Display the application header
 */
void display_header(void) {
    printf("\n");
    printf(COLOR_BOLD COLOR_CYAN "╔══════════════════════════════════════════════════════════╗\n");
    printf("║             GIT MASTER - Branch Management               ║\n");
    printf("╚══════════════════════════════════════════════════════════╝" COLOR_RESET "\n");
}

/**
 * Display repository status summary
 */
void display_repo_status(repo_status_t *status) {
    if (status == NULL) {
        return;
    }
    
    printf("\n");
    printf(COLOR_BOLD "Repository Status:" COLOR_RESET "\n");
    printf("  Path: %s\n", status->repo_path);
    printf("  Current Branch: " COLOR_GREEN "%s" COLOR_RESET "\n", status->current_branch);
    
    if (status->has_uncommitted_changes) {
        printf("  Changes: ");
        if (status->staged_files_count > 0) {
            printf(COLOR_GREEN "%d staged" COLOR_RESET " ", status->staged_files_count);
        }
        if (status->modified_files_count > 0) {
            printf(COLOR_YELLOW "%d modified" COLOR_RESET " ", status->modified_files_count);
        }
        if (status->untracked_files_count > 0) {
            printf(COLOR_RED "%d untracked" COLOR_RESET, status->untracked_files_count);
        }
        printf("\n");
    } else {
        printf("  Changes: " COLOR_GREEN "Clean" COLOR_RESET "\n");
    }
    
    printf("\n");
}

/* ============================================================================
 * Menu Display Functions
 * ============================================================================ */

/**
 * Display main menu
 */
void display_main_menu(void) {
    printf(COLOR_BOLD "\n=== Main Menu ===" COLOR_RESET "\n\n");
    printf("  1. " COLOR_CYAN "Branch Management" COLOR_RESET "\n");
    printf("  2. " COLOR_CYAN "Commit Management" COLOR_RESET "\n");
    printf("  3. " COLOR_CYAN "Merge Operations" COLOR_RESET "\n");
    printf("  4. " COLOR_CYAN "Remote & Push/Pull" COLOR_RESET "\n");
    printf("  5. " COLOR_MAGENTA "History & Restore" COLOR_RESET "\n");
    printf("  6. " COLOR_CYAN "View Status" COLOR_RESET "\n");
    printf("  7. " COLOR_CYAN "View Log" COLOR_RESET "\n");
    printf("  0. " COLOR_RED "Exit" COLOR_RESET "\n");
}

/**
 * Display history menu
 */
void display_history_menu(void) {
    printf(COLOR_BOLD "\n=== History & Restore ===" COLOR_RESET "\n\n");
    printf("  1. " COLOR_CYAN "View Commit History" COLOR_RESET "\n");
    printf("  2. " COLOR_CYAN "Show Commit Details" COLOR_RESET "\n");
    printf("  3. " COLOR_CYAN "Show Commit Diff" COLOR_RESET "\n");
    printf("  4. " COLOR_CYAN "List Files in Commit" COLOR_RESET "\n");
    printf("  5. " COLOR_GREEN "Restore File from Commit" COLOR_RESET "\n");
    printf("  6. " COLOR_YELLOW "Revert Commit" COLOR_RESET " (creates undo commit)\n");
    printf("  7. " COLOR_RED "Reset to Commit" COLOR_RESET " (dangerous!)\n");
    printf("  8. " COLOR_MAGENTA "Cherry-pick Commit" COLOR_RESET "\n");
    printf("  9. " COLOR_CYAN "Compare Two Commits" COLOR_RESET "\n");
    printf(" 10. " COLOR_CYAN "View Reflog" COLOR_RESET " (recover lost commits)\n");
    printf(" 11. " COLOR_GREEN "Recover from Reflog" COLOR_RESET "\n");
    printf("  0. " COLOR_YELLOW "Back to Main Menu" COLOR_RESET "\n");
}

/**
 * Display branch management menu
 */
void display_branch_menu(void) {
    printf(COLOR_BOLD "\n=== Branch Management ===" COLOR_RESET "\n\n");
    printf("  1. " COLOR_GREEN "Create New Branch" COLOR_RESET "\n");
    printf("  2. " COLOR_CYAN "Switch Branch" COLOR_RESET "\n");
    printf("  3. " COLOR_YELLOW "List All Branches" COLOR_RESET "\n");
    printf("  4. " COLOR_RED "Delete Branch" COLOR_RESET "\n");
    printf("  5. " COLOR_MAGENTA "Rename Branch" COLOR_RESET "\n");
    printf("  6. " COLOR_CYAN "View Branch Details" COLOR_RESET "\n");
    printf("  0. " COLOR_YELLOW "Back to Main Menu" COLOR_RESET "\n");
}

/**
 * Display commit management menu
 */
void display_commit_menu(void) {
    printf(COLOR_BOLD "\n=== Commit Management ===" COLOR_RESET "\n\n");
    printf("  1. " COLOR_GREEN "Stage All Changes" COLOR_RESET "\n");
    printf("  2. " COLOR_GREEN "Stage Specific File" COLOR_RESET "\n");
    printf("  3. " COLOR_CYAN "Commit Staged Changes" COLOR_RESET "\n");
    printf("  4. " COLOR_YELLOW "View Uncommitted Changes" COLOR_RESET "\n");
    printf("  5. " COLOR_YELLOW "View Diff" COLOR_RESET "\n");
    printf("  6. " COLOR_RED "Discard Changes" COLOR_RESET "\n");
    printf("  7. " COLOR_MAGENTA "Stash Changes" COLOR_RESET "\n");
    printf("  8. " COLOR_MAGENTA "Pop Stash" COLOR_RESET "\n");
    printf("  9. " COLOR_MAGENTA "List Stash" COLOR_RESET "\n");
    printf("  0. " COLOR_YELLOW "Back to Main Menu" COLOR_RESET "\n");
}

/**
 * Display merge menu
 */
void display_merge_menu(void) {
    printf(COLOR_BOLD "\n=== Merge Operations ===" COLOR_RESET "\n\n");
    printf("  1. " COLOR_CYAN "Preview Merge" COLOR_RESET " (check for conflicts)\n");
    printf("  2. " COLOR_GREEN "Merge Branch" COLOR_RESET " (default strategy)\n");
    printf("  3. " COLOR_GREEN "Merge Branch" COLOR_RESET " (no fast-forward)\n");
    printf("  4. " COLOR_YELLOW "Squash Merge" COLOR_RESET "\n");
    printf("  5. " COLOR_RED "Abort Current Merge" COLOR_RESET "\n");
    printf("  0. " COLOR_YELLOW "Back to Main Menu" COLOR_RESET "\n");
}

/**
 * Display remote menu
 */
void display_remote_menu(void) {
    printf(COLOR_BOLD "\n=== Remote & Push/Pull ===" COLOR_RESET "\n\n");
    printf("  1. " COLOR_CYAN "Show Remotes" COLOR_RESET "\n");
    printf("  2. " COLOR_GREEN "Add Remote" COLOR_RESET "\n");
    printf("  3. " COLOR_RED "Remove Remote" COLOR_RESET "\n");
    printf("  4. " COLOR_CYAN "Fetch from Remote" COLOR_RESET "\n");
    printf("  5. " COLOR_GREEN "Push to Remote" COLOR_RESET "\n");
    printf("  6. " COLOR_GREEN "Push (Set Upstream)" COLOR_RESET "\n");
    printf("  7. " COLOR_YELLOW "Pull from Remote" COLOR_RESET "\n");
    printf("  8. " COLOR_CYAN "Show Sync Status" COLOR_RESET "\n");
    printf("  0. " COLOR_YELLOW "Back to Main Menu" COLOR_RESET "\n");
}

/* ============================================================================
 * Menu Handler Functions
 * ============================================================================ */

/**
 * Handle branch management menu
 */
void handle_branch_menu(void) {
    int choice;
    char *input = NULL;
    char *input2 = NULL;
    
    while (g_running) {
        clear_screen();
        display_header();
        
        /* Show current branch */
        char current[MAX_BRANCH_NAME];
        if (get_current_branch(current, sizeof(current)) == GM_SUCCESS) {
            printf("\nCurrent branch: " COLOR_GREEN "%s" COLOR_RESET "\n", current);
        }
        
        display_branch_menu();
        choice = get_menu_choice(0, 6);
        
        printf("\n");
        
        switch (choice) {
            case 0:
                return;
                
            case 1: /* Create Branch */
                input = get_user_input("Enter new branch name: ", MAX_BRANCH_NAME);
                if (input != NULL && strlen(input) > 0) {
                    input2 = get_user_input("Base branch (Enter for current): ", MAX_BRANCH_NAME);
                    if (input2 != NULL && strlen(input2) > 0) {
                        create_branch(input, input2);
                    } else {
                        create_branch(input, NULL);
                    }
                    if (input2) { free(input2); input2 = NULL; }
                    
                    if (get_user_confirmation("Switch to new branch?")) {
                        switch_branch(input);
                    }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 2: /* Switch Branch */
                /* List branches first */
                {
                    branch_info_t *branches = NULL;
                    int count = 0;
                    
                    if (list_branches(&branches, &count, false) == GM_SUCCESS && count > 0) {
                        printf("Available branches:\n");
                        for (int i = 0; i < count; i++) {
                            printf("  %s%s%s\n", 
                                   branches[i].is_current ? COLOR_GREEN "* " : "  ",
                                   branches[i].name,
                                   branches[i].is_current ? COLOR_RESET : "");
                        }
                        printf("\n");
                        free(branches);
                    }
                }
                
                input = get_user_input("Enter branch name to switch to: ", MAX_BRANCH_NAME);
                if (input != NULL && strlen(input) > 0) {
                    switch_branch(input);
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 3: /* List Branches */
                {
                    branch_info_t *branches = NULL;
                    int count = 0;
                    
                    bool include_remote = get_user_confirmation("Include remote branches?");
                    
                    if (list_branches(&branches, &count, include_remote) == GM_SUCCESS) {
                        printf("\n" COLOR_BOLD "Branches (%d):" COLOR_RESET "\n", count);
                        for (int i = 0; i < count; i++) {
                            printf("  %s%s%s", 
                                   branches[i].is_current ? COLOR_GREEN "* " : "  ",
                                   branches[i].name,
                                   branches[i].is_current ? COLOR_RESET : "");
                            if (branches[i].has_upstream) {
                                printf(" -> %s", branches[i].remote);
                            }
                            printf("\n");
                        }
                        if (count == 0) {
                            printf("  (no branches)\n");
                        }
                        free(branches);
                    }
                }
                wait_for_enter();
                break;
                
            case 4: /* Delete Branch */
                input = get_user_input("Enter branch name to delete: ", MAX_BRANCH_NAME);
                if (input != NULL && strlen(input) > 0) {
                    printf(COLOR_YELLOW "Warning: This will delete branch '%s'\n" COLOR_RESET, input);
                    if (get_user_confirmation("Are you sure?")) {
                        bool force = get_user_confirmation("Force delete (even if not merged)?");
                        delete_branch(input, force);
                    } else {
                        PRINT_INFO("Cancelled");
                    }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 5: /* Rename Branch */
                input = get_user_input("Enter current branch name: ", MAX_BRANCH_NAME);
                if (input != NULL && strlen(input) > 0) {
                    input2 = get_user_input("Enter new name: ", MAX_BRANCH_NAME);
                    if (input2 != NULL && strlen(input2) > 0) {
                        rename_branch(input, input2);
                    }
                    if (input2) { free(input2); input2 = NULL; }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 6: /* View Branch Details */
                input = get_user_input("Enter branch name: ", MAX_BRANCH_NAME);
                if (input != NULL && strlen(input) > 0) {
                    branch_info_t info;
                    if (get_branch_info(input, &info) == GM_SUCCESS) {
                        printf("\n" COLOR_BOLD "Branch: %s" COLOR_RESET "\n", info.name);
                        printf("  Current: %s\n", info.is_current ? "Yes" : "No");
                        printf("  Last commit: %.8s\n", info.last_commit_hash);
                        printf("  Message: %s\n", info.last_commit_msg);
                        if (info.has_upstream) {
                            printf("  Upstream: %s\n", info.remote);
                            printf("  Ahead: %d, Behind: %d\n", 
                                   info.commits_ahead, info.commits_behind);
                        }
                    }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            default:
                PRINT_ERROR("Invalid choice");
                wait_for_enter();
        }
    }
}

/**
 * Handle commit management menu
 */
void handle_commit_menu(void) {
    int choice;
    char *input = NULL;
    
    while (g_running) {
        clear_screen();
        display_header();
        
        /* Show quick status */
        repo_status_t *status = get_repo_status();
        if (status != NULL) {
            display_repo_status(status);
            free_repo_status(status);
        }
        
        display_commit_menu();
        choice = get_menu_choice(0, 9);
        
        printf("\n");
        
        switch (choice) {
            case 0:
                return;
                
            case 1: /* Stage All */
                stage_all_changes();
                wait_for_enter();
                break;
                
            case 2: /* Stage File */
                /* Show unstaged files */
                {
                    char **files = NULL;
                    int count = 0;
                    if (get_uncommitted_changes(&files, &count) == GM_SUCCESS && count > 0) {
                        printf("Modified files:\n");
                        for (int i = 0; i < count; i++) {
                            printf("  %s\n", files[i]);
                        }
                        free_string_array(files, count);
                        printf("\n");
                    }
                }
                
                input = get_user_input("Enter file path to stage: ", MAX_PATH_LEN);
                if (input != NULL && strlen(input) > 0) {
                    stage_file(input);
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 3: /* Commit */
                input = get_user_input("Enter commit message: ", MAX_COMMIT_MSG);
                if (input != NULL && strlen(input) > 0) {
                    commit_changes(input);
                } else {
                    PRINT_ERROR("Commit message cannot be empty");
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 4: /* View Uncommitted Changes */
                show_status();
                wait_for_enter();
                break;
                
            case 5: /* View Diff */
                {
                    bool staged = get_user_confirmation("Show staged diff? (n for unstaged)");
                    show_diff(staged);
                }
                wait_for_enter();
                break;
                
            case 6: /* Discard Changes */
                printf(COLOR_RED "Warning: This will permanently discard changes!\n" COLOR_RESET);
                if (get_user_confirmation("Discard ALL changes?")) {
                    discard_all_changes();
                } else {
                    input = get_user_input("Enter specific file to discard (or Enter to cancel): ", MAX_PATH_LEN);
                    if (input != NULL && strlen(input) > 0) {
                        discard_changes(input);
                    }
                    if (input) { free(input); input = NULL; }
                }
                wait_for_enter();
                break;
                
            case 7: /* Stash */
                input = get_user_input("Stash message (optional): ", MAX_COMMIT_MSG);
                stash_changes(input);
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 8: /* Pop Stash */
                pop_stash();
                wait_for_enter();
                break;
                
            case 9: /* List Stash */
                list_stash();
                wait_for_enter();
                break;
                
            default:
                PRINT_ERROR("Invalid choice");
                wait_for_enter();
        }
    }
}

/**
 * Handle merge menu
 */
void handle_merge_menu(void) {
    int choice;
    char *input = NULL;
    
    while (g_running) {
        clear_screen();
        display_header();
        
        /* Show current branch */
        char current[MAX_BRANCH_NAME];
        if (get_current_branch(current, sizeof(current)) == GM_SUCCESS) {
            printf("\nCurrent branch: " COLOR_GREEN "%s" COLOR_RESET "\n", current);
        }
        
        /* Check for merge in progress */
        if (is_merge_in_progress()) {
            printf(COLOR_YELLOW "\n⚠ A merge is currently in progress!\n" COLOR_RESET);
        }
        
        display_merge_menu();
        choice = get_menu_choice(0, 5);
        
        printf("\n");
        
        switch (choice) {
            case 0:
                return;
                
            case 1: /* Preview Merge */
                {
                    /* List branches */
                    branch_info_t *branches = NULL;
                    int count = 0;
                    
                    if (list_branches(&branches, &count, false) == GM_SUCCESS && count > 0) {
                        printf("Available branches:\n");
                        for (int i = 0; i < count; i++) {
                            if (!branches[i].is_current) {
                                printf("  %s\n", branches[i].name);
                            }
                        }
                        printf("\n");
                        free(branches);
                    }
                }
                
                input = get_user_input("Enter branch to preview merge from: ", MAX_BRANCH_NAME);
                if (input != NULL && strlen(input) > 0) {
                    preview_merge(input);
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 2: /* Merge (Default) */
            case 3: /* Merge (No FF) */
            case 4: /* Squash Merge */
                {
                    /* List branches */
                    branch_info_t *branches = NULL;
                    int count = 0;
                    
                    if (list_branches(&branches, &count, false) == GM_SUCCESS && count > 0) {
                        printf("Available branches:\n");
                        for (int i = 0; i < count; i++) {
                            if (!branches[i].is_current) {
                                printf("  %s\n", branches[i].name);
                            }
                        }
                        printf("\n");
                        free(branches);
                    }
                }
                
                input = get_user_input("Enter branch to merge: ", MAX_BRANCH_NAME);
                if (input != NULL && strlen(input) > 0) {
                    merge_strategy_t strategy;
                    switch (choice) {
                        case 3:
                            strategy = MERGE_STRATEGY_NO_FF;
                            break;
                        case 4:
                            strategy = MERGE_STRATEGY_SQUASH;
                            break;
                        default:
                            strategy = MERGE_STRATEGY_DEFAULT;
                    }
                    
                    printf(COLOR_BOLD "\nMerge Strategy: " COLOR_RESET);
                    switch (strategy) {
                        case MERGE_STRATEGY_NO_FF:
                            printf("No Fast-Forward\n");
                            break;
                        case MERGE_STRATEGY_SQUASH:
                            printf("Squash\n");
                            break;
                        default:
                            printf("Default (Fast-Forward if possible)\n");
                    }
                    
                    if (get_user_confirmation("Proceed with merge?")) {
                        merge_result_t *result = merge_branch(input, strategy);
                        
                        if (result != NULL) {
                            if (result->has_conflicts) {
                                printf(COLOR_RED "\n⚠ MERGE BLOCKED DUE TO CONFLICTS!\n" COLOR_RESET);
                                printf("The merge was automatically aborted to prevent issues.\n");
                                printf("Please resolve conflicts manually or use a different approach.\n");
                            } else if (!result->success) {
                                printf(COLOR_RED "\nMerge failed: %s\n" COLOR_RESET, result->error_message);
                            }
                            free_merge_result(result);
                        }
                    } else {
                        PRINT_INFO("Merge cancelled");
                    }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 5: /* Abort Merge */
                if (is_merge_in_progress()) {
                    if (get_user_confirmation("Abort the current merge?")) {
                        abort_merge();
                    }
                } else {
                    PRINT_INFO("No merge in progress");
                }
                wait_for_enter();
                break;
                
            default:
                PRINT_ERROR("Invalid choice");
                wait_for_enter();
        }
    }
}

/**
 * Handle remote menu
 */
void handle_remote_menu(void) {
    int choice;
    char *input = NULL;
    char *input2 = NULL;
    
    while (g_running) {
        clear_screen();
        display_header();
        
        /* Show current branch and remotes */
        char current[MAX_BRANCH_NAME];
        if (get_current_branch(current, sizeof(current)) == GM_SUCCESS) {
            printf("\nCurrent branch: " COLOR_GREEN "%s" COLOR_RESET "\n", current);
        }
        
        display_remote_menu();
        choice = get_menu_choice(0, 8);
        
        printf("\n");
        
        switch (choice) {
            case 0:
                return;
                
            case 1: /* Show Remotes */
                show_remotes();
                wait_for_enter();
                break;
                
            case 2: /* Add Remote */
                input = get_user_input("Enter remote name (e.g., origin): ", MAX_BRANCH_NAME);
                if (input != NULL && strlen(input) > 0) {
                    input2 = get_user_input("Enter remote URL: ", MAX_PATH_LEN);
                    if (input2 != NULL && strlen(input2) > 0) {
                        add_remote(input, input2);
                    }
                    if (input2) { free(input2); input2 = NULL; }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 3: /* Remove Remote */
                show_remotes();
                input = get_user_input("Enter remote name to remove: ", MAX_BRANCH_NAME);
                if (input != NULL && strlen(input) > 0) {
                    if (get_user_confirmation("Remove remote?")) {
                        remove_remote(input);
                    }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 4: /* Fetch */
                {
                    char **remotes = NULL;
                    int count = 0;
                    
                    if (list_remotes(&remotes, &count) == GM_SUCCESS && count > 0) {
                        printf("Available remotes:\n");
                        for (int i = 0; i < count; i++) {
                            printf("  %s\n", remotes[i]);
                        }
                        free_string_array(remotes, count);
                        printf("\n");
                    }
                }
                
                if (get_user_confirmation("Fetch from all remotes?")) {
                    fetch_all();
                } else {
                    input = get_user_input("Enter remote name: ", MAX_BRANCH_NAME);
                    if (input != NULL && strlen(input) > 0) {
                        fetch_remote(input);
                    }
                    if (input) { free(input); input = NULL; }
                }
                wait_for_enter();
                break;
                
            case 5: /* Push */
                {
                    char *remote_name = get_user_input("Remote name (Enter for origin): ", MAX_BRANCH_NAME);
                    char *branch_name = get_user_input("Branch name (Enter for current): ", MAX_BRANCH_NAME);
                    
                    push_branch(
                        (remote_name && strlen(remote_name) > 0) ? remote_name : NULL,
                        (branch_name && strlen(branch_name) > 0) ? branch_name : NULL,
                        false
                    );
                    
                    if (remote_name) free(remote_name);
                    if (branch_name) free(branch_name);
                }
                wait_for_enter();
                break;
                
            case 6: /* Push with Set Upstream */
                {
                    char *remote_name = get_user_input("Remote name (Enter for origin): ", MAX_BRANCH_NAME);
                    char *branch_name = get_user_input("Branch name (Enter for current): ", MAX_BRANCH_NAME);
                    
                    push_branch(
                        (remote_name && strlen(remote_name) > 0) ? remote_name : NULL,
                        (branch_name && strlen(branch_name) > 0) ? branch_name : NULL,
                        true
                    );
                    
                    if (remote_name) free(remote_name);
                    if (branch_name) free(branch_name);
                }
                wait_for_enter();
                break;
                
            case 7: /* Pull */
                {
                    char *remote_name = get_user_input("Remote name (Enter for origin): ", MAX_BRANCH_NAME);
                    char *branch_name = get_user_input("Branch name (Enter for current): ", MAX_BRANCH_NAME);
                    
                    pull_branch(
                        (remote_name && strlen(remote_name) > 0) ? remote_name : NULL,
                        (branch_name && strlen(branch_name) > 0) ? branch_name : NULL
                    );
                    
                    if (remote_name) free(remote_name);
                    if (branch_name) free(branch_name);
                }
                wait_for_enter();
                break;
                
            case 8: /* Sync Status */
                show_sync_status();
                wait_for_enter();
                break;
                
            default:
                PRINT_ERROR("Invalid choice");
                wait_for_enter();
        }
    }
}

/**
 * Handle history menu
 */
void handle_history_menu(void) {
    int choice;
    char *input = NULL;
    char *input2 = NULL;
    
    while (g_running) {
        clear_screen();
        display_header();
        
        /* Show current branch */
        char current[MAX_BRANCH_NAME];
        if (get_current_branch(current, sizeof(current)) == GM_SUCCESS) {
            printf("\nCurrent branch: " COLOR_GREEN "%s" COLOR_RESET "\n", current);
        }
        
        display_history_menu();
        choice = get_menu_choice(0, 11);
        
        printf("\n");
        
        switch (choice) {
            case 0:
                return;
                
            case 1: /* View Commit History */
                {
                    char *count_str = get_user_input("Number of commits to show (Enter for 20): ", 16);
                    int count = 20;
                    if (count_str != NULL && strlen(count_str) > 0) {
                        count = atoi(count_str);
                        if (count <= 0) count = 20;
                    }
                    if (count_str) free(count_str);
                    
                    show_commit_history(count, false);
                }
                wait_for_enter();
                break;
                
            case 2: /* Show Commit Details */
                show_commit_history(10, false);
                input = get_user_input("Enter commit hash: ", 64);
                if (input != NULL && strlen(input) > 0) {
                    show_commit_details(input);
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 3: /* Show Commit Diff */
                show_commit_history(10, false);
                input = get_user_input("Enter commit hash: ", 64);
                if (input != NULL && strlen(input) > 0) {
                    show_commit_diff(input);
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 4: /* List Files in Commit */
                show_commit_history(10, false);
                input = get_user_input("Enter commit hash: ", 64);
                if (input != NULL && strlen(input) > 0) {
                    list_commit_files(input);
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 5: /* Restore File from Commit */
                show_commit_history(10, false);
                input = get_user_input("Enter commit hash: ", 64);
                if (input != NULL && strlen(input) > 0) {
                    /* Show files in that commit */
                    list_commit_files(input);
                    
                    input2 = get_user_input("Enter file path to restore: ", MAX_PATH_LEN);
                    if (input2 != NULL && strlen(input2) > 0) {
                        if (get_user_confirmation("Restore this file?")) {
                            restore_file_from_commit(input, input2);
                        }
                    }
                    if (input2) { free(input2); input2 = NULL; }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 6: /* Revert Commit */
                show_commit_history(10, false);
                printf("\n" COLOR_YELLOW "Revert creates a new commit that undoes changes." COLOR_RESET "\n");
                printf("This is the SAFE way to undo a commit.\n\n");
                
                input = get_user_input("Enter commit hash to revert: ", 64);
                if (input != NULL && strlen(input) > 0) {
                    show_commit_details(input);
                    if (get_user_confirmation("Revert this commit?")) {
                        revert_commit(input);
                    }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 7: /* Reset to Commit */
                show_commit_history(10, false);
                printf("\n" COLOR_RED "⚠ WARNING: Reset can permanently lose commits!" COLOR_RESET "\n");
                printf("Options:\n");
                printf("  soft  - Keep changes staged\n");
                printf("  mixed - Keep changes unstaged (default)\n");
                printf("  hard  - DISCARD all changes (dangerous!)\n\n");
                
                input = get_user_input("Enter commit hash to reset to: ", 64);
                if (input != NULL && strlen(input) > 0) {
                    show_commit_details(input);
                    
                    printf(COLOR_RED "\nThis will move HEAD to commit %s\n" COLOR_RESET, input);
                    printf("All commits after this will become orphaned!\n\n");
                    
                    if (get_user_confirmation("Are you SURE you want to reset?")) {
                        input2 = get_user_input("Reset mode (soft/mixed/hard): ", 16);
                        
                        const char *mode = "mixed";
                        if (input2 != NULL && strlen(input2) > 0) {
                            if (strcmp(input2, "soft") == 0) mode = "soft";
                            else if (strcmp(input2, "hard") == 0) {
                                if (get_user_confirmation("FINAL WARNING: Hard reset DELETES uncommitted work!")) {
                                    mode = "hard";
                                } else {
                                    mode = NULL;
                                }
                            }
                        }
                        
                        if (mode != NULL) {
                            reset_to_commit(input, mode);
                        }
                        
                        if (input2) { free(input2); input2 = NULL; }
                    }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 8: /* Cherry-pick Commit */
                printf("Cherry-pick applies a specific commit to the current branch.\n\n");
                
                input = get_user_input("Enter commit hash or branch to view: ", 64);
                if (input != NULL && strlen(input) > 0) {
                    /* Show commits from that ref */
                    char cmd[MAX_COMMAND_LEN];
                    snprintf(cmd, sizeof(cmd), "log --oneline -10 \"%s\"", input);
                    cmd_result_t *result = exec_git_command(cmd);
                    if (result != NULL && result->exit_code == 0 && result->output != NULL) {
                        printf("\nRecent commits on %s:\n%s\n", input, result->output);
                    }
                    if (result) free_cmd_result(result);
                }
                if (input) { free(input); input = NULL; }
                
                input = get_user_input("Enter commit hash to cherry-pick: ", 64);
                if (input != NULL && strlen(input) > 0) {
                    show_commit_details(input);
                    if (get_user_confirmation("Cherry-pick this commit?")) {
                        cherry_pick_commit(input);
                    }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 9: /* Compare Two Commits */
                show_commit_history(10, false);
                printf("\n");
                
                input = get_user_input("Enter first commit hash: ", 64);
                if (input != NULL && strlen(input) > 0) {
                    input2 = get_user_input("Enter second commit hash: ", 64);
                    if (input2 != NULL && strlen(input2) > 0) {
                        compare_commits(input, input2);
                    }
                    if (input2) { free(input2); input2 = NULL; }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            case 10: /* View Reflog */
                {
                    char *count_str = get_user_input("Number of entries to show (Enter for 20): ", 16);
                    int count = 20;
                    if (count_str != NULL && strlen(count_str) > 0) {
                        count = atoi(count_str);
                        if (count <= 0) count = 20;
                    }
                    if (count_str) free(count_str);
                    
                    show_reflog(count);
                }
                wait_for_enter();
                break;
                
            case 11: /* Recover from Reflog */
                show_reflog(15);
                printf("\n");
                
                input = get_user_input("Enter reflog reference (e.g., HEAD@{2} or hash): ", 64);
                if (input != NULL && strlen(input) > 0) {
                    printf("\nOptions:\n");
                    printf("  1. Create a new branch at this point\n");
                    printf("  2. Reset current branch to this point\n\n");
                    
                    int recover_choice = get_menu_choice(1, 2);
                    
                    if (recover_choice == 1) {
                        input2 = get_user_input("Enter new branch name: ", MAX_BRANCH_NAME);
                        if (input2 != NULL && strlen(input2) > 0) {
                            recover_from_reflog(input, input2);
                        }
                        if (input2) { free(input2); input2 = NULL; }
                    } else if (recover_choice == 2) {
                        if (get_user_confirmation("Reset current branch to this point?")) {
                            recover_from_reflog(input, NULL);
                        }
                    }
                }
                if (input) { free(input); input = NULL; }
                wait_for_enter();
                break;
                
            default:
                PRINT_ERROR("Invalid choice");
                wait_for_enter();
        }
    }
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

/**
 * Print usage information
 */
void print_usage(const char *program_name) {
    printf("\n" COLOR_BOLD "Git Master - Git Branch Management System" COLOR_RESET "\n\n");
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help      Show this help message\n");
    printf("  -v, --verbose   Enable verbose output\n");
    printf("  --version       Show version information\n");
    printf("  --daemon        Run in background daemon mode (polls for remote changes)\n");
    printf("  --daemon-fg     Run daemon in foreground (for testing)\n");
    printf("\n");
    printf("Daemon Mode:\n");
    printf("  The daemon monitors your git repositories and sends desktop notifications\n");
    printf("  when remote changes are detected. Configure in ~/.config/git_master/.git_master.conf\n");
    printf("\n");
    printf("Git Master is an interactive program for managing Git branches,\n");
    printf("commits, and merges with fault tolerance and conflict prevention.\n\n");
}

/**
 * Run the daemon mode
 */
int run_daemon_mode(bool foreground) {
    printf(COLOR_BOLD COLOR_CYAN "\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║             GIT MASTER - Daemon Mode                     ║\n");
    printf("╚══════════════════════════════════════════════════════════╝" COLOR_RESET "\n\n");
    
    /* Load configuration */
    config_t *config = config_load_or_create(NULL);
    if (config == NULL) {
        PRINT_ERROR("Failed to load or create configuration");
        return 1;
    }
    
    /* Show configuration */
    printf("Configuration:\n");
    printf("  Poll rate: %d ms\n", config->daemon.poll_rate_ms);
    printf("  Auto-detect repos: %s\n", config->daemon.auto_detect_repos ? "yes" : "no");
    printf("  Auto-fetch: %s\n", config->daemon.auto_fetch ? "yes" : "no");
    printf("  Notifications: %s\n", config->notifications.enabled ? "enabled" : "disabled");
    printf("\n");
    
    /* Initialize daemon */
    daemon_state_t *daemon = daemon_init(config);
    if (daemon == NULL) {
        PRINT_ERROR("Failed to initialize daemon");
        config_free(config);
        return 1;
    }
    
    /* Start daemon */
    if (daemon_start(daemon) != GM_SUCCESS) {
        PRINT_ERROR("Failed to start daemon");
        daemon_cleanup(daemon);
        config_free(config);
        return 1;
    }
    
    if (foreground) {
        printf(COLOR_GREEN "Daemon running in foreground. Press Ctrl+C to stop.\n" COLOR_RESET);
        printf("\nMonitoring for remote changes...\n");
        printf("(Notifications will appear when changes are detected)\n\n");
        
        /* Wait for interrupt signal */
        while (g_running && daemon_is_running(daemon)) {
            sleep(1);
        }
        
        printf("\nShutting down daemon...\n");
    } else {
        /* Fork to background */
        pid_t pid = fork();
        
        if (pid < 0) {
            PRINT_ERROR("Failed to fork daemon process");
            daemon_cleanup(daemon);
            config_free(config);
            return 1;
        }
        
        if (pid > 0) {
            /* Parent process */
            printf(COLOR_GREEN "Daemon started in background (PID: %d)\n" COLOR_RESET, pid);
            printf("Use 'kill %d' or 'pkill git_master' to stop\n", pid);
            return 0;
        }
        
        /* Child process - become session leader */
        setsid();
        
        /* Close standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        /* Redirect to /dev/null */
        int null_fd = open("/dev/null", O_RDWR);
        if (null_fd >= 0) {
            dup2(null_fd, STDIN_FILENO);
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            if (null_fd > 2) close(null_fd);
        }
        
        /* Run forever */
        while (g_running && daemon_is_running(daemon)) {
            sleep(1);
        }
    }
    
    daemon_cleanup(daemon);
    config_free(config);
    
    return 0;
}

/**
 * Main entry point
 */
int main(int argc, char *argv[]) {
    bool verbose = false;
    bool daemon_mode = false;
    bool daemon_foreground = false;
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            printf("Git Master v1.0.0\n");
            return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        }
        if (strcmp(argv[i], "--daemon") == 0) {
            daemon_mode = true;
            daemon_foreground = false;
        }
        if (strcmp(argv[i], "--daemon-fg") == 0) {
            daemon_mode = true;
            daemon_foreground = true;
        }
    }
    
    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Run daemon mode if requested */
    if (daemon_mode) {
        return run_daemon_mode(daemon_foreground);
    }
    
    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize application state */
    g_app_state = init_app_state(verbose, false);
    if (g_app_state == NULL) {
        PRINT_ERROR("Failed to initialize application");
        return 1;
    }
    
    /* Check if we're in a Git repository */
    bool is_repo = false;
    check_git_repository(NULL, &is_repo);
    
    if (!is_repo) {
        clear_screen();
        display_header();
        printf("\n" COLOR_YELLOW "Current directory is not a Git repository.\n" COLOR_RESET);
        
        if (get_user_confirmation("Initialize a new Git repository here?")) {
            if (init_repository(NULL) != GM_SUCCESS) {
                PRINT_ERROR("Failed to initialize repository");
                cleanup_app_state(g_app_state);
                return 1;
            }
            PRINT_SUCCESS("Initialized new Git repository");
        } else {
            PRINT_INFO("Please run this program from within a Git repository.");
            cleanup_app_state(g_app_state);
            return 0;
        }
    }
    
    /* Main menu loop */
    while (g_running) {
        clear_screen();
        display_header();
        
        /* Get and display repository status */
        repo_status_t *status = get_repo_status();
        if (status != NULL) {
            display_repo_status(status);
            free_repo_status(status);
        }
        
        display_main_menu();
        int choice = get_menu_choice(0, 7);
        
        switch (choice) {
            case 0:
                g_running = 0;
                break;
                
            case 1:
                handle_branch_menu();
                break;
                
            case 2:
                handle_commit_menu();
                break;
                
            case 3:
                handle_merge_menu();
                break;
                
            case 4:
                handle_remote_menu();
                break;
                
            case 5:
                handle_history_menu();
                break;
                
            case 6:
                printf("\n");
                show_status();
                wait_for_enter();
                break;
                
            case 7:
                printf("\n");
                show_log(20);
                wait_for_enter();
                break;
                
            default:
                PRINT_ERROR("Invalid choice. Please try again.");
                wait_for_enter();
        }
    }
    
    /* Cleanup */
    clear_screen();
    printf(COLOR_GREEN "\nThank you for using Git Master!\n" COLOR_RESET);
    
    cleanup_app_state(g_app_state);
    
    return 0;
}
