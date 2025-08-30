#pragma once

#include "arena.cc"
#include "dirutils.cc"
#include "dlist.cc"
#include "grid.cc"
#include "slice.cc"
#include "solver.cc"

#include <sys/types.h>

struct CellOptions {
    Location root;
    Slice<Location> mine_options;

    CellOptions() : root(Location{0, 0}), mine_options(Slice<Location>{}) {}
    CellOptions(Location root, Slice<Location> mine_options)
        : root(root), mine_options(mine_options) {}
};

struct OptionsKeeper {
    DList<CellOptions> options;

    auto addOptions(CellOptions ops) -> void { this->options.push(ops); }

    auto removeOptions(CellOptions ops) -> void {
        for (size_t i = 0; i < this->options.len; ++i) {
            if (this->options.at(i).root.eql(ops.root)) {
                this->options.swapRemove(i);
                break;
            }
        }
    }
};

struct OneOfAwareRule {
    static auto onEpochStart(Grid grid, void *data) -> void {
        auto rule = static_cast<OneOfAwareRule *>(data);
        return rule->onStart(grid);
    }

    static auto onEpochFinish(Grid grid, void *data) -> void {
        auto rule = static_cast<OneOfAwareRule *>(data);
        return rule->onFinish(grid);
    }

    static auto applyRule(Grid grid, size_t row, size_t col, void *data)
        -> bool {
        auto rule = static_cast<OneOfAwareRule *>(data);
        return rule->apply(grid, row, col);
    }

    Arena &rule_arena;
    OptionsKeeper keeper;
    size_t epoch_start_len;

    OneOfAwareRule(Arena &rule_arena)
        : rule_arena(rule_arena), keeper({}), epoch_start_len(rule_arena.len) {}

    auto registerRule(GridSolver &solver) -> void {
        solver.registerRule(
            GridSolver::Rule{applyRule, onEpochStart, onEpochFinish, this});
    }

    auto onStart(Grid grid) -> void {
        for (Cell &cell : grid.cells) {
            if (cell.eff_number != 1) {
                continue;
            }

            size_t loc_count = 0;

            { // new scope so I can reuse the iterator names
                auto neighbor_op = Op<Grid::Neighbor>::empty();
                auto neighbor_it = grid.neighborIterator(cell);
                while ((neighbor_op = neighbor_it.next()).valid) {
                    Grid::Neighbor neighbor = neighbor_op.get();
                    if ((*neighbor.cell).display_type ==
                        CellDisplayType::cdt_hidden) {
                        ++loc_count;
                    }
                }
            }

            Location *loc_ptr = this->rule_arena.pushTN<Location>(loc_count);
            loc_count = 0;

            { // new scope so I can reuse the iterator names
                auto neighbor_op = Op<Grid::Neighbor>::empty();
                auto neighbor_it = grid.neighborIterator(cell);
                while ((neighbor_op = neighbor_it.next()).valid) {
                    Grid::Neighbor neighbor = neighbor_op.get();
                    if ((*neighbor.cell).display_type ==
                        CellDisplayType::cdt_hidden) {
                        loc_ptr[loc_count++] = neighbor.loc;
                    }
                }
            }

            this->keeper.addOptions(CellOptions{
                grid.cellLocation(cell), Slice<Location>{loc_ptr, loc_count}});
        }
    }

    auto onFinish(Grid grid) -> void {
        this->keeper.options.clearRetainCapacity();
        this->rule_arena.reset(this->epoch_start_len);
    }

    auto apply(Grid grid, size_t row, size_t col) -> bool {
        Location cur_loc{row, col};
        Cell cur = grid[cur_loc];
        if (cur.display_type != CellDisplayType::cdt_value ||
            cur.type != CellType::ct_number) {
            return false;
        }

        return this->applyInner(grid, cur_loc, this->keeper.options.slice(),
                                Slice<CellOptions>{});
    }

    auto applyInner(Grid grid, Location cur_loc,
                    Slice<CellOptions> remaining_ops,
                    Slice<CellOptions> applied_ops) -> bool {
        size_t mark = this->rule_arena.len;
        bool did_work = false;

        if (remaining_ops.len == 0) {
            Cell &cell = grid[cur_loc];

            did_work = flagPossibleCells(grid, cell, applied_ops) || did_work;
            did_work = revealPossibleCells(grid, cell, applied_ops) || did_work;

            return did_work;
        }

        for (size_t rem_idx = 0; rem_idx < remaining_ops.len; ++rem_idx) {
            CellOptions next_op = remaining_ops[rem_idx];

            if (!isValidOp(next_op, cur_loc, applied_ops, grid.dims)) {
                continue;
            }

            CellOptions *pushed = this->rule_arena.pushT<CellOptions>(next_op);
            Slice<CellOptions> inner_slice{pushed - applied_ops.len,
                                           applied_ops.len + 1};

            did_work = this->applyInner(grid, cur_loc,
                                        remaining_ops.slice(rem_idx + 1),
                                        inner_slice) ||
                       did_work;

            this->rule_arena.reset(mark);
        }

        return did_work;
    }

