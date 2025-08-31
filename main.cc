#include "arena.cc"
#include "dirutils.cc"
#include "graphics/bakedfont.cc"
#include "graphics/common.cc"
#include "graphics/gl.cc"
#include "graphics/quadprogram.cc"
#include "graphics/texture2d.cc"
#include "graphics/utils.cc"
#include "graphics/window.cc"
#include "grid.cc"
#include "one_of_aware.cc"
#include "op.cc"
#include "slice.cc"
#include "solver.cc"

// auto generated
#include "generated/generated.cc"

#include <assert.h>
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
    solver.registerRule(GridSolver::Rule{flag_remaining_cells,
                                         STR_SLICE("flag_remaining_cells")});
    solver.registerRule(
        GridSolver::Rule{show_hidden_cells, STR_SLICE("show_hidden_cells")});
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

    static auto isEmpty(LinkedList &ll) -> bool { return ll.next == &ll; }

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

struct HBox {
    struct LocIterator {
        LinkedList<Dims> *cur;
        LinkedList<Dims> *end;
        SLocation cur_loc;
        size_t gap;
        size_t height;

        auto getNext() -> SRect {
            assert(this->hasNext() && "Invalid iterator");

            Dims cur_dims = this->cur->val;
            SLocation cur_loc = this->cur_loc;

            this->cur = this->cur->next;
            this->cur_loc.col += cur_dims.width + this->gap;

            return SRect{cur_loc, Dims{cur_dims.width, this->height}};
        }

        auto hasNext() -> bool { return cur != end; }
    };

    size_t gap;
    Dims total_dims;
    LinkedList<Dims> items_sentinel;

    HBox() : HBox(0) {}
    HBox(size_t gap) : gap(gap), total_dims{}, items_sentinel{} {
        LinkedList<Dims>::initSentinel(this->items_sentinel);
    }

    auto pushItem(Arena &arena, Dims dims) -> void {
        LinkedList<Dims> *element = arena.pushT<LinkedList<Dims>>({dims});

        if (!LinkedList<Dims>::isEmpty(this->items_sentinel)) {
            this->total_dims.width += this->gap;
        }
        this->total_dims.width += dims.width;

        if (this->total_dims.height < dims.height) {
            this->total_dims.height = dims.height;
        }

        this->items_sentinel.enqueue(element);
    }

    auto itemsIterator(SLocation base) -> LocIterator {
        return LocIterator{this->items_sentinel.next, &this->items_sentinel,
                           base, this->gap, this->total_dims.height};
    }
};

struct VBox {
    struct LocIterator {
        LinkedList<Dims> *cur;
        LinkedList<Dims> *end;
        SLocation cur_loc;
        size_t gap;
        size_t width;

        auto getNext() -> SRect {
            assert(this->hasNext() && "Invalid iterator");

            Dims cur_dims = this->cur->val;
            SLocation cur_loc = this->cur_loc;

            this->cur = this->cur->next;
            this->cur_loc.row += cur_dims.height + this->gap;

            return SRect{cur_loc, Dims{this->width, cur_dims.height}};
        }

        auto hasNext() -> bool { return cur != end; }
    };

    size_t gap;
    Dims total_dims;
    LinkedList<Dims> items_sentinel;

    VBox() : VBox(0) {}
    VBox(size_t gap) : gap(gap), total_dims{}, items_sentinel{} {
        LinkedList<Dims>::initSentinel(this->items_sentinel);
    }

    auto pushItem(Arena &arena, Dims dims) -> void {
        LinkedList<Dims> *element = arena.pushT<LinkedList<Dims>>({dims});

        if (!LinkedList<Dims>::isEmpty(this->items_sentinel)) {
            this->total_dims.height += this->gap;
        }
        this->total_dims.height += dims.height;

        if (this->total_dims.width < dims.width) {
            this->total_dims.width = dims.width;
        }

        this->items_sentinel.enqueue(element);
    }

