#pragma once

#include "arena.cc"
#include "dirutils.cc"
#include "op.cc"
#include "slice.cc"

#include <stddef.h>

enum CellType {
    ct_number,
    ct_mine,
};

enum CellDisplayType {
    cdt_hidden,
    cdt_value,
    cdt_flag,
    cdt_maybe_flag,
};

struct Cell {
    CellType type;
    CellDisplayType display_type;
    unsigned char number; // if type == number, the associated value (0-8)
    unsigned char eff_number;
};

struct Grid {
    struct Row {
        Slice<Cell> cells;
        auto operator[](size_t col) -> Cell & { return this->cells[col]; }
    };

    struct Neighbor {
        Location loc;
        Cell *cell;
    };

    struct RowIterator {
        Grid *grid;
        size_t row_ind;

        auto next() -> Op<Row> {
            if (this->row_ind >= grid->dims.height) {
                return Op<Row>::empty();
            }
            return (*this->grid)[this->row_ind++];
        }
    };

    struct NeighborIterator {
        Grid *grid;
        ::NeighborIterator iterator;

        auto next() -> Op<Neighbor> {
            Op<::Location> neighbor_op = this->iterator.next();
            if (!neighbor_op.valid) {
                return Op<Neighbor>::empty();
            }

            ::Location loc = neighbor_op.get();
            return Neighbor{loc, &(*this->grid)[loc]};
        }
    };

    Slice<Cell> cells;
    Dims dims;
    size_t mine_count;

    inline auto operator[](size_t row) -> Row {
        size_t row_s = (row + 0) * this->dims.width;
        size_t row_e = (row + 1) * this->dims.width;
        return Row{this->cells.slice(row_s, row_e)};
    }

    inline auto operator[](Location loc) -> Cell & {
        return (*this)[loc.row][loc.col];
    }

    auto cellLocation(Cell *cell) -> Location {
        size_t cell_idx = this->cells.indexOf(cell);

        size_t row = cell_idx / this->dims.width;
        size_t col = cell_idx % this->dims.width;

        return Location{row, col};
    }

    inline auto rowIterator() -> RowIterator { return RowIterator{this, 0}; };

    inline auto neighborIterator(size_t row, size_t col) -> NeighborIterator {
        return NeighborIterator{
            this, ::NeighborIterator{Location{row, col}, this->dims}};
    };

    inline auto neighborIterator(Location loc) -> NeighborIterator {
        return NeighborIterator{this, ::NeighborIterator{loc, this->dims}};
    };

    inline auto neighborIterator(Cell *cell) -> NeighborIterator {
        Location loc = this->cellLocation(cell);
        return this->neighborIterator(loc);
    }
};

auto generateGrid(Arena *arena, Dims dims, size_t mine_count,
                  Location start_loc) -> Grid;
auto resetGrid(Grid *grid) -> void;
auto gridSolved(Grid grid) -> bool;
auto gridLost(Grid grid) -> bool;
auto losingCell(Grid grid) -> Location;
auto gridRemainingFlags(Grid grid) -> long;
auto printGrid(Grid grid, bool internal) -> void;

#define FlagCellLocType(name, grid, loc)                                       \
    auto(name)(Grid * grid, Location loc) -> void
#define FlagCellCellType(name, grid, cell)                                     \
    auto(name)(Grid * grid, Cell * cell) -> void
#define UnflagCellLocType(name, grid, loc)                                     \
    auto(name)(Grid * grid, Location loc) -> void
#define UnflagCellCellType(name, grid, cell)                                   \
    auto(name)(Grid * grid, Cell * cell) -> void
#define UncoverSelfAndNeighborsLocType(name, grid, loc)                        \
    auto(name)(Grid * grid, Location loc) -> void
#define UncoverSelfAndNeighborsCellType(name, grid, cell)                      \
    auto(name)(Grid * grid, Cell * cell) -> void

typedef FlagCellLocType(FlagCellLoc, grid, loc);
typedef FlagCellCellType(FlagCellCell, grid, cell);
typedef UnflagCellLocType(UnflagCellLoc, grid, loc);
typedef UnflagCellCellType(UnflagCellCell, grid, cell);
typedef UncoverSelfAndNeighborsLocType(UncoverSelfAndNeighborsLoc, grid, loc);
typedef UncoverSelfAndNeighborsCellType(UncoverSelfAndNeighborsCell, grid,
                                        cell);

struct GridApi {
    FlagCellLoc *flagCellLoc;
    FlagCellCell *flagCellCell;
    UnflagCellLoc *unflagCellLoc;
    UnflagCellCell *unflagCellCell;
    UncoverSelfAndNeighborsLoc *uncoverSelfAndNeighborsLoc;
    UncoverSelfAndNeighborsCell *uncoverSelfAndNeighborsCell;

    FlagCellLocType(flagCell, grid, loc) { this->flagCellLoc(grid, loc); }
    FlagCellCellType(flagCell, grid, cell) { this->flagCellCell(grid, cell); }
    UnflagCellLocType(unflagCell, grid, loc) { this->unflagCellLoc(grid, loc); }
    UnflagCellCellType(unflagCell, grid, cell) {
        this->unflagCellCell(grid, cell);
    }
    UncoverSelfAndNeighborsLocType(uncoverSelfAndNeighbors, grid, loc) {
        this->uncoverSelfAndNeighborsLoc(grid, loc);
    }
    UncoverSelfAndNeighborsCellType(uncoverSelfAndNeighbors, grid, cell) {
        this->uncoverSelfAndNeighborsCell(grid, cell);
    }
};
