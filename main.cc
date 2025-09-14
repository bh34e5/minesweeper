#include "arena.cc"
#include "dirutils.cc"
#include "graphics/bakedfont.cc"
#include "graphics/common.cc"
#include "graphics/containers.cc"
#include "graphics/gl.cc"
#include "graphics/quadprogram.cc"
#include "graphics/texture2d.cc"
#include "graphics/utils.cc"
#include "graphics/window.cc"
#include "grid.cc"
#include "linkedlist.cc"
#include "one_of_aware.cc"
#include "op.cc"
#include "slice.cc"
#include "solver.cc"

// auto generated
#include "generated/generated.cc"

#include <GLFW/glfw3.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define LEN(arr) (sizeof(arr) / sizeof(*arr))

// local rules {{{1
// flag_remaining_cells {{{2
auto flag_remaining_cells(Grid *grid, size_t row, size_t col, void *) -> bool {
    Cell cur = (*grid)[row][col];
    if (cur.display_type != CellDisplayType::cdt_value ||
        cur.type != CellType::ct_number) {
        return false;
    }

    if (cur.eff_number == 0) {
        return false;
    }

    size_t hidden_count = 0;

    auto neighbor_op = Op<Grid::Neighbor>::empty();
    auto neighbor_it = grid->neighborIterator(row, col);
    while ((neighbor_op = neighbor_it.next()).valid) {
        Cell *cell = neighbor_op.get().cell;
        if (cell->display_type == CellDisplayType::cdt_hidden) {
            ++hidden_count;
        }
    }

    if (cur.eff_number == hidden_count) {
        // flag all hidden cells
        bool did_work = false;

        auto neighbor_op = Op<Grid::Neighbor>::empty();
        auto neighbor_it = grid->neighborIterator(row, col);
        while ((neighbor_op = neighbor_it.next()).valid) {
            Grid::Neighbor neighbor = neighbor_op.get();
            Cell *cell = neighbor.cell;
            if (cell->display_type == CellDisplayType::cdt_hidden) {
                flagCell(grid, neighbor.loc);
                did_work = true;
            }
        }
        return did_work;
    }

    return false;
};
// }}}2

// show_hidden_cells {{{2
auto show_hidden_cells(Grid *grid, size_t row, size_t col, void *) -> bool {
    Cell cur = (*grid)[row][col];
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
        auto neighbor_it = grid->neighborIterator(row, col);
        while ((neighbor_op = neighbor_it.next()).valid) {
            Grid::Neighbor neighbor = neighbor_op.get();
            Cell *cell = neighbor.cell;
            if (cell->display_type == CellDisplayType::cdt_hidden) {
                uncoverSelfAndNeighbors(grid, neighbor.loc);
                did_work = true;
            }
        }
        return did_work;
    }

    return false;
}
// }}}2

// click remaining cells {{{2
auto click_remaining_cells(Grid *grid, size_t row, size_t col, void *) -> bool {
    long remainingFlags = gridRemainingFlags(*grid);
    if (remainingFlags == 0) {
        Cell &cell = (*grid)[row][col];
        if (cell.display_type == CellDisplayType::cdt_hidden) {
            uncoverSelfAndNeighbors(grid, &cell);
            return true;
        }
    }
    return false;
}
// }}}2
// }}}1

auto testGrid() -> void {
    srand(0);

    Arena grid_arena = makeArena(MEGABYTES(10));

    Grid grid = generateGrid(&grid_arena, Dims{9, 18}, 34, Location{5, 5});

    GridSolver::Rule flag_remaining_rule =
        makeRule(&flag_remaining_cells, STR_SLICE("flag_remaining_cells"));
    GridSolver::Rule show_hidden_rule =
        makeRule(&show_hidden_cells, STR_SLICE("show_hidden_cells"));

    GridSolver solver{};
    initSolver(&solver);

    solver.registerRule(&grid_arena, flag_remaining_rule);
    solver.registerRule(&grid_arena, show_hidden_rule);
    registerPatterns(&grid_arena, &solver);

    OneOfAwareRule one_of_aware_rule = makeOneOfAware(MEGABYTES(10));
    one_of_aware_rule.registerRule(&grid_arena, &solver);

    bool is_solvable = solver.solvable(&grid);

    printGrid(grid);
    printf("The grid %s solvable\n", is_solvable ? "is" : "is not");

    deleteOneOfAware(&one_of_aware_rule);
    freeArena(&grid_arena);
}

void handle_error(int error, char const *description) {
    fprintf(stderr, "GLFW Error (%d)): %s\n", error, description);
}

struct Context {
    typedef Window<Context> ThisWindow;

    // enum Event {{{1
    enum Event {
        et_empty,
        et_mouse_press,
        et_mouse_release,
        et_right_mouse_press,
        et_right_mouse_release,
        et_animation,
    };
    // }}}1

    // struct Element {{{1
    struct Element {
        enum Type {
            et_empty,
            et_background,
            et_modal_background,
            et_lose_flame,
            et_continue_btn,
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

        static auto makeRectElement(Type type, SLocation loc, Dims dims)
            -> Element {
            return Element{type, loc, dims, {}, {}, {}, {}, {}};
        }

