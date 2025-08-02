#pragma once

#include "ref.cc"

#include <assert.h>
#include <sys/types.h>

template <typename T> struct Slice {
    T *ptr;
    size_t len;

    explicit Slice(T *ptr, size_t len) : ptr(ptr), len(len) {}

    auto operator[](size_t idx) -> T & {
        assert(idx < this->len && "Out of bounds slice access");
        return this->ptr[idx];
    }

    auto operator+(size_t idx) -> T * {
        assert(idx < this->len && "Out of bounds slice access");
        return this->ptr + idx;
    }

    auto slice(size_t start, size_t end) -> Slice<T> {
        assert(start <= end && "Invalid slice range");
        assert(start < this->len && "Out of bounds start access");
        assert(end <= this->len && "Out of bounds end access");

        size_t slice_len = end - start;
        if (slice_len == 0) {
            return Slice<T>{nullptr, 0};
        }
        return Slice<T>{this->ptr + start, slice_len};
    }

    auto begin() -> T * { return this->ptr; }
    auto end() -> T * { return this->ptr + this->len; }
};
