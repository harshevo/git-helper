/**
 * gui.c - Raygui-based GUI for Git Master
 * 
 * Optional graphical user interface using raylib and raygui.
 * Compile with: -DGUI_ENABLED -lraylib -lm -lpthread -ldl -lrt
 */

#ifdef GUI_ENABLED

#include "raylib.h"

/* raygui implementation */
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "config.h"
#include <pthread.h>

/* ============================================================================
 * GUI Constants
 * ============================================================================ */

#define GUI_WINDOW_TITLE    "Git Master"
#define GUI_FONT_SIZE       16
#define GUI_PADDING         10
#define GUI_BUTTON_HEIGHT   30
#define GUI_PANEL_WIDTH     250
#define GUI_STATUS_HEIGHT   150
#define GUI_MAX_LOG_LINES   100
#define GUI_SCROLL_SPEED    20

/* Colors */
#define COLOR_BG            (Color){ 30, 30, 35, 255 }
#define COLOR_PANEL         (Color){ 40, 42, 48, 255 }
#define COLOR_ACCENT        (Color){ 80, 160, 220, 255 }
#define COLOR_SUCCESS       (Color){ 80, 200, 120, 255 }
#define COLOR_WARNING       (Color){ 220, 180, 80, 255 }
#define COLOR_ERROR         (Color){ 220, 80, 80, 255 }
#define COLOR_TEXT          (Color){ 220, 220, 225, 255 }
#define COLOR_TEXT_DIM      (Color){ 150, 150, 160, 255 }

/* ============================================================================
 * GUI State
 * ============================================================================ */

typedef enum {
    GUI_VIEW_MAIN = 0,
    GUI_VIEW_BRANCHES,
    GUI_VIEW_COMMITS,
    GUI_VIEW_HISTORY,
    GUI_VIEW_REMOTES,
    GUI_VIEW_DIFF,
    GUI_VIEW_SETTINGS
} gui_view_t;

typedef struct {
    /* Window state */
    int width;
    int height;
    bool running;
    bool minimized;
    
    /* Current view */
    gui_view_t current_view;
    
    /* Repository state */
    char repo_path[MAX_PATH_LEN];
    char current_branch[MAX_BRANCH_NAME];
    bool is_repo;
    int staged_count;
    int modified_count;
    int untracked_count;
    int commits_ahead;
    int commits_behind;
    
    /* UI state */
    float scroll_offset;
    int selected_branch;
    int selected_commit;
    char input_text[512];
    bool input_active;
    char status_message[256];
    Color status_color;
    float status_timer;
    
    /* Branches list */
    char **branches;
    int branch_count;
    
    /* Commits list */
    char **commits;
    int commit_count;
    
    /* Diff content */
    char *diff_content;
    
    /* Log output */
    char log_lines[GUI_MAX_LOG_LINES][256];
    int log_line_count;
    float log_scroll;
    
    /* Configuration */
    config_t *config;
    
    /* Threading */
    pthread_mutex_t state_lock;
    volatile bool refresh_needed;
} gui_state_t;

static gui_state_t *g_gui = NULL;

/* ============================================================================
 * GUI Initialization
 * ============================================================================ */

/**
 * Initialize the GUI state
 */
gui_state_t* gui_init(config_t *config) {
    gui_state_t *gui = calloc(1, sizeof(gui_state_t));
    if (gui == NULL) return NULL;
    
    gui->config = config;
    gui->running = true;
    gui->current_view = GUI_VIEW_MAIN;
    gui->status_color = COLOR_TEXT;
    
    /* Use config settings */
    gui->width = config != NULL ? config->gui.window_width : 1200;
    gui->height = config != NULL ? config->gui.window_height : 800;
    
    pthread_mutex_init(&gui->state_lock, NULL);
    
    g_gui = gui;
    
    return gui;
}

/**
 * Cleanup GUI state
 */
void gui_cleanup(gui_state_t *gui) {
    if (gui == NULL) return;
    
    /* Free branch list */
    if (gui->branches != NULL) {
        for (int i = 0; i < gui->branch_count; i++) {
            free(gui->branches[i]);
        }
        free(gui->branches);
    }
    
    /* Free commits list */
    if (gui->commits != NULL) {
        for (int i = 0; i < gui->commit_count; i++) {
            free(gui->commits[i]);
        }
        free(gui->commits);
    }
    
    /* Free diff content */
    if (gui->diff_content != NULL) {
        free(gui->diff_content);
    }
    
    pthread_mutex_destroy(&gui->state_lock);
    
    free(gui);
    
    if (g_gui == gui) {
        g_gui = NULL;
    }
}

