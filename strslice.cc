#pragma once

#include "op.cc"
#include "slice.cc"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

typedef Slice<char const> StrSlice;

auto str_slice(char const *s) -> StrSlice { return StrSlice{s, strlen(s)}; }
auto str_slice(char const *s, size_t len) -> StrSlice {
    return StrSlice{s, len};
}

#define STR_SLICE(s) (str_slice(s, sizeof(s) - 1))
#define STR_ARGS(ss) (int)ss.len, ss.ptr

auto lastIdxOf(StrSlice haystack, char c) -> size_t {
    char t;
    size_t idx = 0;
    while ((idx < haystack.len) && ((t = haystack[idx]) != c)) {
        ++idx;
    }
    return idx;
}

auto startsWith(StrSlice haystack, StrSlice needle) -> bool {
    if (haystack.len < needle.len) {
        return false;
    }
    return strncmp(haystack.ptr, needle.ptr, needle.len) == 0;
}

auto endsWith(StrSlice haystack, StrSlice needle) -> bool {
    if (haystack.len < needle.len) {
        return false;
    }

    size_t offset = haystack.len - needle.len;
    return strncmp(haystack.ptr + offset, needle.ptr, needle.len) == 0;
}

struct PatternIterator {
    StrSlice haystack;
    StrSlice needle;
    size_t idx;

    explicit PatternIterator(StrSlice haystack, StrSlice needle)
        : haystack(haystack), needle(needle), idx(0) {}

    auto next() -> Op<size_t> {
        if (this->idx >= this->haystack.len) {
            return Op<size_t>::empty();
        }

        if (startsWith(this->haystack.slice(this->idx), this->needle)) {
            size_t ret = this->idx;
            this->idx += this->needle.len;
            return ret;
        }

        ++this->idx;
        return this->next();
    }
};

auto toZString(StrSlice str) -> char * {
    char *z_str = new char[str.len + 1];
    snprintf(z_str, str.len + 1, "%.*s", STR_ARGS(str));

    return z_str;
}
