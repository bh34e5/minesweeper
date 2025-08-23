#include "arena.cc"
#include "dirutils.cc"
#include "graphics.cc"
#include "grid.cc"
#include "one_of_aware.cc"
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

    if (cur.eff_number == 0) {
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

    if (cur.eff_number == hidden_count) {
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

auto testGrid() -> void {
    srand(0);

    Arena arena{MEGABYTES(10)};

    Grid grid = generateGrid(arena, Dims{9, 18}, 34, Location{5, 5});

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
}

void handle_error(int error, char const *description) {
    fprintf(stderr, "GLFW Error (%d)): %s\n", error, description);
}

struct WindowContext {
    typedef Window<WindowContext> ThisWindow;

    QuadProgram quad_program;
    Dims tex_dims;

    WindowContext(ThisWindow &window, Dims tex_dims)
        : quad_program(window.quadProgram()), tex_dims(tex_dims) {
        glFrontFace(GL_CCW);
        glClearColor(0.0, 0.0, 0.0, 0.0);

        this->quad_program.texture.grayscale(120, tex_dims);
    }

    void framebufferSizeCallback(ThisWindow &window, int width, int height) {
        assert(width >= 0 && "Invalid width");
        assert(height >= 0 && "Invalid height");

        glViewport(0, 0, width, height);

        window.renderNow();
    }

    auto render(ThisWindow &window) -> void {
        window.renderQuad(this->quad_program, Location{15, 15}, this->tex_dims);
    }
};

int main() {
    testGrid();

    GLFW glfw{&handle_error};

    Window<WindowContext> window{800, 600, "Hello, world", Dims{100, 500}};
    window.setPin();
    window.setPos(500, 500);
    window.show();

    double last_time = glfwGetTime();
    while (!window.shouldClose()) {
        window.render();

        glfwPollEvents();

        double next_time = glfwGetTime();

        double delta_time = next_time - last_time;
        last_time = next_time;

        if (window.isKeyPressed(GLFW_KEY_Q)) {
            window.close();
        }
    }

    return 0;
}
