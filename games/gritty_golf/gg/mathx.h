/*
 * mathx.h  -  RECURSIVE math functions
 *
 *   [T12 Rekursif]  a function that calls ITSELF on a smaller input
 *   [T12 Math]      plus conversions that use <math.h>
 */
#ifndef GG_MATHX_H
#define GG_MATHX_H

long long Math_Factorial(int n);              /* n!                       */
long long Math_PowInt(long long base, int e); /* base^e by repeated squaring */
int       Math_Gcd(int a, int b);             /* Euclid's algorithm       */
long long Math_Fibonacci(int n);              /* recursion + memo table   */

float Math_DegToRad(float deg);
float Math_RadToDeg(float rad);

#endif /* GG_MATHX_H */
