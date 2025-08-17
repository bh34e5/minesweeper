#pragma once

#include <assert.h>
#include <sys/types.h>

#define MEGABYTES(n) (10 * 1024 * 1024)

struct Arena {
    void *ptr;
    size_t cap;
    size_t len;

    struct Marker {
        Arena &arena;
        size_t mark;

        Marker(Arena &arena) : arena(arena), mark(arena.len) {}
        ~Marker() { this->arena.reset(this->mark); }
    };

    explicit Arena(size_t cap) : ptr(new char[cap]), cap(cap), len(0) {}
    ~Arena() {
        delete[] static_cast<char *>(this->ptr);
        this->ptr = nullptr;
        this->cap = 0;
        this->len = 0;
    }

    auto mark() -> Marker { return Marker{*this}; }

    auto reset(size_t mark) -> void {
        assert(mark < this->cap && "Invalid mark");
        assert(mark <= this->len && "Invalid mark");

        this->len = mark;
    }

    auto push(size_t len) -> void * {
        assert(this->len + len <= this->cap && "Out of memory");

        void *res = static_cast<char *>(this->ptr) + this->len;
        this->len += len;

        return res;
    }

    auto pushN(size_t count, size_t size) -> void * {
        assert(this->len + (count * size) <= this->cap && "Out of memory");

        void *res = static_cast<char *>(this->ptr) + this->len;
        this->len += count * size;

        return res;
    }

    template <typename T> auto pushT() -> T * { return this->pushT(T{}); }

    template <typename T> auto pushT(T init) -> T * {
        auto ptr = static_cast<T *>(this->push(sizeof(T)));
        *ptr = init;

        return ptr;
    }

    template <typename T> auto pushTN(size_t count) -> T * {
        return this->pushTN(count, T{});
    }

    template <typename T> auto pushTN(size_t count, T init) -> T * {
        auto ptr = static_cast<T *>(this->pushN(count, sizeof(T)));

        for (size_t i = 0; i < count; ++i) {
            ptr[i] = init;
        }

        return ptr;
    }
};
