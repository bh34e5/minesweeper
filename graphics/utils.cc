#pragma once

#include "../dirutils.cc"

#include <math.h>
#include <sys/types.h>

auto shrinkToFit(Dims base, Dims target) -> Dims {
    double factor = 1.0;

    if (target.width > base.width) {
        factor = fmin(factor, (double)base.width / (double)target.width);
    }

    if (target.height > base.height) {
        factor = fmin(factor, (double)base.height / (double)target.height);
    }

    size_t width_adj = target.width * factor;
    size_t height_adj = target.height * factor;

    return Dims{width_adj, height_adj};
}

auto centerIn(SRect base, Dims target) -> SRect {
    ssize_t row_diff = 0;
    ssize_t col_diff = 0;

    if (target.width > base.dims.width) {
        col_diff = -((ssize_t)(target.width - base.dims.width));
    } else {
        col_diff = base.dims.width - target.width;
    }

    if (target.height > base.dims.height) {
        row_diff = -((ssize_t)(target.height - base.dims.height));
    } else {
        row_diff = base.dims.height - target.height;
    }

    return SRect{
        SLocation{base.ul.row + row_diff / 2, base.ul.col + col_diff / 2},
        target};
}

inline auto centerInShrink(SRect base, Dims target) -> SRect {
    return centerIn(base, shrinkToFit(base.dims, target));
}