/* ============================================================================
 * Status Updates
 * ============================================================================ */

/**
 * Set status message
 */
static void gui_set_status(gui_state_t *gui, const char *message, Color color) {
    if (gui == NULL) return;
    
    strncpy(gui->status_message, message, sizeof(gui->status_message) - 1);
    gui->status_color = color;
    gui->status_timer = 5.0f; /* Show for 5 seconds */
}

/**
 * Add log line
 */
static void gui_log(gui_state_t *gui, const char *message) {
    if (gui == NULL) return;
    
    /* Shift lines up if full */
    if (gui->log_line_count >= GUI_MAX_LOG_LINES) {
        for (int i = 0; i < GUI_MAX_LOG_LINES - 1; i++) {
            strcpy(gui->log_lines[i], gui->log_lines[i + 1]);
        }
        gui->log_line_count = GUI_MAX_LOG_LINES - 1;
    }
    
    strncpy(gui->log_lines[gui->log_line_count], message, 255);
    gui->log_line_count++;
}

/* ============================================================================
 * Repository Refresh
 * ============================================================================ */

/**
 * Refresh repository status
 */
static void gui_refresh_repo(gui_state_t *gui) {
    if (gui == NULL) return;
    
    pthread_mutex_lock(&gui->state_lock);
    
    /* Check if git repo */
    bool is_repo = false;
    check_git_repository(NULL, &is_repo);
    gui->is_repo = is_repo;
    
    if (!is_repo) {
        strcpy(gui->current_branch, "Not a repository");
        pthread_mutex_unlock(&gui->state_lock);
        return;
    }
    
    /* Get current branch */
    get_current_branch(gui->current_branch, sizeof(gui->current_branch));
    
    /* Get repo path */
    getcwd(gui->repo_path, sizeof(gui->repo_path));
    
    /* Get status counts */
    repo_status_t *status = get_repo_status();
    if (status != NULL) {
        gui->staged_count = status->staged_files_count;
        gui->modified_count = status->modified_files_count;
        gui->untracked_count = status->untracked_files_count;
        free_repo_status(status);
    }
    
    gui->refresh_needed = false;
    
    pthread_mutex_unlock(&gui->state_lock);
}

/**
 * Refresh branch list
 */
static void gui_refresh_branches(gui_state_t *gui) {
    if (gui == NULL) return;
    
    pthread_mutex_lock(&gui->state_lock);
    
    /* Free old list */
    if (gui->branches != NULL) {
        for (int i = 0; i < gui->branch_count; i++) {
            free(gui->branches[i]);
        }
        free(gui->branches);
        gui->branches = NULL;
        gui->branch_count = 0;
    }
    
    /* Get new list */
    branch_info_t *branches = NULL;
    int count = 0;
    
    if (list_branches(&branches, &count, false) == GM_SUCCESS && count > 0) {
        gui->branches = calloc(count, sizeof(char*));
        if (gui->branches != NULL) {
            for (int i = 0; i < count; i++) {
                gui->branches[i] = strdup(branches[i].name);
                if (branches[i].is_current) {
                    gui->selected_branch = i;
                }
            }
            gui->branch_count = count;
        }
        free(branches);
    }
    
    pthread_mutex_unlock(&gui->state_lock);
}

/**
 * Refresh commit list
 */
static void gui_refresh_commits(gui_state_t *gui, int count) {
    if (gui == NULL) return;
    
    pthread_mutex_lock(&gui->state_lock);
    
    /* Free old list */
    if (gui->commits != NULL) {
        for (int i = 0; i < gui->commit_count; i++) {
            free(gui->commits[i]);
        }
        free(gui->commits);
        gui->commits = NULL;
        gui->commit_count = 0;
    }
    
    /* Get commits */
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "log --oneline -n %d", count);
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result != NULL && result->exit_code == 0 && result->output != NULL) {
        /* Count lines */
        int lines = 0;
        for (char *p = result->output; *p; p++) {
            if (*p == '\n') lines++;
        }
        if (lines > 0) {
            gui->commits = calloc(lines, sizeof(char*));
            if (gui->commits != NULL) {
                char *line = strtok(result->output, "\n");
                int i = 0;
                while (line != NULL && i < lines) {
                    gui->commits[i] = strdup(line);
                    i++;
                    line = strtok(NULL, "\n");
                }
                gui->commit_count = i;
            }
        }
    }
    
    if (result != NULL) {
        free_cmd_result(result);
    }
    
    pthread_mutex_unlock(&gui->state_lock);
}

