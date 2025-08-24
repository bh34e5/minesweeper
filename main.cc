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

struct Context {
    typedef Window<Context> ThisWindow;

    struct Event {
        enum Type {
            et_empty,
            et_mouse_press,
            et_mouse_release,
        };

        Type type;
        Event *next;
        Event *prev;

        Event(Type type) : type(type), next(nullptr), prev(nullptr) {}

        static auto initSentinel(Event &ev) -> void {
            ev.next = &ev;
            ev.prev = &ev;
        }

        auto enqueue(Event *ev) -> void {
            ev->prev = this->prev;
            ev->next = this;

            ev->prev->next = ev;
            ev->next->prev = ev;
        }

        auto dequeue() -> Event * {
            if (this->next == this) {
                return nullptr;
            }

            Event *ev = this->next;
            ev->prev->next = ev->next;
            ev->next->prev = ev->prev;

            ev->prev = nullptr;
            ev->next = nullptr;

            return ev;
        }
    };

    struct Element {
        enum Type {
            et_empty,
            et_generate_grid,
        };

        Type type;
        Location loc;
        Dims dims;
        Element *next;

        Element(Type type) : type(type), loc{}, dims{}, next(nullptr) {}
        Element(Type type, Location loc, Dims dims)
            : type(type), loc(loc), dims(dims), next(nullptr) {}

        auto push(Element *el) -> void {
            el->next = this->next;
            this->next = el;
        }

        auto pop() -> Element * {
            if (this->next == nullptr) {
                return nullptr;
            }

            Element *el = this->next;
            this->next = el->next;
            el->next = nullptr;

            return el;
        }

        auto contains(Location loc) -> bool {
            if (loc.row < this->loc.row) {
                return false;
            }
            if (loc.col < this->loc.col) {
                return false;
            }
            if (loc.row > this->loc.row + this->dims.height) {
                return false;
            }
            if (loc.col > this->loc.col + this->dims.width) {
                return false;
            }
            return true;
        }
    };

    Arena arena;
    size_t mark;

    QuadProgram quad_program;
    Texture2D button;
    Texture2D background;

    bool mouse_down;
    Location down_mouse_pos;
    Event sentinel;
    Element elements;

    Arena grid_arena;
    Grid grid;

    Context(ThisWindow &window, Arena &&arena, Arena &&grid_arena)
        : arena(std::move(arena)), mark(this->arena.len),
          quad_program(window.quadProgram()), button{}, background{},
          mouse_down(false), down_mouse_pos{}, sentinel{Event::Type::et_empty},
          elements{Element::Type::et_empty}, grid_arena(std::move(grid_arena)),
          grid{} {
        glFrontFace(GL_CCW);
        glClearColor(0.0, 0.0, 0.0, 0.0);

        this->button.grayscale(204, Dims{1, 1});
        this->background.grayscale(120, Dims{1, 1});

        Event::initSentinel(this->sentinel);
    }

    void framebufferSizeCallback(ThisWindow &window, int width, int height) {
        assert(width >= 0 && "Invalid width");
        assert(height >= 0 && "Invalid height");

        glViewport(0, 0, width, height);

        window.renderNow();
    }

    void mouseButtonCallback(ThisWindow &window, int button, int action, int) {
        if (button == GLFW_MOUSE_BUTTON_1) {
            if (action == GLFW_PRESS) {
                Event *ev =
                    this->arena.pushT(Event{Event::Type::et_mouse_press});

                this->mouse_down = true;
                this->down_mouse_pos = window.getClampedMouseLocation();
                this->sentinel.enqueue(ev);
            } else {
                Event *ev =
                    this->arena.pushT(Event{Event::Type::et_mouse_release});

                this->mouse_down = false;
                this->sentinel.enqueue(ev);
            }
        }
    }

