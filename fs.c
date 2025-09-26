#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <glob.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>

#define MAX_LINE_LEN 4096	// longest line we'll try to display
#define UNUSED(x) (void)(x)	// tell compiler when we intentionally don't use a variable
#define TAB_WIDTH 4
#define FS_VERSION "2.2.1"

// ------------------ Options structure ------------------
typedef struct {
    bool ignore_case;		// -i
    bool reverse_find;		// -r
    bool show_line_numbers;	// -n
    bool show_filename;		// -f
    bool filename_title;	// -F
    bool filename_only;		// -m
    bool count_only;		// -c
    bool show_version;		// -v
    int before;   			// -bN
    int after;   			// -aN
    int line_limit; 		// -lN
    int line_crop; 			// -LN
    const char *pattern;
} Options;

// ------------------ Option handler type ------------------
// Define an array of pointers to handler functions
// each handler function takes the pointer to the option's storage (above) and the command line argument
typedef void (*OptionHandler)(Options *opts, const char *arg);

// Option table entry
typedef struct {
    const char *name;
    OptionHandler handler;
    const char *help;
} OptionDef;

// ------------------ Option Handlers ------------------
void handle_ignore_case(Options *opts, const char *arg) {UNUSED(arg); opts->ignore_case = true; }
void handle_reverse_find(Options *opts, const char *arg) {UNUSED(arg); opts->reverse_find = true; }
void handle_show_line_numbers(Options *opts, const char *arg) {UNUSED(arg); opts->show_line_numbers = true; }
void handle_show_filename(Options *opts, const char *arg) {UNUSED(arg); opts->show_filename = true; }
void handle_filename_title(Options *opts, const char *arg) {UNUSED(arg); opts->filename_title = true; }
void handle_filename_only(Options *opts, const char *arg) {UNUSED(arg); opts->filename_only = true; }
void handle_count_only(Options *opts, const char *arg) {UNUSED(arg); opts->count_only = true; }
void handle_show_version(Options *opts, const char *arg) {UNUSED(arg); opts->show_version = true; }

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
    if (n < 0) n = 0;
    if (n >= MAX_LINE_LEN) n = MAX_LINE_LEN -1;
    opts->line_limit = n;
}
void handle_limit_crop(Options *opts, const char *arg) {
    int n = atoi(arg + 2);  // parse "-LN"
    if (n < 0) n = 0;
    if (n >= MAX_LINE_LEN) n = MAX_LINE_LEN -1;
    opts->line_crop = n;
}

// ------------------ Option Table ------------------
OptionDef option_table[] = {
//   name  handler              help
    {"-i", handle_ignore_case, "Ignore case when searching"},
    {"-r", handle_reverse_find, "Show lines that do NOT match"},
    {"-n", handle_show_line_numbers, "Show line numbers"},
    {"-f", handle_show_filename, "Show file basename on each line"},
    {"-F", handle_filename_title, "Show file name as section title"},
    {"-m", handle_filename_only, "Only show file names that have a match"},
    {"-c", handle_count_only, "Print only a count of matching lines"},
    {"-v", handle_show_version, "Print fs version information"},
    {"-b", handle_before, "Print N lines before a match (e.g. -b2)"},
    {"-a", handle_after, "Print N lines after a match (e.g. -a3)"},
    {"-l", handle_limit_line, "Print only the first n chars of each line (e.g. -l20)"},
    {"-L", handle_limit_crop, "Crop the first n chars of each line (e.g. -L5)"},
    {NULL, NULL, NULL} // sentinel 
};

