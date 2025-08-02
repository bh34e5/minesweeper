#pragma once

template <typename T> struct Ref {
    T *val;

    Ref(T &val) : val(&val) {}
    operator T &() { return *this->val; }
};
