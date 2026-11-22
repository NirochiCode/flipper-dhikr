/* Minimal, dependency-free test helpers for host unit tests. */
#pragma once

#include <stdio.h>

extern int g_tests_run;
extern int g_tests_failed;
extern int g_current_failed;

#define EXPECT(cond)                                                              \
    do {                                                                          \
        if(!(cond)) {                                                             \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            g_current_failed = 1;                                                 \
        }                                                                         \
    } while(0)

#define EXPECT_EQ_U(actual, expected)                                             \
    do {                                                                          \
        unsigned long _a = (unsigned long)(actual);                               \
        unsigned long _e = (unsigned long)(expected);                             \
        if(_a != _e) {                                                            \
            printf(                                                               \
                "    FAIL %s:%d: %s == %lu, expected %lu\n",                      \
                __FILE__,                                                         \
                __LINE__,                                                         \
                #actual,                                                          \
                _a,                                                               \
                _e);                                                              \
            g_current_failed = 1;                                                 \
        }                                                                         \
    } while(0)

#define RUN_TEST(fn)                                                              \
    do {                                                                          \
        g_current_failed = 0;                                                     \
        fn();                                                                     \
        g_tests_run++;                                                            \
        if(g_current_failed) g_tests_failed++;                                    \
        printf("%s %s\n", g_current_failed ? "[FAIL]" : "[ OK ]", #fn);          \
    } while(0)
