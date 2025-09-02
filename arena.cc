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

    auto subarena(size_t cap) -> Arena {
        assert(this->mark_count == 0 && "Subarena from a marked arena");

        if (cap == 0) {
            cap = this->cap - this->len;
        }

        char *ptr = this->pushTN<char>(cap);

        Arena res{ptr, cap};
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

auto makeArena(size_t capacity) -> Arena {
    void *ptr = new char[capacity];
    assert(ptr != nullptr && "Out of memory");

    Arena res{ptr, capacity, 0, 0};
    return res;
}

auto freeArena(Arena *arena) -> void {
    delete[] (char *)arena->ptr;

    arena->ptr = nullptr;
    arena->cap = 0;
    arena->len = 0;
    arena->mark_count = 0;
}
