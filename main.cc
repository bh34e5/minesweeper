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
#include <utility>

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

template <typename T> struct LinkedList {
    T val;
    LinkedList *next;
    LinkedList *prev;

    LinkedList(T val) : val(val), next(nullptr), prev(nullptr) {}
    template <typename... Args>
    LinkedList(Args &&...args)
        : val{std::forward<Args>(args)...}, next(nullptr), prev(nullptr) {}

    static auto initSentinel(LinkedList &ll) -> void {
        ll.next = &ll;
        ll.prev = &ll;
    }

    auto enqueue(LinkedList *ll) -> void {
        ll->prev = this->prev;
        ll->next = this;

        ll->prev->next = ll;
        ll->next->prev = ll;
    }

    auto dequeue() -> LinkedList * {
        if (this->next == this) {
            return nullptr;
        }

        LinkedList *ll = this->next;
        ll->prev->next = ll->next;
        ll->next->prev = ll->prev;

        ll->prev = nullptr;
        ll->next = nullptr;

        return ll;
    }
};

struct Context {
    typedef Window<Context> ThisWindow;

    struct Event {
        enum Type {
            et_empty,
            et_mouse_press,
            et_mouse_release,
        };

        Type type;
    };

    struct Element {
        enum Type {
            et_empty,
            et_background,
            et_grid_cell,
            et_generate_grid_btn,
            et_text,
        };

        Type type;
        Location loc;
        Dims dims;
        StrSlice text;

        Element(Type type) : type(type), loc{}, dims{}, text{} {}
        Element(Type type, Location loc, Dims dims)
            : type(type), loc(loc), dims(dims), text{} {}
        Element(Type type, Location loc, StrSlice text)
            : type(type), loc(loc), dims{}, text(text) {}

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

    typedef LinkedList<Event> LLEvent;
    typedef LinkedList<Element> LLElement;

    Arena arena;
    size_t mark;

    QuadProgram quad_program;
    BakedFont baked_font;
    Texture2D button;
    Texture2D background;

    bool mouse_down;
    Location down_mouse_pos;
    LLEvent ev_sentinel;
    LLElement el_sentinel;

    Arena grid_arena;
    Grid grid;

    static size_t const font_pixel_height = 30;

    Context(ThisWindow &window, Arena &&arena, Arena &&grid_arena)
        : arena(std::move(arena)), mark(this->arena.len),
          quad_program(window.quadProgram()),
          baked_font{window.bakedFont(this->arena, "./fonts/Roboto-Black.ttf",
                                      font_pixel_height)},
          button{}, background{}, mouse_down(false), down_mouse_pos{},
          ev_sentinel{Event::Type::et_empty},
          el_sentinel{Element::Type::et_empty},
          grid_arena(std::move(grid_arena)), grid{} {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glFrontFace(GL_CCW);
        glClearColor(0.0, 0.0, 0.0, 0.0);

        this->button.grayscale(204, Dims{1, 1});
        this->background.grayscale(120, Dims{1, 1});
        this->baked_font.setColor(Color::grayscale(225));

        LLEvent::initSentinel(this->ev_sentinel);
        LLElement::initSentinel(this->el_sentinel);
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
                LLEvent *ev = this->arena.pushT<LLEvent>(
                    Event{Event::Type::et_mouse_press});

                this->mouse_down = true;
                this->down_mouse_pos = window.getClampedMouseLocation();
                this->ev_sentinel.enqueue(ev);
            } else {
                LLEvent *ev = this->arena.pushT<LLEvent>(
                    Event{Event::Type::et_mouse_release});

                this->mouse_down = false;
                this->ev_sentinel.enqueue(ev);
            }
        }
    }

    auto buildScene(ThisWindow &window) -> void {
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

        this->pushElement(
            {Element::Type::et_background, render_loc, render_dims});

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

                    this->pushElement(
                        {Element::Type::et_grid_cell, cell_loc, cell_dims});
                }
            }
        } else {
            Dims button_dims{100, 50};
            Location button_loc{
                (h - button_dims.height) / 2,
                (w - button_dims.width) / 2,
            };

            this->pushElement(
                {Element::Type::et_generate_grid_btn, button_loc, button_dims});

            Location text_loc{render_loc.row + 10, render_loc.col + 10};
            this->pushElement(
                {Element::Type::et_text, text_loc, STR_SLICE("Hi I'm Text")});
        }
    }

    auto renderScene(ThisWindow &window) -> void {
        LLElement *el = &this->el_sentinel;

        while ((el = el->next) != &this->el_sentinel) {
            switch (el->val.type) {
            case Element::Type::et_empty:
                break;
            case Element::Type::et_background: {
                auto guard = this->quad_program.withTexture(this->background);
                this->renderQuad(window, el->val.loc, el->val.dims);
            } break;
            case Element::Type::et_grid_cell: {
                auto guard = this->quad_program.withTexture(this->button);
                this->renderQuad(window, el->val.loc, el->val.dims);
            } break;
            case Element::Type::et_generate_grid_btn: {
                auto guard = this->quad_program.withTexture(this->button);
                this->renderQuad(window, el->val.loc, el->val.dims);
            } break;
            case Element::Type::et_text: {
                this->renderText(window, el->val.loc, el->val.text);
            } break;
            }
        }
    }

    auto render(ThisWindow &window) -> void {
        this->arena.reset(this->mark);
        LLEvent::initSentinel(this->ev_sentinel);
        LLElement::initSentinel(this->el_sentinel);

        buildScene(window);
        renderScene(window);
    }

    auto processEvents(ThisWindow &window) -> void {
        LLEvent *ev = nullptr;
        while ((ev = this->ev_sentinel.dequeue()) != nullptr) {
            switch (ev->val.type) {
            case Event::Type::et_empty:
                break;
            case Event::Type::et_mouse_press: {
                LLElement *el = &this->el_sentinel;

                bool keep_processing_elems = true;
                while ((el = el->prev) != &this->el_sentinel &&
                       keep_processing_elems) {
                    if (el->val.contains(this->down_mouse_pos)) {
                        switch (el->val.type) {
                        case Element::Type::et_empty:
                        case Element::Type::et_background:
                        case Element::Type::et_grid_cell:
                        case Element::Type::et_text:
                            break;
                        case Element::Type::et_generate_grid_btn: {
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

    auto pushElement(Element el) -> void {
        LLElement *ll = this->arena.pushT<LLElement>(el);
        this->el_sentinel.enqueue(ll);
    }

    auto renderQuad(ThisWindow &window, Location loc, Dims dims) -> void {
        window.renderQuad(this->quad_program, loc, dims);
    }

    auto renderText(ThisWindow &window, Location loc, StrSlice text) -> void {
        window.renderText(this->baked_font, loc, text);
    }
};

int main() {
    testGrid();

    GLFW glfw{&handle_error};

    Arena window_arena{MEGABYTES(4)};
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
