#include "arena.cc"
#include "dirutils.cc"
#include "graphics/bakedfont.cc"
#include "graphics/common.cc"
#include "graphics/gl.cc"
#include "graphics/quadprogram.cc"
#include "graphics/texture2d.cc"
#include "graphics/window.cc"
#include "grid.cc"
#include "one_of_aware.cc"
#include "op.cc"
#include "slice.cc"
#include "solver.cc"

// auto generated
#include "generated/generated.cc"

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
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

    enum Event {
        et_empty,
        et_mouse_press,
        et_mouse_release,
        et_right_mouse_press,
        et_right_mouse_release,
    };

    struct Element {
        enum Type {
            et_empty,
            et_background,
            et_empty_grid_cell,
            et_revealed_grid_cell,
            et_flagged_grid_cell,
            et_maybe_flagged_grid_cell,
            et_width_inc,
            et_width_dec,
            et_height_inc,
            et_height_dec,
            et_generate_grid_btn,
            et_text,
        };

        Type type;
        SLocation loc;
        Dims dims;
        StrSlice text;
        Color color;
        Location cell_loc;

        Element(Type type)
            : type(type), loc{}, dims{}, text{}, color{}, cell_loc{} {}
        Element(Type type, SLocation loc, Dims dims)
            : type(type), loc(loc), dims(dims), text{}, color{}, cell_loc{} {}
        Element(Type type, SLocation loc, StrSlice text, Color color)
            : type(type), loc(loc), dims{}, text(text), color(color),
              cell_loc{} {}
        Element(Type type, SLocation loc, Dims dims, Location cell_loc)
            : type(type), loc(loc), dims(dims), text{}, color{},
              cell_loc(cell_loc) {}

        auto contains(SLocation loc) -> bool {
            if (loc.row < this->loc.row) {
                return false;
            }
            if (loc.col < this->loc.col) {
                return false;
            }

            ssize_t bottom_row = this->loc.row + this->dims.height;
            ssize_t right_col = this->loc.col + this->dims.width;

            if (loc.row > bottom_row) {
                return false;
            }
            if (loc.col > right_col) {
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
    SLocation down_mouse_pos;
    bool right_mouse_down;
    SLocation down_right_mouse_pos;

    LLEvent ev_sentinel;
    LLElement el_sentinel;

    bool preview_grid;
    size_t width_input;
    size_t height_input;

    Arena grid_arena;
    Grid grid;

    static size_t const font_pixel_height = 30;

    Context(ThisWindow &window)
        : arena{MEGABYTES(4)}, mark(this->arena.len),
          quad_program(window.quadProgram()),
          baked_font{window.bakedFont(this->arena, "./fonts/Roboto-Black.ttf",
                                      font_pixel_height)},
          button{}, background{}, mouse_down(false), down_mouse_pos{},
          right_mouse_down(false), down_right_mouse_pos{},
          ev_sentinel{Event::et_empty}, el_sentinel{Element::Type::et_empty},
          preview_grid(false), width_input(10), height_input(10),
          grid_arena{KILOBYTES(4)}, grid{} {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glFrontFace(GL_CCW);
        glClearColor(0.0, 0.0, 0.0, 0.0);

        this->button.grayscale(204, Dims{1, 1});
        this->background.grayscale(120, Dims{1, 1});

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
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                LLEvent *ev =
                    this->arena.pushT<LLEvent>(Event{Event::et_mouse_press});

                this->mouse_down = true;
                this->down_mouse_pos = window.getClampedMouseLocation();
                this->ev_sentinel.enqueue(ev);
            } else {
                LLEvent *ev =
                    this->arena.pushT<LLEvent>(Event{Event::et_mouse_release});

                this->mouse_down = false;
                this->ev_sentinel.enqueue(ev);
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                LLEvent *ev = this->arena.pushT<LLEvent>(
                    Event{Event::et_right_mouse_press});

                this->right_mouse_down = true;
                this->down_right_mouse_pos = window.getClampedMouseLocation();
                this->ev_sentinel.enqueue(ev);
            } else {
                LLEvent *ev = this->arena.pushT<LLEvent>(
                    Event{Event::et_right_mouse_release});

                this->right_mouse_down = false;
                this->ev_sentinel.enqueue(ev);
            }
        }
    }

    auto buildScene(ThisWindow &window) -> void {
        Dims window_dims = window.getDims();
        size_t w = window_dims.width;
        size_t h = window_dims.height;

        size_t padding = 15;
        Dims render_dims{(w > (2 * padding)) ? (w - 2 * padding) : 0,
                         (h > (2 * padding)) ? (h - 2 * padding) : 0};

        SLocation render_loc{
            static_cast<ssize_t>((w - render_dims.width) / 2),
            static_cast<ssize_t>((h - render_dims.height) / 2)};

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
                ssize_t r_pos =
                    r * (cell_height + 2 * cell_padding) + cell_padding;

                for (size_t c = 0; c < this->grid.dims.width; ++c) {
                    ssize_t c_pos =
                        c * (cell_width + 2 * cell_padding) + cell_padding;

                    SLocation cell_loc{render_loc.row + r_pos,
                                       render_loc.col + c_pos};

                    Cell &cell = this->grid[r][c];

                    Element::Type et = Element::Type::et_empty;
                    switch (cell.display_type) {
                    case CellDisplayType::cdt_hidden: {
                        et = Element::Type::et_empty_grid_cell;
                    } break;
                    case CellDisplayType::cdt_value: {
                        et = Element::Type::et_revealed_grid_cell;
                    } break;
                    case CellDisplayType::cdt_flag: {
                        et = Element::Type::et_flagged_grid_cell;
                    } break;
                    case CellDisplayType::cdt_maybe_flag: {
                        et = Element::Type::et_maybe_flagged_grid_cell;
                    } break;
                    }

                    this->pushElement(
                        {et, cell_loc, cell_dims, Location{r, c}});
                }
            }
        } else if (this->preview_grid) {
            size_t cell_padding = 1;
            size_t r_padding = this->height_input * 2 * cell_padding;
            size_t c_padding = this->width_input * 2 * cell_padding;

            size_t cell_width =
                (render_dims.width - c_padding) / this->width_input;
            size_t cell_height =
                (render_dims.height - r_padding) / this->height_input;

            Dims cell_dims{cell_width, cell_height};

            for (size_t r = 0; r < this->height_input; ++r) {
                ssize_t r_pos =
                    r * (cell_height + 2 * cell_padding) + cell_padding;

                for (size_t c = 0; c < this->width_input; ++c) {
                    ssize_t c_pos =
                        c * (cell_width + 2 * cell_padding) + cell_padding;

                    SLocation cell_loc{render_loc.row + r_pos,
                                       render_loc.col + c_pos};

                    this->pushElement({Element::Type::et_empty_grid_cell,
                                       cell_loc, cell_dims, Location{r, c}});
                }
            }
        } else {
            this->buildGenerateScene(render_dims);
        }
    }

    auto buildGenerateScene(Dims render_dims) -> void {
        size_t w = render_dims.width;
        size_t h = render_dims.height;

        Dims button_dims{
            125, 20 + font_pixel_height}; // TODO(bhester): get text bounds

        SLocation button_loc{static_cast<ssize_t>((h - button_dims.height) / 2),
                             static_cast<ssize_t>((w - button_dims.width) / 2)};

        size_t width_len = 9 + 1;   // "Width: dd" + null
        size_t height_len = 10 + 1; // "Height: dd" + null

        char *width_label = this->arena.pushTN<char>(width_len);
        char *height_label = this->arena.pushTN<char>(height_len);

        StrSlice width_slice = sliceNPrintf(width_label, width_len,
                                            "Width: %zu", this->width_input);
        StrSlice height_slice = sliceNPrintf(height_label, height_len,
                                             "Height: %zu", this->height_input);

        ssize_t signed_pixel_height = font_pixel_height;
        SLocation width_loc{button_loc.row - 2 * (signed_pixel_height + 10),
                            button_loc.col + 10};
        SLocation height_loc{button_loc.row - 1 * (signed_pixel_height + 10),
                             button_loc.col + 10};

        // push DEC,INC,LABEL
        this->pushElement(
            {Element::Type::et_width_dec,
             SLocation{width_loc.row,
                       width_loc.col - 2 * (signed_pixel_height + 5)},
             Dims{font_pixel_height, font_pixel_height}});
        this->pushElement(
            {Element::Type::et_width_inc,
             SLocation{width_loc.row,
                       width_loc.col - 1 * (signed_pixel_height + 5)},
             Dims{font_pixel_height, font_pixel_height}});
        this->pushElement({Element::Type::et_text, width_loc, width_slice,
                           Color::grayscale(50)});

        // push DEC,INC,LABEL
        this->pushElement(
            {Element::Type::et_height_dec,
             SLocation{height_loc.row,
                       height_loc.col - 2 * (signed_pixel_height + 5)},
             Dims{font_pixel_height, font_pixel_height}});
        this->pushElement(
            {Element::Type::et_height_inc,
             SLocation{height_loc.row,
                       height_loc.col - 1 * (signed_pixel_height + 5)},
             Dims{font_pixel_height, font_pixel_height}});
        this->pushElement({Element::Type::et_text, height_loc, height_slice,
                           Color::grayscale(50)});

        this->pushElement(
            {Element::Type::et_generate_grid_btn, button_loc, button_dims});
    }

    auto renderScene(ThisWindow &window) -> void {
        LLElement *el = &this->el_sentinel;

        while ((el = el->next) != &this->el_sentinel) {
            switch (el->val.type) {
            case Element::Type::et_empty:
                break;
            case Element::Type::et_background: {
                auto guard = this->quad_program.withTexture(this->background);
                this->renderQuad(window, SRect{el->val.loc, el->val.dims});
            } break;
            case Element::Type::et_empty_grid_cell: {
                auto guard = this->quad_program.withTexture(this->button);
                this->renderQuad(window, SRect{el->val.loc, el->val.dims});
            } break;
            case Element::Type::et_revealed_grid_cell: {
                SRect render_rect{el->val.loc, el->val.dims};
                {
                    auto guard =
                        this->quad_program.withTexture(this->background);
                    this->renderQuad(window, render_rect);
                }

                // TODO(bhester): change font color based on the number?
                this->baked_font.setColor(Color::grayscale(225));

                unsigned char cell_val = this->grid[el->val.cell_loc].number;
                switch (cell_val) {
                case 0:
                    // don't render a number
                    break;
                default: {
                    StrSlice nums = STR_SLICE("12345678");
                    this->renderCenteredText(
                        window, render_rect,
                        nums.slice(cell_val - 1, cell_val));
                } break;
                }
            } break;
            case Element::Type::et_flagged_grid_cell: {
                SRect render_rect{el->val.loc, el->val.dims};
                {
                    auto guard = this->quad_program.withTexture(this->button);
                    this->renderQuad(window, render_rect);
                }

                this->baked_font.setColor(Color::grayscale(0));
                this->renderCenteredText(window, render_rect, STR_SLICE("F"));
            } break;
            case Element::Type::et_maybe_flagged_grid_cell: {
                SRect render_rect{el->val.loc, el->val.dims};
                {
                    auto guard = this->quad_program.withTexture(this->button);
                    this->renderQuad(window, render_rect);
                }

                this->baked_font.setColor(Color::grayscale(0));
                this->renderCenteredText(window, render_rect, STR_SLICE("?"));
            } break;
            case Element::Type::et_generate_grid_btn: {
                SRect render_rect{el->val.loc, el->val.dims};
                this->renderButton(window, render_rect, STR_SLICE("Generate"),
                                   this->button, Color::grayscale(50));
            } break;
            case Element::Type::et_text: {
                this->baked_font.setColor(el->val.color);
                this->renderText(window, el->val.loc, el->val.text);
            } break;
            case Element::Type::et_width_inc:
            case Element::Type::et_height_inc: {
                SRect render_rect{el->val.loc, el->val.dims};
                this->renderButton(window, render_rect, STR_SLICE("+"),
                                   this->button, Color::grayscale(50));
            } break;
            case Element::Type::et_width_dec:
            case Element::Type::et_height_dec: {
                SRect render_rect{el->val.loc, el->val.dims};
                this->renderButton(window, render_rect, STR_SLICE("-"),
                                   this->button, Color::grayscale(50));
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
            switch (ev->val) {
            case Event::et_empty:
            case Event::et_mouse_release:
            case Event::et_right_mouse_release:
                break;
            case Event::et_mouse_press: {
                LLElement *el = &this->el_sentinel;

                bool keep_processing_elems = true;
                while ((el = el->prev) != &this->el_sentinel &&
                       keep_processing_elems) {
                    if (el->val.contains(this->down_mouse_pos)) {
                        switch (el->val.type) {
                        case Element::Type::et_empty:
                        case Element::Type::et_background:
                        case Element::Type::et_revealed_grid_cell:
                        case Element::Type::et_flagged_grid_cell:
                        case Element::Type::et_maybe_flagged_grid_cell:
                        case Element::Type::et_text:
                            break;
                        case Element::Type::et_empty_grid_cell: {
                            keep_processing_elems = false;

                            if (this->preview_grid) {
                                printf("Generating grid\n");

                                this->preview_grid = false;
                                this->grid = generateGrid(
                                    this->grid_arena,
                                    Dims{this->width_input, this->height_input},
                                    15, el->val.cell_loc);
                            } else {
                                uncoverSelfAndNeighbors(this->grid,
                                                        el->val.cell_loc);
                            }

                            window.needs_render = true;
                        } break;
                        case Element::Type::et_generate_grid_btn: {
                            keep_processing_elems = false;

                            printf("Previewing grid\n");

                            this->preview_grid = true;
                            window.needs_render = true;
                        } break;
                        case Element::Type::et_width_inc: {
                            keep_processing_elems = false;

                            if (this->width_input < 99) {
                                ++this->width_input;
                                window.needs_render = true;
                            }
                        } break;
                        case Element::Type::et_width_dec: {
                            keep_processing_elems = false;

                            if (this->width_input > 1) {
                                --this->width_input;
                                window.needs_render = true;
                            }
                        } break;
                        case Element::Type::et_height_inc: {
                            keep_processing_elems = false;

                            if (this->height_input < 99) {
                                ++this->height_input;
                                window.needs_render = true;
                            }
                        } break;
                        case Element::Type::et_height_dec: {
                            keep_processing_elems = false;

                            if (this->height_input > 1) {
                                --this->height_input;
                                window.needs_render = true;
                            }
                        } break;
                        }
                    }
                }
            } break;
            case Event::et_right_mouse_press: {
                LLElement *el = &this->el_sentinel;

                bool keep_processing_elems = true;
                while ((el = el->prev) != &this->el_sentinel &&
                       keep_processing_elems) {
                    if (el->val.contains(this->down_right_mouse_pos)) {
                        switch (el->val.type) {
                        case Element::Type::et_empty:
                        case Element::Type::et_background:
                        case Element::Type::et_revealed_grid_cell:
                        case Element::Type::et_maybe_flagged_grid_cell:
                        case Element::Type::et_text:
                        case Element::Type::et_generate_grid_btn:
                        case Element::Type::et_width_inc:
                        case Element::Type::et_width_dec:
                        case Element::Type::et_height_inc:
                        case Element::Type::et_height_dec:
                            break;
                        case Element::Type::et_empty_grid_cell: {
                            keep_processing_elems = false;

                            flagCell(this->grid, el->val.cell_loc);

                            window.needs_render = true;
                        } break;
                        case Element::Type::et_flagged_grid_cell: {
                            keep_processing_elems = false;

                            unflagCell(this->grid, el->val.cell_loc);

                            window.needs_render = true;
                        } break;
                        }
                    }
                }
            } break;
            }
        }
    }

    auto pushElement(Element el) -> void {
        LLElement *ll = this->arena.pushT<LLElement>(el);
        this->el_sentinel.enqueue(ll);
    }

    auto renderButton(ThisWindow &window, SRect rect, StrSlice text,
                      Texture2D &bg, Color fg) -> void {
        {
            auto guard = this->quad_program.withTexture(bg);
            this->renderQuad(window, rect);
        }
        this->baked_font.setColor(fg);
        this->renderCenteredText(window, rect, text);
    }

    auto renderQuad(ThisWindow &window, SRect rect) -> void {
        window.renderQuad(this->quad_program, rect);
    }

    auto renderText(ThisWindow &window, SLocation loc, StrSlice text) -> void {
        window.renderText(this->baked_font, loc, text);
    }

    auto renderText(ThisWindow &window, SRect rect, StrSlice text) -> void {
        window.renderText(this->baked_font, rect, text);
    }

    auto renderCenteredText(ThisWindow &window, SRect rect, StrSlice text)
        -> void {
        window.renderCenteredText(this->baked_font, rect, text);
    }
};

int main() {
    testGrid();

    GLFW glfw{&handle_error};

    Window<Context> window{800, 600, "Hello, world"};
    window.setPin();
    window.setPos(500, 500);
    window.show();

    double fps = 60.0;
    double mspf = 1000.0 / fps;

    double last_time = glfwGetTime();
    while (!window.shouldClose()) {
        window.render();
        window.processEvents();

        double next_time = glfwGetTime();
        double delta_time_ms = 1000.0 * (next_time - last_time);

        last_time = next_time;

        if (delta_time_ms < mspf) {
            unsigned int sleep_time = mspf - delta_time_ms;
            usleep(sleep_time);
        }

        if (window.isKeyPressed(GLFW_KEY_Q)) {
            window.close();
        }
    }

    return 0;
}
