#pragma once

#include "../dirutils.cc"
#include "../strslice.cc"
#include "../utils.cc"
#include "common.cc"
#include "gl.cc"
#include "quadprogram.cc"
#include "stb_truetype.cc"
#include "utils.cc"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>

auto scaleFloat(float val, ssize_t text_val, ssize_t base_val,
                ssize_t text_range, ssize_t base_range) -> ssize_t {
    ssize_t offset = val - base_val;
    ssize_t scaled = offset * base_range / text_range;
    return scaled + text_val;
}

struct BakedFont {
    float pixel_height;
    Dims bmp_dims;
    stbtt_bakedchar chardata[96]; // printable characters

    Texture2D texture;

    Program program;

    GLint n_pos;
    GLint n_tex_p;
    GLint tex;
    GLint font_color;

    GLuint vao;
    GLuint vbo;

    auto setColor(Color color) -> void {
        this->program.useProgram();

        GLfloat color_vec[3]{
            glColor(color.r),
            glColor(color.g),
            glColor(color.b),
        };

        glUniform3fv(this->font_color, 1, color_vec);
    }

    auto quadProgram() -> QuadProgram {
        QuadProgram q{this->program, this->n_pos, this->n_tex_p,
                      this->tex,     this->vao,   this->vbo};
        return q;
    }

    auto renderTextBaseline(SLocation loc, StrSlice text, Dims window_dims)
        -> void {
        float xpos = loc.col;
        float ypos = loc.row;
        for (char c : text) {
            // fallback to Space
            char render_c = inRange<char>(32, c, 127) ? c : 32;

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(this->chardata, this->bmp_dims.width,
                               this->bmp_dims.height, render_c - 32, &xpos,
                               &ypos, &q, 1);

            SLocation char_loc_ul{static_cast<ssize_t>(q.y0),
                                  static_cast<ssize_t>(q.x0)};
            SLocation char_loc_br{static_cast<ssize_t>(q.y1),
                                  static_cast<ssize_t>(q.x1)};

            SRect char_rect = SRect::fromCorners(char_loc_ul, char_loc_br);

            this->quadProgram().renderAt(char_rect, q.s0, q.t0, q.s1, q.t1,
                                         window_dims, this->texture);
        }
    }

    auto renderText(SLocation loc, StrSlice text, Dims window_dims) -> void {
        SRect text_rect = this->getTextRect(text);
        SLocation render_loc{loc.row - text_rect.ul.row,
                             loc.col - text_rect.ul.col};

        this->renderTextBaseline(render_loc, text, window_dims);
    }

    auto renderText(SRect rect, StrSlice text, Dims window_dims) -> void {
        SRect text_rect = this->getTextRect(text);
        SRect text_bounds{rect.ul, shrinkToFit(rect.dims, text_rect.dims)};

        this->renderTextInBounds(rect, text_rect, text_bounds, text,
                                 window_dims);
    }

    auto renderCenteredText(SRect rect, StrSlice text, Dims window_dims)
        -> void {
        SRect text_rect = this->getTextRect(text);
        SRect text_bounds = centerInShrink(rect, text_rect.dims);

        this->renderTextInBounds(rect, text_rect, text_bounds, text,
                                 window_dims);
    }

