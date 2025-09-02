#pragma once

#include <assert.h>

template <typename T> struct Op {
    T val;
    bool valid;

    // the constructor below forces us to have a empty constructor here as well
    Op() : val{}, valid{false} {}

    // non explicit constructor so that functions returning an Optional just
    // just return a value without having to wrap it
    Op(T val) : val{val}, valid{true} {}

    inline auto get() -> T & {
        assert(this->valid && "Accessing invalid optional");
        return this->val;
    }

    static auto empty() -> Op { return Op{}; }
};
