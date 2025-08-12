#pragma once

#include "../dirutils.cc"
#include "../grid.cc"

#include <sys/types.h>

struct Pattern_pat_13_1 {
    Cell &c_0_0;
    Cell &c_0_1;
    Cell &c_0_2;
    Cell &c_1_0;
    Cell &c_1_1;
    Cell &c_1_2;
    Cell &c_2_0;
    Cell &c_2_1;
    Cell &c_2_2;
};

auto checkPattern_pat_13_1(Grid grid, size_t row, size_t col, Pattern_pat_13_1 pat) -> bool {
    // check pattern match

    if ((pat.c_0_0.display_type != CellDisplayType::cdt_value || pat.c_0_0.type != CellType::ct_number) && (pat.c_0_0.display_type != CellDisplayType::cdt_flag)) {
        return false;
    }

    if ((pat.c_0_1.display_type != CellDisplayType::cdt_value || pat.c_0_1.type != CellType::ct_number) && (pat.c_0_1.display_type != CellDisplayType::cdt_flag)) {
        return false;
    }

    if ((pat.c_0_2.display_type != CellDisplayType::cdt_value || pat.c_0_2.type != CellType::ct_number) && (pat.c_0_2.display_type != CellDisplayType::cdt_flag)) {
        return false;
    }

    if (pat.c_1_0.display_type != CellDisplayType::cdt_value || pat.c_1_0.type != CellType::ct_number || pat.c_1_0.eff_number != 1) {
        return false;
    }

    if (pat.c_1_1.display_type != CellDisplayType::cdt_value || pat.c_1_1.type != CellType::ct_number || pat.c_1_1.eff_number != 3) {
        return false;
    }

    if (pat.c_1_2.display_type != CellDisplayType::cdt_hidden) {
        return false;
    }

    if (pat.c_2_0.display_type != CellDisplayType::cdt_hidden) {
        return false;
    }

    if (pat.c_2_1.display_type != CellDisplayType::cdt_hidden) {
        return false;
    }

    if (pat.c_2_2.display_type != CellDisplayType::cdt_hidden) {
        return false;
    }

    // pattern has been matched

    bool did_work = false;

    if (pat.c_1_2.display_type == CellDisplayType::cdt_hidden || pat.c_1_2.display_type == CellDisplayType::cdt_maybe_flag) {
        flagCell(grid, pat.c_1_2);
        did_work = true;
    }

    if (pat.c_2_2.display_type == CellDisplayType::cdt_hidden || pat.c_2_2.display_type == CellDisplayType::cdt_maybe_flag) {
        flagCell(grid, pat.c_2_2);
        did_work = true;
    }

    return did_work;
}

auto pat_13_1_0n(Grid grid, size_t row, size_t col) -> bool {
    size_t pat_width = 3;
    size_t pat_height = 3;

    if ((col + pat_width) > grid.dims.width) {
        return false;
    }
    if ((row + pat_height) > grid.dims.height) {
        return false;
    }

    Pattern_pat_13_1 pat{
        grid[row + 0][col + 0],
        grid[row + 0][col + 1],
        grid[row + 0][col + 2],
        grid[row + 1][col + 0],
        grid[row + 1][col + 1],
        grid[row + 1][col + 2],
        grid[row + 2][col + 0],
        grid[row + 2][col + 1],
        grid[row + 2][col + 2],
    };

    return checkPattern_pat_13_1(grid, row, col, pat);
};

auto pat_13_1_90n(Grid grid, size_t row, size_t col) -> bool {
    size_t pat_width = 3;
    size_t pat_height = 3;

    if ((col + pat_width) > grid.dims.width) {
        return false;
    }
    if ((row + pat_height) > grid.dims.height) {
        return false;
    }

    Pattern_pat_13_1 pat{
        grid[row + 0][col + 2],
        grid[row + 1][col + 2],
        grid[row + 2][col + 2],
        grid[row + 0][col + 1],
        grid[row + 1][col + 1],
        grid[row + 2][col + 1],
        grid[row + 0][col + 0],
        grid[row + 1][col + 0],
        grid[row + 2][col + 0],
    };

    return checkPattern_pat_13_1(grid, row, col, pat);
};

auto pat_13_1_180n(Grid grid, size_t row, size_t col) -> bool {
    size_t pat_width = 3;
    size_t pat_height = 3;

    if ((col + pat_width) > grid.dims.width) {
        return false;
    }
    if ((row + pat_height) > grid.dims.height) {
        return false;
    }

    Pattern_pat_13_1 pat{
        grid[row + 2][col + 2],
        grid[row + 2][col + 1],
        grid[row + 2][col + 0],
        grid[row + 1][col + 2],
        grid[row + 1][col + 1],
        grid[row + 1][col + 0],
        grid[row + 0][col + 2],
        grid[row + 0][col + 1],
        grid[row + 0][col + 0],
    };

    return checkPattern_pat_13_1(grid, row, col, pat);
};

