#pragma once

#include "slice.cc"

#include <assert.h>
#include <sys/types.h>
#include <utility> // for std::move

template <typename T> struct DList {
    T *ptr;
    size_t cap;
    size_t len;

    DList() : ptr(nullptr), cap(0), len(0) {}
    ~DList() {
        delete[] this->ptr;
        this->ptr = nullptr;
        this->cap = 0;
        this->len = 0;
    }

    auto push(T &&item) -> void {
        ensureCap(this->len + 1);
        this->ptr[this->len++] = std::move(item);
    }

    auto push(T const &item) -> void {
        T copy = item;
        this->push(std::move(copy));
    }

    auto swapRemove(size_t idx) -> T {
        assert(idx < this->len && "Out of bounds");

        if (idx == this->len - 1) {
            // special case, just removing the last item
            T res{std::move(this->ptr[--idx])};
            return res;
        } else {
            T res{std::move(this->ptr[--idx])};
            T tmp{std::move(this->ptr[this->len - 1])};
            this->ptr[idx] = std::move(tmp);
            return res;
        }
    }

    auto clearRetainCapacity() -> void {
        for (size_t i = 0; i < this->len; ++i) {
            this->ptr[i] = T{};
        }

        this->len = 0;
    }

    auto at(size_t idx) -> T & {
        assert(idx < this->len && "Out of bounds");
        return this->ptr[idx];
    }

    auto ensureCap(size_t cap) -> void {
        if (this->cap >= cap) {
            return;
        }

        size_t next_cap = this->cap ? this->cap : 8;
        while (next_cap < cap) {
            next_cap = 2 * next_cap;
        }

        T *next_ptr = new T[next_cap];
        assert(next_ptr != nullptr && "Out of memory");

        for (size_t i = 0; i < this->len; ++i) {
            next_ptr[i] = std::move(this->ptr[i]);
        }

        delete[] this->ptr;

        this->ptr = next_ptr;
        this->cap = next_cap;
    }

    auto slice() -> Slice<T> { return Slice<T>{this->ptr, this->len}; }
};
