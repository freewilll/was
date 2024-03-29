#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lexer.h"
#include "was.h"

// ANSI color codes
#define LOCUS "\e[0;01m" // Bold
#define BRED "\e[1;31m"  // Bright red
#define BMAG "\e[1;35m"  // Bright magenta
#define RESET "\e[0m"    // Reset

// Report an internal error and exit
void panic(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "Internal error: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static void print_filename_and_linenumber(int is_tty) {
    if (is_tty) fprintf(stderr, LOCUS);
    fprintf(stderr, "%s:%d: ", cur_filename, cur_line);
    if (is_tty) fprintf(stderr, RESET);
}

// Report an error by itself (no filename or line number) and exit
void simple_error(char *format, ...) {
    va_list ap;
    va_start(ap, format);

    int is_tty = isatty(2);
    if (is_tty) fprintf(stderr, BRED);
    fprintf(stderr, "error: ");
    if (is_tty) fprintf(stderr, RESET);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

// Report an error with a filename and line number and exit
void _error(char *format, va_list ap) {
    int is_tty = isatty(2);
    print_filename_and_linenumber(is_tty);
    if (is_tty) fprintf(stderr, BRED);
    fprintf(stderr, "error: ");
    if (is_tty) fprintf(stderr, RESET);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void error(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    _error(format, ap);
}

// Returns 1 if string ends with substring
int string_ends_with(const char *string, const char *substring) {
    int string_len = strlen(string);
    int substring_len = strlen(substring);

    if (string_len < substring_len) return 0;

    return !memcmp(string + string_len - substring_len, substring, substring_len);
}

// Encode value into a preallocated array data, returning the size
int encode_sleb128(int value, char *data) {
    int pos = 0;
    int more = 1;
    while (more) {
        unsigned char c = value & 0x7f;
        value >>= 7;

        int sign_bit = c & 0x40;
        if ((value == 0 && !sign_bit) || (value == -1 && sign_bit))
            more = 0;

        else
            c |= 0x80;

        data[pos++] = c;
    }

    return pos;
}

// Encode value into a preallocated array data, returning the size
// The value is not a proper unsigned integer, so this will likely fail for
// stupidly huge values.
int encode_uleb128(int value, char *data) {
    int pos = 0;
    while (1) {
        unsigned char c = value & 0x7f;
        value >>= 7;
        if (value) c |= 0x80;
        data[pos++] = c;
        if (!value) break;
    }

    return pos;
}