    auto itemsIterator(SLocation base) -> LocIterator {
        return LocIterator{this->items_sentinel.next, &this->items_sentinel,
                           base, this->gap, this->total_dims.width};
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
            et_modal_background,
            et_restart_btn,
            et_empty_grid_cell,
            et_revealed_grid_cell,
            et_flagged_grid_cell,
            et_maybe_flagged_grid_cell,
            et_width_inc,
            et_width_dec,
            et_height_inc,
            et_height_dec,
            et_mine_inc,
            et_mine_dec,
            et_generate_grid_btn,
            et_step_solver_btn,
            et_text,
        };

        Type type;
        SLocation loc;
        Dims dims;
        Dims padding;
        StrSlice text;
        Color color;
        Location cell_loc;
        bool disabled;

        Element(Type type)
            : type(type), loc{}, dims{}, padding{}, text{}, color{}, cell_loc{},
              disabled{} {}
        Element(Type type, SLocation loc, Dims dims)
            : type(type), loc(loc), dims(dims), padding{}, text{}, color{},
              cell_loc{}, disabled{} {}
        Element(Type type, SLocation loc, Dims dims, StrSlice text)
            : type(type), loc(loc), dims(dims), padding{}, text(text), color{},
              cell_loc{}, disabled{} {}
        Element(Type type, SLocation loc, StrSlice text, Color color)
            : type(type), loc(loc), dims{}, padding{}, text(text), color(color),
              cell_loc{}, disabled{} {}
        Element(Type type, SLocation loc, Dims dims, Location cell_loc)
            : type(type), loc(loc), dims(dims), padding{}, text{}, color{},
              cell_loc(cell_loc), disabled{} {}
        Element(Type type, SLocation loc, bool disabled)
            : type(type), loc(loc), dims{}, padding{}, text{}, color{},
              cell_loc{}, disabled(disabled) {}
        Element(Type type, SLocation loc, Dims dims, bool disabled)
            : type(type), loc(loc), dims(dims), padding{}, text{}, color{},
              cell_loc{}, disabled(disabled) {}
        Element(Type type, SLocation loc, Dims dims, Dims padding,
                StrSlice text, bool disabled)
            : type(type), loc(loc), dims(dims), padding(padding), text(text),
              color{}, cell_loc{}, disabled(disabled) {}

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
    Texture2D disabled_button;
    Texture2D focus_button;
    Texture2D active_button;

    Texture2D background;
    Texture2D modal_background;

    bool mouse_down;
    SLocation down_mouse_pos;
    bool right_mouse_down;
    SLocation down_right_mouse_pos;

    LLEvent ev_sentinel;
    LLElement el_sentinel;

    bool preview_grid;
    size_t width_input;
    size_t height_input;
    size_t mine_input;

    Arena grid_arena;
    Grid grid;
    GridSolver &solver;
    Op<size_t> last_work_rule;
    Op<bool> last_step_success;

    static constexpr Color const TEXT_COLOR = Color::grayscale(50);

    Context(ThisWindow &window, GridSolver &solver)
        : arena{MEGABYTES(4)}, mark(this->arena.len),
          quad_program(window.quadProgram()),
          baked_font{
              window.bakedFont(this->arena, "./fonts/Roboto-Black.ttf", 15)},
          button{}, disabled_button{}, focus_button{}, active_button{},
          background{}, modal_background{}, mouse_down(false), down_mouse_pos{},
          right_mouse_down(false), down_right_mouse_pos{},
          ev_sentinel{Event::et_empty}, el_sentinel{Element::Type::et_empty},
          preview_grid(false), width_input(10), height_input(10),
          mine_input(15), grid_arena{KILOBYTES(4)}, grid{}, solver{solver},
          last_work_rule{Op<size_t>::empty()},
          last_step_success{Op<bool>::empty()} {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glFrontFace(GL_CCW);
        glClearColor(0.0, 0.0, 0.0, 0.0);

        this->button.grayscale(200, Dims{1, 1});
        this->focus_button.grayscale(180, Dims{1, 1});
        this->active_button.grayscale(160, Dims{1, 1});
        this->disabled_button.grayscale(220, Dims{1, 1});

        this->background.grayscale(120, Dims{1, 1});
        this->modal_background.grayscale(0, 128, Dims{1, 1});

        LLEvent::initSentinel(this->ev_sentinel);
        LLElement::initSentinel(this->el_sentinel);
    }

