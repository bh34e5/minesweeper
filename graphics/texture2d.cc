#pragma once

#include "../dirutils.cc"
#include "common.cc"
#include "gl.cc"

#include <stdio.h>
#include <sys/types.h>
#include <utility>

struct Texture2D {
    GLuint texture;

    Texture2D() : texture(0) {
        glGenTextures(1, &this->texture);

        this->useTex();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    Texture2D(Texture2D const &other) = delete;
    Texture2D(Texture2D &&other) : texture(other.texture) { other.texture = 0; }

    ~Texture2D() {
        if (texture) {
            printf("Deleting a texture\n");
        }

        glDeleteTextures(1, &this->texture);
    }

    Texture2D &operator=(Texture2D &&other) {
        Texture2D tmp{std::move(*this)};

        this->texture = other.texture;
        other.texture = 0;

        return *this;
    }

    auto useTex() -> void { glBindTexture(GL_TEXTURE_2D, this->texture); }

    auto solidColor(Color color, Dims dims) -> void {
        this->solidColor(color, 255, dims);
    }

    auto solidColor(Color color, unsigned char alpha, Dims dims) -> void {
        this->useTex();

        size_t element_size = sizeof(Color) + 1;
        size_t data_len = dims.width * dims.height * element_size;
        unsigned char *data = new unsigned char[data_len];

        for (size_t i = 0; i < data_len; i += element_size) {
            data[i + 0] = color.r;
            data[i + 1] = color.g;
            data[i + 2] = color.b;
            data[i + 3] = alpha;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dims.width, dims.height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, data);

        delete[] data;
    }

    auto grayscale(unsigned char g, Dims dims) {
        solidColor(Color::grayscale(g), dims);
    }

    auto grayscale(unsigned char g, unsigned alpha, Dims dims) {
        solidColor(Color::grayscale(g), alpha, dims);
    }

    auto bindAlphaData(Dims dims, unsigned char *pixels) -> void {
        this->useTex();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, dims.width, dims.height, 0,
                     GL_RED, GL_UNSIGNED_BYTE, pixels);
    }
};
