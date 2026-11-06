#pragma once

#include <cstdio>

inline int latch_failures = 0;

#define CHECK(cond)                                                                                        \
    do {                                                                                                   \
        if (!(cond)) {                                                                                     \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);                                \
            ++latch_failures;                                                                              \
        }                                                                                                  \
    } while (0)