        static auto makeTextElement(Type type, SLocation loc, StrSlice text,
                                    Color color) -> Element {
            return Element{type, loc, {}, {}, text, color, {}, {}};
        }

        static auto makeCellElement(Type type, SLocation loc, Dims dims,
                                    Location cell_loc) -> Element {
            return Element{type, loc, dims, {}, {}, {}, cell_loc, {}};
        }

        static auto makeButtonElement(Type type, SLocation loc, Dims dims,
                                      Dims padding, StrSlice text,
                                      bool disabled) -> Element {
            return Element{type, loc, dims, padding, text, {}, {}, disabled};
        }

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
    // }}}1

    typedef LinkedList<Event> LLEvent;
    typedef LinkedList<Element> LLElement;

    Arena arena;

    QuadProgram quad_program;
    BakedFont baked_font;

    Texture2D button;
    Texture2D disabled_button;
    Texture2D focus_button;
    Texture2D active_button;

    Texture2D background;
    Texture2D modal_background;
    Texture2D lose_flame;

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
    GridSolver solver;

    bool did_step;
    size_t last_work_rule;
    bool last_step_success;

    bool lose_animation_playing;
    double lose_animation_t;
    Location lose_animation_source;

    static constexpr Color const TEXT_COLOR = Color::grayscale(50);
    static constexpr Color const DARK_RED = Color{140, 30, 30};

    void framebufferSizeCallback(ThisWindow *window, int width, int height) {
        assert(width >= 0 && "Invalid width");
        assert(height >= 0 && "Invalid height");

        glViewport(0, 0, width, height);

        window->needs_rerender = true;
    }

    void cursorPosCallback(ThisWindow *window, double, double) {
        window->needs_repaint = true;
    }

    void mouseButtonCallback(ThisWindow *window, int button, int action, int) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                LLEvent *ev = this->arena.pushT(LLEvent{Event::et_mouse_press});

