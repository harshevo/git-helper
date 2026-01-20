/**
 * config.h - Configuration System for Git Master
 * 
 * Handles configuration file parsing, shortcuts, and settings.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "git_master.h"
#include <pthread.h>

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

#define CONFIG_FILE_NAME        ".git_master.conf"
#define CONFIG_MAX_SHORTCUTS    64
#define CONFIG_MAX_REPOS        32
#define CONFIG_MAX_LINE_LEN     1024
#define CONFIG_KEY_MAX_LEN      64
#define CONFIG_VALUE_MAX_LEN    512

#define DEFAULT_POLL_RATE_MS    2000
#define DEFAULT_NOTIFICATION_TIMEOUT 5000
#define MIN_POLL_RATE_MS        500
#define MAX_POLL_RATE_MS        60000

/* ============================================================================
 * Shortcut Actions
 * ============================================================================ */

typedef enum {
    ACTION_NONE = 0,
    ACTION_STATUS,
    ACTION_STAGE_ALL,
    ACTION_COMMIT,
    ACTION_PUSH,
    ACTION_PULL,
    ACTION_FETCH,
    ACTION_BRANCH_LIST,
    ACTION_BRANCH_CREATE,
    ACTION_BRANCH_SWITCH,
    ACTION_BRANCH_DELETE,
    ACTION_MERGE,
    ACTION_STASH,
    ACTION_STASH_POP,
    ACTION_LOG,
    ACTION_DIFF,
    ACTION_DIFF_STAGED,
    ACTION_REVERT,
    ACTION_RESET_SOFT,
    ACTION_RESET_HARD,
    ACTION_CHERRY_PICK,
    ACTION_REFLOG,
    ACTION_OPEN_GUI,
    ACTION_QUIT,
    ACTION_MAX
} shortcut_action_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/* Keyboard shortcut definition */
typedef struct {
    char key[16];               /* Key combination (e.g., "ctrl+s", "alt+p") */
    shortcut_action_t action;   /* Action to perform */
    char description[128];      /* Human-readable description */
    bool enabled;
} shortcut_t;

/* Monitored repository */
typedef struct {
    char path[MAX_PATH_LEN];        /* Local path to repository */
    char remote_url[MAX_PATH_LEN];  /* Remote URL (for detection) */
    char remote_name[64];           /* Remote name (e.g., "origin") */
    char branch[MAX_BRANCH_NAME];   /* Branch to monitor */
    bool active;                    /* Currently being monitored */
    bool auto_detect;               /* Auto-detected vs manually added */
    time_t last_check;              /* Last time we checked for updates */
    time_t last_remote_update;      /* Last known remote update time */
    int commits_behind;             /* How far behind remote */
    int commits_ahead;              /* How far ahead of remote */
} monitored_repo_t;

/* Notification settings */
typedef struct {
    bool enabled;
    bool sound_enabled;
    int timeout_ms;
    bool show_on_remote_changes;
    bool show_on_conflicts;
    bool show_on_commit_complete;
    bool show_on_push_complete;
    bool show_on_pull_complete;
    bool show_on_repo_detect;
    char icon_path[MAX_PATH_LEN];
} notification_settings_t;

/* Display settings */
typedef struct {
    bool use_colors;
    bool side_by_side_diff;
    int diff_context_lines;
    int terminal_width;
    bool show_line_numbers;
    bool syntax_highlighting;
} display_settings_t;

/* GUI settings */
typedef struct {
    bool enabled;
    int window_width;
    int window_height;
    bool start_minimized;
    bool show_in_tray;
    int font_size;
    char theme[64];
} gui_settings_t;

/* Daemon settings */
typedef struct {
    bool enabled;
    int poll_rate_ms;
    bool auto_fetch;
    bool auto_detect_repos;
    bool run_on_startup;
    char pid_file[MAX_PATH_LEN];
    char log_file[MAX_PATH_LEN];
} daemon_settings_t;

/* Main configuration structure */
typedef struct {
    /* File info */
    char config_path[MAX_PATH_LEN];
    time_t config_mtime;            /* Last modification time for hot reload */
    
    /* Shortcuts */
    shortcut_t shortcuts[CONFIG_MAX_SHORTCUTS];
    int shortcut_count;
    
    /* Monitored repositories */
    monitored_repo_t repos[CONFIG_MAX_REPOS];
    int repo_count;
    
    /* Settings */
    notification_settings_t notifications;
    display_settings_t display;
    gui_settings_t gui;
    daemon_settings_t daemon;
    
    /* State */
    bool loaded;
    pthread_mutex_t lock;
} config_t;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

/* Configuration lifecycle */
config_t* config_create(void);
void config_destroy(config_t *config);
gm_error_t config_load(config_t *config, const char *path);
gm_error_t config_save(config_t *config);
gm_error_t config_reload_if_changed(config_t *config);
gm_error_t config_create_default(const char *path);

/* Shortcut management */
gm_error_t config_add_shortcut(config_t *config, const char *key, 
                                shortcut_action_t action, const char *desc);
gm_error_t config_remove_shortcut(config_t *config, const char *key);
shortcut_action_t config_get_action_for_key(config_t *config, const char *key);
const char* config_action_to_string(shortcut_action_t action);
shortcut_action_t config_string_to_action(const char *str);

/* Repository management */
gm_error_t config_add_repo(config_t *config, const char *path, 
                           const char *remote_url, const char *remote_name);
gm_error_t config_remove_repo(config_t *config, const char *path);
monitored_repo_t* config_find_repo(config_t *config, const char *path);
monitored_repo_t* config_find_repo_by_url(config_t *config, const char *url);

/* Settings access */
void config_set_poll_rate(config_t *config, int ms);
int config_get_poll_rate(config_t *config);
void config_set_notifications_enabled(config_t *config, bool enabled);
bool config_get_notifications_enabled(config_t *config);

/* Utility */
char* config_get_default_path(void);
void config_print(config_t *config);

#endif /* CONFIG_H */
