/*
 * mathx.c  -  recursion examples that the game and the console really use
 *
 *  Every recursive function has
 *     (1) a BASE CASE      : the answer is known, stop calling yourself
 *     (2) a RECURSIVE CASE : call yourself with a SMALLER problem
 *  Forget (1) or make (2) not smaller and you get infinite recursion
 *  (stack overflow).
 */
#include "mathx.h"
#include "common.h"

/*
 * n! = n * (n-1)!          0! = 1
 *   PSEUDOCODE
 *     FUNCTION fact(n)
 *        IF n <= 1 THEN return 1            // base case
 *        return n * fact(n - 1)             // recursive case
 *   Trace for fact(4):  4 * fact(3) = 4 * 3 * fact(2) = 4 * 3 * 2 * fact(1) = 24
 */
long long Math_Factorial(int n)
{
    if (n <= 1)
        return 1;
    return n * Math_Factorial(n - 1);
}

/*
 * Fast power: b^e with only O(log e) multiplications.
 *      b^e = (b^(e/2))^2          when e is even
 *      b^e = b * (b^(e/2))^2      when e is odd
 * Used by the console to show how many different shots exist:
 *      (360 angles * 100 powers)^3 strikes.
 */
long long Math_PowInt(long long base, int e)
{
    if (e <= 0)
        return 1;                              /* base case: x^0 = 1 */
    long long half = Math_PowInt(base, e / 2); /* [T04] integer division */
    return (e % 2 == 0) ? half * half : base * half * half;
}

/*
 * Euclid: gcd(a, b) = gcd(b, a mod b),  gcd(a, 0) = a
 * Used to print a window's aspect ratio, e.g. 1920x1080 -> 16:9.
 */
int Math_Gcd(int a, int b)
{
    if (b == 0)
        return a < 0 ? -a : a;                 /* base case */
    return Math_Gcd(b, a % b);                 /* [T04] modulo shrinks the problem */
}

/*
 * Fibonacci with MEMOISATION.  Plain fib(n) = fib(n-1) + fib(n-2) recomputes
 * the same values millions of times.  An ARRAY remembers results already
 * computed, turning exponential time into linear time.
 */
long long Math_Fibonacci(int n)
{
    static long long memo[91];                 /* fib(90) is the last that fits in 64 bits */
    if (n < 0) return 0;
    if (n > 90) n = 90;
    if (n <= 1) return n;                      /* base cases: fib(0)=0, fib(1)=1 */
    /* cppcheck-suppress arrayIndexOutOfBoundsCond ; false positive: n was clamped to 0..90 above */
    if (memo[n] != 0) return memo[n];          /* already known */
    memo[n] = Math_Fibonacci(n - 1) + Math_Fibonacci(n - 2);
    return memo[n];
}

float Math_DegToRad(float deg) { return deg * GG_PI / 180.0f; }
float Math_RadToDeg(float rad) { return rad * 180.0f / GG_PI; }