/* ============================================================================
 * Drawing Functions
 * ============================================================================ */

/**
 * Draw the sidebar navigation
 */
static void draw_sidebar(gui_state_t *gui, Rectangle bounds) {
    /* Background */
    DrawRectangleRec(bounds, COLOR_PANEL);
    
    /* Title */
    DrawText("Git Master", bounds.x + GUI_PADDING, bounds.y + GUI_PADDING, 24, COLOR_ACCENT);
    
    float y = bounds.y + 60;
    float button_width = bounds.width - GUI_PADDING * 2;
    
    /* Navigation buttons */
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING, y, button_width, GUI_BUTTON_HEIGHT }, 
                  "# Main")) {
        gui->current_view = GUI_VIEW_MAIN;
    }
    y += GUI_BUTTON_HEIGHT + 5;
    
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING, y, button_width, GUI_BUTTON_HEIGHT },
                  "Branches")) {
        gui->current_view = GUI_VIEW_BRANCHES;
        gui_refresh_branches(gui);
    }
    y += GUI_BUTTON_HEIGHT + 5;
    
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING, y, button_width, GUI_BUTTON_HEIGHT },
                  "Commits")) {
        gui->current_view = GUI_VIEW_COMMITS;
    }
    y += GUI_BUTTON_HEIGHT + 5;
    
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING, y, button_width, GUI_BUTTON_HEIGHT },
                  "History")) {
        gui->current_view = GUI_VIEW_HISTORY;
        gui_refresh_commits(gui, 50);
    }
    y += GUI_BUTTON_HEIGHT + 5;
    
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING, y, button_width, GUI_BUTTON_HEIGHT },
                  "Remotes")) {
        gui->current_view = GUI_VIEW_REMOTES;
    }
    y += GUI_BUTTON_HEIGHT + 5;
    
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING, y, button_width, GUI_BUTTON_HEIGHT },
                  "Diff")) {
        gui->current_view = GUI_VIEW_DIFF;
    }
    y += GUI_BUTTON_HEIGHT + 5;
    
    /* Separator */
    y += 20;
    DrawLine(bounds.x + GUI_PADDING, y, bounds.x + bounds.width - GUI_PADDING, y, COLOR_TEXT_DIM);
    y += 20;
    
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING, y, button_width, GUI_BUTTON_HEIGHT },
                  "Settings")) {
        gui->current_view = GUI_VIEW_SETTINGS;
    }
    
    /* Status at bottom */
    y = bounds.y + bounds.height - 100;
    
    DrawText("Current Branch:", bounds.x + GUI_PADDING, y, 12, COLOR_TEXT_DIM);
    y += 16;
    DrawText(gui->current_branch, bounds.x + GUI_PADDING, y, 14, COLOR_SUCCESS);
    y += 20;
    
    char status_text[64];
    snprintf(status_text, sizeof(status_text), "S:%d M:%d U:%d", 
             gui->staged_count, gui->modified_count, gui->untracked_count);
    DrawText(status_text, bounds.x + GUI_PADDING, y, 12, COLOR_TEXT);
}

/**
 * Draw main view
 */
