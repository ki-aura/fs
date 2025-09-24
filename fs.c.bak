#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <glob.h>
#include <ctype.h>
#include <stdarg.h>


#define MAX_LINE_LEN 4096

// ------------------ Options structure ------------------
typedef struct {
    bool ignore_case;		// -i
    bool show_line_numbers;	// -n
    bool show_filename;		// -f
    bool count_only;		// -c
    int before;   			// -bN
    int after;   			// -aN
    int line_limit; 		// -lN
    const char *pattern;
} Options;

// ------------------ Option handler type ------------------
typedef void (*OptionHandler)(Options *opts, const char *arg);

// Option table entry
typedef struct {
    const char *name;
    OptionHandler handler;
    const char *help;
} OptionDef;

// ------------------ Option Handlers ------------------
void handle_ignore_case(Options *opts, const char *arg) { opts->ignore_case = true; }
void handle_show_line_numbers(Options *opts, const char *arg) { opts->show_line_numbers = true; }
void handle_show_filename(Options *opts, const char *arg) { opts->show_filename = true; }
void handle_count_only(Options *opts, const char *arg) { opts->count_only = true; }

void handle_before(Options *opts, const char *arg) {
    int n = atoi(arg + 2);  // parse "-bN"
    if (n < 0) n = 0;
    opts->before = n;
}

void handle_after(Options *opts, const char *arg) {
    int n = atoi(arg + 2);  // parse "-aN"
    if (n < 0) n = 0;
    opts->after = n;
}

void handle_limit_line(Options *opts, const char *arg) {
    int n = atoi(arg + 2);  // parse "-lN"
    if (n < 0) n = 20;
    if (n >= MAX_LINE_LEN) n = MAX_LINE_LEN -1;
    opts->line_limit = n;
}

// ------------------ Option Table ------------------
OptionDef option_table[] = {
    {"-i", handle_ignore_case, "Ignore case when searching"},
    {"-n", handle_show_line_numbers, "Show line numbers"},
    {"-f", handle_show_filename, "Show file name"},
    {"-c", handle_count_only, "Print only a count of matching lines"},
    {"-b", handle_before, "Print N lines before a match (e.g. -b2)"},
    {"-a", handle_after, "Print N lines after a match (e.g. -a3)"},
    {"-l", handle_limit_line, "Print only the first n chars of each line (e.g. -l20)"},
    {NULL, NULL, NULL}
};

// ------------------ Case-insensitive substring search ------------------
bool line_contains(const char *line, const char *pattern, bool ignore_case) {
    if (!ignore_case) return strstr(line, pattern) != NULL;

    static char lower_line[MAX_LINE_LEN];
    static char lower_pat[1024];

    strncpy(lower_line, line, sizeof(lower_line));
    lower_line[sizeof(lower_line)-1] = '\0';
    strncpy(lower_pat, pattern, sizeof(lower_pat));
    lower_pat[sizeof(lower_pat)-1] = '\0';

    for (char *p = lower_line; *p; p++) *p = tolower((unsigned char)*p);
    for (char *p = lower_pat; *p; p++) *p = tolower((unsigned char)*p);

    return strstr(lower_line, lower_pat) != NULL;
}

// ------------------ Options Parsing ------------------
void parse_options(int argc, char *argv[], Options *opts, int *first_file_index) {
    *opts = (Options){0};  // zero-initialise all fields
    // modify any non-zero defaults
    opts->line_limit = MAX_LINE_LEN-1;

    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            bool recognized = false;
            for (OptionDef *opt = option_table; opt->name; opt++) {
                size_t len = strlen(opt->name);
                if (strncmp(argv[i], opt->name, len) == 0) {
                    opt->handler(opts, argv[i]);
                    recognized = true;
                    break;
                }
            }
            if (!recognized) {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
        } else {
            opts->pattern = argv[i];
            i++;
            break;
        }
    }

    if (!opts->pattern) {
        fprintf(stderr, "Error: no search pattern specified.\n");
        exit(EXIT_FAILURE);
    }

    *first_file_index = i;
    if (*first_file_index >= argc) {
        fprintf(stderr, "Error: no files specified.\n");
        exit(EXIT_FAILURE);
    }
}

// ------------------ Helper for appending to a string ---------
void append_to_buffer(char *buf, size_t bufsize, const char *fmt, ...) {
    if (bufsize == 0) return;  // nothing to do

    size_t len = strlen(buf);
    if (len >= bufsize - 1) return;  // no space left

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + len, bufsize - len, fmt, ap);
    va_end(ap);

    // n is chars written. it could be >= remaining space if truncated, but buffer is still null-terminated
}

