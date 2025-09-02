#pragma once

#include "utils.cc"

#include <assert.h>
#include <stddef.h>

#define SLICE(T, s) (Slice<T>{s, ARRAY_LEN(s)})

template <typename T> struct Slice {
    T *ptr;
    size_t len;

    auto operator[](size_t idx) -> T & {
        assert(idx < this->len && "Out of bounds slice access");
        return this->ptr[idx];
    }

    auto operator+(size_t idx) -> T * {
        assert(idx < this->len && "Out of bounds slice access");
        return this->ptr + idx;
    }

    auto slice(size_t start) -> Slice<T> {
        return this->slice(start, this->len);
    }

    auto slice(size_t start, size_t end) -> Slice<T> {
        assert(start <= end && "Invalid slice range");
        assert(start <= this->len && "Out of bounds start access");
        assert(end <= this->len && "Out of bounds end access");

        size_t slice_len = end - start;
        if (slice_len == 0) {
            return Slice<T>{nullptr, 0};
        }
        return Slice<T>{this->ptr + start, slice_len};
    }

    auto begin() -> T * { return this->ptr; }
    auto end() -> T * { return this->ptr + this->len; }

    auto contains(T *ptr) -> bool {
        char *c_start = static_cast<char *>(static_cast<void *>(this->ptr));
        char *c_ptr = static_cast<char *>(static_cast<void *>(ptr));

        if (c_ptr < c_start) {
            return false;
        }

        ptrdiff_t diff = c_ptr - c_start;
        if (diff % (sizeof(T)) != 0) {
            return false;
        }

        // we have checked alignment above, this is safe
        size_t idx = ptr - this->ptr;
        if (idx >= this->len) {
            return false;
        }
        return true;
    }

    auto indexOf(T *item) -> size_t {
        assert(this->contains(item));
        return item - this->ptr;
    }

    auto eql(Slice other) -> bool {
        return this->ptr == other.ptr && this->len == other.len;
    }
};