// ------------------ Options Parsing ------------------
void parse_options(int argc, char *argv[], Options *opts, int *first_file_index) {
    // zero-initialise all fields in options
    *opts = (Options){0};  
    // modify any non-zero defaults (e.g. -l defaults to full line, not 0)
    opts->line_limit = MAX_LINE_LEN-1;

    int i; // start from 1 as argv[0] is the name of the executable
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            bool recognized = false;
            for (OptionDef *opt = option_table; opt->name; opt++) { // runs until hits sentinel
                size_t len = strlen(opt->name);
                // if we find a match to our option list, call the handler function
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
        	// we've reached the first command line arg that doesn't start with a '-'
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
    
/*    if (*first_file_index >= argc){
        fprintf(stderr, "Error: no files specified.\n");
        exit(EXIT_FAILURE);
    }*/
}

// ------------------ Case-insensitive substring search ------------------
bool line_contains(const char *line, const char *pattern, bool ignore_case, bool reverse_find) {

// +++++++++
// Handle -v: 1 of 2: return lines that do NOT match
// +++++++++
	// case sensitive search
    if (!ignore_case) {
    	if (reverse_find) return strstr(line, pattern) == NULL;
    	else return strstr(line, pattern) != NULL;
    }

// +++++++++
// Handle -i: convert line and search pattern to lower case
// +++++++++
	// ignore_case == true so convert everything
    static char lower_line[MAX_LINE_LEN];	
    static char lower_pat[1024];	// if pattern > 1024 ignore the end of the pattern

	// safe copy into the lower buffers
    strncpy(lower_line, line, sizeof(lower_line));
    lower_line[sizeof(lower_line)-1] = '\0';
    strncpy(lower_pat, pattern, sizeof(lower_pat));
    lower_pat[sizeof(lower_pat)-1] = '\0';

	// now make them all lower case
    for (char *p = lower_line; *p; p++) *p = tolower((unsigned char)*p);
    for (char *p = lower_pat; *p; p++) *p = tolower((unsigned char)*p);
	
// +++++++++
// Handle -v: 2 of 2: return lines that do NOT match
// +++++++++
	// return the string serach 
    if (reverse_find) return strstr(lower_line, lower_pat) == NULL;
    else return strstr(lower_line, lower_pat) != NULL;
}

// ------------------ Helper for appending to a string ---------
void append_to_buffer(char *buf, size_t bufsize, const char *fmt, ...) {
    if (bufsize == 0) return;  // nothing to do

    size_t len = strlen(buf);
    if (len >= bufsize - 1) return;  // no space left

    va_list ap;
    va_start(ap, fmt);
    // buf+len starts appending after any existing buffer content
    // bufsize-len ensures we don't write over the total buffer size
    int n = vsnprintf(buf + len, bufsize - len, fmt, ap);
    va_end(ap);

    // n is chars written - kept for potential debugging. even if n is less than intended
    // (i.e. bufsize - len), vsnprintf will still null terminate the string (i.e. buf)
    UNUSED(n);
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
                int max_chars, int crop_chars, bool show_line_nums, bool show_fname)
{
    char buffer[MAX_LINE_LEN];  // should be at least as large as your fgets buffer
	char line_expanded[MAX_LINE_LEN]; // buffer for tab expansion
	char *line_to_print;		// final modified line
	char blank[1]="";			// what we print if crop or max reduces to nothing
	size_t len;					// length of the modified buffer line

    // Make a local copy of line to safely modify & null terminate for safety
    strncpy(buffer, line, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    
    // Strip trailing newline if present - we don't want to duplicate this later
    len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';
        len--;
    }

    // Expand tabs safely - extra spaces to tab stop
    size_t out = 0;
    int col = 0;
    for (size_t i = 0; buffer[i] != '\0' && out < sizeof(line_expanded)-1; i++) {
        if (buffer[i] == '\t') {
        	// expand tab 
            int spaces = TAB_WIDTH - (col % TAB_WIDTH); // i.e. spaces to next tab stop
            for (int s = 0; s < spaces && out < sizeof(line_expanded)-1; s++) {
                line_expanded[out++] = ' ';
                col++;
            }
        } else {
        	// no tab to expand
            line_expanded[out++] = buffer[i];
            col++;
        }
    }
    line_expanded[out] = '\0'; 		//terminate the expanded line 
    line_to_print = line_expanded;	//make this the line to print
    len = out;						// reset len

// +++++++++
// Handle -L: crop the start of the line 
// +++++++++
	if (crop_chars> 0 ) {
		if (len > (size_t)crop_chars) line_to_print = line_to_print + crop_chars;
		else line_to_print = blank;
	}

// +++++++++
// Handle -l: Adjust max_chars if the line is shorter than the max specified 
// +++++++++
    if ((size_t)max_chars > len)  max_chars = (int)len;

// +++++++++
// Handle -f: Print with optional file name prefix
// +++++++++
    char prefix[256]; prefix[0] = '\0';
	if (show_fname) append_to_buffer(prefix, sizeof(prefix), "%s:", get_basename(filename));

// +++++++++
// Handle -n: Print with optional line numbers prefix
// +++++++++
	if (show_line_nums) append_to_buffer(prefix, sizeof(prefix), "%04d:", lineno);
	
	// and print the modified line up to max chars in length (+ any prefix)
    printf("%s%.*s\n", prefix, max_chars, line_to_print);
}


// ------------------ File Processing ------------------
typedef struct {
    int lineno;
    char *line;
} BeforeLine;

void process_file(FILE *fp, const char *filename, const Options *opts) {
//    FILE *fp = fopen(filename, "r");
//    if (!fp) { perror(filename); return; }

// +++++++++
// Handle -b: 1 of 3: create a buffer to capture rolling set of previous lines
// +++++++++
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

// +++++++++
// Handle -F: show file name headers before the matched lines. 
// +++++++++
	if(opts->filename_title) printf("\n----------------------\n"
									"File: %s\n"
									"----------------------\n", filename);

    while (fgets(line, sizeof(line), fp)) {
        bool match = line_contains(line, opts->pattern, opts->ignore_case, opts->reverse_find);

        // --- handle match ---
        if (match) {
// +++++++++
// Handle -m: ONLY show file names where there are matches (not the matches). 
// +++++++++
			if(opts->filename_only) {
				printf("File: %s\n", filename);
				goto CleanUp;
				}

			// without the -m option we process every line
            match_count++;

// +++++++++
// Handle -b: 2 of 3: print n lines from before the match; or as many as the buffer has
// +++++++++
            // print before lines in chronological order
            // this uses a circular buffer of size specified in the -b option
			if (before_size > 0) { // only do this if there's a buffer or the mod can throw a runtime error
				int start = (buf_pos + (before_size - buf_count)) % before_size;
				for (int i = 0; i < buf_count; i++) {
					int idx = (start + i) % before_size;
// +++++++++
// Handle -c: 1 of 2: don't print lines if match count requested
// +++++++++
					if (!opts->count_only && before_buf[idx].line) {
						print_line(filename, before_buf[idx].line, before_buf[idx].lineno, 
							opts->line_limit, opts->line_crop, opts->show_line_numbers, opts->show_filename);
					}
				}
			}

// +++++++++
// Handle -a: 1 of 2: set point from which we print n lines after the match; or as many as there are left in the file
// +++++++++
            // set after-counter for printing lines after this match
            after_counter = opts->after;
        }

// +++++++++
// Handle -a: 2 of 2: continue to print lines after the match until the counter runs down
// +++++++++
        // --- print current line if match OR after-counter active ---
        if (match || after_counter > 0) {
            if (!opts->count_only) {
	            print_line(filename, line, lineno, 
	            	opts->line_limit, opts->line_crop, opts->show_line_numbers, opts->show_filename);
            }
            // decrement after-counter only for non-match lines 
            if (!match && after_counter > 0)
                after_counter--;
        }

// +++++++++
// Handle -b: 3 of 3: push all lines into the buffer (curcular) so historical lines can be printed 
// +++++++++
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

// +++++++++
// Handle -c: 2 of 2: print match count if requested
// +++++++++
    if (opts->count_only) {
        printf("%s:%d\n", get_basename(filename), match_count);
    }

CleanUp:
    // --- cleanup ---
    if (before_buf) {
        for (int j = 0; j < before_size; j++)
            free(before_buf[j].line);
        free(before_buf);
    }

//    fclose(fp);
}


// ------------------ Main ------------------
int main(int argc, char *argv[]) {
	// if we are bring piped from stdin, we expect at least 2 args (fs and pattern), 
	// otherwise we expect at least a filename so args 3 or more
	int expected_args;
	if (isatty(fileno(stdin))) expected_args=3; else expected_args=2;

    if (argc < expected_args) {	
        fprintf(stderr, "Usage: %s [options] pattern files...\n", argv[0]);
        fprintf(stderr, "Options:\n");
        for (OptionDef *opt = option_table; opt->name; opt++) {
            fprintf(stderr, "  %s\t%s\n", opt->name, opt->help);
        }
        return EXIT_FAILURE;
    }

    Options opts; // this is our full options list if values
    int first_file_index;	
    // this parses the options including the search pattern and updated first_file_index
    // to give is the location on the command line of the first file / file wildcard
    parse_options(argc, argv, &opts, &first_file_index);

// +++++++++
// Handle -v: show version
// +++++++++
	if (opts.show_version){
		printf("fs version v%s\n", FS_VERSION);
		return EXIT_SUCCESS;
	}

    if (first_file_index >= argc) {
        // No files specified on the command line; check if stdin has been used to pipe data in
		if (isatty(fileno(stdin))) {
			// stdin is a terminal → no pipe or redirection
			fprintf(stderr, "Error: no input provided via pipe or file.\n");
			return EXIT_FAILURE;
		}
		// we're good - stdin has something to check
        process_file(stdin, "<stdin>", &opts);
    } else {
		// process each command line file or file wildcard
		for (int i = first_file_index; i < argc; i++) {
			glob_t globbuf; // holds array of matching files if there's a wildcard
			// glob will return = 0 if there's a wildcard that actually matches files
			// it will return non-0 if there's a single file (no wildcard) or the wildcard doesn't match any files
			if (glob(argv[i], 0, NULL, &globbuf) == 0) {
				// so we only get here if there's a wildcard (file?.c or *.h etc) and this actually matches files
				// if so, process them and then free the glob array
				for (size_t j = 0; j < globbuf.gl_pathc; j++) {
					FILE *fp = fopen(globbuf.gl_pathv[j], "r");
					if (!fp) {
						perror(globbuf.gl_pathv[j]);
						continue;   // print error but continue
					}
					process_file(fp, globbuf.gl_pathv[j], &opts);
					fclose(fp);
				}
				globfree(&globbuf);
			} else {
				// we only get here if the file was not a wildcard, or was a wildcard that didn't match 
				// any files. either way, the process_file func will simply reject what it can't find 
				FILE *fp = fopen(argv[i], "r");
				if (!fp) {
					perror(argv[i]);
					continue;   // print error but continue
				}
				process_file(fp, argv[i], &opts);
				fclose(fp);
			}
		}
    }

    return EXIT_SUCCESS;
}