    auto renderTextInBounds(SRect rect, SRect text_rect, SRect text_bounds,
                            StrSlice text, Dims window_dims) -> void {
        float xpos = 0.0f;
        float ypos = 0.0f;

        for (char c : text) {
            // fallback to Space
            char render_c = inRange<char>(32, c, 127) ? c : 32;

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(this->chardata, this->bmp_dims.width,
                               this->bmp_dims.height, render_c - 32, &xpos,
                               &ypos, &q, 1);

            ssize_t scaled_x0 =
                scaleFloat(q.x0, text_bounds.ul.col, text_rect.ul.col,
                           text_bounds.dims.width, text_rect.dims.width);
            ssize_t scaled_x1 =
                scaleFloat(q.x1, text_bounds.ul.col, text_rect.ul.col,
                           text_bounds.dims.width, text_rect.dims.width);

            ssize_t scaled_y0 =
                scaleFloat(q.y0, text_bounds.ul.row, text_rect.ul.row,
                           text_bounds.dims.height, text_rect.dims.height);
            ssize_t scaled_y1 =
                scaleFloat(q.y1, text_bounds.ul.row, text_rect.ul.row,
                           text_bounds.dims.height, text_rect.dims.height);

            SRect char_rect =
                SRect::fromCorners(SLocation{scaled_y0, scaled_x0},
                                   SLocation{scaled_y1, scaled_x1});

            // NOTE(bhester): we don't scale the texture coordinates because we
            // want to render the whole character, just in a different rectangle
            this->quadProgram().renderAt(char_rect, q.s0, q.t0, q.s1, q.t1,
                                         window_dims, this->texture);
        }
    }

    auto getTextRect(StrSlice text) -> SRect {
        float xpos = 0.0f;
        float ypos = 0.0f;

        float min_x = +FLT_MAX;
        float min_y = +FLT_MAX;
        float max_x = -FLT_MAX;
        float max_y = -FLT_MAX;

        for (char c : text) {
            // fallback to Space
            char render_c = inRange<char>(32, c, 127) ? c : 32;

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(this->chardata, this->bmp_dims.width,
                               this->bmp_dims.height, render_c - 32, &xpos,
                               &ypos, &q, 1);

            min_x = fminf(min_x, q.x0);
            min_y = fminf(min_y, q.y0);
            max_x = fmaxf(min_x, q.x1);
            max_y = fmaxf(min_y, q.y1);
        }

        // since we are casting to unsigned, make sure neither dimension is
        // negative

        SLocation text_bb_ul{static_cast<ssize_t>(min_y),
                             static_cast<ssize_t>(min_x)};
        SLocation text_bb_br{static_cast<ssize_t>(max_y),
                             static_cast<ssize_t>(max_x)};

        return SRect::fromCorners(text_bb_ul, text_bb_br);
    }

    auto getTextDims(StrSlice text) -> Dims {
        SRect text_rect = this->getTextRect(text);
        return text_rect.dims;
    }

    auto getLineHeightTextDims(StrSlice text) -> Dims {
        SRect text_rect = this->getTextRect(text);

        assert(text_rect.ul.row <= 0);
        if (-text_rect.ul.row < this->pixel_height) {
            ssize_t row_diff = this->pixel_height + text_rect.ul.row;

            text_rect.ul.row -= row_diff;
            text_rect.dims.height += row_diff;
        }
        return text_rect.dims;
    }
};

auto makeBakedFont(float pixel_height, Dims bmp_dims,
                   stbtt_bakedchar chardata[96], Texture2D texture, Program p,
                   GLint n_pos, GLint n_tex_p, GLint tex, GLint font_color)
    -> BakedFont {
    BakedFont bf{pixel_height, bmp_dims, {},  texture,   p,
                 n_pos,        n_tex_p,  tex, font_color};

    memcpy(bf.chardata, chardata, sizeof(bf.chardata));

    glGenVertexArrays(1, &bf.vao);
    glGenBuffers(1, &bf.vbo);

    bf.program.useProgram();
    glBindVertexArray(bf.vao);
    glBindBuffer(GL_ARRAY_BUFFER, bf.vbo);
    glEnableVertexAttribArray(bf.n_pos);
    glEnableVertexAttribArray(bf.n_tex_p);

    glVertexAttribPointer(bf.n_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          (void const *)(0 * sizeof(GLfloat)));
    glVertexAttribPointer(bf.n_tex_p, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat),
                          (void const *)(2 * sizeof(GLfloat)));

    return bf;
}

auto deleteBakedFont(BakedFont *baked_font) -> void {
    glDeleteVertexArrays(1, &baked_font->vao);
    glDeleteBuffers(1, &baked_font->vbo);

    deleteProgram(&baked_font->program);
    deleteTexture(&baked_font->texture);
}
