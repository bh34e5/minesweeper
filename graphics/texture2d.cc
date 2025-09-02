#pragma once

#include "../arena.cc"
#include "../dirutils.cc"
#include "common.cc"
#include "gl.cc"

#include <stdio.h>

struct Texture2D {
    GLuint texture;

    auto useTex() -> void { glBindTexture(GL_TEXTURE_2D, this->texture); }

    auto solidColor(Arena *arena, Color color, Dims dims) -> void {
        this->solidColor(arena, color, 255, dims);
    }

    auto solidColor(Arena *arena, Color color, unsigned char alpha, Dims dims)
        -> void {
        this->useTex();

        size_t element_size = sizeof(Color) + 1;
        size_t data_len = dims.width * dims.height * element_size;

        auto marker = arena->mark();
        unsigned char *data = arena->pushTN<unsigned char>(data_len);

        for (size_t i = 0; i < data_len; i += element_size) {
            data[i + 0] = color.r;
            data[i + 1] = color.g;
            data[i + 2] = color.b;
            data[i + 3] = alpha;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dims.width, dims.height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, data);
    }

    auto grayscale(Arena *arena, unsigned char g, Dims dims) {
        solidColor(arena, Color::grayscale(g), dims);
    }

    auto grayscale(Arena *arena, unsigned char g, unsigned alpha, Dims dims) {
        solidColor(arena, Color::grayscale(g), alpha, dims);
    }

    auto bindAlphaData(Dims dims, unsigned char *pixels) -> void {
        this->useTex();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, dims.width, dims.height, 0,
                     GL_RED, GL_UNSIGNED_BYTE, pixels);
    }
};

auto makeTexture() -> Texture2D {
    Texture2D t{};
    glGenTextures(1, &t.texture);

    t.useTex();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return t;
}

auto deleteTexture(Texture2D *texture) -> void {
    glDeleteTextures(1, &texture->texture);
    texture->texture = 0;
}
