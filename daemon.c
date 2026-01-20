/**
 * daemon.c - Background Daemon for Git Master
 * 
 * Handles background monitoring, repo detection, remote change detection,
 * and Linux desktop notifications using libnotify.
 */

#include "config.h"
#include <pthread.h>
#include <signal.h>
#include <sys/inotify.h>
#include <linux/limits.h>
#include <dlfcn.h>

/* ============================================================================
 * Notification System (libnotify wrapper)
 * ============================================================================ */

/* libnotify function pointers (dynamic loading) */
typedef void* (*notify_init_fn)(const char*);
typedef void (*notify_uninit_fn)(void);
typedef void* (*notify_notification_new_fn)(const char*, const char*, const char*);
typedef int (*notify_notification_show_fn)(void*, void**);
typedef void (*notify_notification_set_timeout_fn)(void*, int);
typedef void (*notify_notification_set_urgency_fn)(void*, int);

static void *libnotify_handle = NULL;
static notify_init_fn fn_notify_init = NULL;
static notify_uninit_fn fn_notify_uninit = NULL;
static notify_notification_new_fn fn_notify_notification_new = NULL;
static notify_notification_show_fn fn_notify_notification_show = NULL;
static notify_notification_set_timeout_fn fn_notify_notification_set_timeout = NULL;
static notify_notification_set_urgency_fn fn_notify_notification_set_urgency = NULL;

/* Notification urgency levels */
typedef enum {
    NOTIFY_URGENCY_LOW = 0,
    NOTIFY_URGENCY_NORMAL = 1,
    NOTIFY_URGENCY_CRITICAL = 2
} notify_urgency_t;

/**
 * Initialize notification system
 */
bool notify_system_init(void) {
    /* Try to load libnotify dynamically */
    libnotify_handle = dlopen("libnotify.so.4", RTLD_LAZY);
    if (libnotify_handle == NULL) {
        libnotify_handle = dlopen("libnotify.so", RTLD_LAZY);
    }
    
    if (libnotify_handle == NULL) {
        PRINT_WARNING("libnotify not found - notifications disabled");
        PRINT_INFO("Install libnotify: sudo apt install libnotify4 (Debian/Ubuntu)");
        PRINT_INFO("                   sudo pacman -S libnotify (Arch)");
        return false;
    }
    
    fn_notify_init = (notify_init_fn)dlsym(libnotify_handle, "notify_init");
    fn_notify_uninit = (notify_uninit_fn)dlsym(libnotify_handle, "notify_uninit");
    fn_notify_notification_new = (notify_notification_new_fn)dlsym(libnotify_handle, 
                                                                    "notify_notification_new");
    fn_notify_notification_show = (notify_notification_show_fn)dlsym(libnotify_handle, 
                                                                      "notify_notification_show");
    fn_notify_notification_set_timeout = (notify_notification_set_timeout_fn)dlsym(
        libnotify_handle, "notify_notification_set_timeout");
    fn_notify_notification_set_urgency = (notify_notification_set_urgency_fn)dlsym(
        libnotify_handle, "notify_notification_set_urgency");
    
    if (fn_notify_init == NULL || fn_notify_notification_new == NULL ||
        fn_notify_notification_show == NULL) {
        PRINT_WARNING("libnotify functions not found - notifications disabled");
        dlclose(libnotify_handle);
        libnotify_handle = NULL;
        return false;
    }
    
    fn_notify_init("Git Master");
    return true;
}

/**
 * Cleanup notification system
 */
void notify_system_cleanup(void) {
    if (libnotify_handle != NULL) {
        if (fn_notify_uninit != NULL) {
            fn_notify_uninit();
        }
        dlclose(libnotify_handle);
        libnotify_handle = NULL;
    }
}

/**
 * Send a desktop notification
 */
