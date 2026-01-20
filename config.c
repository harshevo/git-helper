/**
 * config.c - Configuration System Implementation for Git Master
 * 
 * Handles configuration file parsing, shortcuts, settings, and hot reload.
 */

#include "config.h"
#include <ctype.h>
#include <pwd.h>

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

static const char *DEFAULT_CONFIG = 
"# Git Master Configuration File\n"
"# This file is auto-reloaded when modified\n"
"\n"
"[daemon]\n"
"enabled = true\n"
"poll_rate_ms = 2000\n"
"auto_fetch = true\n"
"auto_detect_repos = true\n"
"run_on_startup = false\n"
"\n"
"[notifications]\n"
"enabled = true\n"
"sound_enabled = false\n"
"timeout_ms = 5000\n"
"show_on_remote_changes = true\n"
"show_on_conflicts = true\n"
"show_on_commit_complete = true\n"
"show_on_push_complete = true\n"
"show_on_pull_complete = true\n"
"show_on_repo_detect = true\n"
"\n"
"[display]\n"
"use_colors = true\n"
"side_by_side_diff = true\n"
"diff_context_lines = 3\n"
"terminal_width = 120\n"
"show_line_numbers = true\n"
"syntax_highlighting = true\n"
"\n"
"[gui]\n"
"enabled = false\n"
"window_width = 1200\n"
"window_height = 800\n"
"start_minimized = false\n"
"show_in_tray = true\n"
"font_size = 14\n"
"theme = dark\n"
"\n"
"[shortcuts]\n"
"# Format: key = action\n"
"# Available actions: status, stage_all, commit, push, pull, fetch,\n"
"#   branch_list, branch_create, branch_switch, branch_delete,\n"
"#   merge, stash, stash_pop, log, diff, diff_staged,\n"
"#   revert, reset_soft, reset_hard, cherry_pick, reflog, open_gui, quit\n"
"ctrl+s = status\n"
"ctrl+a = stage_all\n"
"ctrl+c = commit\n"
"ctrl+p = push\n"
"ctrl+u = pull\n"
"ctrl+f = fetch\n"
"ctrl+b = branch_list\n"
"ctrl+n = branch_create\n"
"ctrl+w = branch_switch\n"
"ctrl+m = merge\n"
"ctrl+z = stash\n"
"ctrl+x = stash_pop\n"
"ctrl+l = log\n"
"ctrl+d = diff\n"
"ctrl+g = open_gui\n"
"ctrl+q = quit\n"
"\n"
"[repos]\n"
"# Format: path = remote_url\n"
"# Add your repositories here for monitoring\n"
"# Example:\n"
"# /home/user/projects/myrepo = git@github.com:user/myrepo.git\n"
"\n";

/* ============================================================================
 * Action String Mapping
 * ============================================================================ */

typedef struct {
    shortcut_action_t action;
    const char *name;
} action_map_t;

