/* test_math.c - recursion helpers, vector math, easing curves */
#include "test_framework.h"
#include "mathx.h"
#include "vec2.h"
#include "level.h"

static void test_factorial(void)
{
    CHECK_INT(Math_Factorial(0), 1);
    CHECK_INT(Math_Factorial(1), 1);
    CHECK_INT(Math_Factorial(5), 120);
    CHECK_INT(Math_Factorial(10), 3628800);
    CHECK_INT(Math_Factorial(20), 2432902008176640000LL);   /* largest that fits in 64 bits */
    CHECK_INT(Math_Factorial(-3), 1);                        /* defined as "base case" */
    for (int n = 1; n <= 20; n++)                            /* n! == n * (n-1)! */
        CHECK_INT(Math_Factorial(n), n * Math_Factorial(n - 1));
}

static void test_power(void)
{
    CHECK_INT(Math_PowInt(2, 10), 1024);
    CHECK_INT(Math_PowInt(7, 0), 1);
    CHECK_INT(Math_PowInt(7, 1), 7);
    CHECK_INT(Math_PowInt(10, 18), 1000000000000000000LL);
    CHECK_INT(Math_PowInt(3, 13), 1594323);
    CHECK_INT(Math_PowInt(5, -2), 1);                        /* e <= 0 -> 1 */
    /* compare with plain repeated multiplication */
    for (int b = 1; b <= 9; b++) {
        long long expect = 1;
        for (int e = 0; e <= 15; e++) {
            CHECK_INT(Math_PowInt(b, e), expect);
            expect *= b;
        }
    }
    CHECK_INT(Math_PowInt(36360, 3), 36360LL * 36360LL * 36360LL);
}

static void test_gcd(void)
{
    CHECK_INT(Math_Gcd(48, 18), 6);
    CHECK_INT(Math_Gcd(18, 48), 6);
    CHECK_INT(Math_Gcd(17, 5), 1);
    CHECK_INT(Math_Gcd(0, 9), 9);
    CHECK_INT(Math_Gcd(9, 0), 9);
    CHECK_INT(Math_Gcd(1920, 1080), 120);
    CHECK_INT(Math_Gcd(-12, 18), 6);
    CHECK_INT(Math_Gcd(100, 100), 100);
    CHECK_INT(1920 / Math_Gcd(1920, 1080), 16);              /* 16:9 */
    CHECK_INT(1080 / Math_Gcd(1920, 1080), 9);
}

static void test_fibonacci(void)
{
    static const long long first[] = { 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55 };
    for (int i = 0; i < 11; i++)
        CHECK_INT(Math_Fibonacci(i), first[i]);
    CHECK_INT(Math_Fibonacci(50), 12586269025LL);
    CHECK_INT(Math_Fibonacci(90), 2880067194370816120LL);
    CHECK_INT(Math_Fibonacci(-4), 0);
    CHECK_INT(Math_Fibonacci(500), Math_Fibonacci(90));      /* clamped */
    for (int n = 2; n <= 90; n++)                            /* f(n) = f(n-1) + f(n-2) */
        CHECK_INT(Math_Fibonacci(n), Math_Fibonacci(n - 1) + Math_Fibonacci(n - 2));
}

static void test_angles(void)
{
    CHECK_NEAR(Math_DegToRad(180.0f), GG_PI, 1e-6);
    CHECK_NEAR(Math_DegToRad(90.0f), GG_PI / 2.0f, 1e-6);
    CHECK_NEAR(Math_RadToDeg(GG_PI), 180.0, 1e-4);
    for (float d = -360.0f; d <= 360.0f; d += 15.0f)
        CHECK_NEAR(Math_RadToDeg(Math_DegToRad(d)), d, 1e-3);
}