auto pat_13_1_270n(Grid grid, size_t row, size_t col) -> bool {
    size_t pat_width = 3;
    size_t pat_height = 3;

    if ((col + pat_width) > grid.dims.width) {
        return false;
    }
    if ((row + pat_height) > grid.dims.height) {
        return false;
    }

    Pattern_pat_13_1 pat{
        grid[row + 2][col + 0],
        grid[row + 1][col + 0],
        grid[row + 0][col + 0],
        grid[row + 2][col + 1],
        grid[row + 1][col + 1],
        grid[row + 0][col + 1],
        grid[row + 2][col + 2],
        grid[row + 1][col + 2],
        grid[row + 0][col + 2],
    };

    return checkPattern_pat_13_1(grid, row, col, pat);
};

auto pat_13_1_0r(Grid grid, size_t row, size_t col) -> bool {
    size_t pat_width = 3;
    size_t pat_height = 3;

    if ((col + pat_width) > grid.dims.width) {
        return false;
    }
    if ((row + pat_height) > grid.dims.height) {
        return false;
    }

    Pattern_pat_13_1 pat{
        grid[row + 0][col + 2],
        grid[row + 0][col + 1],
        grid[row + 0][col + 0],
        grid[row + 1][col + 2],
        grid[row + 1][col + 1],
        grid[row + 1][col + 0],
        grid[row + 2][col + 2],
        grid[row + 2][col + 1],
        grid[row + 2][col + 0],
    };

    return checkPattern_pat_13_1(grid, row, col, pat);
};

auto pat_13_1_90r(Grid grid, size_t row, size_t col) -> bool {
    size_t pat_width = 3;
    size_t pat_height = 3;

    if ((col + pat_width) > grid.dims.width) {
        return false;
    }
    if ((row + pat_height) > grid.dims.height) {
        return false;
    }

    Pattern_pat_13_1 pat{
        grid[row + 0][col + 0],
        grid[row + 1][col + 0],
        grid[row + 2][col + 0],
        grid[row + 0][col + 1],
        grid[row + 1][col + 1],
        grid[row + 2][col + 1],
        grid[row + 0][col + 2],
        grid[row + 1][col + 2],
        grid[row + 2][col + 2],
    };

    return checkPattern_pat_13_1(grid, row, col, pat);
};

auto pat_13_1_180r(Grid grid, size_t row, size_t col) -> bool {
    size_t pat_width = 3;
    size_t pat_height = 3;

    if ((col + pat_width) > grid.dims.width) {
        return false;
    }
    if ((row + pat_height) > grid.dims.height) {
        return false;
    }

    Pattern_pat_13_1 pat{
        grid[row + 2][col + 0],
        grid[row + 2][col + 1],
        grid[row + 2][col + 2],
        grid[row + 1][col + 0],
        grid[row + 1][col + 1],
        grid[row + 1][col + 2],
        grid[row + 0][col + 0],
        grid[row + 0][col + 1],
        grid[row + 0][col + 2],
    };

    return checkPattern_pat_13_1(grid, row, col, pat);
};

auto pat_13_1_270r(Grid grid, size_t row, size_t col) -> bool {
    size_t pat_width = 3;
    size_t pat_height = 3;

    if ((col + pat_width) > grid.dims.width) {
        return false;
    }
    if ((row + pat_height) > grid.dims.height) {
        return false;
    }

    Pattern_pat_13_1 pat{
        grid[row + 2][col + 2],
        grid[row + 1][col + 2],
        grid[row + 0][col + 2],
        grid[row + 2][col + 1],
        grid[row + 1][col + 1],
        grid[row + 0][col + 1],
        grid[row + 2][col + 0],
        grid[row + 1][col + 0],
        grid[row + 0][col + 0],
    };

    return checkPattern_pat_13_1(grid, row, col, pat);
};

auto pat_13_1(Grid grid, size_t row, size_t col) -> bool {
    bool did_work_0n = pat_13_1_0n(grid, row, col);
    bool did_work_90n = pat_13_1_90n(grid, row, col);
    bool did_work_180n = pat_13_1_180n(grid, row, col);
    bool did_work_270n = pat_13_1_270n(grid, row, col);

    bool did_work_0r = pat_13_1_0r(grid, row, col);
    bool did_work_90r = pat_13_1_90r(grid, row, col);
    bool did_work_180r = pat_13_1_180r(grid, row, col);
    bool did_work_270r = pat_13_1_270r(grid, row, col);

    return did_work_0n || did_work_180n || did_work_90n || did_work_270n ||
           did_work_0r || did_work_180r || did_work_90r || did_work_270r;
}
