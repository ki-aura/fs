#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// -----------------------------------------------------------------
// Memory Safe Allocation Wrappers
// -----------------------------------------------------------------

// Safe malloc wrapper: allocates memory or exits on failure.
void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL && size > 0) {
        fprintf(stderr, "Fatal: Out of memory (xmalloc failed for %zu bytes).\n", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

// Safe calloc wrapper: allocates and zeros memory or exits on failure.
void *xcalloc(size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb, size);
    if (ptr == NULL && nmemb > 0 && size > 0) {
        fprintf(stderr, "Fatal: Out of memory (xcalloc failed for %zu elements of %zu bytes).\n", nmemb, size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

// Safe realloc wrapper: resizes memory or exits on failure.
// Note: If realloc fails, the original block is freed before exit to prevent a leak.
void *xrealloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL && size > 0) {
        fprintf(stderr, "Fatal: Out of memory (xrealloc failed for %zu bytes).\n", size);
        free(ptr); // Free the original pointer if we exit
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

// -----------------------------------------------------------------
// Portable get_line Function
// -----------------------------------------------------------------

/**
 * @brief Reads an entire line from a stream, similar to POSIX getline().
 *
 * This function uses standard C's fgets() and dynamic memory management
 * to safely read lines of arbitrary length.
 *
 * @param lineptr A pointer to a char* buffer pointer. The buffer is resized
 * or allocated as necessary.
 * @param n       A pointer to a size_t holding the current buffer capacity.
 * @param stream  The FILE stream to read from.
 * @return The number of characters read (including the newline character, if present),
 * or -1 on failure (including EOF).
 *
 * NOTE: The buffer pointed to by *lineptr is always null-terminated.
 * The buffer *lineptr must be freed by the caller.
 */
ssize_t get_line(char **lineptr, size_t *n, FILE *stream) {
    // We must use a separate variable for the block size, as *n is the capacity.
    static const size_t BLOCK_SIZE = 3; // adjust to a reasonable size for the job 
    size_t total_read = 0; 

    // 1. Initialize buffer if it's the first call or buffer is too small
    if (*lineptr == NULL || *n < BLOCK_SIZE) {
        *n = BLOCK_SIZE;
        *lineptr = xrealloc(*lineptr, *n);
    }

    while (1) {
        // Calculate remaining space in the current buffer
        size_t remaining = *n - total_read;
        
        // 2. Read a block of data using fgets
        // Reads up to (remaining - 1) chars and ensures null termination.
        // We start writing from the current end of the data: *lineptr + total_read
        char *result = fgets(*lineptr + total_read, (int)remaining, stream);

        if (result == NULL) {
            // Read failed (EOF or error)
            if (total_read == 0) {
                return -1; // Nothing was read
            }
            // Something was read before EOF/error, so the line ends here.
            break; 
        }

        // 3. Update the count of characters read
        // The length of the new block is strlen(buffer_start_of_new_data)
        size_t new_len = strlen(*lineptr + total_read);
        total_read += new_len;

        // 4. Check for Newline Character
        if (total_read > 0 && (*lineptr)[total_read - 1] == '\n') {
            break; // Line is complete
        }
        
        // 5. Check if the block was filled without a newline
        // If the number of characters read is exactly (remaining - 1),
        // it means fgets filled the block and no newline was found.
        if (new_len == remaining - 1) { 
            // The buffer is full. Must resize.
            *n += BLOCK_SIZE;
            *lineptr = xrealloc(*lineptr, *n);
            
            // total_read is already correct. Continue to next iteration 
            // to fill the newly available space.
        } else {
             // Block was NOT full, but no newline was found (must have hit EOF/error
             // which was caught by result == NULL, or a short read for some reason).
             // Since we already handled result == NULL, we can break safely here.
             break;
        }
    }

    return (ssize_t)total_read;
}


// -----------------------------------------------------------------
// Demonstration Main
// -----------------------------------------------------------------

int main() {
    char *line = NULL; 
    size_t len = 0;   
    ssize_t read_count;

    printf("Enter text line by line (Ctrl+D or Ctrl+Z to stop):\n");
    
    // Read from standard input until EOF (-1 is returned)
    while ((read_count = get_line(&line, &len, stdin)) != -1) {
        printf("Read %zd characters. Buffer capacity: %zu. Line: %s", 
               read_count, len, line);
        
        // Check if the newline was NOT present (i.e., the line ended exactly 
        // at EOF, which is the only time get_line returns a line without '\n').
        if (read_count > 0 && line[read_count - 1] != '\n') {
             printf("\n"); // Add newline for clean terminal output if missing
        }
    }

    printf("\n--- EOF Reached (Input stream closed) ---\n");
    
    // CRUCIAL: Free the dynamically allocated buffer
    if (line) {
        free(line);
        line = NULL;
    }

    return EXIT_SUCCESS;
}