static void test_vec2(void)
{
    Vec2 a = V2(3.0f, 4.0f), b = V2(-1.0f, 2.0f);
    CHECK_NEAR(V2_Add(a, b).x, 2.0, 1e-6);
    CHECK_NEAR(V2_Add(a, b).y, 6.0, 1e-6);
    CHECK_NEAR(V2_Sub(a, b).x, 4.0, 1e-6);
    CHECK_NEAR(V2_Sub(a, b).y, 2.0, 1e-6);
    CHECK_NEAR(V2_Scale(a, 2.0f).y, 8.0, 1e-6);
    CHECK_NEAR(V2_Dot(a, b), 5.0, 1e-6);
    CHECK_NEAR(V2_Len(a), 5.0, 1e-6);
    CHECK_NEAR(V2_LenSq(a), 25.0, 1e-6);
    CHECK_NEAR(V2_Dist(a, V2(0, 0)), 5.0, 1e-6);
    CHECK_NEAR(V2_Len(V2_Norm(a)), 1.0, 1e-6);
    CHECK_NEAR(V2_Norm(V2(0, 0)).x, 0.0, 1e-9);              /* zero vector stays zero */
    CHECK_NEAR(V2_Norm(V2(0, 0)).y, 0.0, 1e-9);
}

static void test_rotate(void)
{
    Vec2 r = V2_Rotate(V2(1.0f, 0.0f), GG_PI / 2.0f);        /* 90 deg clockwise on screen */
    CHECK_NEAR(r.x, 0.0, 1e-6);
    CHECK_NEAR(r.y, 1.0, 1e-6);
    r = V2_Rotate(V2(1.0f, 0.0f), GG_PI);
    CHECK_NEAR(r.x, -1.0, 1e-6);
    CHECK_NEAR(r.y, 0.0, 1e-5);
    for (float ang = -6.0f; ang < 6.0f; ang += 0.37f)        /* rotation keeps the length */
        CHECK_NEAR(V2_Len(V2_Rotate(V2(3.0f, -7.0f), ang)), V2_Len(V2(3.0f, -7.0f)), 1e-4);
}

static void test_clamp_lerp(void)
{
    CHECK_NEAR(Clampf(5.0f, 0.0f, 3.0f), 3.0, 0);
    CHECK_NEAR(Clampf(-1.0f, 0.0f, 3.0f), 0.0, 0);
    CHECK_NEAR(Clampf(2.0f, 0.0f, 3.0f), 2.0, 0);
    CHECK_NEAR(Lerpf(10.0f, 20.0f, 0.0f), 10.0, 1e-6);
    CHECK_NEAR(Lerpf(10.0f, 20.0f, 1.0f), 20.0, 1e-6);
    CHECK_NEAR(Lerpf(10.0f, 20.0f, 0.25f), 12.5, 1e-6);
}

static void test_easing(void)
{
    CHECK_NEAR(Ease_BackOut(0.0f), 0.0, 1e-5);
    CHECK_NEAR(Ease_BackOut(1.0f), 1.0, 1e-5);
    CHECK(Ease_BackOut(0.7f) > 1.0f);                        /* it overshoots the target */
    CHECK(Ease_BackOut(0.5f) > 0.5f);

    CHECK_NEAR(Ease_CubicInOut(0.0f), 0.0, 1e-6);
    CHECK_NEAR(Ease_CubicInOut(1.0f), 1.0, 1e-6);
    CHECK_NEAR(Ease_CubicInOut(0.5f), 0.5, 1e-6);
    float prev = -1.0f;
    for (int i = 0; i <= 100; i++) {                         /* never goes backwards */
        float v = Ease_CubicInOut((float)i / 100.0f);
        CHECK(v >= prev);
        CHECK(v >= 0.0f && v <= 1.0f);
        prev = v;
    }
    for (int i = 0; i <= 10; i++) {                          /* point-symmetric around (0.5, 0.5) */
        float t = (float)i / 10.0f;
        CHECK_NEAR(Ease_CubicInOut(t) + Ease_CubicInOut(1.0f - t), 1.0, 1e-5);
    }
}

void run_math_tests(void)
{
    RUN(test_factorial);
    RUN(test_power);
    RUN(test_gcd);
    RUN(test_fibonacci);
    RUN(test_angles);
    RUN(test_vec2);
    RUN(test_rotate);
    RUN(test_clamp_lerp);
    RUN(test_easing);
}