    auto render(ThisWindow &window) -> void {
        this->arena.reset(this->mark);
        Event::initSentinel(this->sentinel);

        Dims window_dims = window.getDims();
        size_t w = window_dims.width;
        size_t h = window_dims.height;

        size_t padding = 15;
        Dims render_dims{
            (w > (2 * padding)) ? (w - 2 * padding) : 0,
            (h > (2 * padding)) ? (h - 2 * padding) : 0,
        };

        Location render_loc{
            (w - render_dims.width) / 2,
            (h - render_dims.height) / 2,
        };

        {
            auto guard = this->quad_program.withTexture(this->background);
            this->renderQuad(window, render_loc, render_dims);
        }

        if (this->grid.dims.area() > 0) {
            size_t cell_padding = 1;
            size_t r_padding = this->grid.dims.height * 2 * cell_padding;
            size_t c_padding = this->grid.dims.width * 2 * cell_padding;

            size_t cell_width =
                (render_dims.width - c_padding) / this->grid.dims.width;
            size_t cell_height =
                (render_dims.height - r_padding) / this->grid.dims.height;

            Dims cell_dims{cell_width, cell_height};

            for (size_t r = 0; r < this->grid.dims.height; ++r) {
                size_t r_pos =
                    r * (cell_height + 2 * cell_padding) + cell_padding;

                for (size_t c = 0; c < this->grid.dims.width; ++c) {
                    size_t c_pos =
                        c * (cell_width + 2 * cell_padding) + cell_padding;

                    Location cell_loc{render_loc.row + r_pos,
                                      render_loc.col + c_pos};

                    this->renderCell(window, this->grid[r][c], cell_loc,
                                     cell_dims);
                }
            }
        } else {
            Dims button_dims{100, 50};
            Location button_loc{
                (h - button_dims.height) / 2,
                (w - button_dims.width) / 2,
            };

            auto guard = this->quad_program.withTexture(this->button);
            this->renderQuad(window, button_loc, button_dims);

            Element *el = this->arena.pushT(Element{
                Element::Type::et_generate_grid, button_loc, button_dims});

            this->elements.push(el);
        }
    }

    auto renderCell(ThisWindow &window, Cell &cell, Location cell_loc,
                    Dims cell_dims) -> void {
        auto guard = this->quad_program.withTexture(this->button);
        this->renderQuad(window, cell_loc, cell_dims);
    }

    auto processEvents(ThisWindow &window) -> void {
        Event *ev = nullptr;
        while ((ev = this->sentinel.dequeue()) != nullptr) {
            switch (ev->type) {
            case Event::Type::et_empty:
                break;
            case Event::Type::et_mouse_press: {
                Element *el = &this->elements;

                bool keep_processing_elems = true;
                while ((el = el->next) != nullptr && keep_processing_elems) {
                    if (el->contains(this->down_mouse_pos)) {
                        switch (el->type) {
                        case Element::Type::et_empty:
                            break;
                        case Element::Type::et_generate_grid: {
                            keep_processing_elems = false;

                            printf("Generating grid\n");

                            this->grid =
                                generateGrid(this->grid_arena, Dims{10, 10}, 15,
                                             Location{5, 5});

                            window.needs_render = true;
                        } break;
                        }
                    }
                }
            }
            case Event::Type::et_mouse_release:
                break;
            }
        }
    }

    auto renderQuad(ThisWindow &window, Location loc, Dims dims) -> void {
        window.renderQuad(this->quad_program, loc, dims);
    }
};

int main() {
    testGrid();

    GLFW glfw{&handle_error};

    Arena window_arena{KILOBYTES(4)};
    Arena grid_arena{KILOBYTES(4)};

    Window<Context> window{800, 600, "Hello, world", std::move(window_arena),
                           std::move(grid_arena)};
    window.setPin();
    window.setPos(500, 500);
    window.show();

    double last_time = glfwGetTime();
    while (!window.shouldClose()) {
        window.render();
        window.processEvents();

        double next_time = glfwGetTime();

        double delta_time = next_time - last_time;
        last_time = next_time;

        if (window.isKeyPressed(GLFW_KEY_Q)) {
            window.close();
        }
    }

    return 0;
}
