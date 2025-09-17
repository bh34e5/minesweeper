#pragma once

#include "grid.h"

#include "arena.cc"
#include "dirutils.cc"
#include "op.cc"
#include "slice.cc"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

auto flagCell(Grid *grid, Location loc) -> void {
    Cell &cell = (*grid)[loc];
    if (cell.display_type == CellDisplayType::cdt_flag) {
        return;
    }

    cell.display_type = CellDisplayType::cdt_flag;

    auto neighbor_op = Op<Grid::Neighbor>::empty();
    auto neighbor_it = grid->neighborIterator(loc);
    while ((neighbor_op = neighbor_it.next()).valid) {
        --(*neighbor_op.get().cell).eff_number;
    }
}

auto flagCell(Grid *grid, Cell *cell) -> void {
    Location cell_loc = grid->cellLocation(cell);
    flagCell(grid, cell_loc);
}

auto unflagCell(Grid *grid, Location loc) -> void {
    Cell &cell = (*grid)[loc];
    if (cell.display_type != CellDisplayType::cdt_flag) {
        return;
    }

    cell.display_type = CellDisplayType::cdt_hidden;

    auto neighbor_op = Op<Grid::Neighbor>::empty();
    auto neighbor_it = grid->neighborIterator(loc);
    while ((neighbor_op = neighbor_it.next()).valid) {
        ++(*neighbor_op.get().cell).eff_number;
    }
}

auto unflagCell(Grid *grid, Cell *cell) -> void {
    Location cell_loc = grid->cellLocation(cell);
    unflagCell(grid, cell_loc);
}

auto resetGrid(Grid *grid) -> void {
    for (auto &cell : grid->cells) {
        switch (cell.display_type) {
        case cdt_flag: {
            unflagCell(grid, &cell);
        } break;
        case cdt_value:
        case cdt_maybe_flag: {
            cell.display_type = CellDisplayType::cdt_hidden;
        } break;
        case cdt_hidden: {
            // already hidden, do nothing
        } break;
        }
    }
}

auto uncoverSelfAndNeighbors(Grid *grid, Location loc) -> void {
    Cell &cell = (*grid)[loc];
    if (cell.display_type != CellDisplayType::cdt_hidden) {
        return;
    }

    switch (cell.type) {
    case ct_number: {
        cell.display_type = CellDisplayType::cdt_value;
        if (cell.number == 0) {
            auto neighbor_op = Op<Grid::Neighbor>::empty();
            auto neighbor_it = grid->neighborIterator(loc);
            while ((neighbor_op = neighbor_it.next()).valid) {
                Grid::Neighbor neighbor = neighbor_op.get();
                uncoverSelfAndNeighbors(grid, neighbor.loc);
            }
        }
    } break;
    case ct_mine: {
        // keep it hidden
    } break;
    }
}

auto uncoverSelfAndNeighbors(Grid *grid, Cell *cell) -> void {
    Location cell_loc = grid->cellLocation(cell);
    uncoverSelfAndNeighbors(grid, cell_loc);
}

auto generateGrid(Arena *arena, Dims dims, size_t mine_count,
                  Location start_loc) -> Grid {
    size_t cell_count = dims.area();
    assert(cell_count > 0 && "Invalid dimensions");
    assert(start_loc.row < dims.height && "Invalid start row");
    assert(start_loc.col < dims.width && "Invalid start column");

    size_t neighbor_count = neighborCount(start_loc, dims);
    assert(mine_count < (cell_count - neighbor_count) && "Invalid mine count");

    Cell *cells = arena->pushTN<Cell>(cell_count);
    Grid grid{Slice<Cell>{cells, cell_count}, dims, mine_count};

    // initialize cells
    for (Cell &cell : grid.cells) {
        cell = Cell{CellType::ct_number, CellDisplayType::cdt_hidden, 0, 0};
    }

    // fill mines
    size_t remaining_mines = mine_count;
    while (remaining_mines > 0) {
        size_t ind = rand() % cell_count;
        size_t row = ind / dims.width;
        size_t col = ind % dims.width;

        Cell &cur = grid[row][col];

        bool current_mine = cur.type == CellType::ct_mine;
        bool current_start = row == start_loc.row && col == start_loc.col;
        bool current_neighbor = isNeighbor(Location{row, col}, start_loc);

        if (current_mine || current_start || current_neighbor) {
            continue;
        }

        cur.type = CellType::ct_mine;
        --remaining_mines;

        auto neighbor_op = Op<Grid::Neighbor>::empty();
        auto neighbor_it = grid.neighborIterator(row, col);
        while ((neighbor_op = neighbor_it.next()).valid) {
            Cell *cell = neighbor_op.get().cell;
            if (cell->type == CellType::ct_number) {
                ++cell->number;
                ++cell->eff_number;
            }
        }
    }

    // uncover initial click
    uncoverSelfAndNeighbors(&grid, start_loc);

    return grid;
}

