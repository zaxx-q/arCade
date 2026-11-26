/* Portable string helpers (strlcpy / stricmp are not in standard C). */
#ifndef ARCADE_STR_H
#define ARCADE_STR_H

#include "arcade/common.h"

/* Copies at most size-1 chars and always NUL-terminates (size > 0). Returns strlen(src). */
size_t arc_strlcpy(char *dst, const char *src, size_t size);
int arc_stricmp(const char *a, const char *b); /* case-insensitive compare (ASCII) */
bool arc_contains_nocase(const char *haystack, const char *needle);

/* fgets() for a text that is already in memory (an asset): copies the next line of text[*pos .. len) into
 * `out`, keeping its '\n' like fgets does, at most size-1 characters, NUL-terminated, and moves *pos past
 * what it copied. A longer line comes out in pieces, again like fgets. Returns false when no text is left. */
bool arc_next_line(const char *text, size_t len, size_t *pos, char *out, size_t size);

#endif
