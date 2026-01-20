/**
 * diff_viewer.c - Side-by-Side Diff Viewer for Git Master
 *
 * Displays diffs in a colorful side-by-side format for easy comparison.
 */

#include "git_master.h"
#include "config.h"
#include <wchar.h>
#include <locale.h>
#include <sys/ioctl.h>
#include <termios.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DIFF_MAX_LINE_LEN   1024
#define DIFF_DEFAULT_WIDTH  120
#define DIFF_MIN_COLUMN     40
#define DIFF_GUTTER_WIDTH   4

/* Diff line types */
typedef enum {
    DIFF_LINE_CONTEXT = 0,
    DIFF_LINE_ADDED,
    DIFF_LINE_REMOVED,
    DIFF_LINE_MODIFIED,
    DIFF_LINE_HEADER,
    DIFF_LINE_HUNK,
    DIFF_LINE_BINARY,
    DIFF_LINE_EMPTY
} diff_line_type_t;

/* Single diff line */
typedef struct {
    diff_line_type_t type;
    int left_num;
    int right_num;
    char left_content[DIFF_MAX_LINE_LEN];
    char right_content[DIFF_MAX_LINE_LEN];
} diff_line_t;

/* Diff hunk */
typedef struct {
    int old_start;
    int old_count;
    int new_start;
    int new_count;
    char header[256];
    diff_line_t *lines;
    int line_count;
    int line_capacity;
} diff_hunk_t;

/* File diff */
typedef struct {
    char old_path[MAX_PATH_LEN];
    char new_path[MAX_PATH_LEN];
    bool is_new;
    bool is_deleted;
    bool is_binary;
    bool is_renamed;
    diff_hunk_t *hunks;
    int hunk_count;
    int hunk_capacity;
    int additions;
    int deletions;
} file_diff_t;

/* ============================================================================
 * Color Definitions
 * ============================================================================ */

#define DIFF_COLOR_RESET       "\033[0m"
#define DIFF_COLOR_HEADER      "\033[1;36m"     /* Bold Cyan */
#define DIFF_COLOR_HUNK        "\033[36m"       /* Cyan */
#define DIFF_COLOR_ADD_BG      "\033[42;30m"    /* Green bg, black text */
#define DIFF_COLOR_ADD_FG      "\033[32m"       /* Green */
#define DIFF_COLOR_DEL_BG      "\033[41;37m"    /* Red bg, white text */
#define DIFF_COLOR_DEL_FG      "\033[31m"       /* Red */
#define DIFF_COLOR_MOD_BG      "\033[43;30m"    /* Yellow bg, black text */
#define DIFF_COLOR_MOD_FG      "\033[33m"       /* Yellow */
#define DIFF_COLOR_LINE_NUM    "\033[90m"       /* Gray */
#define DIFF_COLOR_SEPARATOR   "\033[90m"       /* Gray */
#define DIFF_COLOR_CONTEXT     "\033[37m"       /* White */
#define DIFF_COLOR_EMPTY_BG    "\033[100m"      /* Dark gray bg */

/* ============================================================================
 * Terminal Utilities
 * ============================================================================ */

/**
 * Get terminal width
 */
static int get_terminal_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return DIFF_DEFAULT_WIDTH;
}

/**
 * Get visible string length (excluding ANSI escape codes)
 */
static size_t visible_strlen(const char *str) {
    if (str == NULL) return 0;
    
    size_t len = 0;
    bool in_escape = false;
    
    for (const char *p = str; *p; p++) {
        if (*p == '\033') {
            in_escape = true;
        } else if (in_escape && *p == 'm') {
            in_escape = false;
        } else if (!in_escape) {
            len++;
        }
    }
    
    return len;
}

/**
 * Truncate or pad string to exact width
 */
