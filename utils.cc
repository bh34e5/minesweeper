#pragma once

#include <assert.h>
#include <stdlib.h>

#define EXIT myExit

auto myExit(int code) -> void { exit(code); }

template <typename T> auto clamp(T min_val, T val, T max_val) -> T {
    assert(min_val <= max_val && "Invalid clamp range");

    if (val < min_val) {
        val = min_val;
    }

    if (val > max_val) {
        val = max_val;
    }

    return val;
}