static void draw_main_view(gui_state_t *gui, Rectangle bounds) {
    float y = bounds.y + GUI_PADDING;
    
    /* Quick actions */
    DrawText("Quick Actions", bounds.x + GUI_PADDING, y, 20, COLOR_TEXT);
    y += 35;
    
    float button_width = 150;
    float x = bounds.x + GUI_PADDING;
    
    if (GuiButton((Rectangle){ x, y, button_width, GUI_BUTTON_HEIGHT }, "Stage All")) {
        stage_all_changes();
        gui_set_status(gui, "Staged all changes", COLOR_SUCCESS);
        gui->refresh_needed = true;
    }
    x += button_width + 10;
    
    if (GuiButton((Rectangle){ x, y, button_width, GUI_BUTTON_HEIGHT }, "Commit")) {
        gui->current_view = GUI_VIEW_COMMITS;
    }
    x += button_width + 10;
    
    if (GuiButton((Rectangle){ x, y, button_width, GUI_BUTTON_HEIGHT }, "Push")) {
        if (push_branch(NULL, NULL, false) == GM_SUCCESS) {
            gui_set_status(gui, "Pushed successfully", COLOR_SUCCESS);
        } else {
            gui_set_status(gui, "Push failed", COLOR_ERROR);
        }
    }
    x += button_width + 10;
    
    if (GuiButton((Rectangle){ x, y, button_width, GUI_BUTTON_HEIGHT }, "Pull")) {
        if (pull_branch(NULL, NULL) == GM_SUCCESS) {
            gui_set_status(gui, "Pulled successfully", COLOR_SUCCESS);
            gui->refresh_needed = true;
        } else {
            gui_set_status(gui, "Pull failed", COLOR_ERROR);
        }
    }
    
    y += GUI_BUTTON_HEIGHT + 30;
    
    /* Status panel */
    DrawText("Repository Status", bounds.x + GUI_PADDING, y, 20, COLOR_TEXT);
    y += 30;
    
    Rectangle status_panel = { bounds.x + GUI_PADDING, y, 
                               bounds.width - GUI_PADDING * 2, GUI_STATUS_HEIGHT };
    DrawRectangleRec(status_panel, COLOR_PANEL);
    
    float status_y = y + 10;
    float status_x = bounds.x + GUI_PADDING * 2;
    
    DrawText("Path:", status_x, status_y, 14, COLOR_TEXT_DIM);
    DrawText(gui->repo_path, status_x + 80, status_y, 14, COLOR_TEXT);
    status_y += 20;
    
    DrawText("Branch:", status_x, status_y, 14, COLOR_TEXT_DIM);
    DrawText(gui->current_branch, status_x + 80, status_y, 14, COLOR_SUCCESS);
    status_y += 20;
    
    char count_text[64];
    
    DrawText("Staged:", status_x, status_y, 14, COLOR_TEXT_DIM);
    snprintf(count_text, sizeof(count_text), "%d file(s)", gui->staged_count);
    DrawText(count_text, status_x + 80, status_y, 14, 
             gui->staged_count > 0 ? COLOR_SUCCESS : COLOR_TEXT);
    status_y += 20;
    
    DrawText("Modified:", status_x, status_y, 14, COLOR_TEXT_DIM);
    snprintf(count_text, sizeof(count_text), "%d file(s)", gui->modified_count);
    DrawText(count_text, status_x + 80, status_y, 14,
             gui->modified_count > 0 ? COLOR_WARNING : COLOR_TEXT);
    status_y += 20;
    
    DrawText("Untracked:", status_x, status_y, 14, COLOR_TEXT_DIM);
    snprintf(count_text, sizeof(count_text), "%d file(s)", gui->untracked_count);
    DrawText(count_text, status_x + 80, status_y, 14,
             gui->untracked_count > 0 ? COLOR_TEXT_DIM : COLOR_TEXT);
    
    y += GUI_STATUS_HEIGHT + 20;
    
    /* Recent commits */
    DrawText("Recent Commits", bounds.x + GUI_PADDING, y, 20, COLOR_TEXT);
    y += 30;
    
    for (int i = 0; i < gui->commit_count && i < 5; i++) {
        DrawText(gui->commits[i], bounds.x + GUI_PADDING, y, 14, COLOR_TEXT);
        y += 18;
    }
}

/**
 * Draw branches view
 */
