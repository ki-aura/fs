#include <sys/types.h>  // Good practice for POSIX data types (like size_t, etc.)
#include <stddef.h>     // Provides NULL
#include <stdlib.h>     // Standard library functions (often includes NULL)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <glob.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <regex.h>
#include <getopt.h>  // POSIX getopt

#define MAX_LINE_LEN 8192	// longest line we'll try to display or search
#define UNUSED(x) (void)(x)	// tell compiler when we intentionally don't use a variable
#define TAB_WIDTH 4
#define FS_VERSION "2.5.0"

// ------------------Memory safe allocation helpers ----------
void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL && size>0) {
        fprintf(stderr, "Fatal: Out of memory (malloc %zu bytes).\n", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xcalloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (ptr == NULL && count>0 && size>0) {
        fprintf(stderr, "Fatal: Out of memory (calloc %zu count %zu bytes).\n", count, size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    // If realloc fails, the original block pointed to by ptr is left unchanged.
    if (new_ptr == NULL && size > 0) {
        fprintf(stderr, "Fatal: Out of memory (realloc %zu bytes).\n", size);
        free(ptr); // we can at least free up the memory currently pointed to
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

// -----------------------------------------------------
// ------------------ Options Parsing ------------------
// -----------------------------------------------------

// Help table entry
typedef struct {
    const char *name;
    const char *help;
} HelpDef;

// ------------------ Help Table ------------------
HelpDef help_table[] = {
//   name  handler              help
	{"-i",   "Ignore case when searching (default is case sensitive)"},
    {"-r",   "Show lines that do NOT match search pattern"},
    {"-E",   "Treat the pattern as a POSIX regex"},
    {"-n",   "Show line numbers"},
    {"-f",   "Show file basename on each line"},
    {"-F",   "Show file name as section title"},
    {"-m",   "Show only file names that contain a match"},
    {"-c",   "Show only a count of matching lines"},
    {"-v",   "Display fs version information"},
    {"-h",   "Display this help message"},
    {"-b N", "Print N lines before a match (e.g. -b2) maximum 50 lines"},
    {"-a N", "Print N lines after a match (e.g. -a3) no maximum"},
    {"-l N", "Print only the first n chars of each line (e.g. -l20)"},
    {"-L N", "Crop the first n chars of each line (e.g. -L5)"},
    {NULL, NULL} // sentinel
};

const char option_list[] = "irEnfFmcvhb:a:l:L:";

// ------------------ Options structure ------------------
typedef struct {
    bool ignore_case;		// -i
    bool reverse_find;		// -r
    bool use_regex;			// -E
    bool show_line_numbers;	// -n
    bool show_filename;		// -f
    bool filename_title;	// -F
    bool filename_only;		// -m
    bool count_only;		// -c
    bool show_version;		// -v
    bool show_help;			// -h
    int before;   			// -bN
    int after;   			// -aN
    int line_limit; 		// -lN
    int line_crop; 			// -LN
    char *pattern;			// will come from argv[]
} Options;

// ----------------------- parsing function ---------------
void parse_options(int argc, char *argv[], Options *opts, int *first_file_index) {
    *opts = (Options){0};
    opts->line_limit = MAX_LINE_LEN - 1;

    int opt;
    while ((opt = getopt(argc, argv, option_list)) != -1) {
        switch (opt) {
            case 'i': opts->ignore_case = true; break;
            case 'r': opts->reverse_find = true; break;
            case 'E': opts->use_regex = true; break;
            case 'n': opts->show_line_numbers = true; break;
            case 'f': opts->show_filename = true; break;
            case 'F': opts->filename_title = true; break;
            case 'm': opts->filename_only = true; break;
            case 'c': opts->count_only = true; break;
            case 'v': opts->show_version = true; break;
            case 'h': opts->show_help = true; break;

            case 'b': {
                int n = atoi(optarg);
                if (n < 0) n = 0;
                if (n > 50) n = 50;
                opts->before = n;
                break;
            }
            case 'a': {
                int n = atoi(optarg);
                if (n < 0) n = 0;
                opts->after = n;
                break;
            }
            case 'l': {
                int n = atoi(optarg);
                if (n < 0) n = 0;
                if (n >= MAX_LINE_LEN) n = MAX_LINE_LEN - 1;
                opts->line_limit = n;
                break;
            }
            case 'L': {
                int n = atoi(optarg);
                if (n < 0) n = 0;
                if (n >= MAX_LINE_LEN) n = MAX_LINE_LEN - 1;
                opts->line_crop = n;
                break;
            }

            default:
                fprintf(stderr, "Unknown option: -%c\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    // After getopt() finishes, optind points to the first non-option argument.
    if (optind < argc) {
        char *pattern_copy = xmalloc(MAX_LINE_LEN);
        strncpy(pattern_copy, argv[optind], MAX_LINE_LEN - 1);
        pattern_copy[MAX_LINE_LEN - 1] = '\0';
        opts->pattern = pattern_copy;
        optind++;
    }

    // If no pattern was given, enable help unless -v was specified.
    if (!opts->pattern && !opts->show_version)
        opts->show_help = true;

    *first_file_index = optind;
}

// ------------------ show help ------------------
// +++++++++++
// Handle -h: show help table
// +++++++++++
void show_help(void){
	fprintf(stderr, "Usage: fs [options] pattern files...\n");
	fprintf(stderr, "Options:\n");
	for (HelpDef *opt = help_table; opt->name; opt++) {
		fprintf(stderr, "  %s\t%s\n", opt->name, opt->help);
	}
}


// -----------------------------------------------------
// ------------------ Case-insensitive substring search ------------------
// -----------------------------------------------------
bool line_contains(const char *line, const Options *opts, const regex_t *regex) {
    bool matched = false;

// +++++++++++
// Handle -E: 2of2: use regex
// +++++++++++
    if (opts->use_regex) {
        // Use regex
        matched = (regexec(regex, line, 0, NULL, 0) == 0);
    } else if (opts->ignore_case) {

// +++++++++++
// Handle -i: 3of3: convert line to lower case. pattern will already be correct (see 1of2)
// +++++++++++
        char lower_line[MAX_LINE_LEN];
        strncpy(lower_line, line, sizeof(lower_line)-1);
        lower_line[sizeof(lower_line)-1] = '\0';
        for (char *p = lower_line; *p; p++) *p = tolower((unsigned char)*p);
        matched = strstr(lower_line, opts->pattern) != NULL;
    } else {
        matched = strstr(line, opts->pattern) != NULL;
    }

// +++++++++++
// Handle -v: return lines that do NOT match
// +++++++++++
    return opts->reverse_find ? !matched : matched;
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

// -----------------------------------------------------
// ------------------ Helper for stripping filename ---------
// -----------------------------------------------------
const char *get_basename(const char *path){
    if (path == NULL) return NULL;

    const char *last = strrchr(path, '/');
    if (last) return last + 1;  // character after last '/'
    return path;    	      	// no slash, whole string is filename
}

// -----------------------------------------------------
// ------------------ Helper for Line Printing ---------
// -----------------------------------------------------
void print_line(const char *filename, const char *line, int lineno,
                int max_chars, int crop_chars, bool show_line_nums, bool show_fname)
{
    char buffer[MAX_LINE_LEN];  // should be at least as large as your fgets buffer
	char line_expanded[MAX_LINE_LEN]; // buffer for tab expansion
	char *line_to_print;		// final modified line
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

// +++++++++++
// Handle -L: crop the start of the line 
// +++++++++++
	if (crop_chars> 0 ) {
		if (len > (size_t)crop_chars) line_to_print = line_to_print + crop_chars;
		else line_to_print = "";
	}

// +++++++++++
// Handle -l: Adjust max_chars if the line is shorter than the max specified 
// +++++++++++
    if ((size_t)max_chars > len)  max_chars = (int)len;

// +++++++++++
// Handle -f: Print with optional file name prefix
// +++++++++++
    char prefix[256]; prefix[0] = '\0';
	if (show_fname) append_to_buffer(prefix, sizeof(prefix), "%s:", get_basename(filename));

// +++++++++++
// Handle -n: Print with optional line numbers prefix
// +++++++++++
	if (show_line_nums) append_to_buffer(prefix, sizeof(prefix), "%04d:", lineno);
	
	// and print the modified line up to max chars in length (+ any prefix)
    printf("%s%.*s\n", prefix, max_chars, line_to_print);
}


// -----------------------------------------------------
// ------------------ File Processing ------------------
// -----------------------------------------------------

// we'll use this structure to store previous lines for the -b option
typedef struct {
    int lineno;
    char *line;
} BeforeLine;

void process_file(FILE *fp, const char *filename, const Options *opts, const regex_t *regex) {
// +++++++++++
// Handle -b: 1of3: create a buffer to capture rolling set of previous lines
// +++++++++++
    int before_size = opts->before;
    BeforeLine *before_buf = NULL;
    if (before_size > 0) {
        before_buf = xcalloc(before_size, sizeof(BeforeLine));
    }

    char *line = NULL;	
    size_t line_len = 0;
    int lineno = 1;
    int after_counter = 0;
    int buf_pos = 0;
    int buf_count = 0; // number of valid lines in circular buffer
    int match_count = 0;

// +++++++++++
// Handle -F: show file name headers before the matched lines. 
// +++++++++++
	if(opts->filename_title) printf(
		"\n----------------------\nFile: %s\n----------------------\n", filename);

    while (getline(&line, &line_len, fp) != -1) {
        bool match = line_contains(line, opts, regex);

        // --- handle match ---
        if (match) {
// +++++++++++
// Handle -m: 2of2: ONLY show file names where there are matches (not the matched lines). 
// +++++++++++
			if(opts->filename_only) {
				printf("Match Found In: %s\n", filename);
				return; // this is safe as -m turns off -b (no buffer cleanup needed) see 1of2
				}

			// without the -m option we process every line
            match_count++;

// +++++++++++
// Handle -c: 1of3: don't print before lines if match count requested
// +++++++++++
// Handle -b: 2of3: print n lines from before the match; or as many as the buffer has
// +++++++++++
            // print before lines in chronological order
            // this uses a circular buffer of size specified in the -b option
			if (before_size > 0 && !opts->count_only ) { // only do this if there's a buffer or the mod can throw a runtime error
				printf("---\n");
				int start = (buf_pos + (before_size - buf_count)) % before_size;
				for (int i = 0; i < buf_count; i++) {
					int idx = (start + i) % before_size;
					if (before_buf[idx].line) {
						print_line(filename, before_buf[idx].line, before_buf[idx].lineno, 
							opts->line_limit, opts->line_crop, opts->show_line_numbers, opts->show_filename);
					}
				}
			}

// +++++++++++
// Handle -a: 1of2: set point from which we print n lines after the match; or as many as there are left in the file
// +++++++++++
            // set after-counter for printing lines after this match
            after_counter = opts->after;
        }

// +++++++++++
// Handle -c: 2of3: don't print after lines if match count requested
// +++++++++++
// Handle -a: 2of2: continue to print lines after the match until the counter runs down
// +++++++++++
        // --- print current line if match OR after-counter active ---
        if ((match || after_counter > 0) && !opts->count_only){
			print_line(filename, line, lineno, 
				opts->line_limit, opts->line_crop, opts->show_line_numbers, opts->show_filename);
            // decrement after-counter only for non-match lines 
            if (!match && after_counter > 0) {
                after_counter--;
            	// when we come to the end of the after lines print a terminater
            	if (after_counter == 0) printf("+++\n");
            }
        }

// +++++++++++
// Handle -b: 3of3: push each line into the buffer (curcular) so historical lines can be printed 
// +++++++++++
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

// +++++++++++
// Handle -c: 3of3: print match count if requested
// +++++++++++
    if (opts->count_only) {
        printf("%s:%d\n", get_basename(filename), match_count);
    }

    // --- cleanup buffer ---
    free(line);
    if (before_buf) {
        for (int j = 0; j < before_size; j++)
            free(before_buf[j].line);
        free(before_buf);
    }
}


// -----------------------------------------------------
// ------------------ Main ------------------
// -----------------------------------------------------
int main(int argc, char *argv[]) {
    Options opts; // this is our full options list if values
    int first_file_index;	
    regex_t regex;
	int regex_compiled = 0; // flag to know if we need to free
   
    // Parse the options including the search pattern. The updated first_file_index
    // gives the location on the command line of the first filename / file wildcard
    parse_options(argc, argv, &opts, &first_file_index);

// +++++++++++
// Handle -v: show version
// +++++++++++
	if (opts.show_version){
		printf("ki-uara fs version: v%s\n", FS_VERSION);
		return EXIT_SUCCESS;
	}

// +++++++++++
// Handle -h: show help
// +++++++++++
	if (opts.show_help){
		show_help();
		return EXIT_SUCCESS;
	}

// +++++++++++
// Handle invalid number of arguments
// +++++++++++
	// if we are bring piped from stdin, we expect at least 2 args (fs and pattern), 
	// otherwise we expect at least a filename so args 3 or more
	int expected_args;
	if (isatty(fileno(stdin))) expected_args=3; else expected_args=2;

    if (argc < expected_args) {	
        show_help();
        return EXIT_FAILURE;
    }

// +++++++++++
// Handle -i: 1of3: make the pattern lower case (unless regex specified)
// +++++++++++
if (opts.ignore_case && !opts.use_regex) {
    for (char *p = opts.pattern; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
}

// +++++++++++
// Handle -E: 1of2: compile regex 
// +++++++++++
// Handle -i: 2of3: specify REG_ICASE if -i
// +++++++++++
if (opts.use_regex) {
    int flags = REG_NOSUB;  // we don’t need match offsets
    if (opts.ignore_case) flags |= REG_ICASE;

    int ret = regcomp(&regex, opts.pattern, flags);
    if (ret != 0) {
        char errbuf[256];
        regerror(ret, &regex, errbuf, sizeof(errbuf));
        fprintf(stderr, "Regex compilation failed: %s\n", errbuf);
        exit(EXIT_FAILURE);
    }
    regex_compiled = 1;
}

// +++++++++++
// Handle -m: 1of2: turn off the -b option so that we don't do unnecessary buffer allocations. 
// +++++++++++
if (opts.filename_only) opts.before = 0;


	// ---------------- MAIN PROCESS LOGIC --------------
	if (first_file_index >= argc) {
        // No files specified on the command line; check if stdin has been used to pipe data in
		if (isatty(fileno(stdin))) {
			// stdin is a terminal → no pipe or redirection
			fprintf(stderr, "Error: no input provided via pipe or file.\n");
			return EXIT_FAILURE;
		}
		// we're good - stdin has something to check
        process_file(stdin, "<stdin>", &opts, opts.use_regex ? &regex : NULL);
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
					process_file(fp, globbuf.gl_pathv[j], &opts, opts.use_regex ? &regex : NULL);
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
				process_file(fp, argv[i], &opts, opts.use_regex ? &regex : NULL);
				fclose(fp);
			}
		}
    }
if (regex_compiled) regfree(&regex);
if (opts.pattern) free(opts.pattern);
return EXIT_SUCCESS;

}