static const action_map_t ACTION_MAP[] = {
    { ACTION_STATUS, "status" },
    { ACTION_STAGE_ALL, "stage_all" },
    { ACTION_COMMIT, "commit" },
    { ACTION_PUSH, "push" },
    { ACTION_PULL, "pull" },
    { ACTION_FETCH, "fetch" },
    { ACTION_BRANCH_LIST, "branch_list" },
    { ACTION_BRANCH_CREATE, "branch_create" },
    { ACTION_BRANCH_SWITCH, "branch_switch" },
    { ACTION_BRANCH_DELETE, "branch_delete" },
    { ACTION_MERGE, "merge" },
    { ACTION_STASH, "stash" },
    { ACTION_STASH_POP, "stash_pop" },
    { ACTION_LOG, "log" },
    { ACTION_DIFF, "diff" },
    { ACTION_DIFF_STAGED, "diff_staged" },
    { ACTION_REVERT, "revert" },
    { ACTION_RESET_SOFT, "reset_soft" },
    { ACTION_RESET_HARD, "reset_hard" },
    { ACTION_CHERRY_PICK, "cherry_pick" },
    { ACTION_REFLOG, "reflog" },
    { ACTION_OPEN_GUI, "open_gui" },
    { ACTION_QUIT, "quit" },
    { ACTION_NONE, NULL }
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Trim whitespace from both ends of a string
 */
static char* config_trim(char *str) {
    if (str == NULL) return NULL;
    
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    return str;
}

/**
 * Parse a boolean value from string
 */
static bool config_parse_bool(const char *value) {
    if (value == NULL) return false;
    
    return (strcasecmp(value, "true") == 0 ||
            strcasecmp(value, "yes") == 0 ||
            strcasecmp(value, "1") == 0 ||
            strcasecmp(value, "on") == 0);
}

/**
 * Parse an integer value from string
 */
static int config_parse_int(const char *value, int default_val) {
    if (value == NULL) return default_val;
    
    char *endptr;
    long val = strtol(value, &endptr, 10);
    
    if (endptr == value || *endptr != '\0') {
        return default_val;
    }
    
    return (int)val;
}

/* ============================================================================
 * Configuration Lifecycle
 * ============================================================================ */

/**
 * Create a new configuration object
 */
config_t* config_create(void) {
    config_t *config = (config_t*)safe_calloc(1, sizeof(config_t));
    
    if (config == NULL) {
        return NULL;
    }
    
    pthread_mutex_init(&config->lock, NULL);
    
    /* Set defaults */
    config->notifications.enabled = true;
    config->notifications.timeout_ms = DEFAULT_NOTIFICATION_TIMEOUT;
    config->notifications.show_on_remote_changes = true;
    config->notifications.show_on_conflicts = true;
    config->notifications.show_on_repo_detect = true;
    
    config->display.use_colors = true;
    config->display.side_by_side_diff = true;
    config->display.diff_context_lines = 3;
    config->display.terminal_width = 120;
    config->display.show_line_numbers = true;
    
    config->daemon.enabled = true;
    config->daemon.poll_rate_ms = DEFAULT_POLL_RATE_MS;
    config->daemon.auto_fetch = true;
    config->daemon.auto_detect_repos = true;
    
    config->gui.window_width = 1200;
    config->gui.window_height = 800;
    config->gui.font_size = 14;
    strncpy(config->gui.theme, "dark", sizeof(config->gui.theme) - 1);
    
    return config;
}

/**
 * Create a configuration with defaults (alias for config_create)
 */
config_t* config_create_with_defaults(void) {
    return config_create();
}

/**
 * Destroy a configuration object
 */
void config_destroy(config_t *config) {
    if (config == NULL) return;
    
    pthread_mutex_destroy(&config->lock);
    free(config);
}

/**
 * Alias for config_destroy
 */
void config_free(config_t *config) {
    config_destroy(config);
}

/**
 * Load configuration or create if missing
 */
config_t* config_load_or_create(const char *path) {
    config_t *config = config_create();
    if (config == NULL) {
        return NULL;
    }
    
    gm_error_t err = config_load(config, path);
    if (err != GM_SUCCESS) {
        config_destroy(config);
        return NULL;
    }
    
    return config;
}

/**
 * Get default configuration file path
 */
char* config_get_default_path(void) {
    static char path[MAX_PATH_LEN];
    
    /* Try XDG_CONFIG_HOME first */
    const char *xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config != NULL && strlen(xdg_config) > 0) {
        snprintf(path, sizeof(path), "%s/git_master/%s", xdg_config, CONFIG_FILE_NAME);
        return path;
    }
    
    /* Fall back to ~/.config */
    const char *home = getenv("HOME");
    if (home == NULL) {
        struct passwd *pw = getpwuid(getuid());
        if (pw != NULL) {
            home = pw->pw_dir;
        }
    }
    
    if (home != NULL) {
        snprintf(path, sizeof(path), "%s/.config/git_master/%s", home, CONFIG_FILE_NAME);
    } else {
        snprintf(path, sizeof(path), "./%s", CONFIG_FILE_NAME);
    }
    
    return path;
}

/**
 * Create default configuration file
 */
