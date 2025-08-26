#pragma once

#include "../utils.cc"
#include "gl.cc"

#include <stdio.h>

inline auto glColor(unsigned char c) -> GLfloat {
    GLfloat c_float = c;
    return c_float / 255.0f;
}

struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;

    static auto grayscale(unsigned char g) -> Color { return Color{g, g, g}; }
};

inline auto toGlLoc(size_t val, size_t range) -> GLfloat {
    double pct = static_cast<double>(val) / static_cast<double>(range);
    double gl = pct * 2.0 - 1.0;

    return static_cast<GLfloat>(gl);
}

struct GLFW {
    GLFW(GLFWerrorfun callback) {
        glfwSetErrorCallback(callback);

        int res = glfwInit();
        if (res < 0) {
            fprintf(stderr, "Failed to init glfw");
            EXIT(1);
        }

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }

    ~GLFW() { glfwTerminate(); }

    auto setErrorCallback(GLFWerrorfun callback) -> void {
        glfwSetErrorCallback(callback);
    }
};