                this->mouse_down = true;
                this->down_mouse_pos = window->getClampedMouseLocation();
                this->ev_sentinel.enqueue(ev);
            } else {
                LLEvent *ev =
                    this->arena.pushT(LLEvent{Event::et_mouse_release});

                this->mouse_down = false;
                this->ev_sentinel.enqueue(ev);
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                LLEvent *ev =
                    this->arena.pushT(LLEvent{Event::et_right_mouse_press});

                this->right_mouse_down = true;
                this->down_right_mouse_pos = window->getClampedMouseLocation();
                this->ev_sentinel.enqueue(ev);
            } else {
                LLEvent *ev =
                    this->arena.pushT(LLEvent{Event::et_right_mouse_release});

                this->right_mouse_down = false;
                this->ev_sentinel.enqueue(ev);
            }
        }
    }

    // build scene {{{1
    // build scene {{{2
    auto buildScene(ThisWindow *window, double dt_s) -> void {
        Dims window_dims = window->getDims();
        size_t w = window_dims.width;
        size_t h = window_dims.height;

        size_t padding = 15;
        Dims render_dims{(w > (2 * padding)) ? (w - 2 * padding) : 0,
                         (h > (2 * padding)) ? (h - 2 * padding) : 0};

        SLocation render_loc{
            static_cast<ssize_t>((w - render_dims.width) / 2),
            static_cast<ssize_t>((h - render_dims.height) / 2)};

        SRect render_rect{render_loc, render_dims};

        this->pushElement(Element::makeRectElement(Element::Type::et_background,
                                                   render_loc, render_dims));

        if (this->preview_grid || this->grid.dims.area() > 0) {
            this->buildPlayScene(render_rect, dt_s);
        } else {
            this->buildGenerateScene(render_dims);
        }
    }
    // }}}2

    // build play scene {{{2
    auto buildPlayScene(SRect render_rect, double dt_s) -> void {
        VBox game_box{};
        initVBox(&game_box, 0, render_rect.dims.height);

        HBox grid_solver_box{};
        initHBox(&grid_solver_box, 0, render_rect.dims.width);

        long remaining_flags = this->preview_grid
                                   ? this->mine_input
                                   : gridRemainingFlags(this->grid);

        size_t mine_len = 12 + 1; // "Mines: (-)dddd" + null
        char *mines_label = this->arena.pushTN<char>(mine_len);
        StrSlice mine_slice =
            sliceNPrintf(mines_label, mine_len, "Mines: %ld", remaining_flags);

        Dims mine_label_dims = this->getTextDims(mine_slice, Dims{0, 20});

        size_t factor = 5; // must be > 1
        size_t solver_width =
            clamp<size_t>(250, render_rect.dims.width / factor, 800);

        grid_solver_box.pushItem(&this->arena, Dims{}, 1);
        grid_solver_box.pushItem(&this->arena, Dims{solver_width, 0});

        game_box.pushItem(&this->arena, mine_label_dims);
        game_box.pushItem(&this->arena, grid_solver_box.getDims(), 1);

        VBox::LocIterator game_box_it = game_box.itemsIterator(render_rect.ul);

        SRect mine_label_rect = game_box_it.getNext();
        SRect grid_solver_rect = game_box_it.getNext();

        assert(!game_box_it.hasNext());

        HBox::LocIterator grid_solver_box_it = grid_solver_box.itemsIterator(
            grid_solver_rect.ul, grid_solver_rect.dims.height);

        SRect grid_rect = grid_solver_box_it.getNext();
        SRect solver_rect = grid_solver_box_it.getNext();

        assert(!grid_solver_box_it.hasNext());

        this->pushElement(Element::makeTextElement(Element::Type::et_text,
                                                   mine_label_rect.ul,
                                                   mine_slice, TEXT_COLOR));
        this->buildSolverPane(solver_rect);

        size_t cell_padding = 1;
        Dims cell_dims = this->getGameCellDims(grid_rect, cell_padding);

        if (this->preview_grid) {
            this->buildPreviewGrid(grid_rect, cell_dims, cell_padding);
            return;
        } else {
            this->buildGameGrid(grid_rect, cell_dims, cell_padding);
        }

        if (this->lose_animation_playing) {
            Location source = this->lose_animation_source;

            this->lose_animation_t += dt_s;
            if (this->lose_animation_t > 1.0) {
                this->lose_animation_playing = false;
                this->lose_animation_t = 1.0;
                this->lose_animation_source = {};
            }

            ssize_t r_pos = source.row * (cell_dims.height + 2 * cell_padding) +
                            cell_padding;
            ssize_t c_pos = source.col * (cell_dims.width + 2 * cell_padding) +
                            cell_padding;

            SLocation cell_loc{render_rect.ul.row + r_pos,
                               render_rect.ul.col + c_pos};
            SRect cell_rect{cell_loc, cell_dims};

            double rect_width =
                2.0 * render_rect.dims.width * this->lose_animation_t;
            double rect_height =
                2.0 * render_rect.dims.height * this->lose_animation_t;

            Dims rect_dims{(size_t)rect_width, (size_t)rect_height};
            SRect flame_rect = centerIn(cell_rect, rect_dims);

            this->pushElement(Element::makeRectElement(
                Element::Type::et_lose_flame, flame_rect.ul, flame_rect.dims));

            LLEvent *event = this->arena.pushT<LLEvent>({Event::et_animation});
            this->ev_sentinel.enqueue(event);
        } else if (gridSolved(this->grid)) {
            // Show You Win modal and Restart button

            this->pushElement(
                Element::makeRectElement(Element::Type::et_modal_background,
                                         render_rect.ul, render_rect.dims));

            StrSlice you_win_slice = STR_SLICE("You Win!");
            StrSlice restart_slice = STR_SLICE("Restart");

            Dims button_padding{10, 10};
            Dims button_dims =
                this->getButtonDims(restart_slice, {}, button_padding);

            VBox box{};
            initVBox(&box, 10);

            box.pushItem(&this->arena, this->getTextDims(you_win_slice));
            box.pushItem(&this->arena, button_dims);
            SRect base_rect = centerIn(render_rect, box.getDims());

            Dims min_modal_dims{render_rect.dims.width / 3,
                                render_rect.dims.height / 3};

            Dims modal_dims = expandToMin(min_modal_dims, base_rect.dims);
            SLocation modal_loc = centerIn(render_rect, modal_dims).ul;

            this->pushElement(Element::makeRectElement(
                Element::Type::et_background, modal_loc, modal_dims));

            VBox::LocIterator vbox_it = box.itemsIterator(base_rect.ul);

            this->pushElement(Element::makeTextElement(
                Element::Type::et_text, vbox_it.getNext().ul, you_win_slice,
                TEXT_COLOR));

            SRect button_rect = vbox_it.getNext();
            this->pushButtonAt(Element::Type::et_restart_btn,
                               centerIn(button_rect, button_dims).ul,
                               restart_slice, {}, button_padding);

            assert(!vbox_it.hasNext());
        } else if (gridLost(this->grid)) {
            // Show You Lose modal, Continue, and Restart buttons

            this->pushElement(
                Element::makeRectElement(Element::Type::et_modal_background,
                                         render_rect.ul, render_rect.dims));

            StrSlice you_lose_slice = STR_SLICE("You Lose!");
            StrSlice continue_slice = STR_SLICE("Continue");
            StrSlice restart_slice = STR_SLICE("Restart");

            Dims button_padding{10, 10};
            Dims continue_button_dims =
                this->getButtonDims(continue_slice, {}, button_padding);
            Dims restart_button_dims =
                this->getButtonDims(restart_slice, {}, button_padding);

            VBox box{};
            initVBox(&box, 10);

            HBox button_box{};
            initHBox(&button_box, 10);

            button_box.pushItem(&this->arena, continue_button_dims);
            button_box.pushItem(&this->arena, restart_button_dims);

            box.pushItem(&this->arena, this->getTextDims(you_lose_slice));
            box.pushItem(&this->arena, button_box.getDims());
            SRect base_rect = centerIn(render_rect, box.getDims());

            Dims min_modal_dims{render_rect.dims.width / 3,
                                render_rect.dims.height / 3};

            Dims modal_dims = expandToMin(min_modal_dims, base_rect.dims);
            SLocation modal_loc = centerIn(render_rect, modal_dims).ul;

            this->pushElement(Element::makeRectElement(
                Element::Type::et_background, modal_loc, modal_dims));

            VBox::LocIterator vbox_it = box.itemsIterator(base_rect.ul);

            this->pushElement(Element::makeTextElement(
                Element::Type::et_text, vbox_it.getNext().ul, you_lose_slice,
                TEXT_COLOR));

            SRect button_rect =
                centerIn(vbox_it.getNext(), button_box.getDims());
            HBox::LocIterator hbox_it =
                button_box.itemsIterator(button_rect.ul);

            Element *continue_event = this->pushButtonAt(
                Element::Type::et_continue_btn,
                centerIn(hbox_it.getNext(), continue_button_dims).ul,
                continue_slice, {}, button_padding);

            continue_event->cell_loc = losingCell(this->grid);

            this->pushButtonAt(
                Element::Type::et_restart_btn,
                centerIn(hbox_it.getNext(), restart_button_dims).ul,
                restart_slice, {}, button_padding);

            assert(!hbox_it.hasNext());
            assert(!vbox_it.hasNext());
        }
    }
    // }}}2

    // build grid {{{2
    // build game grid {{{3
    auto buildGameGrid(SRect grid_rect, Dims cell_dims, size_t cell_padding)
        -> void {
        for (size_t r = 0; r < this->grid.dims.height; ++r) {
            ssize_t r_pos =
                r * (cell_dims.height + 2 * cell_padding) + cell_padding;

            for (size_t c = 0; c < this->grid.dims.width; ++c) {
                ssize_t c_pos =
                    c * (cell_dims.width + 2 * cell_padding) + cell_padding;

                SLocation cell_loc{grid_rect.ul.row + r_pos,
                                   grid_rect.ul.col + c_pos};

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

                this->pushElement(Element::makeCellElement(
                    et, cell_loc, cell_dims, Location{r, c}));
            }
        }
    }
    // }}}3

    // build preview grid {{{3
    auto buildPreviewGrid(SRect grid_rect, Dims cell_dims, size_t cell_padding)
        -> void {
        for (size_t r = 0; r < this->height_input; ++r) {
            ssize_t r_pos =
                r * (cell_dims.height + 2 * cell_padding) + cell_padding;

            for (size_t c = 0; c < this->width_input; ++c) {
                ssize_t c_pos =
                    c * (cell_dims.width + 2 * cell_padding) + cell_padding;

                SLocation cell_loc{grid_rect.ul.row + r_pos,
                                   grid_rect.ul.col + c_pos};

                this->pushElement(Element::makeCellElement(
                    Element::Type::et_empty_grid_cell, cell_loc, cell_dims,
                    Location{r, c}));
            }
        }
    }
    // }}}3

    // get cell dims {{{3
    auto getGameCellDims(SRect grid_rect, size_t cell_padding) -> Dims {
        Dims grid_dims = this->preview_grid
                             ? Dims{this->width_input, this->height_input}
                             : this->grid.dims;

        size_t r_padding = grid_dims.height * 2 * cell_padding;
        size_t c_padding = grid_dims.width * 2 * cell_padding;

        size_t cell_width =
            (grid_rect.dims.width - c_padding) / grid_dims.width;
        size_t cell_height =
            (grid_rect.dims.height - r_padding) / grid_dims.height;

        Dims cell_dims{cell_width, cell_height};
        return cell_dims;
    }
    // }}}3
    // }}}2

    // build solver pane {{{2
    auto buildSolverPane(SRect footer_rect) -> void {
        StrSlice btn_text = STR_SLICE("Step Solver");
        StrSlice rule_used_slice = STR_SLICE("*");
        StrSlice no_rule_applied_slice = STR_SLICE("No Rules Applied");

        Dims padding{5, 5};

        // FIXME(bhester): wherever I use 20, it's likely an implicit reference
        // to pixel_height, so I think I want to get a constant somewhere...
        Dims min_used_dims{20, 20};
        Dims rule_used_dims =
            this->getLineHeightTextDims(rule_used_slice, min_used_dims);

        VBox name_box{};
        initVBox(&name_box, 5);

        VBox footer_contents{};
        initVBox(&footer_contents, 20);

        {
            LinkedList<GridSolver::Rule> *ll = &this->solver.rule_sentinel;
            while ((ll = ll->next) != &this->solver.rule_sentinel) {
                Dims text_dims = this->getTextDims(ll->val.name);
                Dims box_dims{text_dims.width + rule_used_dims.width,
                              text_dims.height < rule_used_dims.height
                                  ? rule_used_dims.height
                                  : text_dims.height};
                name_box.pushItem(&this->arena, box_dims);
            }
        }

        footer_contents.pushItem(&this->arena,
                                 this->getButtonDims(btn_text, {}, padding));
        footer_contents.pushItem(&this->arena, name_box.getDims());
        footer_contents.pushItem(&this->arena,
                                 this->getTextDims(no_rule_applied_slice));

        SRect inner_rect = centerIn(footer_rect, footer_contents.getDims());

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

        bool show_rule_used_mark = this->did_step && this->last_step_success;

        LinkedList<GridSolver::Rule> *ll = &this->solver.rule_sentinel;
        for (size_t rule_idx = 0; rule_idx < this->solver.rule_count;
             ++rule_idx) {
            ll = ll->next;

            SLocation name_box_loc = names_it.getNext().ul;
            SLocation name_loc{name_box_loc.row,
                               name_box_loc.col +
                                   (ssize_t)rule_used_dims.width};

            if (show_rule_used_mark && this->last_work_rule == rule_idx) {
                this->pushElement(
                    Element::makeTextElement(Element::et_text, name_box_loc,
                                             rule_used_slice, rule_color));
            }

            StrSlice rule_name = ll->val.name;
            this->pushElement(Element::makeTextElement(
                Element::Type::et_text, name_loc, rule_name, rule_color));
        }

        if (this->did_step && !this->last_step_success) {
            this->pushElement(
                Element::makeTextElement(Element::Type::et_text, no_rule_loc,
                                         no_rule_applied_slice, DARK_RED));
        }

        assert(!names_it.hasNext());
    }
    // }}}2

    // build generate scene {{{2
    auto buildGenerateScene(Dims render_dims) -> void {
        SRect render_rect{SLocation{0, 0}, render_dims};

        ssize_t inc_dec_dim = 20;
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

        HBox width_box{};
        initHBox(&width_box, 5);

        HBox height_box{};
        initHBox(&height_box, 5);

        HBox mine_box{};
        initHBox(&mine_box, 5);

        VBox vbox{};
        initVBox(&vbox, 5);

        width_box.pushItem(
            &this->arena, this->getButtonDims(dec_slice, inc_dec_dims, Dims{}));
        width_box.pushItem(
            &this->arena, this->getButtonDims(inc_slice, inc_dec_dims, Dims{}));
        width_box.pushItem(&this->arena, this->getTextDims(width_slice));

        height_box.pushItem(
            &this->arena, this->getButtonDims(dec_slice, inc_dec_dims, Dims{}));
        height_box.pushItem(
            &this->arena, this->getButtonDims(inc_slice, inc_dec_dims, Dims{}));
        height_box.pushItem(&this->arena, this->getTextDims(height_slice));

        mine_box.pushItem(&this->arena,
                          this->getButtonDims(dec_slice, inc_dec_dims, Dims{}));
        mine_box.pushItem(&this->arena,
                          this->getButtonDims(inc_slice, inc_dec_dims, Dims{}));
        mine_box.pushItem(&this->arena, this->getTextDims(mine_slice));

        vbox.pushItem(&this->arena, width_box.getDims());
        vbox.pushItem(&this->arena, height_box.getDims());
        vbox.pushItem(&this->arena, mine_box.getDims());
        vbox.pushItem(&this->arena, button_dims);

        SRect box_rect = centerIn(render_rect, vbox.getDims());

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

        this->pushElement(Element::makeTextElement(Element::Type::et_text,
                                                   width_box_it.getNext().ul,
                                                   width_slice, TEXT_COLOR));

        assert(!width_box_it.hasNext());

        // push DEC,INC,LABEL
        this->pushButtonAt(Element::Type::et_height_dec,
                           height_box_it.getNext().ul, dec_slice, inc_dec_dims);

        this->pushButtonAt(Element::Type::et_height_inc,
                           height_box_it.getNext().ul, inc_slice, inc_dec_dims);

        this->pushElement(Element::makeTextElement(Element::Type::et_text,
                                                   height_box_it.getNext().ul,
                                                   height_slice, TEXT_COLOR));

        assert(!height_box_it.hasNext());

        // push DEC,INC,LABEL
        this->pushButtonAt(Element::Type::et_mine_dec, mine_box_it.getNext().ul,
                           dec_slice, inc_dec_dims);

        this->pushButtonAt(Element::Type::et_mine_inc, mine_box_it.getNext().ul,
                           inc_slice, inc_dec_dims);

        this->pushElement(Element::makeTextElement(Element::Type::et_text,
                                                   mine_box_it.getNext().ul,
                                                   mine_slice, TEXT_COLOR));

        assert(!mine_box_it.hasNext());

        // Generate button
        SLocation button_loc = centerIn(button_box, button_dims).ul;
        this->pushButtonAt(Element::Type::et_generate_grid_btn, button_loc,
                           gen_slice, {}, Dims{5, 5});
    }
    // }}}2
    // }}}1

    // render scene {{{1
    auto renderScene(ThisWindow *window) -> void {
        LLElement *el = &this->el_sentinel;

        LLElement *active_element = nullptr;
        LLElement *focus_element = nullptr;
        this->getInteractableElements(window, &active_element, &focus_element);

        while ((el = el->next) != &this->el_sentinel) {
            bool is_active = el == active_element;
            bool is_focus = !this->mouse_down && el == focus_element;

            switch (el->val.type) {
            case Element::Type::et_empty:
                break;
            case Element::Type::et_background: {
                this->renderQuad(window, SRect{el->val.loc, el->val.dims},
                                 this->background);
            } break;
            case Element::Type::et_modal_background: {
                this->renderQuad(window, SRect{el->val.loc, el->val.dims},
                                 this->modal_background);
            } break;
            case Element::Type::et_lose_flame: {
                this->renderQuad(window, SRect{el->val.loc, el->val.dims},
                                 this->lose_flame);
            } break;
            case Element::Type::et_empty_grid_cell: {
                Texture2D texture =
                    this->getButtonTexture(false, is_active, is_focus);

                this->renderQuad(window, SRect{el->val.loc, el->val.dims},
                                 texture);
            } break;
            case Element::Type::et_revealed_grid_cell: {
                SRect bg_rect{el->val.loc, el->val.dims};

                SLocation render_loc{el->val.loc.row + 1, el->val.loc.col + 1};
                Dims render_dims{el->val.dims.width - 2,
                                 el->val.dims.height - 2};

                SRect render_rect{render_loc, render_dims};

                Cell cell = this->grid[el->val.cell_loc];

                this->renderQuad(window, bg_rect, this->button);
                this->renderQuad(window, render_rect, this->background);

                if (cell.type == CellType::ct_mine) {
                    this->baked_font.setColor(DARK_RED);
                    this->renderCenteredText(window, render_rect,
                                             STR_SLICE("*"));
                } else {
                    // TODO(bhester): change font color based on the number?
                    this->baked_font.setColor(Color::grayscale(225));

                    unsigned char cell_val = cell.number;
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
            case Element::Type::et_continue_btn:
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
    // }}}1

    auto interactable(Element::Type type) -> bool {
        switch (type) {
        case Element::Type::et_empty:
        case Element::Type::et_background:
        case Element::Type::et_lose_flame:
        case Element::Type::et_text: {
            return false;
        } break;

        case Element::Type::et_modal_background:
        case Element::Type::et_continue_btn:
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

    auto getInteractableElements(ThisWindow *window, LLElement **active_element,
                                 LLElement **focus_element) -> void {
        LLElement *el = &this->el_sentinel;

        SLocation mouse_loc = window->getMouseLocation();
        SLocation down_loc = this->mouse_down ? this->down_mouse_pos
                                              : SLocation{LONG_MIN, LONG_MIN};

        while ((el = el->prev) != &this->el_sentinel) {
            if (!interactable(el->val.type)) {
                continue;
            }

            if (el->val.contains(mouse_loc)) {
                if (*focus_element == nullptr) {
                    *focus_element = el;
                }
            }

            if (el->val.contains(down_loc)) {
                if (*active_element == nullptr) {
                    *active_element = el;
                }
            }
        }
    }

    auto render(ThisWindow *window, double dt_s) -> void {
        this->arena.reset(0);
        LLEvent::initSentinel(&this->ev_sentinel);
        LLElement::initSentinel(&this->el_sentinel);

        buildScene(window, dt_s);
        renderScene(window);
    }

    auto paint(ThisWindow *window) -> void { renderScene(window); }

    auto processEvents(ThisWindow *window) -> void {
        LLEvent *ev = nullptr;
        while ((ev = this->ev_sentinel.dequeue()) != nullptr) {
            switch (ev->val) {
            case Event::et_empty:
                break;
            case Event::et_mouse_release:
            case Event::et_right_mouse_release: {
                // FIXME(bhester): this may not be necessary in some cases... do
                // I want to propagate active element status?
                window->needs_repaint = true;
            } break;
            case Event::et_mouse_press: {
                LLElement *el = &this->el_sentinel;

                bool input_consumed = false;
                while ((el = el->prev) != &this->el_sentinel &&
                       !input_consumed) {
                    if (el->val.contains(this->down_mouse_pos)) {
                        this->handleLeftClick(window, el, &input_consumed);
                    }
                }

                if (!input_consumed && this->lose_animation_playing) {
                    this->lose_animation_playing = false;
                    this->lose_animation_source = {};
                    this->lose_animation_t = 0.0;
                }
            } break;
            case Event::et_right_mouse_press: {
                LLElement *el = &this->el_sentinel;

                bool input_consumed = false;
                while ((el = el->prev) != &this->el_sentinel &&
                       !input_consumed) {
                    if (el->val.contains(this->down_right_mouse_pos)) {
                        switch (el->val.type) {
                        case Element::Type::et_empty:
                        case Element::Type::et_background:
                        case Element::Type::et_lose_flame:
                        case Element::Type::et_continue_btn:
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
                        case Element::Type::et_modal_background: {
                            input_consumed = true;
                        } break;
                        case Element::Type::et_empty_grid_cell: {
                            input_consumed = true;

                            flagCell(&this->grid, el->val.cell_loc);

                            window->needs_rerender = true;
                        } break;
                        case Element::Type::et_flagged_grid_cell: {
                            input_consumed = true;

                            unflagCell(&this->grid, el->val.cell_loc);

                            window->needs_rerender = true;
                        } break;
                        }
                    }
                }
            } break;
            case Event::et_animation: {
                window->needs_rerender = true;
            } break;
            }
        }
    }

    auto handleLeftClick(ThisWindow *window, LinkedList<Element> *el,
                         bool *input_consumed) -> void {
        switch (el->val.type) {
        case Element::Type::et_empty:
        case Element::Type::et_background:
        case Element::Type::et_lose_flame:
        case Element::Type::et_revealed_grid_cell:
        case Element::Type::et_flagged_grid_cell:
        case Element::Type::et_maybe_flagged_grid_cell:
        case Element::Type::et_text:
            break;
        case Element::Type::et_modal_background: {
            *input_consumed = true;
        } break;
        case Element::Type::et_continue_btn: {
            *input_consumed = true;

            Location cell_loc = el->val.cell_loc;
            this->grid[cell_loc].display_type = CellDisplayType::cdt_flag;

            window->needs_rerender = true;
        } break;
        case Element::Type::et_restart_btn: {
            *input_consumed = true;

            // clean up solver
            this->solver.resetEpoch(&this->grid);

            this->grid_arena.reset(0);
            this->grid = Grid{};

            window->needs_rerender = true;
        } break;
        case Element::Type::et_empty_grid_cell: {
            *input_consumed = true;

            if (this->preview_grid) {
                printf("Generating grid\n");

                this->preview_grid = false;
                this->grid =
                    generateGrid(&this->grid_arena,
                                 Dims{this->width_input, this->height_input},
                                 this->mine_input, el->val.cell_loc);
            } else {
                Cell &cell = this->grid[el->val.cell_loc];
                if (cell.type == CellType::ct_mine) {
                    // just set the cell as the value and we will render the
                    // mine and the "You Lose!" modal based on the fact that
                    // this is showing
                    cell.display_type = CellDisplayType::cdt_value;

                    this->lose_animation_playing = true;
                    this->lose_animation_t = 0.0;
                    this->lose_animation_source = el->val.cell_loc;
                } else {
                    uncoverSelfAndNeighbors(&this->grid, el->val.cell_loc);
                }
            }

            this->did_step = false;
            this->last_work_rule = 0;
            this->last_step_success = false;

            window->needs_rerender = true;
        } break;
        case Element::Type::et_generate_grid_btn: {
            *input_consumed = true;

            printf("Previewing grid\n");

            this->preview_grid = true;
            window->needs_rerender = true;
        } break;
        case Element::Type::et_step_solver_btn: {
            if (el->val.disabled) {
                break;
            }

            printf("Stepping solver\n");

            bool did_work = this->solver.step(&this->grid);
            if (did_work) {
                printf("Solver did work\n");
                this->did_step = true;
                this->last_work_rule = this->solver.state.last_work_rule;
                this->last_step_success = true;
            } else {
                printf("Solver did not do work\n");
                this->did_step = true;
                this->last_work_rule = 0;
                this->last_step_success = false;
            }

            window->needs_rerender = true;
        } break;
        case Element::Type::et_width_inc: {
            *input_consumed = true;

            if (this->width_input < 99) {
                ++this->width_input;
                window->needs_rerender = true;
            }
        } break;
        case Element::Type::et_width_dec: {
            *input_consumed = true;

            if (this->width_input > 1) {
                --this->width_input;
                window->needs_rerender = true;
            }
        } break;
        case Element::Type::et_height_inc: {
            *input_consumed = true;

            if (this->height_input < 99) {
                ++this->height_input;
                window->needs_rerender = true;
            }
        } break;
        case Element::Type::et_height_dec: {
            *input_consumed = true;

            if (this->height_input > 1) {
                --this->height_input;
                window->needs_rerender = true;
            }
        } break;
        case Element::Type::et_mine_inc: {
            *input_consumed = true;

            if (this->mine_input < 99) {
                ++this->mine_input;
                window->needs_rerender = true;
            }
        } break;
        case Element::Type::et_mine_dec: {
            *input_consumed = true;

            if (this->mine_input > 1) {
                --this->mine_input;
                window->needs_rerender = true;
            }
        } break;
        }
    }

    auto pushButtonAt(Element::Type type, SLocation loc, StrSlice text,
                      Dims min_dims = {}, Dims padding = {},
                      bool disabled = false) -> Element * {
        Dims render_dims = this->getButtonDims(text, min_dims, padding);
        Element *element = this->pushElement(Element::makeButtonElement(
            type, loc, render_dims, padding, text, disabled));

        return element;
    }

    auto pushButtonCenteredIn(Element::Type type, SRect rect, StrSlice text,
                              Dims min_dims = {}, Dims padding = {},
                              bool disabled = false) -> Element * {
        Dims render_dims = this->getButtonDims(text, min_dims, padding);
        SRect centered = centerIn(rect, render_dims);

        Element *element = this->pushElement(Element::makeButtonElement(
            type, centered.ul, centered.dims, padding, text, disabled));

        return element;
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

    auto pushElement(Element el) -> Element * {
        LLElement *ll = this->arena.pushT(LLElement{el});
        this->el_sentinel.enqueue(ll);

        return &ll->val;
    }

    auto getButtonTexture(bool disabled, bool active, bool focus) -> Texture2D {
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

    auto renderButton(ThisWindow *window, SRect rect, Dims padding,
                      StrSlice text, bool disabled, bool active, bool focus)
        -> void {
        SRect render_rect = rect;
        if (rect.dims.area() == 0) {
            render_rect.dims = this->baked_font.getTextDims(text);
        }

        Texture2D btn_texture = this->getButtonTexture(disabled, active, focus);
        this->renderQuad(window, render_rect, btn_texture);

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

    auto renderQuad(ThisWindow *window, SRect rect, Texture2D texture) -> void {
        window->renderQuad(&this->quad_program, rect, texture);
    }

    auto renderText(ThisWindow *window, SLocation loc, StrSlice text) -> void {
        window->renderText(&this->baked_font, loc, text);
    }

    auto renderText(ThisWindow *window, SRect rect, StrSlice text) -> void {
        window->renderText(&this->baked_font, rect, text);
    }

    auto renderCenteredText(ThisWindow *window, SRect rect, StrSlice text)
        -> void {
        window->renderCenteredText(&this->baked_font, rect, text);
    }
};

auto initContext(Arena *arena, Window<Context> *window, Context *ctx) -> void {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glFrontFace(GL_CCW);
    glClearColor(0.0, 0.0, 0.0, 0.0);

    GridSolver::Rule flag_remaining_rule =
        makeRule(&flag_remaining_cells, STR_SLICE("flag_remaining_cells"));
    GridSolver::Rule show_hidden_rule =
        makeRule(&show_hidden_cells, STR_SLICE("show_hidden_cells"));
    GridSolver::Rule click_remaining_rule =
        makeRule(&click_remaining_cells, STR_SLICE("click_remaining_cells"));

    initSolver(&ctx->solver);
    ctx->solver.registerRule(arena, flag_remaining_rule);
    ctx->solver.registerRule(arena, show_hidden_rule);
    ctx->solver.registerRule(arena, click_remaining_rule);
    registerPatterns(arena, &ctx->solver);

    ctx->grid_arena = arena->subarena(KILOBYTES(4)); // pull out 4K
    ctx->arena = arena->subarena(0);                 // and use the rest here

    ctx->quad_program = window->makeBaseQuadProgram(&ctx->arena);
    ctx->baked_font =
        window->makeBaseBakedFont(&ctx->arena, "./fonts/Roboto-Black.ttf", 20);

    ctx->button = makeTexture();
    ctx->disabled_button = makeTexture();
    ctx->focus_button = makeTexture();
    ctx->active_button = makeTexture();
    ctx->background = makeTexture();
    ctx->modal_background = makeTexture();

    ctx->width_input = 10;
    ctx->height_input = 10;
    ctx->mine_input = 15;

    ctx->did_step = false;
    ctx->last_work_rule = 0;
    ctx->last_step_success = false;

    // initialize texture values

    ctx->button.grayscale(&ctx->arena, 200, Dims{1, 1});
    ctx->focus_button.grayscale(&ctx->arena, 180, Dims{1, 1});
    ctx->active_button.grayscale(&ctx->arena, 160, Dims{1, 1});
    ctx->disabled_button.grayscale(&ctx->arena, 220, Dims{1, 1});
    ctx->background.grayscale(&ctx->arena, 120, Dims{1, 1});
    ctx->modal_background.grayscale(&ctx->arena, 0, 128, Dims{1, 1});
    ctx->lose_flame.grayscale(&ctx->arena, 255, Dims{1, 1});

    LinkedList<Context::Event>::initSentinel(&ctx->ev_sentinel);
    LinkedList<Context::Element>::initSentinel(&ctx->el_sentinel);
}

auto deinitContext(Context *ctx) -> void {
    deleteBakedFont(&ctx->baked_font);
    deleteQuadProgram(&ctx->quad_program);

    deleteTexture(&ctx->lose_flame);
    deleteTexture(&ctx->modal_background);
    deleteTexture(&ctx->background);
    deleteTexture(&ctx->active_button);
    deleteTexture(&ctx->focus_button);
    deleteTexture(&ctx->disabled_button);
    deleteTexture(&ctx->button);
}

int main() {
    testGrid();

    initGLFW(&handle_error);

    Arena arena = makeArena(MEGABYTES(10));

    Window<Context> window{};
    initWindow(&arena, &window, 800, 600, "Hello, world");

    initContext(&arena, &window, &window.ctx);
    window.setPos(500, 500);
    window.show();

    double fps = 60.0;
    double mspf = 1000.0 / fps;

    double last_time = glfwGetTime();
    while (!window.shouldClose()) {
        double next_time = glfwGetTime();

        window.render(next_time - last_time);
        window.processEvents();

        if (window.isKeyPressed(GLFW_KEY_Q)) {
            window.close();
        } else {
            double frame_time = glfwGetTime();
            double rest_time_ms = 1000.0 * (frame_time - last_time);

            last_time = next_time;

            if (rest_time_ms < mspf) {
                unsigned int sleep_time = mspf - rest_time_ms;
                usleep(sleep_time);
            }
        }
    }

    deinitContext(&window.ctx);
    deleteWindow(&window);
    freeArena(&arena);
    glfwTerminate();
    return 0;
}