gm_error_t config_create_default(const char *path) {
    if (path == NULL) {
        path = config_get_default_path();
    }
    
    /* Create directory if needed */
    char dir_path[MAX_PATH_LEN];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        
        /* Create directory recursively */
        char cmd[MAX_COMMAND_LEN];
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dir_path);
        system(cmd);
    }
    
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        PRINT_ERROR("Cannot create config file: %s", path);
        return GM_ERR_IO_ERROR;
    }
    
    fprintf(fp, "%s", DEFAULT_CONFIG);
    fclose(fp);
    
    PRINT_SUCCESS("Created default configuration: %s", path);
    return GM_SUCCESS;
}

/**
 * Load configuration from file
 */
gm_error_t config_load(config_t *config, const char *path) {
    if (config == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    if (path == NULL) {
        path = config_get_default_path();
    }
    
    pthread_mutex_lock(&config->lock);
    
    strncpy(config->config_path, path, sizeof(config->config_path) - 1);
    
    /* Check if file exists, create default if not */
    struct stat st;
    if (stat(path, &st) != 0) {
        pthread_mutex_unlock(&config->lock);
        gm_error_t err = config_create_default(path);
        if (err != GM_SUCCESS) return err;
        pthread_mutex_lock(&config->lock);
        stat(path, &st);
    }
    
    config->config_mtime = st.st_mtime;
    
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        pthread_mutex_unlock(&config->lock);
        PRINT_ERROR("Cannot open config file: %s", path);
        return GM_ERR_IO_ERROR;
    }
    
    char line[CONFIG_MAX_LINE_LEN];
    char current_section[64] = "";
    
    config->shortcut_count = 0;
    config->repo_count = 0;
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *trimmed = config_trim(line);
        
        /* Skip empty lines and comments */
        if (strlen(trimmed) == 0 || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }
        
        /* Section header */
        if (trimmed[0] == '[') {
            char *end = strchr(trimmed, ']');
            if (end != NULL) {
                *end = '\0';
                strncpy(current_section, trimmed + 1, sizeof(current_section) - 1);
            }
            continue;
        }
        
        /* Key = Value */
        char *equals = strchr(trimmed, '=');
        if (equals == NULL) continue;
        
        *equals = '\0';
        char *key = config_trim(trimmed);
        char *value = config_trim(equals + 1);
        
        /* Parse based on section */
        if (strcmp(current_section, "daemon") == 0) {
            if (strcmp(key, "enabled") == 0) {
                config->daemon.enabled = config_parse_bool(value);
            } else if (strcmp(key, "poll_rate_ms") == 0) {
                config->daemon.poll_rate_ms = config_parse_int(value, DEFAULT_POLL_RATE_MS);
                if (config->daemon.poll_rate_ms < MIN_POLL_RATE_MS) {
                    config->daemon.poll_rate_ms = MIN_POLL_RATE_MS;
                }
                if (config->daemon.poll_rate_ms > MAX_POLL_RATE_MS) {
                    config->daemon.poll_rate_ms = MAX_POLL_RATE_MS;
                }
            } else if (strcmp(key, "auto_fetch") == 0) {
                config->daemon.auto_fetch = config_parse_bool(value);
            } else if (strcmp(key, "auto_detect_repos") == 0) {
                config->daemon.auto_detect_repos = config_parse_bool(value);
            } else if (strcmp(key, "run_on_startup") == 0) {
                config->daemon.run_on_startup = config_parse_bool(value);
            } else if (strcmp(key, "pid_file") == 0) {
                strncpy(config->daemon.pid_file, value, sizeof(config->daemon.pid_file) - 1);
            } else if (strcmp(key, "log_file") == 0) {
                strncpy(config->daemon.log_file, value, sizeof(config->daemon.log_file) - 1);
            }
        } else if (strcmp(current_section, "notifications") == 0) {
            if (strcmp(key, "enabled") == 0) {
                config->notifications.enabled = config_parse_bool(value);
            } else if (strcmp(key, "sound_enabled") == 0) {
                config->notifications.sound_enabled = config_parse_bool(value);
            } else if (strcmp(key, "timeout_ms") == 0) {
                config->notifications.timeout_ms = config_parse_int(value, DEFAULT_NOTIFICATION_TIMEOUT);
            } else if (strcmp(key, "show_on_remote_changes") == 0) {
                config->notifications.show_on_remote_changes = config_parse_bool(value);
            } else if (strcmp(key, "show_on_conflicts") == 0) {
                config->notifications.show_on_conflicts = config_parse_bool(value);
            } else if (strcmp(key, "show_on_commit_complete") == 0) {
                config->notifications.show_on_commit_complete = config_parse_bool(value);
            } else if (strcmp(key, "show_on_push_complete") == 0) {
                config->notifications.show_on_push_complete = config_parse_bool(value);
            } else if (strcmp(key, "show_on_pull_complete") == 0) {
                config->notifications.show_on_pull_complete = config_parse_bool(value);
            } else if (strcmp(key, "show_on_repo_detect") == 0) {
                config->notifications.show_on_repo_detect = config_parse_bool(value);
            } else if (strcmp(key, "icon_path") == 0) {
                strncpy(config->notifications.icon_path, value, 
                        sizeof(config->notifications.icon_path) - 1);
            }
        } else if (strcmp(current_section, "display") == 0) {
            if (strcmp(key, "use_colors") == 0) {
                config->display.use_colors = config_parse_bool(value);
            } else if (strcmp(key, "side_by_side_diff") == 0) {
                config->display.side_by_side_diff = config_parse_bool(value);
            } else if (strcmp(key, "diff_context_lines") == 0) {
                config->display.diff_context_lines = config_parse_int(value, 3);
            } else if (strcmp(key, "terminal_width") == 0) {
                config->display.terminal_width = config_parse_int(value, 120);
            } else if (strcmp(key, "show_line_numbers") == 0) {
                config->display.show_line_numbers = config_parse_bool(value);
            } else if (strcmp(key, "syntax_highlighting") == 0) {
                config->display.syntax_highlighting = config_parse_bool(value);
            }
        } else if (strcmp(current_section, "gui") == 0) {
            if (strcmp(key, "enabled") == 0) {
                config->gui.enabled = config_parse_bool(value);
            } else if (strcmp(key, "window_width") == 0) {
                config->gui.window_width = config_parse_int(value, 1200);
            } else if (strcmp(key, "window_height") == 0) {
                config->gui.window_height = config_parse_int(value, 800);
            } else if (strcmp(key, "start_minimized") == 0) {
                config->gui.start_minimized = config_parse_bool(value);
            } else if (strcmp(key, "show_in_tray") == 0) {
                config->gui.show_in_tray = config_parse_bool(value);
            } else if (strcmp(key, "font_size") == 0) {
                config->gui.font_size = config_parse_int(value, 14);
            } else if (strcmp(key, "theme") == 0) {
                strncpy(config->gui.theme, value, sizeof(config->gui.theme) - 1);
            }
        } else if (strcmp(current_section, "shortcuts") == 0) {
            if (config->shortcut_count < CONFIG_MAX_SHORTCUTS) {
                shortcut_action_t action = config_string_to_action(value);
                if (action != ACTION_NONE) {
                    strncpy(config->shortcuts[config->shortcut_count].key, key,
                            sizeof(config->shortcuts[0].key) - 1);
                    config->shortcuts[config->shortcut_count].action = action;
                    config->shortcuts[config->shortcut_count].enabled = true;
                    snprintf(config->shortcuts[config->shortcut_count].description,
                             sizeof(config->shortcuts[0].description),
                             "%s: %s", key, value);
                    config->shortcut_count++;
                }
            }
        } else if (strcmp(current_section, "repos") == 0) {
            if (config->repo_count < CONFIG_MAX_REPOS) {
                strncpy(config->repos[config->repo_count].path, key,
                        sizeof(config->repos[0].path) - 1);
                strncpy(config->repos[config->repo_count].remote_url, value,
                        sizeof(config->repos[0].remote_url) - 1);
                strncpy(config->repos[config->repo_count].remote_name, "origin",
                        sizeof(config->repos[0].remote_name) - 1);
                config->repos[config->repo_count].active = true;
                config->repos[config->repo_count].auto_detect = false;
                config->repo_count++;
            }
        }
    }
    
    fclose(fp);
    config->loaded = true;
    
    pthread_mutex_unlock(&config->lock);
    
    return GM_SUCCESS;
}