    auto flagPossibleCells(Grid grid, Cell &cell,
                           Slice<CellOptions> applied_ops) -> bool {
        if (cell.eff_number == applied_ops.len) {
            return false;
        }

        size_t hidden_count = 0;

        auto neighbor_op = Op<Grid::Neighbor>::empty();
        auto neighbor_it = grid.neighborIterator(cell);
        while ((neighbor_op = neighbor_it.next()).valid) {
            Grid::Neighbor neighbor = neighbor_op.get();
            if ((*neighbor.cell).display_type != CellDisplayType::cdt_hidden) {
                continue;
            }

            if (isSelectedOp(neighbor.loc, applied_ops)) {
                continue;
            }

            ++hidden_count;
        }

        if (cell.eff_number - applied_ops.len == hidden_count) {
            bool did_work = false;

            auto neighbor_op = Op<Grid::Neighbor>::empty();
            auto neighbor_it = grid.neighborIterator(cell);
            while ((neighbor_op = neighbor_it.next()).valid) {
                Grid::Neighbor neighbor = neighbor_op.get();
                if ((*neighbor.cell).display_type !=
                    CellDisplayType::cdt_hidden) {
                    continue;
                }

                if (isSelectedOp(neighbor.loc, applied_ops)) {
                    continue;
                }

                flagCell(grid, neighbor.loc);
                did_work = true;
            }

            return did_work;
        }

        return false;
    }

    auto revealPossibleCells(Grid grid, Cell &cell,
                             Slice<CellOptions> applied_ops) -> bool {
        bool did_work = false;

        if (cell.eff_number == applied_ops.len) {
            auto neighbor_op = Op<Grid::Neighbor>::empty();
            auto neighbor_it = grid.neighborIterator(cell);
            while ((neighbor_op = neighbor_it.next()).valid) {
                Grid::Neighbor neighbor = neighbor_op.get();
                if ((*neighbor.cell).display_type !=
                    CellDisplayType::cdt_hidden) {
                    continue;
                }

                if (isSelectedOp(neighbor.loc, applied_ops)) {
                    continue;
                }

                uncoverSelfAndNeighbors(grid, neighbor.loc);
                did_work = true;
            }
        }
        return did_work;
    }

    auto isSelectedOp(Location loc, Slice<CellOptions> applied_ops) -> bool {
        for (auto &applied_op : applied_ops) {
            for (auto &mine_op : applied_op.mine_options) {
                if (mine_op.eql(loc)) {
                    return true;
                }
            }
        }
        return false;
    }

    auto isValidOp(CellOptions next_op, Location cur_loc,
                   Slice<CellOptions> applied_ops, Dims grid_dims) -> bool {
        // early exit if the cell is too far away from the current location
        if (!rootCellReachesCurrent(next_op.root, cur_loc)) {
            return false;
        }

        // if there is any overlap, skip this since there could be only one mine
        // in the overlapping areas
        for (auto &applied_op : applied_ops) {
            if (locationsOverlap(next_op, applied_op)) {
                return false;
            }
        }

        // ensure all the possibilities touch the current cell
        for (auto &mine_loc : next_op.mine_options) {
            bool found = false;

            auto neighbor_op = Op<Location>::empty();
            auto neighbor_it = NeighborIterator{cur_loc, grid_dims};
            while ((neighbor_op = neighbor_it.next()).valid) {
                if (neighbor_op.get().eql(mine_loc)) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                return false;
            }
        }

        return true;
    }

    auto rootCellReachesCurrent(Location root_loc, Location cur_loc) -> bool {
        return absDiff(root_loc.row, cur_loc.row) <= 2 &&
               absDiff(root_loc.col, cur_loc.col) <= 2;
    }

    auto locationsOverlap(CellOptions lhs, CellOptions rhs) -> bool {
        for (Location lhs_loc : lhs.mine_options) {
            for (Location rhs_loc : rhs.mine_options) {
                if (lhs_loc.eql(rhs_loc)) {
                    return true;
                }
            }
        }
        return false;
    }
};
