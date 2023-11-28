#ifndef _UTILS_H
#define _UTILS_H

void panic(char *format, ...);
void simple_error(char *format, ...);
void error(char *format, ...);
int string_ends_with(const char *string, const char *substring);

#endif
