#pragma once

#include "slice.cc"

#include <string.h>
#include <sys/types.h>

typedef Slice<char const> StrSlice;

auto str_slice(char const *s) -> StrSlice { return StrSlice{s, strlen(s)}; }
auto str_slice(char const *s, size_t len) -> StrSlice {
    return StrSlice{s, len};
}

#define STR_SLICE(s) (str_slice(s, sizeof(s) - 1))
#define STR_ARGS(ss) (int)ss.len, ss.ptr

auto endsWith(StrSlice haystack, StrSlice needle) -> bool {
    if (haystack.len < needle.len) {
        return false;
    }

    size_t offset = haystack.len - needle.len;
    for (size_t i = 0; i < needle.len; ++i) {
        if (haystack[offset + i] != needle[i]) {
            return false;
        }
    }

    return true;
}
