/* test_util.c - string helpers, swap, time formatting */
#include <ctype.h>
#include <string.h>
#include "test_framework.h"
#include "util.h"
#include "common.h"

static void test_str_copy(void)
{
    char buf[8];
    Str_Copy(buf, sizeof buf, "abc");
    CHECK_STR(buf, "abc");
    Str_Copy(buf, sizeof buf, "abcdefghijkl");               /* too long: truncated + terminated */
    CHECK_STR(buf, "abcdefg");
    CHECK_INT(strlen(buf), 7);
    Str_Copy(buf, sizeof buf, "1234567");                    /* exactly fits */
    CHECK_STR(buf, "1234567");
    Str_Copy(buf, sizeof buf, "");
    CHECK_STR(buf, "");

    char guard[4] = { 'x', 'x', 'x', 'x' };                  /* cap 0 must not write at all */
    Str_Copy(guard, 0, "hello");
    CHECK(guard[0] == 'x');

    char one[1] = { 'x' };                                   /* cap 1: only room for '\0' */
    Str_Copy(one, 1, "hello");
    CHECK(one[0] == '\0');
}

static void test_str_trim(void)
{
    char a[32] = "  hello  ";   Str_Trim(a); CHECK_STR(a, "hello");
    char b[32] = "\tx y\n";     Str_Trim(b); CHECK_STR(b, "x y");
    char c[32] = "   ";         Str_Trim(c); CHECK_STR(c, "");
    char d[32] = "";            Str_Trim(d); CHECK_STR(d, "");
    char e[32] = "nospace";     Str_Trim(e); CHECK_STR(e, "nospace");
    char f[32] = "  left";      Str_Trim(f); CHECK_STR(f, "left");
    char g[32] = "right  \r\n"; Str_Trim(g); CHECK_STR(g, "right");
}

static void test_str_case(void)
{
    char a[32] = "Hello, World 123!";
    Str_ToUpper(a);
    CHECK_STR(a, "HELLO, WORLD 123!");
    Str_ToLower(a);
    CHECK_STR(a, "hello, world 123!");
    char e[4] = "";
    Str_ToUpper(e);
    CHECK_STR(e, "");
    /* every ASCII letter round-trips */
    for (char c = 'a'; c <= 'z'; c++) {
        char s[2] = { c, 0 };
        Str_ToUpper(s);
        CHECK(s[0] == toupper(c));
    }
}

static void test_str_compare(void)
{
    CHECK_INT(Str_CompareNoCase("abc", "ABC"), 0);
    CHECK(Str_CompareNoCase("abc", "abd") < 0);
    CHECK(Str_CompareNoCase("abd", "abc") > 0);
    CHECK(Str_CompareNoCase("ab", "abc") < 0);               /* prefix is smaller */
    CHECK(Str_CompareNoCase("abc", "ab") > 0);
    CHECK_INT(Str_CompareNoCase("", ""), 0);
    CHECK(Str_CompareNoCase("", "a") < 0);
    CHECK(Str_CompareNoCase("Zed", "alpha") > 0);            /* case ignored: z > a */
}

static void test_str_contains(void)
{
    CHECK(Str_ContainsNoCase("Andi Putra", "put"));
    CHECK(Str_ContainsNoCase("Andi Putra", "ANDI"));
    CHECK(Str_ContainsNoCase("Andi Putra", "a"));
    CHECK(Str_ContainsNoCase("abc", ""));                    /* empty needle always matches */
    CHECK(!Str_ContainsNoCase("abc", "abcd"));
    CHECK(!Str_ContainsNoCase("abc", "xyz"));
    CHECK(Str_ContainsNoCase("aab", "ab"));                  /* needs backtracking */
    CHECK(Str_ContainsNoCase("abc", "abc"));
    CHECK(!Str_ContainsNoCase("", "a"));
}

static void test_valid_name(void)
{
    CHECK(Str_IsValidName("Andi"));
    CHECK(Str_IsValidName("Andi Putra"));
    CHECK(Str_IsValidName("player_1-b"));
    CHECK(Str_IsValidName("A"));
    CHECK(!Str_IsValidName(""));
    CHECK(!Str_IsValidName("a,b"));                          /* comma breaks the CSV */
    CHECK(!Str_IsValidName("tab\there"));
    CHECK(!Str_IsValidName("quote\"s"));
    CHECK(!Str_IsValidName("na\xc3\xafve"));                 /* non-ASCII rejected */
    CHECK(Str_IsValidName("12345678901234567890123"));       /* 23 chars: max */
    CHECK(!Str_IsValidName("123456789012345678901234"));     /* 24 chars: too long */
}

static void test_format_time(void)
{
    char t[32];
    Str_FormatTime(t, sizeof t, 0.0f);     CHECK_STR(t, "0:00.0");
    Str_FormatTime(t, sizeof t, 83.7f);    CHECK_STR(t, "1:23.7");
    Str_FormatTime(t, sizeof t, 59.96f);   CHECK_STR(t, "1:00.0");   /* rounds up across the minute */
    Str_FormatTime(t, sizeof t, 3600.0f);  CHECK_STR(t, "60:00.0");
    Str_FormatTime(t, sizeof t, -5.0f);    CHECK_STR(t, "0:00.0");   /* negative clamps to 0 */
    Str_FormatTime(t, sizeof t, 9.04f);    CHECK_STR(t, "0:09.0");
    Str_FormatTime(t, sizeof t, 125.5f);   CHECK_STR(t, "2:05.5");
    char tiny[4];                                                      /* never overflows */
    Str_FormatTime(tiny, sizeof tiny, 1234.5f);
    CHECK(strlen(tiny) <= 3);
}

static void test_today(void)
{
    char d[16];
    Str_Today(d, sizeof d);
    CHECK_INT(strlen(d), 10);
    CHECK(d[4] == '-' && d[7] == '-');
    for (int i = 0; i < 10; i++)
        if (i != 4 && i != 7)
            CHECK(isdigit((unsigned char)d[i]));
    CHECK(d[0] == '2');                                       /* 20xx */
}

static void test_swap(void)
{
    int a = 1, b = 2;
    Swap_Int(&a, &b);
    CHECK_INT(a, 2);
    CHECK_INT(b, 1);
    float x = 1.5f, y = -2.5f;
    Swap_Float(&x, &y);
    CHECK_NEAR(x, -2.5, 0);
    CHECK_NEAR(y, 1.5, 0);
    int same = 7;                                             /* swapping with itself is safe */
    Swap_Int(&same, &same);
    CHECK_INT(same, 7);
}

void run_util_tests(void)
{
    RUN(test_str_copy);
    RUN(test_str_trim);
    RUN(test_str_case);
    RUN(test_str_compare);
    RUN(test_str_contains);
    RUN(test_valid_name);
    RUN(test_format_time);
    RUN(test_today);
    RUN(test_swap);
}
