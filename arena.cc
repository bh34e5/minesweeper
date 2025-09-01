#pragma once

#include "utils.cc"

#include <assert.h>
#include <stdio.h>

#define KILOBYTES(n) (10 * 1024)
#define MEGABYTES(n) (10 * 1024 * 1024)

struct Arena {
    void *ptr;
    size_t cap;
    size_t len;
    size_t mark_count;

    struct Marker {
        Arena &arena;
        size_t mark;

        Marker(Arena &arena) : arena(arena), mark(arena.len) {
            ++this->arena.mark_count;
        }
        Marker(Marker const &other) = delete;
        Marker(Marker &&other) = delete;

        ~Marker() {
            --this->arena.mark_count;
            this->arena.reset(this->mark);
        }
    };

    explicit Arena(size_t cap)
        : ptr(new char[cap]), cap(cap), len(0), mark_count(0) {}
    Arena(Arena const &other) = delete;
    Arena(Arena &&other)
        : ptr(other.ptr), cap(other.cap), len(other.len),
          mark_count(other.mark_count) {
        if (other.mark_count > 0) {
            fprintf(stderr, "Moving marked arena\n");
            EXIT(1);
        }

        other.ptr = nullptr;
        other.cap = 0;
        other.len = 0;
    }

    ~Arena() {
        if (this->mark_count > 0) {
            fprintf(stderr, "Deleting marked arena\n");
            EXIT(1);
        }

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
