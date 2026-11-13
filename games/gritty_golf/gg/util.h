/*
 * util.h  -  string helpers and tiny "pass by pointer" helpers
 *
 *   [T09 String]    C strings are char arrays ending with '\0'
 *   [T10 Pointer]   char* parameters, swap through pointers
 *   [T11 Fungsi]    each helper does ONE job
 */
#ifndef GG_UTIL_H
#define GG_UTIL_H

#include <stddef.h>
#include <stdbool.h>

void Str_Copy(char *dst, size_t cap, const char *src);   /* never overflows dst */
void Str_Trim(char *s);                                   /* strip blanks, in place */
void Str_ToUpper(char *s);
void Str_ToLower(char *s);
int  Str_CompareNoCase(const char *a, const char *b);     /* <0, 0, >0 like strcmp */
bool Str_ContainsNoCase(const char *haystack, const char *needle);
bool Str_IsValidName(const char *s);                      /* 1..23 safe characters */
void Str_FormatTime(char *out, size_t cap, float seconds);/* "m:ss.t"              */
void Str_Today(char *out, size_t cap);                    /* "YYYY-MM-DD"          */

/* swap through pointers: the classic reason C has pointers */
void Swap_Int(int *a, int *b);
void Swap_Float(float *a, float *b);

#endif /* GG_UTIL_H */
