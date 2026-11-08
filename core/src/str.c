#include "arcade/str.h"

#include <ctype.h>
#include <string.h>

size_t arc_strlcpy(char *dst, const char *src, size_t size) {
    size_t n = strlen(src);
    if (size > 0) {
        size_t c = n < size - 1 ? n : size - 1;
        memcpy(dst, src, c);
        dst[c] = '\0';
    }
    return n;
}

int arc_stricmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb)
            return ca - cb;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

bool arc_contains_nocase(const char *haystack, const char *needle) {
    size_t n = strlen(needle);
    if (n == 0)
        return true;
    for (; *haystack; haystack++) {
        size_t i = 0;
        while (i < n && haystack[i] &&
               tolower((unsigned char)haystack[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (i == n)
            return true;
    }
    return false;
}