static void draw_branches_view(gui_state_t *gui, Rectangle bounds) {
    float y = bounds.y + GUI_PADDING;
    
    DrawText("Branches", bounds.x + GUI_PADDING, y, 20, COLOR_TEXT);
    y += 35;
    
    /* Actions */
    float button_width = 120;
    
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING, y, button_width, GUI_BUTTON_HEIGHT },
                  "New Branch")) {
        /* TODO: Show input dialog */
    }
    
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING + button_width + 10, y, 
                               button_width, GUI_BUTTON_HEIGHT }, "Refresh")) {
        gui_refresh_branches(gui);
    }
    
    y += GUI_BUTTON_HEIGHT + 20;
    
    /* Branch list */
    Rectangle list_bounds = { bounds.x + GUI_PADDING, y,
                              bounds.width - GUI_PADDING * 2, 
                              bounds.height - y - GUI_PADDING };
    
    DrawRectangleRec(list_bounds, COLOR_PANEL);
    
    float list_y = y + 5;
    for (int i = 0; i < gui->branch_count; i++) {
        bool is_current = (i == gui->selected_branch);
        bool is_selected = (strcmp(gui->branches[i], gui->current_branch) == 0);
        
        Rectangle item = { list_bounds.x + 5, list_y, list_bounds.width - 10, 25 };
        
        if (is_selected) {
            DrawRectangleRec(item, COLOR_ACCENT);
        }
        
        DrawText(gui->branches[i], item.x + 10, item.y + 5, 14,
                is_selected ? COLOR_BG : COLOR_TEXT);
        
        /* Switch button */
        if (!is_selected) {
            Rectangle switch_btn = { item.x + item.width - 80, item.y + 2, 70, 21 };
            if (GuiButton(switch_btn, "Switch")) {
                switch_branch(gui->branches[i]);
                gui_set_status(gui, "Switched branch", COLOR_SUCCESS);
                gui->refresh_needed = true;
            }
        }
        
        list_y += 28;
        if (list_y > list_bounds.y + list_bounds.height - 30) break;
    }
}

/**
 * Draw commits view (for making new commits)
 */
static void draw_commits_view(gui_state_t *gui, Rectangle bounds) {
    float y = bounds.y + GUI_PADDING;
    
    DrawText("Commit Changes", bounds.x + GUI_PADDING, y, 20, COLOR_TEXT);
    y += 35;
    
    /* Commit message input */
    DrawText("Commit Message:", bounds.x + GUI_PADDING, y, 14, COLOR_TEXT_DIM);
    y += 20;
    
    Rectangle input_bounds = { bounds.x + GUI_PADDING, y,
                               bounds.width - GUI_PADDING * 2, 100 };
    
    GuiTextBox(input_bounds, gui->input_text, sizeof(gui->input_text) - 1, gui->input_active);
    
    /* Click to activate */
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && 
        CheckCollisionPointRec(GetMousePosition(), input_bounds)) {
        gui->input_active = true;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        gui->input_active = false;
    }
    
    y += 110;
    
    /* Buttons */
    float button_width = 150;
    
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING, y, button_width, GUI_BUTTON_HEIGHT },
                  "Commit")) {
        if (strlen(gui->input_text) > 0) {
            if (commit_changes(gui->input_text) == GM_SUCCESS) {
                gui_set_status(gui, "Committed successfully", COLOR_SUCCESS);
                gui->input_text[0] = '\0';
                gui->refresh_needed = true;
            } else {
                gui_set_status(gui, "Commit failed", COLOR_ERROR);
            }
        } else {
            gui_set_status(gui, "Enter a commit message", COLOR_WARNING);
        }
    }
    
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING + button_width + 10, y,
                               button_width, GUI_BUTTON_HEIGHT }, "Stage All & Commit")) {
        if (strlen(gui->input_text) > 0) {
            stage_all_changes();
            if (commit_changes(gui->input_text) == GM_SUCCESS) {
                gui_set_status(gui, "Staged and committed", COLOR_SUCCESS);
                gui->input_text[0] = '\0';
                gui->refresh_needed = true;
            } else {
                gui_set_status(gui, "Commit failed", COLOR_ERROR);
            }
        } else {
            gui_set_status(gui, "Enter a commit message", COLOR_WARNING);
        }
    }
}

/**
 * Draw history view
 */
static void draw_history_view(gui_state_t *gui, Rectangle bounds) {
    float y = bounds.y + GUI_PADDING;
    
    DrawText("Commit History", bounds.x + GUI_PADDING, y, 20, COLOR_TEXT);
    y += 35;
    
    if (GuiButton((Rectangle){ bounds.x + GUI_PADDING, y, 100, GUI_BUTTON_HEIGHT }, "Refresh")) {
        gui_refresh_commits(gui, 50);
    }
    
    y += GUI_BUTTON_HEIGHT + 10;
    
    /* Commit list */
    Rectangle list_bounds = { bounds.x + GUI_PADDING, y,
                              bounds.width - GUI_PADDING * 2,
                              bounds.height - y - GUI_PADDING };
    
    DrawRectangleRec(list_bounds, COLOR_PANEL);
    
    float list_y = y + 5 - gui->scroll_offset;
    
    for (int i = 0; i < gui->commit_count; i++) {
        if (list_y > list_bounds.y + list_bounds.height) break;
        if (list_y > list_bounds.y - 20) {
            DrawText(gui->commits[i], list_bounds.x + 10, list_y, 14, COLOR_TEXT);
        }
        list_y += 20;
    }
    
    /* Scrolling */
    if (CheckCollisionPointRec(GetMousePosition(), list_bounds)) {
        float wheel = GetMouseWheelMove();
        gui->scroll_offset -= wheel * GUI_SCROLL_SPEED;
        
        float max_scroll = (gui->commit_count * 20) - list_bounds.height + 20;
        if (max_scroll < 0) max_scroll = 0;
        
        if (gui->scroll_offset < 0) gui->scroll_offset = 0;
        if (gui->scroll_offset > max_scroll) gui->scroll_offset = max_scroll;
    }
}

