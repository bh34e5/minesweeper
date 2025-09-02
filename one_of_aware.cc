#pragma once

#include "arena.cc"
#include "dirutils.cc"
#include "grid.cc"
#include "linkedlist.cc"
#include "slice.cc"
#include "solver.cc"

#include <sys/types.h>

struct CellOptions {
    Location root;
    Slice<Location> mine_options;
};

struct OptionsKeeper {
    LinkedList<CellOptions> options_sentinel;

    auto addOptions(Arena *arena, CellOptions ops) -> void {
        LinkedList<CellOptions> *ll =
            arena->pushT<LinkedList<CellOptions>>({ops});

        this->options_sentinel.enqueue(ll);
    }

    auto removeOptions(CellOptions ops) -> void {
        LinkedList<CellOptions> *ll = &this->options_sentinel;
        while ((ll = ll->next) != &this->options_sentinel) {
            if (ll->val.root.eql(ops.root)) {
                ll->prev->next = ll->next;
                ll->next->prev = ll->prev;
            }
        }
    }
};

struct OneOfAwareRule {
    static auto onEpochStart(Grid *grid, void *data) -> void {
        auto rule = static_cast<OneOfAwareRule *>(data);
        return rule->onStart(grid);
    }

    static auto onEpochFinish(Grid *grid, void *data) -> void {
        auto rule = static_cast<OneOfAwareRule *>(data);
        return rule->onFinish(grid);
    }

    static auto applyRule(Grid *grid, size_t row, size_t col, void *data)
        -> bool {
        auto rule = static_cast<OneOfAwareRule *>(data);
        return rule->apply(grid, row, col);
    }

    Arena arena;
    OptionsKeeper keeper;

    auto registerRule(Arena *arena, GridSolver *solver) -> void {
        GridSolver::Rule rule = makeRule(applyRule, onEpochStart, onEpochFinish,
                                         this, STR_SLICE("one_of_aware"));

        solver->registerRule(arena, rule);
    }

    auto onStart(Grid *grid) -> void {
        LinkedList<CellOptions>::initSentinel(&this->keeper.options_sentinel);

        for (Cell &cell : grid->cells) {
            if (cell.eff_number != 1) {
                continue;
            }

            size_t loc_count = 0;

            { // new scope so I can reuse the iterator names
                auto neighbor_op = Op<Grid::Neighbor>::empty();
                auto neighbor_it = grid->neighborIterator(&cell);
                while ((neighbor_op = neighbor_it.next()).valid) {
                    Grid::Neighbor neighbor = neighbor_op.get();
                    if ((*neighbor.cell).display_type ==
                        CellDisplayType::cdt_hidden) {
                        ++loc_count;
                    }
                }
            }

            Location *loc_ptr = this->arena.pushTN<Location>(loc_count);
            loc_count = 0;

            { // new scope so I can reuse the iterator names
                auto neighbor_op = Op<Grid::Neighbor>::empty();
                auto neighbor_it = grid->neighborIterator(&cell);
                while ((neighbor_op = neighbor_it.next()).valid) {
                    Grid::Neighbor neighbor = neighbor_op.get();
                    if ((*neighbor.cell).display_type ==
                        CellDisplayType::cdt_hidden) {
                        loc_ptr[loc_count++] = neighbor.loc;
                    }
                }
            }

            this->keeper.addOptions(
                &this->arena, CellOptions{grid->cellLocation(&cell),
                                          Slice<Location>{loc_ptr, loc_count}});
        }
    }

    auto onFinish(Grid *grid) -> void { this->arena.reset(0); }

    auto apply(Grid *grid, size_t row, size_t col) -> bool {
        Location cur_loc{row, col};
        Cell cur = (*grid)[cur_loc];
        if (cur.display_type != CellDisplayType::cdt_value ||
            cur.type != CellType::ct_number) {
            return false;
        }

        return this->applyInner(grid, cur_loc,
                                this->keeper.options_sentinel.next,
                                Slice<CellOptions>{});
    }

    auto applyInner(Grid *grid, Location cur_loc,
                    LinkedList<CellOptions> *remaining_ops,
                    Slice<CellOptions> applied_ops) -> bool {
        size_t mark = this->arena.len;
        if (remaining_ops == &this->keeper.options_sentinel) {
            Cell &cell = (*grid)[cur_loc];

            bool flag_work = flagPossibleCells(grid, &cell, applied_ops);
            bool reveal_work = revealPossibleCells(grid, &cell, applied_ops);

            return flag_work || reveal_work;
        }

        bool did_work = false;

        // prev, since I am about to get the next on the first iteration
        LinkedList<CellOptions> *rem = remaining_ops->prev;
        while ((rem = rem->next) != &this->keeper.options_sentinel) {
            CellOptions next_op = rem->val;

            if (!isValidOp(next_op, cur_loc, applied_ops, grid->dims)) {
                continue;
            }

            CellOptions *pushed = this->arena.pushT<CellOptions>(next_op);
            Slice<CellOptions> inner_slice{pushed - applied_ops.len,
                                           applied_ops.len + 1};

            bool inner_work =
                this->applyInner(grid, cur_loc, rem->next, inner_slice);

            did_work = inner_work || did_work;

            this->arena.reset(mark);
        }

        return did_work;
    }

    auto flagPossibleCells(Grid *grid, Cell *cell,
                           Slice<CellOptions> applied_ops) -> bool {
        if (cell->eff_number == applied_ops.len) {
            return false;
        }

        size_t hidden_count = 0;

        auto neighbor_op = Op<Grid::Neighbor>::empty();
        auto neighbor_it = grid->neighborIterator(cell);
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

        if (cell->eff_number - applied_ops.len == hidden_count) {
            bool did_work = false;

            auto neighbor_op = Op<Grid::Neighbor>::empty();
            auto neighbor_it = grid->neighborIterator(cell);
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

    auto revealPossibleCells(Grid *grid, Cell *cell,
                             Slice<CellOptions> applied_ops) -> bool {
        bool did_work = false;

        if (cell->eff_number == applied_ops.len) {
            auto neighbor_op = Op<Grid::Neighbor>::empty();
            auto neighbor_it = grid->neighborIterator(cell);
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

auto makeOneOfAware(size_t arena_capacity) -> OneOfAwareRule {
    OneOfAwareRule rule{};
    rule.arena = makeArena(arena_capacity);
    return rule;
}

auto deleteOneOfAware(OneOfAwareRule *rule) -> void {
    freeArena(&rule->arena);
    rule->arena = Arena{};
}
