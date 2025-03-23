#ifndef _UTILS_H
#define _UTILS_H

void panic(char *format, ...);
void simple_error(char *format, ...);
void error(char *format, ...);
int string_ends_with(const char *string, const char *substring);

#define ALIGN_UP(offset, alignment) ((((offset) + alignment - 1) & ~(alignment - 1)))
#define PADDING_FOR_ALIGN_UP(offset, alignment) (ALIGN_UP((offset), (alignment)) - (offset))

int encode_sleb128(int value, char *data);
int encode_uleb128(int value, char *data);

#endif