    void framebufferSizeCallback(ThisWindow &window, int width, int height) {
        assert(width >= 0 && "Invalid width");
        assert(height >= 0 && "Invalid height");

        glViewport(0, 0, width, height);

        window.needs_rerender = true;
    }

    void cursorPosCallback(ThisWindow &window, double, double) {
        window.needs_repaint = true;
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

        SRect render_rect{render_loc, render_dims};

        ssize_t factor = 5; // must be > 1
        ssize_t solver_width =
            clamp<size_t>(300, render_dims.width / factor, 800);

        Dims grid_dims{(size_t)(render_dims.width - solver_width),
                       render_dims.height};

        SLocation solver_loc{
            render_loc.row,
            render_loc.col + (ssize_t)(render_dims.width - solver_width),
        };
        Dims solver_dims{(size_t)solver_width, render_dims.height};
        SRect solver_rect{solver_loc, solver_dims};

        this->pushElement(
            {Element::Type::et_background, render_loc, render_dims});

        if (this->grid.dims.area() > 0) {
            size_t cell_padding = 1;
            size_t r_padding = this->grid.dims.height * 2 * cell_padding;
            size_t c_padding = this->grid.dims.width * 2 * cell_padding;

            size_t cell_width =
                (grid_dims.width - c_padding) / this->grid.dims.width;
            size_t cell_height =
                (grid_dims.height - r_padding) / this->grid.dims.height;

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

            this->buildSolverPane(solver_rect);

            if (gridSolved(this->grid)) {
                // Show You Win modal and Restart button

                this->pushElement({Element::Type::et_modal_background,
                                   render_loc, render_dims});

                StrSlice you_win_slice = STR_SLICE("You Win!");
                StrSlice restart_slice = STR_SLICE("Restart");

                Dims button_padding{10, 10};
                Dims button_dims =
                    this->getButtonDims(you_win_slice, {}, button_padding);

                VBox box{10};
                box.pushItem(this->arena, this->getTextDims(you_win_slice));
                box.pushItem(this->arena, button_dims);
                SRect base_rect = centerIn(render_rect, box.total_dims);

                Dims modal_dims{render_dims.width / 3, render_dims.height / 3};
                if (base_rect.dims.width > modal_dims.width) {
                    modal_dims.width = base_rect.dims.width;
                }
                if (base_rect.dims.height > modal_dims.height) {
                    modal_dims.height = base_rect.dims.height;
                }

                SLocation modal_loc = centerIn(render_rect, modal_dims).ul;

                this->pushElement(
                    {Element::Type::et_background, modal_loc, modal_dims});

                VBox::LocIterator vbox_it = box.itemsIterator(base_rect.ul);

                this->pushElement({Element::Type::et_text, vbox_it.getNext().ul,
                                   you_win_slice, TEXT_COLOR});

                SRect button_rect = vbox_it.getNext();
                this->pushButtonAt(Element::Type::et_restart_btn,
                                   centerIn(button_rect, button_dims).ul,
                                   restart_slice, {}, button_padding);

                assert(!vbox_it.hasNext());
            }
        } else if (this->preview_grid) {
            size_t cell_padding = 1;
            size_t r_padding = this->height_input * 2 * cell_padding;
            size_t c_padding = this->width_input * 2 * cell_padding;

            size_t cell_width =
                (grid_dims.width - c_padding) / this->width_input;
            size_t cell_height =
                (grid_dims.height - r_padding) / this->height_input;

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

            this->buildSolverPane(solver_rect);
        } else {
            this->buildGenerateScene(render_dims);
        }
    }

