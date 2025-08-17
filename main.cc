#include "arena.cc"
#include "dirutils.cc"
#include "grid.cc"
#include "op.cc"
#include "slice.cc"
#include "solver.cc"

// auto generated
#include "generated/generated.cc"

#include <stdlib.h>
#include <sys/types.h>

#define LEN(arr) (sizeof(arr) / sizeof(*arr))

auto flag_remaining_cells(Grid grid, size_t row, size_t col, void *) -> bool {
    Cell cur = grid[row][col];
    if (cur.display_type != CellDisplayType::cdt_value ||
        cur.type != CellType::ct_number) {
        return false;
    }

    size_t eff_mine_count = cur.eff_number;
    if (eff_mine_count == 0) {
        return false;
    }

    size_t hidden_count = 0;

    auto neighbor_op = Op<Grid::Neighbor>::empty();
    auto neighbor_it = grid.neighborIterator(row, col);
    while ((neighbor_op = neighbor_it.next()).valid) {
        Cell cell = neighbor_op.get().cell;
        if (cell.display_type == CellDisplayType::cdt_hidden) {
            ++hidden_count;
        }
    }

    if (eff_mine_count == hidden_count) {
        // flag all hidden cells
        bool did_work = false;

        auto neighbor_op = Op<Grid::Neighbor>::empty();
        auto neighbor_it = grid.neighborIterator(row, col);
        while ((neighbor_op = neighbor_it.next()).valid) {
            Grid::Neighbor neighbor = neighbor_op.get();
            Cell cell = neighbor.cell;
            if (cell.display_type == CellDisplayType::cdt_hidden) {
                flagCell(grid, neighbor.loc);
                did_work = true;
            }
        }
        return did_work;
    }

    return false;
};

auto show_hidden_cells(Grid grid, size_t row, size_t col, void *) -> bool {
    Cell cur = grid[row][col];
    if (cur.display_type != CellDisplayType::cdt_value ||
        cur.type != CellType::ct_number) {
        return false;
    }

    size_t mine_count = cur.number;
    if (mine_count == 0) {
        return false;
    }

    size_t eff_mine_count = cur.eff_number;
    if (eff_mine_count == 0) {
        // show all hidden cells
        bool did_work = false;

        auto neighbor_op = Op<Grid::Neighbor>::empty();
        auto neighbor_it = grid.neighborIterator(row, col);
        while ((neighbor_op = neighbor_it.next()).valid) {
            Grid::Neighbor neighbor = neighbor_op.get();
            Cell cell = neighbor.cell;
            if (cell.display_type == CellDisplayType::cdt_hidden) {
                uncoverSelfAndNeighbors(grid, neighbor.loc);
                did_work = true;
            }
        }
        return did_work;
    }

    return false;
}

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
        : rule_arena(rule_arena), keeper({}), epoch_start_len(0) {}

    auto registerRule(GridSolver &solver) -> void {
        solver.registerRule(
            GridSolver::Rule{applyRule, onEpochStart, onEpochFinish, this});
    }

    auto onStart(Grid grid) -> void {
        this->epoch_start_len = this->rule_arena.len;

        for (Cell &cell : grid.cells) {
            if (cell.eff_number != 1) {
                continue;
            }

            size_t loc_count = 0;

            { // new scope so I can reuse the iterator names
                auto neighbor_op = Op<Grid::Neighbor>::empty();
                auto neighbor_it = grid.neighborIterator(cell);
                while ((neighbor_op = neighbor_it.next()).valid) {
                    Grid::Neighbor nbor = neighbor_op.get();
                    if ((*nbor.cell).display_type ==
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
                    Grid::Neighbor nbor = neighbor_op.get();
                    if ((*nbor.cell).display_type ==
                        CellDisplayType::cdt_hidden) {
                        loc_ptr[loc_count++] = nbor.loc;
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
        this->epoch_start_len = 0;
    }

    auto apply(Grid grid, size_t row, size_t col) -> bool {
        Location cur_loc{row, col};
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
            if (cell.eff_number == applied_ops.len) {
                auto neighbor_op = Op<Grid::Neighbor>::empty();
                auto neighbor_it = grid.neighborIterator(cell);
                while ((neighbor_op = neighbor_it.next()).valid) {
                    Grid::Neighbor nbor = neighbor_op.get();
                    if ((*nbor.cell).display_type !=
                        CellDisplayType::cdt_hidden) {
                        continue;
                    }

                    bool valid_loc = true;
                    for (auto &applied_op : applied_ops) {
                        for (auto &mine_op : applied_op.mine_options) {
                            if (mine_op.eql(nbor.loc)) {
                                valid_loc = false;
                                break;
                            }
                        }

                        if (!valid_loc) {
                            break;
                        }
                    }

                    if (!valid_loc) {
                        continue;
                    }

                    uncoverSelfAndNeighbors(grid, nbor.loc);
                    did_work = true;
                }
            }
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

            bool inner_did_work = this->applyInner(
                grid, cur_loc, remaining_ops.slice(rem_idx + 1), inner_slice);

            this->rule_arena.reset(mark);

            did_work = did_work || inner_did_work;
        }
        return did_work;
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

int main() {
    srand(0);

    Arena arena{MEGABYTES(10)};

    Op<Grid> g_op = generateGrid(arena, Dims{9, 18}, 34, Location{5, 5});
    if (!g_op.valid) {
        return 1;
    }

    Grid grid = g_op.get();

    GridSolver solver{};
    solver.registerRule(GridSolver::Rule{flag_remaining_cells});
    solver.registerRule(GridSolver::Rule{show_hidden_cells});
    registerPatterns(solver);

    Arena one_aware_arena{MEGABYTES(10)};
    OneOfAwareRule one_of_aware_rule{one_aware_arena};
    one_of_aware_rule.registerRule(solver);

    bool is_solvable = solver.solvable(grid);

    printGrid(grid);
    printf("The grid %s solvable\n", is_solvable ? "is" : "is not");

    return 0;
}
