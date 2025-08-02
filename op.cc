#pragma once

#include <assert.h>

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

    union {
        Default d_val;
        RefToPtr<T> t_val;
    };
    bool valid;

    Op(T val) : t_val(val), valid(true) {}
    Op(Default val) : d_val(val), valid(false) {}

    inline auto get() -> T {
        assert(this->valid && "Accessing invalid optional");
        return this->t_val;
    }

    static auto empty() -> Op { return Op{Default{}}; }
};
