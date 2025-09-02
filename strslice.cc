#pragma once

#include "arena.cc"
#include "op.cc"
#include "slice.cc"
#include "utils.cc"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef Slice<char const> StrSlice;

auto strSlice(char const *s) -> StrSlice { return StrSlice{s, strlen(s)}; }
auto strSlice(char const *s, size_t len) -> StrSlice {
    return StrSlice{s, len};
}

#define STR_SLICE(s) (strSlice(s, sizeof(s) - 1))
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

auto toZString(Arena *arena, StrSlice str) -> char * {
    char *z_str = arena->pushTN<char>(str.len + 1);
    snprintf(z_str, str.len + 1, "%.*s", STR_ARGS(str));

    return z_str;
}

auto vsliceNPrintf(char *buf, size_t n, char const *format, va_list args)
    -> StrSlice {
    size_t len = vsnprintf(buf, n, format, args);
    if (len >= n) {
        // response truncated
        len = n - 1;
    }
    return StrSlice{buf, len};
}

auto sliceNPrintf(char *buf, size_t n, char const *format, ...) -> StrSlice {
    va_list args;
    va_start(args, format);

    StrSlice result = vsliceNPrintf(buf, n, format, args);
    va_end(args);

    return result;
}
