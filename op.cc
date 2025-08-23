#pragma once

#include <assert.h>
#include <utility> // for std::move

template <typename T> struct RefToPtr {
    T val;

    RefToPtr(T val) : val(val) {}
    operator T() { return this->val; }
};

template <typename T> struct RefToPtr<T &> {
    T *val;

    RefToPtr(T &val) : val(&val) {}
    operator T &() { return *this->val; }
};

template <typename T> struct Op {
    struct Default {};

    char val[sizeof(T)];
    bool valid;

    Op(T const &val) : val{}, valid(true) {
        T *target = static_cast<T *>(static_cast<void *>(this->val));
        *target = val;
    }
    Op(T &&val) : val{}, valid(true) {
        T *target = static_cast<T *>(static_cast<void *>(this->val));
        *target = std::move(val);
    }
    Op(Default val) : val{}, valid(false) {}

    ~Op() {
        if (this->valid) {
            T *target = static_cast<T *>(static_cast<void *>(this->val));
            T disposal = std::move(*target);
            (void)disposal; // moved target here to allow calling destructor
        }
    }

    inline auto get() -> T & {
        assert(this->valid && "Accessing invalid optional");

        T *target = static_cast<T *>(static_cast<void *>(this->val));
        return *target;
    }

    static auto empty() -> Op { return Op{Default{}}; }
};