/**
 * Save configuration to file
 */
gm_error_t config_save(config_t *config) {
    if (config == NULL || strlen(config->config_path) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    pthread_mutex_lock(&config->lock);
    
    FILE *fp = fopen(config->config_path, "w");
    if (fp == NULL) {
        pthread_mutex_unlock(&config->lock);
        PRINT_ERROR("Cannot write config file: %s", config->config_path);
        return GM_ERR_IO_ERROR;
    }
    
    fprintf(fp, "# Git Master Configuration File\n");
    fprintf(fp, "# Auto-generated - manual edits are preserved on reload\n\n");
    
    /* Daemon section */
    fprintf(fp, "[daemon]\n");
    fprintf(fp, "enabled = %s\n", config->daemon.enabled ? "true" : "false");
    fprintf(fp, "poll_rate_ms = %d\n", config->daemon.poll_rate_ms);
    fprintf(fp, "auto_fetch = %s\n", config->daemon.auto_fetch ? "true" : "false");
    fprintf(fp, "auto_detect_repos = %s\n", config->daemon.auto_detect_repos ? "true" : "false");
    fprintf(fp, "run_on_startup = %s\n", config->daemon.run_on_startup ? "true" : "false");
    if (strlen(config->daemon.pid_file) > 0) {
        fprintf(fp, "pid_file = %s\n", config->daemon.pid_file);
    }
    if (strlen(config->daemon.log_file) > 0) {
        fprintf(fp, "log_file = %s\n", config->daemon.log_file);
    }
    fprintf(fp, "\n");
    
    /* Notifications section */
    fprintf(fp, "[notifications]\n");
    fprintf(fp, "enabled = %s\n", config->notifications.enabled ? "true" : "false");
    fprintf(fp, "sound_enabled = %s\n", config->notifications.sound_enabled ? "true" : "false");
    fprintf(fp, "timeout_ms = %d\n", config->notifications.timeout_ms);
    fprintf(fp, "show_on_remote_changes = %s\n", 
            config->notifications.show_on_remote_changes ? "true" : "false");
    fprintf(fp, "show_on_conflicts = %s\n", 
            config->notifications.show_on_conflicts ? "true" : "false");
    fprintf(fp, "show_on_commit_complete = %s\n", 
            config->notifications.show_on_commit_complete ? "true" : "false");
    fprintf(fp, "show_on_push_complete = %s\n", 
            config->notifications.show_on_push_complete ? "true" : "false");
    fprintf(fp, "show_on_pull_complete = %s\n", 
            config->notifications.show_on_pull_complete ? "true" : "false");
    fprintf(fp, "show_on_repo_detect = %s\n", 
            config->notifications.show_on_repo_detect ? "true" : "false");
    fprintf(fp, "\n");
    
    /* Display section */
    fprintf(fp, "[display]\n");
    fprintf(fp, "use_colors = %s\n", config->display.use_colors ? "true" : "false");
    fprintf(fp, "side_by_side_diff = %s\n", config->display.side_by_side_diff ? "true" : "false");
    fprintf(fp, "diff_context_lines = %d\n", config->display.diff_context_lines);
    fprintf(fp, "terminal_width = %d\n", config->display.terminal_width);
    fprintf(fp, "show_line_numbers = %s\n", config->display.show_line_numbers ? "true" : "false");
    fprintf(fp, "syntax_highlighting = %s\n", 
            config->display.syntax_highlighting ? "true" : "false");
    fprintf(fp, "\n");
    
    /* GUI section */
    fprintf(fp, "[gui]\n");
    fprintf(fp, "enabled = %s\n", config->gui.enabled ? "true" : "false");
    fprintf(fp, "window_width = %d\n", config->gui.window_width);
    fprintf(fp, "window_height = %d\n", config->gui.window_height);
    fprintf(fp, "start_minimized = %s\n", config->gui.start_minimized ? "true" : "false");
    fprintf(fp, "show_in_tray = %s\n", config->gui.show_in_tray ? "true" : "false");
    fprintf(fp, "font_size = %d\n", config->gui.font_size);
    fprintf(fp, "theme = %s\n", config->gui.theme);
    fprintf(fp, "\n");
    
    /* Shortcuts section */
    fprintf(fp, "[shortcuts]\n");
    for (int i = 0; i < config->shortcut_count; i++) {
        if (config->shortcuts[i].enabled) {
            fprintf(fp, "%s = %s\n", 
                    config->shortcuts[i].key,
                    config_action_to_string(config->shortcuts[i].action));
        }
    }
    fprintf(fp, "\n");
    
    /* Repos section */
    fprintf(fp, "[repos]\n");
    for (int i = 0; i < config->repo_count; i++) {
        if (!config->repos[i].auto_detect) {
            fprintf(fp, "%s = %s\n", 
                    config->repos[i].path,
                    config->repos[i].remote_url);
        }
    }
    fprintf(fp, "\n");
    
    fclose(fp);
    
    /* Update mtime */
    struct stat st;
    if (stat(config->config_path, &st) == 0) {
        config->config_mtime = st.st_mtime;
    }
    
    pthread_mutex_unlock(&config->lock);
    
    PRINT_SUCCESS("Configuration saved to %s", config->config_path);
    return GM_SUCCESS;
}

/**
 * Check if config file changed and reload if needed
 */
gm_error_t config_reload_if_changed(config_t *config) {
    if (config == NULL || strlen(config->config_path) == 0) {
        return GM_ERR_INVALID_INPUT;
    }
    
    struct stat st;
    if (stat(config->config_path, &st) != 0) {
        return GM_ERR_IO_ERROR;
    }
    
    if (st.st_mtime != config->config_mtime) {
        PRINT_INFO("Configuration file changed, reloading...");
        return config_load(config, config->config_path);
    }
    
    return GM_SUCCESS;
}

/* ============================================================================
 * Action String Mapping
 * ============================================================================ */

/**
 * Convert action enum to string
 */
const char* config_action_to_string(shortcut_action_t action) {
    for (int i = 0; ACTION_MAP[i].name != NULL; i++) {
        if (ACTION_MAP[i].action == action) {
            return ACTION_MAP[i].name;
        }
    }
    return "none";
}

/**
 * Convert string to action enum
 */
shortcut_action_t config_string_to_action(const char *str) {
    if (str == NULL) return ACTION_NONE;
    
    for (int i = 0; ACTION_MAP[i].name != NULL; i++) {
        if (strcasecmp(ACTION_MAP[i].name, str) == 0) {
            return ACTION_MAP[i].action;
        }
    }
    return ACTION_NONE;
}

/* ============================================================================
 * Shortcut Management
 * ============================================================================ */

/**
 * Add a shortcut
 */
gm_error_t config_add_shortcut(config_t *config, const char *key, 
                                shortcut_action_t action, const char *desc) {
    if (config == NULL || key == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    pthread_mutex_lock(&config->lock);
    
    /* Check if shortcut already exists */
    for (int i = 0; i < config->shortcut_count; i++) {
        if (strcasecmp(config->shortcuts[i].key, key) == 0) {
            config->shortcuts[i].action = action;
            if (desc != NULL) {
                strncpy(config->shortcuts[i].description, desc,
                        sizeof(config->shortcuts[0].description) - 1);
            }
            pthread_mutex_unlock(&config->lock);
            return GM_SUCCESS;
        }
    }
    
    /* Add new shortcut */
    if (config->shortcut_count >= CONFIG_MAX_SHORTCUTS) {
        pthread_mutex_unlock(&config->lock);
        return GM_ERR_MEMORY_ALLOC;
    }
    
    strncpy(config->shortcuts[config->shortcut_count].key, key,
            sizeof(config->shortcuts[0].key) - 1);
    config->shortcuts[config->shortcut_count].action = action;
    config->shortcuts[config->shortcut_count].enabled = true;
    
    if (desc != NULL) {
        strncpy(config->shortcuts[config->shortcut_count].description, desc,
                sizeof(config->shortcuts[0].description) - 1);
    }
    
    config->shortcut_count++;
    
    pthread_mutex_unlock(&config->lock);
    return GM_SUCCESS;
}

/**
 * Remove a shortcut
 */
gm_error_t config_remove_shortcut(config_t *config, const char *key) {
    if (config == NULL || key == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    pthread_mutex_lock(&config->lock);
    
    for (int i = 0; i < config->shortcut_count; i++) {
        if (strcasecmp(config->shortcuts[i].key, key) == 0) {
            /* Shift remaining shortcuts */
            for (int j = i; j < config->shortcut_count - 1; j++) {
                config->shortcuts[j] = config->shortcuts[j + 1];
            }
            config->shortcut_count--;
            pthread_mutex_unlock(&config->lock);
            return GM_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&config->lock);
    return GM_ERR_INVALID_INPUT;
}

/**
 * Get action for a key combination
 */
shortcut_action_t config_get_action_for_key(config_t *config, const char *key) {
    if (config == NULL || key == NULL) {
        return ACTION_NONE;
    }
    
    pthread_mutex_lock(&config->lock);
    
    for (int i = 0; i < config->shortcut_count; i++) {
        if (config->shortcuts[i].enabled && 
            strcasecmp(config->shortcuts[i].key, key) == 0) {
            shortcut_action_t action = config->shortcuts[i].action;
            pthread_mutex_unlock(&config->lock);
            return action;
        }
    }
    
    pthread_mutex_unlock(&config->lock);
    return ACTION_NONE;
}

/* ============================================================================
 * Repository Management
 * ============================================================================ */

/**
 * Add a repository to monitor
 */
gm_error_t config_add_repo(config_t *config, const char *path, 
                           const char *remote_url, const char *remote_name) {
    if (config == NULL || path == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    pthread_mutex_lock(&config->lock);
    
    /* Check if repo already exists */
    for (int i = 0; i < config->repo_count; i++) {
        if (strcmp(config->repos[i].path, path) == 0) {
            /* Update existing */
            if (remote_url != NULL) {
                strncpy(config->repos[i].remote_url, remote_url,
                        sizeof(config->repos[0].remote_url) - 1);
            }
            if (remote_name != NULL) {
                strncpy(config->repos[i].remote_name, remote_name,
                        sizeof(config->repos[0].remote_name) - 1);
            }
            pthread_mutex_unlock(&config->lock);
            return GM_SUCCESS;
        }
    }
    
    /* Add new repo */
    if (config->repo_count >= CONFIG_MAX_REPOS) {
        pthread_mutex_unlock(&config->lock);
        return GM_ERR_MEMORY_ALLOC;
    }
    
    strncpy(config->repos[config->repo_count].path, path,
            sizeof(config->repos[0].path) - 1);
    
    if (remote_url != NULL) {
        strncpy(config->repos[config->repo_count].remote_url, remote_url,
                sizeof(config->repos[0].remote_url) - 1);
    }
    
    strncpy(config->repos[config->repo_count].remote_name, 
            remote_name != NULL ? remote_name : "origin",
            sizeof(config->repos[0].remote_name) - 1);
    
    config->repos[config->repo_count].active = true;
    config->repo_count++;
    
    pthread_mutex_unlock(&config->lock);
    return GM_SUCCESS;
}

/**
 * Remove a repository from monitoring
 */
gm_error_t config_remove_repo(config_t *config, const char *path) {
    if (config == NULL || path == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    pthread_mutex_lock(&config->lock);
    
    for (int i = 0; i < config->repo_count; i++) {
        if (strcmp(config->repos[i].path, path) == 0) {
            /* Shift remaining repos */
            for (int j = i; j < config->repo_count - 1; j++) {
                config->repos[j] = config->repos[j + 1];
            }
            config->repo_count--;
            pthread_mutex_unlock(&config->lock);
            return GM_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&config->lock);
    return GM_ERR_INVALID_INPUT;
}

/**
 * Find a repository by path
 */
monitored_repo_t* config_find_repo(config_t *config, const char *path) {
    if (config == NULL || path == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < config->repo_count; i++) {
        if (strcmp(config->repos[i].path, path) == 0) {
            return &config->repos[i];
        }
    }
    
    return NULL;
}

/**
 * Find a repository by remote URL
 */
monitored_repo_t* config_find_repo_by_url(config_t *config, const char *url) {
    if (config == NULL || url == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < config->repo_count; i++) {
        if (strstr(config->repos[i].remote_url, url) != NULL ||
            strstr(url, config->repos[i].remote_url) != NULL) {
            return &config->repos[i];
        }
    }
    
    return NULL;
}

/* ============================================================================
 * Settings Access
 * ============================================================================ */

void config_set_poll_rate(config_t *config, int ms) {
    if (config == NULL) return;
    
    pthread_mutex_lock(&config->lock);
    
    if (ms < MIN_POLL_RATE_MS) ms = MIN_POLL_RATE_MS;
    if (ms > MAX_POLL_RATE_MS) ms = MAX_POLL_RATE_MS;
    
    config->daemon.poll_rate_ms = ms;
    
    pthread_mutex_unlock(&config->lock);
}

int config_get_poll_rate(config_t *config) {
    if (config == NULL) return DEFAULT_POLL_RATE_MS;
    return config->daemon.poll_rate_ms;
}

void config_set_notifications_enabled(config_t *config, bool enabled) {
    if (config == NULL) return;
    
    pthread_mutex_lock(&config->lock);
    config->notifications.enabled = enabled;
    pthread_mutex_unlock(&config->lock);
}

bool config_get_notifications_enabled(config_t *config) {
    if (config == NULL) return false;
    return config->notifications.enabled;
}

/* ============================================================================
 * Debug/Display
 * ============================================================================ */

/**
 * Print configuration to console
 */
void config_print(config_t *config) {
    if (config == NULL) return;
    
    printf("\n" COLOR_BOLD "=== Git Master Configuration ===" COLOR_RESET "\n\n");
    
    printf(COLOR_CYAN "[Daemon]" COLOR_RESET "\n");
    printf("  Enabled: %s\n", config->daemon.enabled ? "yes" : "no");
    printf("  Poll Rate: %d ms\n", config->daemon.poll_rate_ms);
    printf("  Auto Fetch: %s\n", config->daemon.auto_fetch ? "yes" : "no");
    printf("  Auto Detect Repos: %s\n", config->daemon.auto_detect_repos ? "yes" : "no");
    printf("\n");
    
    printf(COLOR_CYAN "[Notifications]" COLOR_RESET "\n");
    printf("  Enabled: %s\n", config->notifications.enabled ? "yes" : "no");
    printf("  Timeout: %d ms\n", config->notifications.timeout_ms);
    printf("\n");
    
    printf(COLOR_CYAN "[Display]" COLOR_RESET "\n");
    printf("  Colors: %s\n", config->display.use_colors ? "yes" : "no");
    printf("  Side-by-side Diff: %s\n", config->display.side_by_side_diff ? "yes" : "no");
    printf("  Terminal Width: %d\n", config->display.terminal_width);
    printf("\n");
    
    printf(COLOR_CYAN "[Shortcuts] (%d configured)" COLOR_RESET "\n", config->shortcut_count);
    for (int i = 0; i < config->shortcut_count && i < 10; i++) {
        printf("  %s = %s\n", config->shortcuts[i].key,
               config_action_to_string(config->shortcuts[i].action));
    }
    if (config->shortcut_count > 10) {
        printf("  ... and %d more\n", config->shortcut_count - 10);
    }
    printf("\n");
    
    printf(COLOR_CYAN "[Repos] (%d monitored)" COLOR_RESET "\n", config->repo_count);
    for (int i = 0; i < config->repo_count && i < 5; i++) {
        printf("  %s\n", config->repos[i].path);
    }
    if (config->repo_count > 5) {
        printf("  ... and %d more\n", config->repo_count - 5);
    }
    printf("\n");
}