    auto buildSolverPane(SRect footer_rect) -> void {
        StrSlice btn_text = STR_SLICE("Step Solver");
        StrSlice rule_used_slice = STR_SLICE("*");
        StrSlice no_rule_applied_slice = STR_SLICE("No Rules Applied");

        Dims padding{5, 5};

        // FIXME(bhester): wherever I use 15, it's likely an implicit reference
        // to pixel_height, so I think I want to get a constant somewhere...
        Dims min_used_dims{15, 15};
        Dims rule_used_dims =
            this->getLineHeightTextDims(rule_used_slice, min_used_dims);

        VBox name_box{5};
        for (auto &rule : this->solver.rules.slice()) {
            Dims text_dims = this->getTextDims(rule.name);
            Dims box_dims{text_dims.width + rule_used_dims.width,
                          text_dims.height < rule_used_dims.height
                              ? rule_used_dims.height
                              : text_dims.height};
            name_box.pushItem(this->arena, box_dims);
        }

        VBox footer_contents{20};
        footer_contents.pushItem(this->arena,
                                 this->getButtonDims(btn_text, {}, padding));
        footer_contents.pushItem(this->arena, name_box.total_dims);
        footer_contents.pushItem(this->arena,
                                 this->getTextDims(no_rule_applied_slice));

        SRect inner_rect = centerIn(footer_rect, footer_contents.total_dims);

        VBox::LocIterator footer_it =
            footer_contents.itemsIterator(inner_rect.ul);

        SRect btn_rect = footer_it.getNext();
        SRect names_rect = footer_it.getNext();
        SLocation no_rule_loc = footer_it.getNext().ul;
        assert(!footer_it.hasNext());

        this->pushButtonCenteredIn(Element::Type::et_step_solver_btn, btn_rect,
                                   btn_text, {}, padding, this->preview_grid);

        Color rule_color = Color::grayscale(80);
        VBox::LocIterator names_it = name_box.itemsIterator(names_rect.ul);
        for (size_t rule_idx = 0; rule_idx < this->solver.rules.len;
             ++rule_idx) {
            SLocation name_box_loc = names_it.getNext().ul;
            SLocation name_loc{name_box_loc.row,
                               name_box_loc.col +
                                   (ssize_t)rule_used_dims.width};

            if (this->last_work_rule.valid &&
                this->last_work_rule.get() == rule_idx) {
                this->pushElement({Element::et_text, name_box_loc,
                                   rule_used_slice, rule_color});
            }

            StrSlice rule_name = this->solver.rules.at(rule_idx).name;
            this->pushElement(
                {Element::Type::et_text, name_loc, rule_name, rule_color});
        }

        if (this->last_step_success.valid && !this->last_step_success.get()) {
            this->pushElement({Element::Type::et_text, no_rule_loc,
                               no_rule_applied_slice, Color{140, 30, 30}});
        }

        assert(!names_it.hasNext());
    }