auto gridSolved(Grid grid) -> bool {
    for (Cell cell : grid.cells) {
        if (cell.type == CellType::ct_number &&
            cell.display_type != CellDisplayType::cdt_value) {
            return false;
        }
    }
    return true;
}

auto gridLost(Grid grid) -> bool {
    for (Cell cell : grid.cells) {
        if (cell.display_type == CellDisplayType::cdt_value &&
            cell.type == CellType::ct_mine) {
            return true;
        }
    }
    return false;
}

auto losingCell(Grid grid) -> Location {
    for (Cell &cell : grid.cells) {
        if (cell.display_type == CellDisplayType::cdt_value &&
            cell.type == CellType::ct_mine) {
            return grid.cellLocation(&cell);
        }
    }
    assert(0 && "Grid is not losing");
}

auto gridRemainingFlags(Grid grid) -> long {
    assert(grid.mine_count <= static_cast<size_t>(LONG_MAX) &&
           "Mine count exceeds long max");

    long remaining = static_cast<long>(grid.mine_count);
    for (auto &cell : grid.cells) {
        if (cell.display_type == CellDisplayType::cdt_flag) {
            remaining -= 1;
        }
    }

    return remaining;
}

auto printCellValue(Cell cell) -> void {
    switch (cell.type) {
    case ct_number: {
        if (cell.number == 0) {
            printf(" ");
        } else {
            printf("%c", cell.number + '0'); // number must be 0-8
        }
    } break;
    case ct_mine: {
        printf("*");
    } break;
    }
}

auto printGridInternal(Grid grid) -> void {
    for (size_t i = 0; i < grid.dims.width; ++i) {
        if (i == 0) {
            printf("+");
        }
        printf("---+");
    }
    printf("\n");

    auto row_op = Op<Grid::Row>::empty();
    auto row_it = grid.rowIterator();
    while ((row_op = row_it.next()).valid) {
        Grid::Row row = row_op.get();

        bool first = true;
        for (Cell cell : row.cells) {
            if (first) {
                printf("|");
                first = false;
            }
            printf(" ");
            printCellValue(cell);
            printf(" |");
        }
        printf("\n");

        for (size_t i = 0; i < grid.dims.width; ++i) {
            if (i == 0) {
                printf("+");
            }
            printf("---+");
        }
        printf("\n");
    }
}

auto printGridExternal(Grid grid) -> void {
    for (size_t i = 0; i < grid.dims.width; ++i) {
        if (i == 0) {
            printf("+");
        }
        printf("---+");
    }
    printf("\n");

    auto row_op = Op<Grid::Row>::empty();
    auto row_it = grid.rowIterator();
    while ((row_op = row_it.next()).valid) {
        Grid::Row row = row_op.get();

        bool first = true;
        for (Cell cell : row.cells) {
            if (first) {
                printf("|");
                first = false;
            }
            printf(" ");

            switch (cell.display_type) {
            case cdt_hidden: {
                printf("_");
            } break;
            case cdt_value: {
                printCellValue(cell);
            } break;
            case cdt_flag: {
                printf("F");
            } break;
            case cdt_maybe_flag: {
                printf("?");
            } break;
            }

            printf(" |");
        }
        printf("\n");

        for (size_t i = 0; i < grid.dims.width; ++i) {
            if (i == 0) {
                printf("+");
            }
            printf("---+");
        }
        printf("\n");
    }
}

auto printGrid(Grid grid, bool internal) -> void {
    if (internal) {
        printGridInternal(grid);
    } else {
        printGridExternal(grid);
    }
}