bool send_notification(const char *title, const char *message, 
                       notify_urgency_t urgency, int timeout_ms) {
    if (libnotify_handle == NULL || fn_notify_notification_new == NULL) {
        /* Fallback to terminal output */
        printf("\n" COLOR_BOLD "[NOTIFICATION] %s" COLOR_RESET "\n", title);
        printf("%s\n\n", message);
        return false;
    }
    
    void *notification = fn_notify_notification_new(title, message, "git");
    if (notification == NULL) {
        return false;
    }
    
    if (fn_notify_notification_set_timeout != NULL) {
        fn_notify_notification_set_timeout(notification, timeout_ms);
    }
    
    if (fn_notify_notification_set_urgency != NULL) {
        fn_notify_notification_set_urgency(notification, (int)urgency);
    }
    
    fn_notify_notification_show(notification, NULL);
    
    return true;
}

/* ============================================================================
 * Daemon State
 * ============================================================================ */

typedef struct {
    config_t *config;
    pthread_t monitor_thread;
    pthread_t watcher_thread;
    volatile bool running;
    volatile bool paused;
    char current_repo[MAX_PATH_LEN];
    int inotify_fd;
    pthread_mutex_t state_lock;
} daemon_state_t;

static daemon_state_t *g_daemon = NULL;

/* ============================================================================
 * Repository Detection
 * ============================================================================ */

/**
 * Find git root from a path
 */
