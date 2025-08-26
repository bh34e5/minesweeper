#pragma once

#include "../dirutils.cc"
#include "common.cc"
#include "gl.cc"
#include "shader.cc"
#include "texture2d.cc"

#include <sys/types.h>
#include <utility>

struct QuadProgram {
    struct WithGuard {
        QuadProgram &ref;
        Texture2D &texture;

        ~WithGuard() { this->ref.swapTexture(this->texture); }
    };

    Program program;
    GLint n_pos;
    GLint n_tex_p;
    GLint tex;

    GLuint vao;
    GLuint vbo;

    Texture2D texture;

    QuadProgram(Program &&program, GLint n_pos, GLint n_tex_p, GLint tex,
                Texture2D &&texture)
        : program(std::move(program)), n_pos(n_pos), n_tex_p(n_tex_p), tex(tex),
          texture(std::move(texture)) {
        glGenVertexArrays(1, &this->vao);
        glGenBuffers(1, &this->vbo);

        this->program.useProgram();
        glBindVertexArray(this->vao);
        glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        glEnableVertexAttribArray(this->n_pos);
        glEnableVertexAttribArray(this->n_tex_p);

        glVertexAttribPointer(this->n_pos, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat),
                              (void const *)(0 * sizeof(GLfloat)));
        glVertexAttribPointer(this->n_tex_p, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat),
                              (void const *)(2 * sizeof(GLfloat)));
    }
    QuadProgram(QuadProgram const &other) = delete;
    QuadProgram(QuadProgram &&other)
        : program(std::move(other.program)), n_pos(other.n_pos),
          n_tex_p(other.n_tex_p), tex(other.tex), vao(other.vao),
          vbo(other.vbo), texture(std::move(other.texture)) {
        other.vao = 0;
        other.vbo = 0;
    }

    ~QuadProgram() {
        glDeleteVertexArrays(1, &this->vao);
        glDeleteBuffers(1, &this->vbo);
    }

    QuadProgram &operator=(QuadProgram &&other) {
        QuadProgram tmp{std::move(*this)};

        this->program = std::move(other.program);
        this->n_pos = other.n_pos;
        this->n_tex_p = other.n_tex_p;
        this->tex = other.tex;
        this->vao = other.vao;
        this->vbo = other.vbo;
        this->texture = std::move(other.texture);

        other.vao = 0;
        other.vbo = 0;

        return *this;
    }

    auto renderAt(SRect rect, float tex_loc_x0, float tex_loc_y0,
                  float tex_loc_x1, float tex_loc_y1, Dims window_dims)
        -> void {
        this->program.useProgram();
        this->setPosition(rect, tex_loc_x0, tex_loc_y0, tex_loc_x1, tex_loc_y1,
                          window_dims);

        glActiveTexture(GL_TEXTURE0);
        glUniform1i(this->tex, 0); // GL_TEXTURE0
        this->texture.useTex();    // bind texture to GL_TEXTURE0

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    auto renderAt(SRect rect, Dims window_dims) -> void {
        this->renderAt(rect, 0.0f, 0.0f, 1.0f, 1.0f, window_dims);
    }

    auto setPosition(SRect rect, float tex_loc_x0, float tex_loc_y0,
                     float tex_loc_x1, float tex_loc_y1, Dims window_dims)
        -> void {
        glBindVertexArray(this->vao);

        SLocation rect_ul = rect.ul;
        SLocation rect_ur = rect.ur();
        SLocation rect_bl = rect.bl();
        SLocation rect_br = rect.br();

        size_t ww = window_dims.width;
        size_t wh = window_dims.height;

        GLfloat nw_r = toGlLoc(wh - rect_ul.row, wh);
        GLfloat nw_c = toGlLoc(rect_ul.col, ww);
        GLfloat ne_r = toGlLoc(wh - rect_ur.row, wh);
        GLfloat ne_c = toGlLoc(rect_ur.col, ww);
        GLfloat sw_r = toGlLoc(wh - rect_bl.row, wh);
        GLfloat sw_c = toGlLoc(rect_bl.col, ww);
        GLfloat se_r = toGlLoc(wh - rect_br.row, wh);
        GLfloat se_c = toGlLoc(rect_br.col, ww);

        GLfloat data[] = {
            nw_c, nw_r, tex_loc_x0, tex_loc_y0, //
            sw_c, sw_r, tex_loc_x0, tex_loc_y1, //
            ne_c, ne_r, tex_loc_x1, tex_loc_y0, //
            se_c, se_r, tex_loc_x1, tex_loc_y1, //
        };

        glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    }

    auto setPosition(SRect rect, Dims window_dims) -> void {
        this->setPosition(rect, 0.0f, 0.0f, 1.0f, 1.0f, window_dims);
    }

    auto swapTexture(Texture2D &other) -> void {
        std::swap(this->texture, other);
    }

    auto withTexture(Texture2D &texture) -> WithGuard {
        this->swapTexture(texture);
        return WithGuard{*this, texture};
    }
};
