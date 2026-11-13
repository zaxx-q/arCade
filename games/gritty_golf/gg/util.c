/*
 * util.c  -  string utilities written by hand (loops over char arrays)
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "util.h"
#include "common.h"

/* ------------------------------------------------------------------ *
 * [T09 String]  A C string is an array of char whose end is marked by the
 * character '\0' (value 0).  There is no "string" type and no length field:
 * every function must walk the array until it meets '\0'.
 * ------------------------------------------------------------------ */

/*
 * Str_Copy : a SAFE strcpy.  Copies at most cap-1 characters and always
 * writes the terminating '\0', so dst can never overflow.
 */
void Str_Copy(char *dst, size_t cap, const char *src)
{
    if (cap == 0)
        return;
    size_t i = 0;
    while (i < cap - 1 && src[i] != '\0') {      /* [T06] while loop */
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* Remove leading and trailing blanks (space, tab, newline) in place. */
void Str_Trim(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))   /* trailing */
        s[--len] = '\0';

    size_t start = 0;
    while (s[start] != '\0' && isspace((unsigned char)s[start]))   /* leading */
        start++;
    if (start > 0)
        memmove(s, s + start, len - start + 1);              /* +1 keeps the '\0' */
}

/* Characters are small integers:  'a' - 'A' == 32.  So subtracting 32
 * turns a lowercase letter into the uppercase one.                      */
void Str_ToUpper(char *s)
{
    for (; *s != '\0'; s++)                      /* walk with a pointer */
        if (*s >= 'a' && *s <= 'z')
            *s -= ('a' - 'A');                   /* [T04] compound assignment */
}

void Str_ToLower(char *s)
{
    for (; *s != '\0'; s++)
        if (*s >= 'A' && *s <= 'Z')
            *s += ('a' - 'A');
}

/* Case-insensitive strcmp: compares letter by letter, ignoring case. */
int Str_CompareNoCase(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb)
            return ca - cb;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* Does `haystack` contain `needle`, ignoring case?  (naive nested loops) */
bool Str_ContainsNoCase(const char *haystack, const char *needle)
{
    size_t hn = strlen(haystack), nn = strlen(needle);
    if (nn == 0)
        return true;
    if (nn > hn)
        return false;
    for (size_t i = 0; i + nn <= hn; i++) {            /* every start position */
        size_t j = 0;
        while (j < nn &&
               tolower((unsigned char)haystack[i + j]) == tolower((unsigned char)needle[j]))
            j++;
        if (j == nn)
            return true;
    }
    return false;
}

/*
 * A name is valid when it has 1..23 characters, each a letter, digit,
 * space, '_' or '-'.  (A comma would break the CSV score file.)
 */
bool Str_IsValidName(const char *s)
{
    size_t len = strlen(s);
    if (len == 0 || len >= PLAYER_NAME_MAX)
        return false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!(isalnum(c) || c == ' ' || c == '_' || c == '-'))
            return false;
    }
    return true;
}

/*
 * Seconds -> "m:ss.t"   using integer division and the MODULO operator.
 *   83.7 s -> tenths = 837 -> mins = 837/600 = 1, secs = (837/10)%60 = 23
 */
void Str_FormatTime(char *out, size_t cap, float seconds)
{
    if (seconds < 0.0f)
        seconds = 0.0f;
    int tenths_total = (int)(seconds * 10.0f + 0.5f);     /* [T03] cast float->int */
    int mins   = tenths_total / 600;                      /* [T04] integer division */
    int secs   = (tenths_total / 10) % 60;                /* [T04] modulo           */
    int tenths = tenths_total % 10;
    snprintf(out, cap, "%d:%02d.%d", mins, secs, tenths);
}

void Str_Today(char *out, size_t cap)
{
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (lt == NULL || strftime(out, cap, "%Y-%m-%d", lt) == 0)
        Str_Copy(out, cap, "0000-00-00");
}

/* ------------------------------------------------------------------ *
 * [T10 Pointer]  PASS BY POINTER.
 * C passes arguments BY VALUE (a copy).  A function that must change its
 * caller's variable therefore receives that variable's ADDRESS.
 *       int x = 1, y = 2;   Swap_Int(&x, &y);   // now x == 2, y == 1
 * ------------------------------------------------------------------ */
void Swap_Int(int *a, int *b)
{
    int tmp = *a;          /* *a = "the value that a points to" (dereference) */
    *a = *b;
    *b = tmp;
}

void Swap_Float(float *a, float *b)
{
    float tmp = *a;
    *a = *b;
    *b = tmp;
}