    auto buildGenerateScene(Dims render_dims) -> void {
        SRect render_rect{SLocation{0, 0}, render_dims};

        ssize_t inc_dec_dim = 15;
        Dims inc_dec_dims{(size_t)inc_dec_dim, (size_t)inc_dec_dim};

        StrSlice dec_slice = STR_SLICE("-");
        StrSlice inc_slice = STR_SLICE("+");
        StrSlice gen_slice = STR_SLICE("Generate");

        size_t width_len = 9 + 1;   // "Width: dd" + null
        size_t height_len = 10 + 1; // "Height: dd" + null
        size_t mine_len = 9 + 1;    // "Mines: dd" + null

        char *width_label = this->arena.pushTN<char>(width_len);
        char *height_label = this->arena.pushTN<char>(height_len);
        char *mine_label = this->arena.pushTN<char>(mine_len);

        StrSlice width_slice = sliceNPrintf(width_label, width_len,
                                            "Width: %zu", this->width_input);
        StrSlice height_slice = sliceNPrintf(height_label, height_len,
                                             "Height: %zu", this->height_input);
        StrSlice mine_slice =
            sliceNPrintf(mine_label, mine_len, "Mines: %zu", this->mine_input);

        Dims button_dims = this->getButtonDims(gen_slice, Dims{}, Dims{5, 5});

        HBox width_box{5};
        width_box.pushItem(
            this->arena, this->getButtonDims(dec_slice, inc_dec_dims, Dims{}));
        width_box.pushItem(
            this->arena, this->getButtonDims(inc_slice, inc_dec_dims, Dims{}));
        width_box.pushItem(this->arena, this->getTextDims(width_slice));

        HBox height_box{5};
        height_box.pushItem(
            this->arena, this->getButtonDims(dec_slice, inc_dec_dims, Dims{}));
        height_box.pushItem(
            this->arena, this->getButtonDims(inc_slice, inc_dec_dims, Dims{}));
        height_box.pushItem(this->arena, this->getTextDims(height_slice));

        HBox mine_box{5};
        mine_box.pushItem(this->arena,
                          this->getButtonDims(dec_slice, inc_dec_dims, Dims{}));
        mine_box.pushItem(this->arena,
                          this->getButtonDims(inc_slice, inc_dec_dims, Dims{}));
        mine_box.pushItem(this->arena, this->getTextDims(mine_slice));

        VBox vbox{5};
        vbox.pushItem(this->arena, width_box.total_dims);
        vbox.pushItem(this->arena, height_box.total_dims);
        vbox.pushItem(this->arena, mine_box.total_dims);
        vbox.pushItem(this->arena, button_dims);

        SRect box_rect = centerIn(render_rect, vbox.total_dims);

        VBox::LocIterator vbox_it = vbox.itemsIterator(box_rect.ul);
        HBox::LocIterator width_box_it =
            width_box.itemsIterator(vbox_it.getNext().ul);
        HBox::LocIterator height_box_it =
            height_box.itemsIterator(vbox_it.getNext().ul);
        HBox::LocIterator mine_box_it =
            mine_box.itemsIterator(vbox_it.getNext().ul);

        SRect button_box = vbox_it.getNext();
        assert(!vbox_it.hasNext());

        // push DEC,INC,LABEL
        this->pushButtonAt(Element::Type::et_width_dec,
                           width_box_it.getNext().ul, dec_slice, inc_dec_dims);

        this->pushButtonAt(Element::Type::et_width_inc,
                           width_box_it.getNext().ul, inc_slice, inc_dec_dims);

        this->pushElement({Element::Type::et_text, width_box_it.getNext().ul,
                           width_slice, TEXT_COLOR});

        assert(!width_box_it.hasNext());

        // push DEC,INC,LABEL
        this->pushButtonAt(Element::Type::et_height_dec,
                           height_box_it.getNext().ul, dec_slice, inc_dec_dims);

        this->pushButtonAt(Element::Type::et_height_inc,
                           height_box_it.getNext().ul, inc_slice, inc_dec_dims);

        this->pushElement({Element::Type::et_text, height_box_it.getNext().ul,
                           height_slice, TEXT_COLOR});

        assert(!height_box_it.hasNext());

        // push DEC,INC,LABEL
        this->pushButtonAt(Element::Type::et_mine_dec, mine_box_it.getNext().ul,
                           dec_slice, inc_dec_dims);

        this->pushButtonAt(Element::Type::et_mine_inc, mine_box_it.getNext().ul,
                           inc_slice, inc_dec_dims);

        this->pushElement({Element::Type::et_text, mine_box_it.getNext().ul,
                           mine_slice, TEXT_COLOR});

        assert(!mine_box_it.hasNext());

        // Generate button
        SLocation button_loc = centerIn(button_box, button_dims).ul;
        this->pushButtonAt(Element::Type::et_generate_grid_btn, button_loc,
                           gen_slice, {}, Dims{5, 5});
    }

