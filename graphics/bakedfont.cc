#pragma once

#include "../dirutils.cc"
#include "../strslice.cc"
#include "../utils.cc"
#include "common.cc"
#include "gl.cc"
#include "quadprogram.cc"
#include "stb_truetype.cc"

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
        SRect text_rect = this->getTextRect(text);
        SLocation render_loc{loc.row - text_rect.ul.row,
                             loc.col - text_rect.ul.col};

        this->renderTextBaseline(render_loc, text, window_dims);
    }

    auto renderText(SRect rect, StrSlice text, Dims window_dims) -> void {
        SRect text_rect = this->getTextRect(text);
        SRect text_bounds = this->getTextRenderRect(rect, text_rect);

        this->renderTextInBounds(rect, text_rect, text_bounds, text,
                                 window_dims);
    }

    auto renderCenteredText(SRect rect, StrSlice text, Dims window_dims)
        -> void {
        SRect text_rect = this->getTextRect(text);
        SRect text_bounds = this->getCenteredTextRenderRect(rect, text_rect);

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
            this->quad_program.renderAt(char_rect, q.s0, q.t0, q.s1, q.t1,
                                        window_dims);
        }
    }

    auto getTextRenderRect(SRect base, SRect text) -> SRect {
        Dims dims = this->getScaledTextDims(base, text);
        return SRect{SLocation{base.ul.row, base.ul.col}, dims};
    }

    auto getCenteredTextRenderRect(SRect base, SRect text) -> SRect {
        Dims dims = this->getScaledTextDims(base, text);

        // fits inside, center it
        ssize_t x_off = (base.dims.width - dims.width) / 2;
        ssize_t y_off = (base.dims.height - dims.height) / 2;

        return SRect{SLocation{base.ul.row + y_off, base.ul.col + x_off}, dims};
    }

    auto getScaledTextDims(SRect base, SRect text) -> Dims {
        size_t b_width = base.dims.width;
        size_t b_height = base.dims.height;

        size_t t_width = text.dims.width;
        size_t t_height = text.dims.height;

        size_t scaled_width = t_width;
        size_t scaled_height = t_height;

        if (t_width > b_width || t_height > b_height) {
            double scale = 1.0;
            scale = fmin(scale, (double)b_width / (double)t_width);
            scale = fmin(scale, (double)b_height / (double)t_height);

            scaled_width = scale * text.dims.width;
            scaled_height = scale * text.dims.height;
        }

        assert(b_width >= scaled_width && "Width invalid");
        assert(b_height >= scaled_height && "Height invalid");

        return Dims{scaled_width, scaled_height};
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
};