// ------------------ Helper for stripping filename ---------
const char *get_basename(const char *path){
    if (path == NULL) return NULL;

    const char *last = strrchr(path, '/');
    if (last) return last + 1;  // character after last '/'
    return path;    	      	// no slash, whole string is filename
}

// ------------------ Helper for Line Printing ---------
void print_line(const char *filename, const char *line, int lineno,
                int max_chars, bool show_line_nums, bool show_fname)
{
    // Make a local copy to safely modify
    char buffer[MAX_LINE_LEN];  // should be at least as large as your fgets buffer
    strncpy(buffer, line, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';

    // Strip trailing newline if present
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';
        len--;
    }

    // Adjust max_chars if the line is shorter
    if ((size_t)max_chars > len) {
        max_chars = (int)len;
    }

    // Print with optional line numbers
    char prefix[256]; prefix[0] = '\0';
	if (show_fname) append_to_buffer(prefix, sizeof(prefix), "%s:", get_basename(filename));
	if (show_line_nums) append_to_buffer(prefix, sizeof(prefix), "%d:", lineno);
    printf("%s%.*s\n", prefix, max_chars, buffer);
}



// ------------------ File Processing ------------------
typedef struct {
    int lineno;
    char *line;
} BeforeLine;

void process_file(const char *filename, const Options *opts) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror(filename); return; }

    int before_size = opts->before;
    BeforeLine *before_buf = NULL;
    if (before_size > 0) {
        before_buf = calloc(before_size, sizeof(BeforeLine));
    }

    char line[MAX_LINE_LEN];
    int lineno = 1;
    int after_counter = 0;
    int buf_pos = 0;
    int buf_count = 0; // number of valid lines in circular buffer
    int match_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        bool match = line_contains(line, opts->pattern, opts->ignore_case);

        // --- handle match ---
        if (match) {
            match_count++;

            // print before lines in chronological order
            int start = (buf_pos + (before_size - buf_count)) % before_size;
            for (int i = 0; i < buf_count; i++) {
                int idx = (start + i) % before_size;
                if (!opts->count_only && before_buf[idx].line) {
                	print_line(filename, before_buf[idx].line, before_buf[idx].lineno, opts->line_limit, opts->show_line_numbers, opts->show_filename);
                }
            }

            // set after-counter for printing lines after this match
            after_counter = opts->after;
        }

        // --- print current line if match or after-counter active ---
        if (match || after_counter > 0) {
            if (!opts->count_only) {
	            print_line(filename, line, lineno, opts->line_limit, opts->show_line_numbers, opts->show_filename);
            }

            // decrement after-counter only for non-match lines
            if (!match && after_counter > 0)
                after_counter--;
        }

        // --- update circular buffer for "before" lines ---
        if (before_size > 0) {
            free(before_buf[buf_pos].line);
            before_buf[buf_pos].line = strdup(line);
            before_buf[buf_pos].lineno = lineno;
            buf_pos = (buf_pos + 1) % before_size;
            if (buf_count < before_size) buf_count++;
        }

        lineno++;
    }

    // --- print match count if requested ---
    if (opts->count_only) {
        printf("%s:%d\n", filename, match_count);
    }

    // --- cleanup ---
    if (before_buf) {
        for (int j = 0; j < before_size; j++)
            free(before_buf[j].line);
        free(before_buf);
    }

    fclose(fp);
}


// ------------------ Main ------------------
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s [options] pattern files...\n", argv[0]);
        fprintf(stderr, "Options:\n");
        for (OptionDef *opt = option_table; opt->name; opt++) {
            fprintf(stderr, "  %s\t%s\n", opt->name, opt->help);
        }
        return EXIT_FAILURE;
    }

    Options opts;
    int first_file_index;
    parse_options(argc, argv, &opts, &first_file_index);

    for (int i = first_file_index; i < argc; i++) {
        glob_t globbuf;
        if (glob(argv[i], 0, NULL, &globbuf) == 0) {
            for (size_t j = 0; j < globbuf.gl_pathc; j++) {
                process_file(globbuf.gl_pathv[j], &opts);
            }
            globfree(&globbuf);
        } else {
            process_file(argv[i], &opts);
        }
    }

    return EXIT_SUCCESS;
}