/**
 * Draw the status bar
 */
static void draw_status_bar(gui_state_t *gui, Rectangle bounds) {
    DrawRectangleRec(bounds, COLOR_PANEL);
    
    if (gui->status_timer > 0) {
        DrawText(gui->status_message, bounds.x + GUI_PADDING, 
                bounds.y + (bounds.height - 14) / 2, 14, gui->status_color);
    }
}

/* ============================================================================
 * Main GUI Loop
 * ============================================================================ */

/**
 * Run the GUI
 */
void gui_run(gui_state_t *gui) {
    if (gui == NULL) return;
    
    /* Initialize raylib */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(gui->width, gui->height, GUI_WINDOW_TITLE);
    SetTargetFPS(60);
    
    /* Apply dark theme */
    GuiSetStyle(DEFAULT, TEXT_SIZE, GUI_FONT_SIZE);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(COLOR_BG));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(COLOR_TEXT));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToInt(COLOR_PANEL));
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(COLOR_ACCENT));
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToInt(COLOR_BG));
    
    /* Initial refresh */
    gui_refresh_repo(gui);
    gui_refresh_commits(gui, 10);
    gui_refresh_branches(gui);
    
    while (!WindowShouldClose() && gui->running) {
        /* Update */
        gui->width = GetScreenWidth();
        gui->height = GetScreenHeight();
        
        /* Update status timer */
        if (gui->status_timer > 0) {
            gui->status_timer -= GetFrameTime();
        }
        
        /* Refresh if needed */
        if (gui->refresh_needed) {
            gui_refresh_repo(gui);
        }
        
        /* Draw */
        BeginDrawing();
        
        ClearBackground(COLOR_BG);
        
        /* Layout */
        Rectangle sidebar = { 0, 0, GUI_PANEL_WIDTH, gui->height - 30 };
        Rectangle content = { GUI_PANEL_WIDTH, 0, 
                             gui->width - GUI_PANEL_WIDTH, gui->height - 30 };
        Rectangle status_bar = { 0, gui->height - 30, gui->width, 30 };
        
        /* Draw components */
        draw_sidebar(gui, sidebar);
        
        switch (gui->current_view) {
            case GUI_VIEW_MAIN:
                draw_main_view(gui, content);
                break;
            case GUI_VIEW_BRANCHES:
                draw_branches_view(gui, content);
                break;
            case GUI_VIEW_COMMITS:
                draw_commits_view(gui, content);
                break;
            case GUI_VIEW_HISTORY:
                draw_history_view(gui, content);
                break;
            default:
                DrawText("View not implemented", content.x + 20, content.y + 20, 20, COLOR_TEXT_DIM);
                break;
        }
        
        draw_status_bar(gui, status_bar);
        
        EndDrawing();
    }
    
    CloseWindow();
}

/**
 * Check if GUI is enabled
 */
bool gui_is_enabled(void) {
    return true;
}

#else /* GUI_ENABLED not defined */

/* Stub functions when GUI is disabled */

#include "git_master.h"

typedef struct gui_state_t {
    int dummy;
} gui_state_t;

gui_state_t* gui_init(void *config) {
    (void)config;
    PRINT_WARNING("GUI support not compiled. Use -DGUI_ENABLED and link with raylib.");
    return NULL;
}

void gui_cleanup(gui_state_t *gui) {
    (void)gui;
}

void gui_run(gui_state_t *gui) {
    (void)gui;
    PRINT_ERROR("GUI not available");
}

bool gui_is_enabled(void) {
    return false;
}

#endif /* GUI_ENABLED */