    auto renderScene(ThisWindow &window) -> void {
        LLElement *el = &this->el_sentinel;

        LLElement *active_element = nullptr;
        LLElement *focus_element = nullptr;
        this->getInteractableElements(window, active_element, focus_element);

        while ((el = el->next) != &this->el_sentinel) {
            bool is_active = el == active_element;
            bool is_focus = !this->mouse_down && el == focus_element;

            switch (el->val.type) {
            case Element::Type::et_empty:
                break;
            case Element::Type::et_background: {
                auto guard = this->quad_program.withTexture(this->background);
                this->renderQuad(window, SRect{el->val.loc, el->val.dims});
            } break;
            case Element::Type::et_modal_background: {
                auto guard =
                    this->quad_program.withTexture(this->modal_background);
                this->renderQuad(window, SRect{el->val.loc, el->val.dims});
            } break;
            case Element::Type::et_empty_grid_cell: {
                Texture2D &texture =
                    this->getButtonTexture(false, is_active, is_focus);

                auto guard = this->quad_program.withTexture(texture);
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
                this->renderButton(window, render_rect, {}, STR_SLICE("F"),
                                   false, is_active, is_focus);
            } break;
            case Element::Type::et_maybe_flagged_grid_cell: {
                SRect render_rect{el->val.loc, el->val.dims};
                this->renderButton(window, render_rect, {}, STR_SLICE("?"),
                                   false, is_active, is_focus);
            } break;
            case Element::Type::et_text: {
                this->baked_font.setColor(el->val.color);
                this->renderText(window, el->val.loc, el->val.text);
            } break;
            case Element::Type::et_restart_btn:
            case Element::Type::et_generate_grid_btn:
            case Element::Type::et_step_solver_btn:
            case Element::Type::et_width_inc:
            case Element::Type::et_width_dec:
            case Element::Type::et_height_inc:
            case Element::Type::et_height_dec:
            case Element::Type::et_mine_inc:
            case Element::Type::et_mine_dec: {
                SRect render_rect{el->val.loc, el->val.dims};
                this->renderButton(window, render_rect, el->val.padding,
                                   el->val.text, el->val.disabled, is_active,
                                   is_focus);
            } break;
            }
        }
    }

    auto interactable(Element::Type type) -> bool {
        switch (type) {
        case Element::Type::et_empty:
        case Element::Type::et_background:
        case Element::Type::et_modal_background:
        case Element::Type::et_text: {
            return false;
        } break;

        case Element::Type::et_restart_btn:
        case Element::Type::et_empty_grid_cell:
        case Element::Type::et_revealed_grid_cell:
        case Element::Type::et_flagged_grid_cell:
        case Element::Type::et_maybe_flagged_grid_cell:
        case Element::Type::et_width_inc:
        case Element::Type::et_width_dec:
        case Element::Type::et_height_inc:
        case Element::Type::et_height_dec:
        case Element::Type::et_mine_inc:
        case Element::Type::et_mine_dec:
        case Element::Type::et_generate_grid_btn:
        case Element::Type::et_step_solver_btn: {
            return true;
        } break;
        }

        assert(0 && "Unreachable");
    }

