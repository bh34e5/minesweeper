#pragma once

#include "dirutils.cc"
#include "grid.cc"

#include <sys/types.h>

auto pat_121(Grid grid, size_t row, size_t col) -> bool {
    size_t pat_width = 3;
    size_t pat_height = 3;

    if ((col + pat_width) > grid.dims.width) {
        return false;
    }
    if ((row + pat_height) > grid.dims.height) {
        return false;
    }

    Cell &c_0_0 = grid[row + 0][col + 0];
    Cell &c_0_1 = grid[row + 0][col + 1];
    Cell &c_0_2 = grid[row + 0][col + 2];
    Cell &c_1_0 = grid[row + 1][col + 0];
    Cell &c_1_1 = grid[row + 1][col + 1];
    Cell &c_1_2 = grid[row + 1][col + 2];
    Cell &c_2_0 = grid[row + 2][col + 0];
    Cell &c_2_1 = grid[row + 2][col + 1];
    Cell &c_2_2 = grid[row + 2][col + 2];

    // check pattern match

    if (c_0_0.display_type != CellDisplayType::cdt_value ||
        c_0_0.type != CellType::ct_number) {
        return false;
    }

    if (c_0_1.display_type != CellDisplayType::cdt_value ||
        c_0_1.type != CellType::ct_number) {
        return false;
    }

    if (c_0_2.display_type != CellDisplayType::cdt_value ||
        c_0_2.type != CellType::ct_number) {
        return false;
    }

    if (c_1_0.display_type != CellDisplayType::cdt_value ||
        c_1_0.type != CellType::ct_number || c_1_0.number != 1) {
        return false;
    }

    if (c_1_1.display_type != CellDisplayType::cdt_value ||
        c_1_1.type != CellType::ct_number || c_1_1.number != 2) {
        return false;
    }

    if (c_1_2.display_type != CellDisplayType::cdt_value ||
        c_1_2.type != CellType::ct_number || c_1_2.number != 1) {
        return false;
    }

    // pattern has been matched

    bool did_work = false;

    if (c_2_0.display_type == CellDisplayType::cdt_hidden ||
        c_2_0.display_type == CellDisplayType::cdt_maybe_flag) {
        c_2_0.display_type = CellDisplayType::cdt_flag;
        did_work = true;
    }

    if (c_2_1.display_type == CellDisplayType::cdt_maybe_flag ||
        c_2_1.display_type == CellDisplayType::cdt_hidden) {
        // mark as hidden to remove possible maybe_flag
        c_2_1.display_type = CellDisplayType::cdt_hidden;
        uncoverSelfAndNeighbors(grid, Location{row + 2, col + 1});
        did_work = true;
    }

    if (c_2_2.display_type == CellDisplayType::cdt_hidden ||
        c_2_2.display_type == CellDisplayType::cdt_maybe_flag) {
        c_2_2.display_type = CellDisplayType::cdt_flag;
        did_work = true;
    }

    return did_work;
}
