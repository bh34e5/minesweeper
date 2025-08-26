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

    auto renderTextBaseline(Location loc, StrSlice text, Dims window_dims)
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

            Location char_loc_ul{static_cast<size_t>(q.y0),
                                 static_cast<size_t>(q.x0)};
            Location char_loc_br{static_cast<size_t>(q.y1),
                                 static_cast<size_t>(q.x1)};

            Dims char_dims = Dims::fromRect(char_loc_ul, char_loc_br);

            this->quad_program.renderAt(char_loc_ul, char_dims, q.s0, q.t0,
                                        q.s1, q.t1, window_dims);
        }
    }

    auto renderText(Location loc, StrSlice text, Dims window_dims) -> void {
        size_t new_row = loc.row + this->pixel_height;
        this->renderTextBaseline(Location{new_row, loc.col}, text, window_dims);
    }
};
