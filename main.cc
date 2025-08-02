#include "dirutils.cc"
#include "grid.cc"
#include "op.cc"
#include "slice.cc"

// auto generated
#include "pat_121.cc"

#include <stdlib.h>
#include <sys/types.h>

#define LEN(arr) (sizeof(arr) / sizeof(*arr))

struct GridSolver {
    struct Rule {
        typedef auto(Apply)(Grid grid, size_t row, size_t col) -> bool;

        Apply *apply;
    };

    Slice<Rule> rules;

    auto solvable(Grid grid) -> bool {
        bool did_work = true;
        while (did_work) {
            did_work = false;
            for (size_t r = 0; r < grid.dims.height; ++r) {
                for (size_t c = 0; c < grid.dims.width; ++c) {
                    for (auto &rule : this->rules) {
                        bool rule_did_work = rule.apply(grid, r, c);
                        did_work = did_work || rule_did_work;
                    }
                }
            }
        }

        return gridSolved(grid);
    }
};

auto flag_remaining_cells(Grid grid, size_t row, size_t col) -> bool {
    Cell cur = grid[row][col];
    if (cur.display_type != CellDisplayType::cdt_value ||
        cur.type != CellType::ct_number) {
        return false;
    }

    size_t mine_count = cur.number;
    if (mine_count == 0) {
        return false;
    }

    size_t hidden_count = 0;
    size_t flag_count = 0;

    auto neighbor_op = Op<Grid::Neighbor>::empty();
    auto neighbor_it = grid.neighborIterator(row, col);
    while ((neighbor_op = neighbor_it.next()).valid) {
        Cell cell = neighbor_op.get().cell;
        if (cell.display_type == CellDisplayType::cdt_hidden) {
            ++hidden_count;
        } else if (cell.display_type == CellDisplayType::cdt_flag) {
            ++flag_count;
        }
    }

    if ((mine_count - flag_count) == hidden_count) {
        // flag all hidden cells
        bool did_work = false;

        auto neighbor_op = Op<Grid::Neighbor>::empty();
        auto neighbor_it = grid.neighborIterator(row, col);
        while ((neighbor_op = neighbor_it.next()).valid) {
            Grid::Neighbor neighbor = neighbor_op.get();
            Cell &cell = neighbor.cell;
            if (cell.display_type == CellDisplayType::cdt_hidden) {
                cell.display_type = CellDisplayType::cdt_flag;
                did_work = true;
            }
        }
        return did_work;
    }

    return false;
};

auto show_hidden_cells(Grid grid, size_t row, size_t col) -> bool {
    Cell cur = grid[row][col];
    if (cur.display_type != CellDisplayType::cdt_value ||
        cur.type != CellType::ct_number) {
        return false;
    }

    size_t mine_count = cur.number;
    if (mine_count == 0) {
        return false;
    }

    size_t flag_count = 0;

    auto neighbor_op = Op<Grid::Neighbor>::empty();
    auto neighbor_it = grid.neighborIterator(row, col);
    while ((neighbor_op = neighbor_it.next()).valid) {
        Cell cell = neighbor_op.get().cell;
        if (cell.display_type == CellDisplayType::cdt_flag) {
            ++flag_count;
        }
    }

    if (mine_count == flag_count) {
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

int main() {
    srand(0);

    Op<Grid> g_op = generateGrid(Dims{9, 18}, 34, Location{5, 5});
    if (!g_op.valid) {
        return 1;
    }

    Grid grid = g_op.get();
    GridSolver::Rule rules[] = {GridSolver::Rule{&flag_remaining_cells},
                                GridSolver::Rule{&show_hidden_cells},
                                GridSolver::Rule{&pat_121}};
    GridSolver solver{Slice<GridSolver::Rule>{rules, LEN(rules)}};

    bool is_solvable = solver.solvable(grid);

    printGrid(grid);
    printf("The grid %s solvable\n", is_solvable ? "is" : "is not");

    delete[] grid.cells.ptr;
    return 0;
}
