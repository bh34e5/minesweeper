#pragma once

#include "dirutils.cc"
#include "op.cc"
#include "ref.cc"
#include "slice.cc"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

enum CellType {
    number,
    mine,
};

enum CellDisplayType {
    hidden,
    value,
    flag,
    maybe_flag,
};

struct Cell {
    CellType type;
    CellDisplayType display_type;
    unsigned char number; // if type == number, the associated value (0-8)
};

struct Grid {
    struct Row {
        Slice<Cell> cells;
        auto operator[](size_t col) -> Cell & { return this->cells[col]; }
    };

    struct Neighbor {
        size_t row;
        size_t col;
        Ref<Cell> cell;
    };

    struct RowIterator {
        Grid &grid;
        size_t row_ind;

        auto next() -> Op<Row> {
            if (this->row_ind >= grid.dims.height) {
                return Op<Row>::empty();
            }
            return this->grid[this->row_ind++];
        }
    };

    struct NeighborIterator {
        Grid &grid;
        ::NeighborIterator iterator;

        auto next() -> Op<Neighbor> {
            Op<::Location> neighbor_op = this->iterator.next();
            if (!neighbor_op.valid) {
                return Op<Neighbor>::empty();
            }

            ::Location n = neighbor_op.get();
            return Neighbor{n.row, n.col, this->grid[n.row][n.col]};
        }
    };

    Slice<Cell> cells;
    Dims dims;

    explicit Grid(Slice<Cell> cells, Dims dims) : cells(cells), dims(dims) {}

    inline auto operator[](size_t row) -> Row {
        size_t row_start = row * this->dims.width;
        size_t row_end = row_start + this->dims.width;
        return Row{this->cells.slice(row_start, row_end)};
    }

    inline auto rowIterator() -> RowIterator { return RowIterator{*this, 0}; };

    inline auto neighborIterator(size_t row, size_t col) -> NeighborIterator {
        return NeighborIterator{
            *this, ::NeighborIterator{Location{row, col}, this->dims, 0}};
    };
};

auto uncoverSelfAndNeighbors(Grid grid, size_t row, size_t col) -> void {
    Cell &cell = grid[row][col];
    if (cell.display_type != CellDisplayType::hidden) {
        return;
    }

    switch (cell.type) {
    case number: {
        cell.display_type = CellDisplayType::value;
        if (cell.number == 0) {
            auto neighbor_op = Op<Grid::Neighbor>::empty();
            auto neighbor_it = grid.neighborIterator(row, col);
            while ((neighbor_op = neighbor_it.next()).valid) {
                Grid::Neighbor neighbor = neighbor_op.get();
                uncoverSelfAndNeighbors(grid, neighbor.row, neighbor.col);
            }
        }
    } break;
    case mine: {
        // keep it hidden
    } break;
    }
}

auto generateGrid(size_t width, size_t height, size_t mine_count,
                  size_t start_r, size_t start_c) -> Op<Grid> {
    assert((width * height) > 0 && "Invalid dimensions");
    assert(start_r < width && "Invalid start row");
    assert(start_c < height && "Invalid start column");

    Location start_loc{start_r, start_c};
    Dims dims{width, height};
    size_t neighbor_count = neighborCount(start_loc, dims);
    assert(mine_count < ((width * height) - neighbor_count) &&
           "Invalid mine count");

    size_t cell_count = width * height;
    Cell *cells = new Cell[cell_count];
    if (cells == nullptr) {
        return Op<Grid>::empty();
    }

    Grid grid{Slice<Cell>{cells, cell_count}, dims};

    // initialize cells
    for (Cell &cell : grid.cells) {
        cell = Cell{CellType::number, CellDisplayType::hidden, 0};
    }

    // fill mines
    size_t remaining_mines = mine_count;
    while (remaining_mines > 0) {
        size_t ind = rand() % cell_count;
        size_t row = ind / width;
        size_t col = ind % width;

        Cell &cur = grid[row][col];

        bool current_mine = cur.type == CellType::mine;
        bool current_start = row == start_r && col == start_c;
        bool current_neighbor = isNeighbor(row, col, start_r, start_c);

        if (current_mine || current_start || current_neighbor) {
            continue;
        }

        cur.type = CellType::mine;
        --remaining_mines;

        auto neighbor_op = Op<Grid::Neighbor>::empty();
        auto neighbor_it = grid.neighborIterator(row, col);
        while ((neighbor_op = neighbor_it.next()).valid) {
            Cell &cell = neighbor_op.get().cell;
            if (cell.type == CellType::number) {
                ++cell.number;
            }
        }
    }

    // uncover initial click
    uncoverSelfAndNeighbors(grid, start_r, start_c);

    return grid;
}

auto gridSolved(Grid grid) -> bool {
    for (Cell cell : grid.cells) {
        if (cell.type == CellType::number &&
            cell.display_type != CellDisplayType::value)
            return false;
    }
    return true;
}

auto printCellValue(Cell cell) -> void {
    switch (cell.type) {
    case number: {
        if (cell.number == 0) {
            printf(" ");
        } else {
            printf("%c", cell.number + '0'); // number must be 0-8
        }
    } break;
    case mine: {
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
            case hidden: {
                printf("_");
            } break;
            case value: {
                printCellValue(cell);
            } break;
            case flag: {
                printf("F");
            } break;
            case maybe_flag: {
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

auto printGrid(Grid grid, bool internal = false) -> void {
    if (internal) {
        printGridInternal(grid);
    } else {
        printGridExternal(grid);
    }
}
