#pragma once

#include "../dirutils.cc"
#include "../strslice.cc"
#include "../utils.cc"
#include "common.cc"
#include "gl.cc"
#include "quadprogram.cc"
#include "stb_truetype.cc"

#include <string.h>
#include <sys/types.h>

struct BakedFont {
    float pixel_height;
    Dims bmp_dims;
    stbtt_bakedchar chardata[96]; // printable characters
    QuadProgram quad_program;
    GLint font_color;

    BakedFont(float pixel_height, Dims bmp_dims, stbtt_bakedchar chardata[96],
              QuadProgram &&quad_progam, GLint font_color)
        : pixel_height(pixel_height), bmp_dims{bmp_dims}, chardata{},
          quad_program{std::move(quad_progam)}, font_color(font_color) {
        memcpy(this->chardata, chardata, 96 * sizeof(*chardata));
    }

    auto setColor(Color color) -> void {
        this->quad_program.program.useProgram();

        GLfloat color_vec[3]{
            glColor(color.r),
            glColor(color.g),
            glColor(color.b),
        };

        glUniform3fv(this->font_color, 1, color_vec);
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

            this->quad_program.renderAt(char_rect, q.s0, q.t0, q.s1, q.t1,
                                        window_dims);
        }
    }

    auto renderText(SLocation loc, StrSlice text, Dims window_dims) -> void {
        ssize_t baseline_row = loc.row + this->pixel_height;
        SLocation baseline_loc{baseline_row, loc.col};
        this->renderTextBaseline(baseline_loc, text, window_dims);
    }
};
