#pragma once

#include <cstdio>

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned char uchar;

#define TEST(name, ...)                         \
  [&] {                                         \
    std::printf("running %s test ... ", #name); \
    std::fflush(stdout);                        \
    bool passed = false;                        \
    try {                                       \
      passed = (__VA_ARGS__);                   \
    } catch (...) {                             \
    }                                           \
    std::puts(passed ? "pass" : "fail");        \
    return passed;                              \
  }()
