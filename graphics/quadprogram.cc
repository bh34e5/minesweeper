#pragma once

#include "../dirutils.cc"
#include "../slice.cc"
#include "common.cc"
#include "gl.cc"
#include "shader.cc"
#include "texture2d.cc"

#include <sys/types.h>

struct QuadProgram {
    Program program;

    GLint n_pos;
    GLint n_tex_p;
    GLint tex;

    GLuint vao;
    GLuint vbo;

    auto renderAt(SRect rect, float tex_loc_x0, float tex_loc_y0,
                  float tex_loc_x1, float tex_loc_y1, Dims window_dims,
                  Texture2D texture) -> void {
        this->program.useProgram();
        this->setPosition(rect, tex_loc_x0, tex_loc_y0, tex_loc_x1, tex_loc_y1,
                          window_dims);

        glActiveTexture(GL_TEXTURE0);
        glUniform1i(this->tex, 0); // GL_TEXTURE0
        texture.useTex();          // bind texture to GL_TEXTURE0

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    auto renderAt(SRect rect, Dims window_dims, Texture2D texture) -> void {
        this->renderAt(rect, 0.0f, 0.0f, 1.0f, 1.0f, window_dims, texture);
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
};

auto makeQuadProgram(Program p, GLint n_pos, GLint n_tex_p, GLint tex)
    -> QuadProgram {
    QuadProgram q{p, n_pos, n_tex_p, tex};

    glGenVertexArrays(1, &q.vao);
    glGenBuffers(1, &q.vbo);

    q.program.useProgram();
    glBindVertexArray(q.vao);
    glBindBuffer(GL_ARRAY_BUFFER, q.vbo);
    glEnableVertexAttribArray(q.n_pos);
    glEnableVertexAttribArray(q.n_tex_p);

    glVertexAttribPointer(q.n_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          (void const *)(0 * sizeof(GLfloat)));
    glVertexAttribPointer(q.n_tex_p, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          (void const *)(2 * sizeof(GLfloat)));

    return q;
}

auto deleteQuadProgram(QuadProgram *quad_program) -> void {
    glDeleteVertexArrays(1, &quad_program->vao);
    glDeleteBuffers(1, &quad_program->vbo);

    deleteProgram(&quad_program->program);
}