static bool find_git_root(const char *path, char *root_out, size_t max_len) {
    if (path == NULL || root_out == NULL) return false;
    
    char check_path[MAX_PATH_LEN];
    strncpy(check_path, path, sizeof(check_path) - 1);
    
    while (strlen(check_path) > 1) {
        char git_dir[MAX_PATH_LEN];
        snprintf(git_dir, sizeof(git_dir), "%s/.git", check_path);
        
        struct stat st;
        if (stat(git_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
            strncpy(root_out, check_path, max_len - 1);
            root_out[max_len - 1] = '\0';
            return true;
        }
        
        /* Go up one directory */
        char *last_slash = strrchr(check_path, '/');
        if (last_slash == NULL || last_slash == check_path) {
            break;
        }
        *last_slash = '\0';
    }
    
    return false;
}

/**
 * Get current working directory of the focused terminal/process
 * This uses /proc/self/cwd for the daemon's own CWD
 */
static bool get_active_directory(char *path_out, size_t max_len) {
    /* For now, just get our own CWD */
    if (getcwd(path_out, max_len) != NULL) {
        return true;
    }
    return false;
}

/**
 * Detect repository from current context
 */
static bool detect_current_repo(char *repo_path, size_t max_len) {
    char cwd[MAX_PATH_LEN];
    
    if (!get_active_directory(cwd, sizeof(cwd))) {
        return false;
    }
    
    return find_git_root(cwd, repo_path, max_len);
}

/* ============================================================================
 * Remote Change Detection
 * ============================================================================ */

/**
 * Check if remote has new commits
 */
static bool check_remote_changes(const char *repo_path, monitored_repo_t *repo) {
    if (repo_path == NULL || repo == NULL) return false;
    
    char cmd[MAX_COMMAND_LEN];
    char original_cwd[MAX_PATH_LEN];
    
    /* Save current directory */
    if (getcwd(original_cwd, sizeof(original_cwd)) == NULL) {
        return false;
    }
    
    /* Change to repo directory */
    if (chdir(repo_path) != 0) {
        return false;
    }
    
    bool has_changes = false;
    
    /* Fetch to update remote refs (silently) */
    snprintf(cmd, sizeof(cmd), "fetch --quiet %s 2>/dev/null", 
             repo->remote_name[0] ? repo->remote_name : "origin");
    cmd_result_t *result = exec_git_command(cmd);
    if (result != NULL) {
        free_cmd_result(result);
    }
    
    /* Check ahead/behind status */
    snprintf(cmd, sizeof(cmd), "rev-list --left-right --count HEAD...@{upstream} 2>/dev/null");
    result = exec_git_command(cmd);
    
    if (result != NULL && result->exit_code == 0 && result->output != NULL) {
        int ahead = 0, behind = 0;
        if (sscanf(result->output, "%d\t%d", &ahead, &behind) == 2) {
            int old_behind = repo->commits_behind;
            repo->commits_ahead = ahead;
            repo->commits_behind = behind;
            
            /* New remote commits detected */
            if (behind > old_behind && old_behind >= 0) {
                has_changes = true;
            }
        }
    }
    
    if (result != NULL) {
        free_cmd_result(result);
    }
    
    repo->last_check = time(NULL);
    
    /* Restore original directory */
    if (chdir(original_cwd) != 0) {
        /* Ignore error, just log */
    }
    
    return has_changes;
}

/**
 * Check for local uncommitted changes
 */
static bool check_local_changes(const char *repo_path, bool *has_staged, bool *has_unstaged) {
    if (repo_path == NULL) return false;
    
    char original_cwd[MAX_PATH_LEN];
    
    if (getcwd(original_cwd, sizeof(original_cwd)) == NULL) {
        return false;
    }
    
    if (chdir(repo_path) != 0) {
        return false;
    }
    
    cmd_result_t *result = exec_git_command("status --porcelain");
    
    bool has_changes = false;
    if (has_staged) *has_staged = false;
    if (has_unstaged) *has_unstaged = false;
    
    if (result != NULL && result->exit_code == 0 && result->output != NULL) {
        if (strlen(result->output) > 0) {
            has_changes = true;
            
            /* Parse status output */
            char *line = result->output;
            while (*line) {
                if (line[0] != ' ' && line[0] != '?' && has_staged) {
                    *has_staged = true;
                }
                if (line[1] != ' ' && has_unstaged) {
                    *has_unstaged = true;
                }
                
                char *next = strchr(line, '\n');
                if (next == NULL) break;
                line = next + 1;
            }
        }
    }
    
    if (result != NULL) {
        free_cmd_result(result);
    }
    
    if (chdir(original_cwd) != 0) {
        /* Ignore */
    }
    
    return has_changes;
}

/* ============================================================================
 * Monitor Thread
 * ============================================================================ */

/**
 * Main monitoring loop
 */
static void* monitor_thread_func(void *arg) {
    daemon_state_t *daemon = (daemon_state_t*)arg;
    
    if (daemon == NULL || daemon->config == NULL) {
        return NULL;
    }
    
    PRINT_INFO("Monitor thread started (poll rate: %d ms)", 
               daemon->config->daemon.poll_rate_ms);
    
    time_t last_config_check = 0;
    char last_detected_repo[MAX_PATH_LEN] = "";
    
    while (daemon->running) {
        if (daemon->paused) {
            usleep(100000); /* 100ms when paused */
            continue;
        }
        
        /* Check for config changes every 5 seconds */
        time_t now = time(NULL);
        if (now - last_config_check >= 5) {
            config_reload_if_changed(daemon->config);
            last_config_check = now;
        }
        
        /* Auto-detect current repository */
        if (daemon->config->daemon.auto_detect_repos) {
            char repo_path[MAX_PATH_LEN];
            
            if (detect_current_repo(repo_path, sizeof(repo_path))) {
                if (strcmp(repo_path, last_detected_repo) != 0) {
                    /* New repo detected */
                    strncpy(last_detected_repo, repo_path, sizeof(last_detected_repo) - 1);
                    strncpy(daemon->current_repo, repo_path, sizeof(daemon->current_repo) - 1);
                    
                    /* Add to monitored repos if not already there */
                    if (config_find_repo(daemon->config, repo_path) == NULL) {
                        config_add_repo(daemon->config, repo_path, "", "origin");
                        
                        /* Mark as auto-detected */
                        monitored_repo_t *repo = config_find_repo(daemon->config, repo_path);
                        if (repo != NULL) {
                            repo->auto_detect = true;
                        }
                        
                        /* Send notification */
                        if (daemon->config->notifications.enabled &&
                            daemon->config->notifications.show_on_repo_detect) {
                            char msg[256];
                            snprintf(msg, sizeof(msg), "Detected repository:\n%s", repo_path);
                            send_notification("Git Master", msg, NOTIFY_URGENCY_LOW,
                                            daemon->config->notifications.timeout_ms);
                        }
                    }
                }
            }
        }
        
        /* Check all monitored repos for remote changes */
        if (daemon->config->daemon.auto_fetch) {
            pthread_mutex_lock(&daemon->config->lock);
            
            for (int i = 0; i < daemon->config->repo_count; i++) {
                monitored_repo_t *repo = &daemon->config->repos[i];
                
                if (!repo->active) continue;
                
                /* Check if it's time to poll this repo */
                time_t time_since_check = now - repo->last_check;
                int poll_interval = daemon->config->daemon.poll_rate_ms / 1000;
                if (poll_interval < 1) poll_interval = 1;
                
                if (time_since_check >= poll_interval) {
                    pthread_mutex_unlock(&daemon->config->lock);
                    
                    bool has_remote_changes = check_remote_changes(repo->path, repo);
                    
                    if (has_remote_changes && 
                        daemon->config->notifications.enabled &&
                        daemon->config->notifications.show_on_remote_changes) {
                        
                        char msg[512];
                        snprintf(msg, sizeof(msg), 
                                 "Repository: %s\n%d new commit(s) available\nPull to update",
                                 repo->path, repo->commits_behind);
                        
                        send_notification("Git Master - Remote Changes", msg,
                                        NOTIFY_URGENCY_NORMAL,
                                        daemon->config->notifications.timeout_ms);
                    }
                    
                    pthread_mutex_lock(&daemon->config->lock);
                }
            }
            
            pthread_mutex_unlock(&daemon->config->lock);
        }
        
        /* Sleep for poll interval */
        usleep(daemon->config->daemon.poll_rate_ms * 1000);
    }
    
    PRINT_INFO("Monitor thread stopped");
    return NULL;
}

/* ============================================================================
 * Daemon Lifecycle
 * ============================================================================ */

/**
 * Initialize the daemon
 */
daemon_state_t* daemon_init(config_t *config) {
    if (config == NULL) {
        return NULL;
    }
    
    daemon_state_t *daemon = (daemon_state_t*)safe_calloc(1, sizeof(daemon_state_t));
    if (daemon == NULL) {
        return NULL;
    }
    
    daemon->config = config;
    daemon->running = false;
    daemon->paused = false;
    daemon->inotify_fd = -1;
    pthread_mutex_init(&daemon->state_lock, NULL);
    
    /* Initialize notification system */
    notify_system_init();
    
    g_daemon = daemon;
    
    return daemon;
}

/**
 * Start the daemon
 */
gm_error_t daemon_start(daemon_state_t *daemon) {
    if (daemon == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    if (daemon->running) {
        return GM_SUCCESS; /* Already running */
    }
    
    pthread_mutex_lock(&daemon->state_lock);
    daemon->running = true;
    pthread_mutex_unlock(&daemon->state_lock);
    
    /* Start monitor thread */
    if (pthread_create(&daemon->monitor_thread, NULL, monitor_thread_func, daemon) != 0) {
        PRINT_ERROR("Failed to start monitor thread");
        daemon->running = false;
        return GM_ERR_COMMAND_FAILED;
    }
    
    PRINT_SUCCESS("Daemon started");
    
    /* Send startup notification */
    if (daemon->config->notifications.enabled) {
        send_notification("Git Master", "Background monitoring started",
                         NOTIFY_URGENCY_LOW, 3000);
    }
    
    return GM_SUCCESS;
}

/**
 * Stop the daemon
 */
gm_error_t daemon_stop(daemon_state_t *daemon) {
    if (daemon == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    if (!daemon->running) {
        return GM_SUCCESS; /* Already stopped */
    }
    
    pthread_mutex_lock(&daemon->state_lock);
    daemon->running = false;
    pthread_mutex_unlock(&daemon->state_lock);
    
    /* Wait for threads to finish */
    pthread_join(daemon->monitor_thread, NULL);
    
    PRINT_SUCCESS("Daemon stopped");
    
    return GM_SUCCESS;
}

/**
 * Pause/resume the daemon
 */
void daemon_set_paused(daemon_state_t *daemon, bool paused) {
    if (daemon == NULL) return;
    
    pthread_mutex_lock(&daemon->state_lock);
    daemon->paused = paused;
    pthread_mutex_unlock(&daemon->state_lock);
    
    if (paused) {
        PRINT_INFO("Daemon paused");
    } else {
        PRINT_INFO("Daemon resumed");
    }
}

/**
 * Check if daemon is running
 */
bool daemon_is_running(daemon_state_t *daemon) {
    if (daemon == NULL) return false;
    return daemon->running;
}

/**
 * Cleanup the daemon
 */
void daemon_cleanup(daemon_state_t *daemon) {
    if (daemon == NULL) return;
    
    daemon_stop(daemon);
    
    notify_system_cleanup();
    
    if (daemon->inotify_fd >= 0) {
        close(daemon->inotify_fd);
    }
    
    pthread_mutex_destroy(&daemon->state_lock);
    
    free(daemon);
    
    if (g_daemon == daemon) {
        g_daemon = NULL;
    }
}

/**
 * Get the global daemon instance
 */
daemon_state_t* daemon_get_instance(void) {
    return g_daemon;
}

/**
 * Manually trigger a check for a specific repo
 */
gm_error_t daemon_check_repo(daemon_state_t *daemon, const char *repo_path) {
    if (daemon == NULL || repo_path == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    monitored_repo_t *repo = config_find_repo(daemon->config, repo_path);
    if (repo == NULL) {
        return GM_ERR_INVALID_INPUT;
    }
    
    PRINT_INFO("Checking repository: %s", repo_path);
    
    bool has_changes = check_remote_changes(repo_path, repo);
    
    if (has_changes) {
        PRINT_WARNING("Remote has %d new commit(s)", repo->commits_behind);
    } else {
        PRINT_INFO("Repository is up to date");
    }
    
    return GM_SUCCESS;
}

/**
 * Get current repo being monitored
 */
const char* daemon_get_current_repo(daemon_state_t *daemon) {
    if (daemon == NULL) return NULL;
    return daemon->current_repo;
}

/* ============================================================================
 * Notification Helpers for Actions
 * ============================================================================ */

/**
 * Send notification for action completion
 */
void notify_action_complete(config_t *config, const char *action, 
                            const char *details, bool success) {
    if (config == NULL || !config->notifications.enabled) {
        return;
    }
    
    char title[128];
    snprintf(title, sizeof(title), "Git Master - %s", action);
    
    notify_urgency_t urgency = success ? NOTIFY_URGENCY_LOW : NOTIFY_URGENCY_CRITICAL;
    
    send_notification(title, details != NULL ? details : 
                     (success ? "Operation completed" : "Operation failed"),
                     urgency, config->notifications.timeout_ms);
}

/**
 * Send notification for conflicts
 */
void notify_conflicts(config_t *config, const char *operation, int file_count) {
    if (config == NULL || !config->notifications.enabled ||
        !config->notifications.show_on_conflicts) {
        return;
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Conflicts detected during %s\n%d file(s) need resolution",
             operation, file_count);
    
    send_notification("Git Master - Conflict!", msg, NOTIFY_URGENCY_CRITICAL,
                     config->notifications.timeout_ms * 2);
}

/**
 * Send notification for remote changes
 */
void notify_remote_changes(config_t *config, const char *repo_name, int commits_behind) {
    if (config == NULL || !config->notifications.enabled ||
        !config->notifications.show_on_remote_changes) {
        return;
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Repository: %s\n%d new commit(s) available",
             repo_name, commits_behind);
    
    send_notification("Git Master - Updates Available", msg, NOTIFY_URGENCY_NORMAL,
                     config->notifications.timeout_ms);
}