static void fit_to_width(const char *src, char *dest, size_t width, bool use_colors) {
    if (src == NULL || dest == NULL || width == 0) {
        dest[0] = '\0';
        return;
    }
    
    size_t src_len = strlen(src);
    size_t visible_len = visible_strlen(src);
    
    if (visible_len <= width) {
        /* Pad with spaces */
        strcpy(dest, src);
        for (size_t i = visible_len; i < width; i++) {
            strcat(dest, " ");
        }
    } else {
        /* Truncate - need to be careful with escape codes */
        size_t copied = 0;
        size_t visible = 0;
        bool in_escape = false;
        
        while (copied < src_len && visible < width - 3) {
            if (src[copied] == '\033') {
                in_escape = true;
            }
            
            dest[copied] = src[copied];
            copied++;
            
            if (in_escape && src[copied - 1] == 'm') {
                in_escape = false;
            } else if (!in_escape) {
                visible++;
            }
        }
        
        dest[copied] = '\0';
        strcat(dest, "...");
        
        /* Pad to width */
        visible = visible_strlen(dest);
        for (size_t i = visible; i < width; i++) {
            strcat(dest, " ");
        }
    }
}

/**
 * Remove tabs and control characters
 */
static void sanitize_line(char *line) {
    if (line == NULL) return;
    
    char *src = line;
    char *dst = line;
    
    while (*src) {
        if (*src == '\t') {
            /* Replace tab with spaces */
            int spaces = 4 - ((dst - line) % 4);
            for (int i = 0; i < spaces && (dst - line) < DIFF_MAX_LINE_LEN - 1; i++) {
                *dst++ = ' ';
            }
            src++;
        } else if (*src == '\r' || *src == '\n') {
            src++;
        } else if (*src >= 32 || *src < 0) {
            *dst++ = *src++;
        } else {
            src++;
        }
    }
    *dst = '\0';
}

/* ============================================================================
 * Diff Parsing
 * ============================================================================ */

/**
 * Parse a hunk header (@@ -start,count +start,count @@)
 */
static bool parse_hunk_header(const char *line, diff_hunk_t *hunk) {
    if (line == NULL || hunk == NULL) return false;
    if (strncmp(line, "@@", 2) != 0) return false;
    
    int old_start = 0, old_count = 1, new_start = 0, new_count = 1;
    
    /* Parse: @@ -old_start,old_count +new_start,new_count @@ */
    const char *p = line + 2;
    while (*p == ' ') p++;
    
    if (*p == '-') {
        p++;
        sscanf(p, "%d,%d", &old_start, &old_count);
    }
    
    p = strchr(p, '+');
    if (p != NULL) {
        p++;
        sscanf(p, "%d,%d", &new_start, &new_count);
    }
    
    hunk->old_start = old_start;
    hunk->old_count = old_count;
    hunk->new_start = new_start;
    hunk->new_count = new_count;
    
    strncpy(hunk->header, line, sizeof(hunk->header) - 1);
    
    return true;
}

/**
 * Add a line to a hunk
 */
static void hunk_add_line(diff_hunk_t *hunk, diff_line_t *line) {
    if (hunk == NULL || line == NULL) return;
    
    if (hunk->line_count >= hunk->line_capacity) {
        int new_capacity = hunk->line_capacity == 0 ? 32 : hunk->line_capacity * 2;
        diff_line_t *new_lines = realloc(hunk->lines, new_capacity * sizeof(diff_line_t));
        if (new_lines == NULL) return;
        hunk->lines = new_lines;
        hunk->line_capacity = new_capacity;
    }
    
    hunk->lines[hunk->line_count++] = *line;
}

/**
 * Free a file diff structure
 */
static void free_file_diff(file_diff_t *diff) {
    if (diff == NULL) return;
    
    for (int i = 0; i < diff->hunk_count; i++) {
        if (diff->hunks[i].lines != NULL) {
            free(diff->hunks[i].lines);
        }
    }
    
    if (diff->hunks != NULL) {
        free(diff->hunks);
    }
    
    free(diff);
}

/**
 * Parse unified diff output into structured format
 */
