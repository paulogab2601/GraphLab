/*
 * test_helpers.h -- Lightweight assert-style test framework for Phase 1.
 *
 * Goals:
 *   - Zero external dependencies (only stdio/stdlib).
 *   - One test = one static void function. Failures fprintf and return; main
 *     keeps running so a single broken test doesn't mask the rest.
 *   - Exit code == number of failed tests (clipped to 1), so CTest sees them.
 *
 * We will swap in Unity or CMocka later if richer features are needed.
 */
#ifndef GRAPHLAB_TEST_HELPERS_H
#define GRAPHLAB_TEST_HELPERS_H

#include <stdio.h>
#include <stdlib.h>

static int gl_tests    = 0;
static int gl_failures = 0;

#define TEST_BEGIN(name)                                                       \
    do {                                                                       \
        ++gl_tests;                                                            \
        fprintf(stderr, "[ RUN  ] %s\n", (name));                              \
    } while (0)

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "[ FAIL ] %s:%d  expected: %s\n",                  \
                    __FILE__, __LINE__, #cond);                                \
            ++gl_failures;                                                     \
            return;                                                            \
        }                                                                      \
    } while (0)

#define EXPECT_EQ_INT(a, b)                                                    \
    do {                                                                       \
        long long _a = (long long)(a);                                         \
        long long _b = (long long)(b);                                         \
        if (_a != _b) {                                                        \
            fprintf(stderr,                                                    \
                    "[ FAIL ] %s:%d  %s == %s (got %lld vs %lld)\n",           \
                    __FILE__, __LINE__, #a, #b, _a, _b);                       \
            ++gl_failures;                                                     \
            return;                                                            \
        }                                                                      \
    } while (0)

#define EXPECT_EQ_DBL(a, b, eps)                                               \
    do {                                                                       \
        double _a = (double)(a);                                               \
        double _b = (double)(b);                                               \
        double _d = (_a - _b);                                                 \
        if (_d < 0) _d = -_d;                                                  \
        if (_d > (double)(eps)) {                                              \
            fprintf(stderr,                                                    \
                    "[ FAIL ] %s:%d  |%g - %g| > %g  (%s vs %s)\n",            \
                    __FILE__, __LINE__, _a, _b, (double)(eps), #a, #b);        \
            ++gl_failures;                                                     \
            return;                                                            \
        }                                                                      \
    } while (0)

#define TEST_REPORT()                                                          \
    do {                                                                       \
        fprintf(stderr, "\n--- %d tests, %d failures ---\n",                   \
                gl_tests, gl_failures);                                        \
        return gl_failures == 0 ? 0 : 1;                                       \
    } while (0)

#endif /* GRAPHLAB_TEST_HELPERS_H */
