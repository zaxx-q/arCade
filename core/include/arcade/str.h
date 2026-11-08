/* Portable string helpers (strlcpy / stricmp are not in standard C). */
#ifndef ARCADE_STR_H
#define ARCADE_STR_H

#include "arcade/common.h"

/* Copies at most size-1 chars and always NUL-terminates (size > 0). Returns strlen(src). */
size_t arc_strlcpy(char *dst, const char *src, size_t size);
int arc_stricmp(const char *a, const char *b); /* case-insensitive compare (ASCII) */
bool arc_contains_nocase(const char *haystack, const char *needle);

#endif