static file_diff_t* parse_unified_diff(const char *diff_text) {
    if (diff_text == NULL || strlen(diff_text) == 0) {
        return NULL;
    }
    
    file_diff_t *diff = calloc(1, sizeof(file_diff_t));
    if (diff == NULL) return NULL;
    
    diff->hunk_capacity = 8;
    diff->hunks = calloc(diff->hunk_capacity, sizeof(diff_hunk_t));
    if (diff->hunks == NULL) {
        free(diff);
        return NULL;
    }
    
    /* Make a working copy */
    char *text_copy = strdup(diff_text);
    if (text_copy == NULL) {
        free(diff->hunks);
        free(diff);
        return NULL;
    }
    
    diff_hunk_t *current_hunk = NULL;
    int left_num = 0, right_num = 0;
    
    char *line = strtok(text_copy, "\n");
    while (line != NULL) {
        diff_line_t dl = {0};
        
        if (strncmp(line, "diff --git", 10) == 0 ||
            strncmp(line, "index ", 6) == 0) {
            /* Header line - skip */
        } else if (strncmp(line, "---", 3) == 0) {
            /* Old file path */
            if (strlen(line) > 4) {
                const char *path = line + 4;
                if (path[0] == 'a' && path[1] == '/') path += 2;
                strncpy(diff->old_path, path, sizeof(diff->old_path) - 1);
            }
        } else if (strncmp(line, "+++", 3) == 0) {
            /* New file path */
            if (strlen(line) > 4) {
                const char *path = line + 4;
                if (path[0] == 'b' && path[1] == '/') path += 2;
                strncpy(diff->new_path, path, sizeof(diff->new_path) - 1);
            }
        } else if (strncmp(line, "@@", 2) == 0) {
            /* Hunk header */
            if (diff->hunk_count >= diff->hunk_capacity) {
                int new_cap = diff->hunk_capacity * 2;
                diff_hunk_t *new_hunks = realloc(diff->hunks, new_cap * sizeof(diff_hunk_t));
                if (new_hunks == NULL) break;
                diff->hunks = new_hunks;
                diff->hunk_capacity = new_cap;
            }
            
            current_hunk = &diff->hunks[diff->hunk_count];
            memset(current_hunk, 0, sizeof(diff_hunk_t));
            parse_hunk_header(line, current_hunk);
            diff->hunk_count++;
            
            left_num = current_hunk->old_start;
            right_num = current_hunk->new_start;
        } else if (current_hunk != NULL) {
            char prefix = line[0];
            const char *content = line + 1;
            
            sanitize_line((char*)content);
            
            if (prefix == '-') {
                dl.type = DIFF_LINE_REMOVED;
                dl.left_num = left_num++;
                dl.right_num = -1;
                strncpy(dl.left_content, content, sizeof(dl.left_content) - 1);
                diff->deletions++;
            } else if (prefix == '+') {
                dl.type = DIFF_LINE_ADDED;
                dl.left_num = -1;
                dl.right_num = right_num++;
                strncpy(dl.right_content, content, sizeof(dl.right_content) - 1);
                diff->additions++;
            } else if (prefix == ' ') {
                dl.type = DIFF_LINE_CONTEXT;
                dl.left_num = left_num++;
                dl.right_num = right_num++;
                strncpy(dl.left_content, content, sizeof(dl.left_content) - 1);
                strncpy(dl.right_content, content, sizeof(dl.right_content) - 1);
            } else if (prefix == '\\') {
                /* "\ No newline at end of file" */
                dl.type = DIFF_LINE_CONTEXT;
                strncpy(dl.left_content, content, sizeof(dl.left_content) - 1);
                strncpy(dl.right_content, content, sizeof(dl.right_content) - 1);
            }
            
            if (dl.type != 0 || prefix == ' ') {
                hunk_add_line(current_hunk, &dl);
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    free(text_copy);
    return diff;
}

/* ============================================================================
 * Side-by-Side Display
 * ============================================================================ */

/**
 * Print the diff header
 */
static void print_diff_header(file_diff_t *diff, int width) {
    printf("\n");
    
    /* Top border */
    printf(DIFF_COLOR_HEADER);
    for (int i = 0; i < width; i++) printf("═");
    printf(DIFF_COLOR_RESET "\n");
    
    /* File names */
    printf(DIFF_COLOR_HEADER "  File: " DIFF_COLOR_RESET);
    
    if (diff->is_renamed || strcmp(diff->old_path, diff->new_path) != 0) {
        printf(DIFF_COLOR_DEL_FG "%s" DIFF_COLOR_RESET " → ", diff->old_path);
        printf(DIFF_COLOR_ADD_FG "%s" DIFF_COLOR_RESET, diff->new_path);
    } else {
        printf("%s", diff->new_path);
    }
    
    printf("  ");
    printf(DIFF_COLOR_ADD_FG "+%d" DIFF_COLOR_RESET " ", diff->additions);
    printf(DIFF_COLOR_DEL_FG "-%d" DIFF_COLOR_RESET, diff->deletions);
    printf("\n");
    
    /* Bottom border */
    printf(DIFF_COLOR_HEADER);
    for (int i = 0; i < width; i++) printf("─");
    printf(DIFF_COLOR_RESET "\n");
}

/**
 * Print a single side-by-side line
 */
static void print_side_by_side_line(diff_line_t *line, int col_width, 
                                     bool show_line_nums, bool use_colors) {
    char left_buf[DIFF_MAX_LINE_LEN * 2] = "";
    char right_buf[DIFF_MAX_LINE_LEN * 2] = "";
    const char *left_color = "";
    const char *right_color = "";
    const char *reset = use_colors ? DIFF_COLOR_RESET : "";
    
    int gutter_width = show_line_nums ? DIFF_GUTTER_WIDTH : 0;
    int content_width = col_width - gutter_width - 1;
    
    switch (line->type) {
        case DIFF_LINE_REMOVED:
            left_color = use_colors ? DIFF_COLOR_DEL_FG : "";
            fit_to_width(line->left_content, left_buf, content_width, use_colors);
            break;
            
        case DIFF_LINE_ADDED:
            right_color = use_colors ? DIFF_COLOR_ADD_FG : "";
            fit_to_width(line->right_content, right_buf, content_width, use_colors);
            break;
            
        case DIFF_LINE_CONTEXT:
            fit_to_width(line->left_content, left_buf, content_width, use_colors);
            fit_to_width(line->right_content, right_buf, content_width, use_colors);
            break;
            
        default:
            break;
    }
    
    /* Print left side */
    if (show_line_nums) {
        if (line->left_num > 0) {
            printf(DIFF_COLOR_LINE_NUM "%4d" DIFF_COLOR_RESET, line->left_num);
        } else {
            printf("    ");
        }
    }
    
    if (line->type == DIFF_LINE_REMOVED) {
        printf("%s-%s%s", left_color, left_buf, reset);
    } else if (line->type == DIFF_LINE_ADDED) {
        /* Empty left side for additions */
        if (use_colors) printf(DIFF_COLOR_EMPTY_BG);
        for (int i = 0; i < content_width + 1; i++) printf(" ");
        printf("%s", reset);
    } else {
        printf(" %s%s", left_buf, reset);
    }
    
    /* Separator */
    printf(DIFF_COLOR_SEPARATOR " │ " DIFF_COLOR_RESET);
    
    /* Print right side */
    if (show_line_nums) {
        if (line->right_num > 0) {
            printf(DIFF_COLOR_LINE_NUM "%4d" DIFF_COLOR_RESET, line->right_num);
        } else {
            printf("    ");
        }
    }
    
    if (line->type == DIFF_LINE_ADDED) {
        printf("%s+%s%s", right_color, right_buf, reset);
    } else if (line->type == DIFF_LINE_REMOVED) {
        /* Empty right side for removals */
        if (use_colors) printf(DIFF_COLOR_EMPTY_BG);
        for (int i = 0; i < content_width + 1; i++) printf(" ");
        printf("%s", reset);
    } else {
        printf(" %s%s", right_buf, reset);
    }
    
    printf("\n");
}

/**
 * Print a hunk header
 */
static void print_hunk_header(diff_hunk_t *hunk, int width, bool use_colors) {
    if (use_colors) printf(DIFF_COLOR_HUNK);
    
    /* Print the @@ line centered */
    int header_len = strlen(hunk->header);
    int padding = (width - header_len) / 2;
    
    for (int i = 0; i < padding; i++) printf("─");
    printf(" %s ", hunk->header);
    for (int i = 0; i < width - padding - header_len - 2; i++) printf("─");
    
    if (use_colors) printf(DIFF_COLOR_RESET);
    printf("\n");
}

/**
 * Display diff in side-by-side format
 */
void show_side_by_side_diff(const char *diff_text, display_settings_t *settings) {
    if (diff_text == NULL || strlen(diff_text) == 0) {
        PRINT_INFO("No differences to display");
        return;
    }
    
    /* Default settings if not provided */
    display_settings_t default_settings = {
        .use_colors = true,
        .side_by_side_diff = true,
        .diff_context_lines = 3,
        .terminal_width = 120,
        .show_line_numbers = true,
        .syntax_highlighting = true
    };
    
    if (settings == NULL) {
        settings = &default_settings;
    }
    
    /* Get terminal width */
    int term_width = get_terminal_width();
    if (settings->terminal_width > 0 && settings->terminal_width < term_width) {
        term_width = settings->terminal_width;
    }
    
    /* Calculate column width (half of terminal minus separator) */
    int col_width = (term_width - 3) / 2;  /* 3 for " │ " separator */
    if (col_width < DIFF_MIN_COLUMN) {
        col_width = DIFF_MIN_COLUMN;
    }
    
    /* Parse the diff */
    file_diff_t *diff = parse_unified_diff(diff_text);
    if (diff == NULL) {
        /* Not a unified diff, just print as-is */
        printf("%s\n", diff_text);
        return;
    }
    
    /* Print header */
    print_diff_header(diff, term_width);
    
    /* Print each hunk */
    for (int h = 0; h < diff->hunk_count; h++) {
        diff_hunk_t *hunk = &diff->hunks[h];
        
        print_hunk_header(hunk, term_width, settings->use_colors);
        
        /* Pair up removed/added lines for side-by-side view */
        int i = 0;
        while (i < hunk->line_count) {
            diff_line_t *line = &hunk->lines[i];
            
            if (line->type == DIFF_LINE_REMOVED) {
                /* Look ahead for a matching addition */
                int j = i + 1;
                while (j < hunk->line_count && hunk->lines[j].type == DIFF_LINE_REMOVED) {
                    j++;
                }
                
                /* Print removed/added pairs */
                int removed_start = i;
                int removed_end = j;
                int added_start = j;
                int added_end = j;
                
                while (added_end < hunk->line_count && 
                       hunk->lines[added_end].type == DIFF_LINE_ADDED) {
                    added_end++;
                }
                
                int num_removed = removed_end - removed_start;
                int num_added = added_end - added_start;
                int pairs = (num_removed > num_added) ? num_removed : num_added;
                
                for (int p = 0; p < pairs; p++) {
                    diff_line_t combined = {0};
                    
                    if (p < num_removed) {
                        combined.type = DIFF_LINE_REMOVED;
                        combined.left_num = hunk->lines[removed_start + p].left_num;
                        strncpy(combined.left_content, 
                               hunk->lines[removed_start + p].left_content,
                               sizeof(combined.left_content) - 1);
                    }
                    
                    if (p < num_added) {
                        if (combined.type == DIFF_LINE_REMOVED) {
                            combined.type = DIFF_LINE_MODIFIED;
                        } else {
                            combined.type = DIFF_LINE_ADDED;
                        }
                        combined.right_num = hunk->lines[added_start + p].right_num;
                        strncpy(combined.right_content,
                               hunk->lines[added_start + p].right_content,
                               sizeof(combined.right_content) - 1);
                    }
                    
                    print_side_by_side_line(&combined, col_width, 
                                           settings->show_line_numbers,
                                           settings->use_colors);
                }
                
                i = added_end;
            } else {
                /* Context or other line */
                print_side_by_side_line(line, col_width,
                                       settings->show_line_numbers,
                                       settings->use_colors);
                i++;
            }
        }
    }
    
    /* Footer */
    printf(DIFF_COLOR_HEADER);
    for (int i = 0; i < term_width; i++) printf("═");
    printf(DIFF_COLOR_RESET "\n\n");
    
    free_file_diff(diff);
}

/**
 * Show diff for a file with side-by-side view
 */
gm_error_t show_file_diff_sbs(const char *file_path, bool staged, 
                               display_settings_t *settings) {
    char cmd[MAX_COMMAND_LEN];
    
    if (file_path != NULL && strlen(file_path) > 0) {
        if (staged) {
            snprintf(cmd, sizeof(cmd), "diff --cached -- \"%s\"", file_path);
        } else {
            snprintf(cmd, sizeof(cmd), "diff -- \"%s\"", file_path);
        }
    } else {
        if (staged) {
            snprintf(cmd, sizeof(cmd), "diff --cached");
        } else {
            snprintf(cmd, sizeof(cmd), "diff");
        }
    }
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0 && result->output == NULL) {
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output != NULL && strlen(result->output) > 0) {
        show_side_by_side_diff(result->output, settings);
    } else {
        PRINT_INFO("No differences");
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Show diff between two commits
 */
gm_error_t show_commit_diff_sbs(const char *commit1, const char *commit2,
                                 display_settings_t *settings) {
    char cmd[MAX_COMMAND_LEN];
    
    if (commit2 != NULL && strlen(commit2) > 0) {
        snprintf(cmd, sizeof(cmd), "diff \"%s\" \"%s\"", commit1, commit2);
    } else {
        snprintf(cmd, sizeof(cmd), "show --format='' \"%s\"", commit1);
    }
    
    cmd_result_t *result = exec_git_command(cmd);
    
    if (result == NULL) {
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->exit_code != 0) {
        if (result->error != NULL) {
            PRINT_ERROR("%s", result->error);
        }
        free_cmd_result(result);
        return GM_ERR_COMMAND_FAILED;
    }
    
    if (result->output != NULL && strlen(result->output) > 0) {
        show_side_by_side_diff(result->output, settings);
    } else {
        PRINT_INFO("No differences");
    }
    
    free_cmd_result(result);
    return GM_SUCCESS;
}

/**
 * Interactive diff viewer with scrolling
 * Returns: 0 = continue, 1 = accept changes, -1 = reject changes
 */
int interactive_diff_viewer(const char *diff_text, display_settings_t *settings) {
    if (diff_text == NULL) return 0;
    
    /* For now, just show the diff and return */
    /* Full interactive mode would require ncurses */
    show_side_by_side_diff(diff_text, settings);
    
    printf("\n");
    printf("[a] Accept changes  [r] Reject changes  [q] Continue\n");
    printf("Choice: ");
    
    char input[16];
    if (fgets(input, sizeof(input), stdin) != NULL) {
        char c = input[0];
        if (c == 'a' || c == 'A') return 1;
        if (c == 'r' || c == 'R') return -1;
    }
    
    return 0;
}

/**
 * Show unified diff with colors (simpler output)
 */
void show_colored_diff(const char *diff_text, bool use_colors) {
    if (diff_text == NULL) return;
    
    char *copy = strdup(diff_text);
    if (copy == NULL) {
        printf("%s", diff_text);
        return;
    }
    
    char *line = strtok(copy, "\n");
    while (line != NULL) {
        if (use_colors) {
            if (strncmp(line, "+++", 3) == 0 || strncmp(line, "---", 3) == 0) {
                printf(DIFF_COLOR_HEADER "%s" DIFF_COLOR_RESET "\n", line);
            } else if (line[0] == '+') {
                printf(DIFF_COLOR_ADD_FG "%s" DIFF_COLOR_RESET "\n", line);
            } else if (line[0] == '-') {
                printf(DIFF_COLOR_DEL_FG "%s" DIFF_COLOR_RESET "\n", line);
            } else if (strncmp(line, "@@", 2) == 0) {
                printf(DIFF_COLOR_HUNK "%s" DIFF_COLOR_RESET "\n", line);
            } else if (strncmp(line, "diff ", 5) == 0) {
                printf(DIFF_COLOR_HEADER "%s" DIFF_COLOR_RESET "\n", line);
            } else {
                printf("%s\n", line);
            }
        } else {
            printf("%s\n", line);
        }
        
        line = strtok(NULL, "\n");
    }
    
    free(copy);
}