    auto getInteractableElements(ThisWindow &window, LLElement *&active_element,
                                 LLElement *&focus_element) -> void {
        LLElement *el = &this->el_sentinel;

        SLocation mouse_loc = window.getMouseLocation();
        SLocation down_loc = this->mouse_down ? this->down_mouse_pos
                                              : SLocation{LONG_MIN, LONG_MIN};

        while ((el = el->prev) != &this->el_sentinel) {
            if (!interactable(el->val.type)) {
                continue;
            }

            if (el->val.contains(mouse_loc)) {
                if (focus_element == nullptr) {
                    focus_element = el;
                }
            }

            if (el->val.contains(down_loc)) {
                if (active_element == nullptr) {
                    active_element = el;
                }
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

    auto paint(ThisWindow &window) -> void { renderScene(window); }

    auto processEvents(ThisWindow &window) -> void {
        LLEvent *ev = nullptr;
        while ((ev = this->ev_sentinel.dequeue()) != nullptr) {
            switch (ev->val) {
            case Event::et_empty:
                break;
            case Event::et_mouse_release:
            case Event::et_right_mouse_release: {
                // FIXME(bhester): this may not be necessary in some cases... do
                // I want to propagate active element status?
                window.needs_repaint = true;
            } break;
            case Event::et_mouse_press: {
                LLElement *el = &this->el_sentinel;

                bool keep_processing_elems = true;
                while ((el = el->prev) != &this->el_sentinel &&
                       keep_processing_elems) {
                    if (el->val.contains(this->down_mouse_pos)) {
                        switch (el->val.type) {
                        case Element::Type::et_empty:
                        case Element::Type::et_background:
                        case Element::Type::et_modal_background:
                        case Element::Type::et_revealed_grid_cell:
                        case Element::Type::et_flagged_grid_cell:
                        case Element::Type::et_maybe_flagged_grid_cell:
                        case Element::Type::et_text:
                            break;
                        case Element::Type::et_restart_btn: {
                            keep_processing_elems = false;

                            // clean up solver
                            this->solver.resetEpoch(this->grid);

                            this->grid_arena.reset(0);
                            this->grid = Grid{};

                            window.needs_rerender = true;
                        } break;
                        case Element::Type::et_empty_grid_cell: {
                            keep_processing_elems = false;

                            if (this->preview_grid) {
                                printf("Generating grid\n");

                                this->preview_grid = false;
                                this->grid = generateGrid(
                                    this->grid_arena,
                                    Dims{this->width_input, this->height_input},
                                    this->mine_input, el->val.cell_loc);
                            } else {
                                uncoverSelfAndNeighbors(this->grid,
                                                        el->val.cell_loc);

                                this->last_work_rule = Op<size_t>::empty();
                                this->last_step_success = Op<bool>::empty();
                            }

                            window.needs_rerender = true;
                        } break;
                        case Element::Type::et_generate_grid_btn: {
                            keep_processing_elems = false;

                            printf("Previewing grid\n");

                            this->preview_grid = true;
                            window.needs_rerender = true;
                        } break;
                        case Element::Type::et_step_solver_btn: {
                            if (el->val.disabled) {
                                break;
                            }

                            printf("Stepping solver\n");

                            bool did_work = this->solver.step(this->grid);
                            if (did_work) {
                                printf("Solver did work\n");
                                this->last_work_rule =
                                    this->solver.state.last_work_rule;
                                this->last_step_success = true;
                            } else {
                                printf("Solver did not do work\n");
                                this->last_work_rule = Op<size_t>::empty();
                                this->last_step_success = false;
                            }

                            window.needs_rerender = true;
                        } break;
                        case Element::Type::et_width_inc: {
                            keep_processing_elems = false;

                            if (this->width_input < 99) {
                                ++this->width_input;
                                window.needs_rerender = true;
                            }
                        } break;
                        case Element::Type::et_width_dec: {
                            keep_processing_elems = false;

                            if (this->width_input > 1) {
                                --this->width_input;
                                window.needs_rerender = true;
                            }
                        } break;
                        case Element::Type::et_height_inc: {
                            keep_processing_elems = false;

                            if (this->height_input < 99) {
                                ++this->height_input;
                                window.needs_rerender = true;
                            }
                        } break;
                        case Element::Type::et_height_dec: {
                            keep_processing_elems = false;

                            if (this->height_input > 1) {
                                --this->height_input;
                                window.needs_rerender = true;
                            }
                        } break;
                        case Element::Type::et_mine_inc: {
                            keep_processing_elems = false;

                            if (this->mine_input < 99) {
                                ++this->mine_input;
                                window.needs_rerender = true;
                            }
                        } break;
                        case Element::Type::et_mine_dec: {
                            keep_processing_elems = false;

                            if (this->mine_input > 1) {
                                --this->mine_input;
                                window.needs_rerender = true;
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
                        case Element::Type::et_modal_background:
                        case Element::Type::et_restart_btn:
                        case Element::Type::et_revealed_grid_cell:
                        case Element::Type::et_maybe_flagged_grid_cell:
                        case Element::Type::et_text:
                        case Element::Type::et_generate_grid_btn:
                        case Element::Type::et_step_solver_btn:
                        case Element::Type::et_width_inc:
                        case Element::Type::et_width_dec:
                        case Element::Type::et_height_inc:
                        case Element::Type::et_height_dec:
                        case Element::Type::et_mine_inc:
                        case Element::Type::et_mine_dec:
                            break;
                        case Element::Type::et_empty_grid_cell: {
                            keep_processing_elems = false;

                            flagCell(this->grid, el->val.cell_loc);

                            window.needs_rerender = true;
                        } break;
                        case Element::Type::et_flagged_grid_cell: {
                            keep_processing_elems = false;

                            unflagCell(this->grid, el->val.cell_loc);

                            window.needs_rerender = true;
                        } break;
                        }
                    }
                }
            } break;
            }
        }
    }

    auto pushButtonAt(Element::Type type, SLocation loc, StrSlice text,
                      Dims min_dims = {}, Dims padding = {},
                      bool disabled = false) -> SRect {
        Dims render_dims = this->getButtonDims(text, min_dims, padding);
        SRect rect{loc, render_dims};

        pushElement({type, loc, render_dims, padding, text, disabled});
        return rect;
    }

    auto pushButtonCenteredIn(Element::Type type, SRect rect, StrSlice text,
                              Dims min_dims = {}, Dims padding = {},
                              bool disabled = false) -> SRect {
        Dims render_dims = this->getButtonDims(text, min_dims, padding);
        SRect centered = centerIn(rect, render_dims);

        pushElement(
            {type, centered.ul, centered.dims, padding, text, disabled});
        return centered;
    }

    auto getTextDims(StrSlice text, Dims min_dims = {}) -> Dims {
        Dims text_dims = this->baked_font.getTextDims(text);
        return expandToMin(text_dims, min_dims);
    }

    auto getLineHeightTextDims(StrSlice text, Dims min_dims = {}) -> Dims {
        Dims text_dims = this->baked_font.getLineHeightTextDims(text);
        return expandToMin(text_dims, min_dims);
    }

    auto getButtonDims(StrSlice text, Dims min_dims, Dims padding) -> Dims {
        Dims text_dims = this->baked_font.getLineHeightTextDims(text);
        Dims render_dims = expandToMin(text_dims, min_dims);

        if (padding.width > 0) {
            render_dims.width += 2 * padding.width;
        }
        if (padding.height > 0) {
            render_dims.height += 2 * padding.height;
        }

        return render_dims;
    }

    auto pushElement(Element el) -> void {
        LLElement *ll = this->arena.pushT<LLElement>(el);
        this->el_sentinel.enqueue(ll);
    }

    auto getButtonTexture(bool disabled, bool active, bool focus)
        -> Texture2D & {
        if (disabled) {
            return this->disabled_button;
        } else if (active) {
            return this->active_button;
        } else if (focus) {
            return this->focus_button;
        } else {
            return this->button;
        }
    }

    auto renderButton(ThisWindow &window, SRect rect, Dims padding,
                      StrSlice text, bool disabled, bool active, bool focus)
        -> void {
        SRect render_rect = rect;
        if (rect.dims.area() == 0) {
            render_rect.dims = this->baked_font.getTextDims(text);
        }

        Texture2D &btn_texture =
            this->getButtonTexture(disabled, active, focus);

        auto guard = this->quad_program.withTexture(btn_texture);
        this->renderQuad(window, render_rect);

        SRect text_rect{
            SLocation{
                render_rect.ul.row + (ssize_t)padding.height,
                render_rect.ul.col + (ssize_t)padding.width,
            },
            Dims{
                render_rect.dims.width - 2 * padding.width,
                render_rect.dims.height - 2 * padding.height,
            },
        };

        unsigned char font_color = disabled ? 100 : 50;
        this->baked_font.setColor(Color::grayscale(font_color));
        this->renderCenteredText(window, text_rect, text);
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

    GridSolver solver;
    solver.registerRule(GridSolver::Rule{flag_remaining_cells,
                                         STR_SLICE("flag_remaining_cells")});
    solver.registerRule(
        GridSolver::Rule{show_hidden_cells, STR_SLICE("show_hidden_cells")});
    registerPatterns(solver);

    Window<Context> window{800, 600, "Hello, world", solver};
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
