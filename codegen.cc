#include "dirutils.cc"
#include "op.cc"
#include "slice.cc"
#include "strslice.cc"
#include "tokens.cc"
#include "utils.cc"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

static StrSlice PAT_SUFFIX = STR_SLICE(".pat");
static StrSlice OUT_DIR = STR_SLICE("generated/");
static StrSlice OUT_PREFIX = STR_SLICE("pat_");
static StrSlice OUT_SUFFIX = STR_SLICE(".cc");

auto usage(char const *path) -> void {
    fprintf(stderr, "%s [filename]\n", path);
    fprintf(stderr, "\n");
    fprintf(stderr, "filename - name of pattern file (.pat) to compile\n");
}

auto getContents(char const *filename) -> Op<StrSlice> {
    FILE *f = fopen(filename, "r");
    if (f == nullptr) {
        return Op<StrSlice>::empty();
    }

    fseek(f, 0, SEEK_END);
    size_t file_len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *ptr = new char[file_len];
    if (ptr == nullptr) {
        return Op<StrSlice>::empty();
    }

    size_t nread = fread(ptr, file_len, 1, f);
    if (nread != 1) {
        delete[] ptr;
        return Op<StrSlice>::empty();
    }

    fclose(f);

    return StrSlice{ptr, file_len};
}

struct FileArgs {
    StrSlice in_name;
    StrSlice in_root;
    StrSlice in_prefix;
    StrSlice in_suffix;
    StrSlice out_slice;
    StrSlice out_root;
    char const *out_name;
};

auto getFileArgs(char const *in_name) -> FileArgs {
    StrSlice in_slice = str_slice(in_name);

    if (!endsWith(in_slice, PAT_SUFFIX)) {
        fprintf(stderr, "Invalid in file %s\n", in_name);
        EXIT(1);
    }

    size_t last_path_sep = lastIdxOf(in_slice, '/');
    size_t prefix_end = (last_path_sep == in_slice.len) ? 0 : last_path_sep;
    size_t file_start = (last_path_sep == in_slice.len) ? 0 : last_path_sep + 1;

    size_t in_root_len = (in_slice.len - file_start) - PAT_SUFFIX.len;

    StrSlice in_prefix = in_slice.slice(0, prefix_end);
    StrSlice in_root = in_slice.slice(file_start, file_start + in_root_len);
    StrSlice in_suffix = in_slice.slice(file_start + in_root_len);

    size_t out_len =
        in_root.len + OUT_DIR.len + OUT_PREFIX.len + OUT_SUFFIX.len;

    char *ptr = new char[out_len + 1]; // len + 1 for null terminator
    snprintf(ptr, out_len + 1, "%.*s%.*s%.*s%.*s", STR_ARGS(OUT_DIR),
             STR_ARGS(OUT_PREFIX), STR_ARGS(in_root), STR_ARGS(OUT_SUFFIX));

    StrSlice out_slice{ptr, out_len};
    StrSlice out_root =
        out_slice.slice(OUT_DIR.len, out_slice.len - OUT_SUFFIX.len);

    return FileArgs{in_slice,  in_root,  in_prefix, in_suffix,
                    out_slice, out_root, ptr};
}

struct Walls {
    unsigned int north;
    unsigned int east;
    unsigned int south;
    unsigned int west;

    explicit Walls() : north(0), east(0), south(0), west(0) {}
};

enum PatternCellType {
    pct_any,
    pct_number,
    pct_literal,
    pct_not_flag,
    pct_flag,
    pct_execute,
};

struct PatternCell {
    PatternCellType type;
    unsigned char number;
};

struct Action {
    PatternCell pre_cond;
    PatternCell post_cond;
    Location loc;
};

struct Pattern {
    Dims dims;
    Walls walls;
    Slice<PatternCell> cells;
    Slice<Action> actions;
};

auto readPattern(StrSlice _contents) -> Pattern {
    size_t width = 0;
    size_t height = 0;

    // read once to get dimensions and assert matching dimensions

    bool first_char = true;
    bool first_part = true;
    bool first_row = true;
    size_t part_height = 0;
    size_t row_width = 0;

    Walls walls{};
    StrSlice cur_contents{};
    StrSlice pattern_start = _contents;

    cur_contents = _contents;
    while (cur_contents.len > 0) {
        Token token_resp = nextToken(cur_contents);
        switch (token_resp.type) {
        case tt_wall: {
            if (!first_char) {
                fprintf(stderr, "Invalid wall directive\n");
                EXIT(1);
            }

            Token next = nextToken(cur_contents);
            do {
                if (next.type != TokenType::tt_space) {
                    fprintf(stderr, "Expected a space\n");
                    EXIT(1);
                }

                unsigned int *target = nullptr;

                Token target_tok = nextToken(cur_contents);
                switch (target_tok.type) {
                case tt_north: {
                    target = &walls.north;
                } break;
                case tt_east: {
                    target = &walls.east;
                } break;
                case tt_south: {
                    target = &walls.south;
                } break;
                case tt_west: {
                    target = &walls.west;
                } break;
                case tt_any:
                case tt_number:
                case tt_literal:
                case tt_not_flag:
                case tt_flag:
                case tt_execute:
                case tt_space:
                case tt_newline:
                case tt_wall:
                case tt_eof: {
                    fprintf(stderr, "Unexpected token type\n");
                    EXIT(1);
                } break;
                }

                expectToken(cur_contents, TokenType::tt_space);

                Token number_tok =
                    expectToken(cur_contents, TokenType::tt_literal);
                assert(target != nullptr && "Unset target");
                *target = number_tok.data;

                next = nextToken(cur_contents);
            } while (next.type != TokenType::tt_eof &&
                     next.type != TokenType::tt_newline);

            expectToken(cur_contents, TokenType::tt_newline);

            // set the location we should use when reading pattern info
            pattern_start = cur_contents;
        } break;
        case tt_newline:
        case tt_eof: {
            if (row_width == 0) {
                if (first_part) {
                    if (part_height == 0) {
                        fprintf(stderr, "Invalid dimensions (%d)\n", __LINE__);
                        EXIT(1);
                    }

                    height = part_height;
                    first_part = false;
                } else {
                    if (part_height != height) {
                        fprintf(stderr, "Invalid dimensions (%d)\n", __LINE__);
                        EXIT(1);
                    }
                }

                part_height = 0;
            } else {
                ++part_height;

                if (first_row) {
                    width = row_width;
                    first_row = false;
                } else {
                    if (row_width != width) {
                        fprintf(stderr, "Invalid dimensions (%d)\n", __LINE__);
                        EXIT(1);
                    }
                }

                row_width = 0;
            }
        } break;
        case tt_literal: {
            for (char c : token_resp.slice) {
                if (!isMinesweeperNumber(c - '0')) {
                    fprintf(stderr, "Invalid minesweeper number %c\n", c);
                    EXIT(1);
                }
                ++row_width;
            }
        } break;
        case tt_any:
        case tt_number:
        case tt_not_flag:
        case tt_flag:
        case tt_execute: {
            ++row_width;
        } break;
        case tt_north:
        case tt_east:
        case tt_south:
        case tt_west:
        case tt_space: {
            fprintf(stderr, "Unexpected token\n");
            EXIT(1);
        } break;
        }

        first_char = false;
    }

    // read again to fill in pattern info

    Dims dims{width, height};

    PatternCell *cells_ptr = new PatternCell[dims.area()];
    Slice<PatternCell> cells{cells_ptr, dims.area()};

    size_t actions_len = 0;
    Action *actions_ptr = new Action[dims.area()];

    if (cells_ptr == nullptr || actions_ptr == nullptr) {
        fprintf(stderr, "Out of memory\n");
        EXIT(1);
    }

    cur_contents = pattern_start;
    for (size_t r = 0; r < dims.height; ++r) {
        for (size_t c = 0; c < dims.width; ++c) {
            PatternCell &in_cell = cells[r * dims.width + c];

            Token token_resp = nextToken(cur_contents);
            switch (token_resp.type) {
            case tt_any: {
                in_cell = PatternCell{PatternCellType::pct_any};
            } break;
            case tt_number: {
                in_cell = PatternCell{PatternCellType::pct_number};
            } break;
            case tt_literal: {
                for (char ch : token_resp.slice) {
                    unsigned char mnum = ch - '0';
                    assert(isMinesweeperNumber(mnum) &&
                           "Invalid minesweeper number");

                    // cannot use in_cell here because we have multiple columns
                    cells[r * dims.width + c] =
                        PatternCell{PatternCellType::pct_literal, mnum};

                    ++c;
                }
                --c; // we increment one too many times in the above loop
            } break;
            case tt_not_flag: {
                in_cell = PatternCell{PatternCellType::pct_not_flag};
            } break;
            case tt_flag:
            case tt_execute:
            case tt_space:
            case tt_newline:
            case tt_wall:
            case tt_north:
            case tt_east:
            case tt_south:
            case tt_west:
            case tt_eof: {
                fprintf(stderr, "Unexpected token type\n");
                EXIT(1);
            } break;
            }
        }
        // read new line
        expectToken(cur_contents, TokenType::tt_newline);
    }

    // read new line
    expectToken(cur_contents, TokenType::tt_newline);

    for (size_t r = 0; r < dims.height; ++r) {
        for (size_t c = 0; c < dims.width; ++c) {
            PatternCell in_cell = cells[r * dims.width + c];
            PatternCell out_cell{};

            Token token_resp = nextToken(cur_contents);
            switch (token_resp.type) {
            case tt_any: {
                out_cell = PatternCell{PatternCellType::pct_any};
            } break;
            case tt_number: {
                out_cell = PatternCell{PatternCellType::pct_number};
            } break;
            case tt_literal: {
                for (char ch : token_resp.slice) {
                    unsigned char mnum = ch - '0';
                    assert(isMinesweeperNumber(mnum) &&
                           "Invalid minesweeper number");

                    in_cell = cells[r * dims.width + c];
                    out_cell = PatternCell{PatternCellType::pct_literal, mnum};

                    if (out_cell.type != in_cell.type ||
                        (out_cell.type == PatternCellType::pct_literal &&
                         out_cell.number != in_cell.number)) {
                        fprintf(stderr, "Invalid action\n");
                        EXIT(1);
                    }

                    ++c;
                }
                --c; // we increment one too many times in the above loop
            } break;
            case tt_not_flag: {
                out_cell = PatternCell{PatternCellType::pct_not_flag};
            } break;
            case tt_flag: {
                out_cell = PatternCell{PatternCellType::pct_flag};
            } break;
            case tt_execute: {
                out_cell = PatternCell{PatternCellType::pct_execute};
            } break;
            case tt_space:
            case tt_newline:
            case tt_wall:
            case tt_north:
            case tt_east:
            case tt_south:
            case tt_west:
            case tt_eof: {
                fprintf(stderr, "Unexpected token type\n");
                EXIT(1);
            } break;
            }

            switch (out_cell.type) {
            case pct_flag:
            case pct_execute: {
                actions_ptr[actions_len++] =
                    Action{in_cell, out_cell, Location{r, c}};
            } break;
            default: {
                if (out_cell.type != in_cell.type ||
                    (out_cell.type == PatternCellType::pct_literal &&
                     out_cell.number != in_cell.number)) {
                    fprintf(stderr, "Invalid action\n");
                    EXIT(1);
                }
            } break;
            }
        }
        // read new line
        expectToken(cur_contents, TokenType::tt_newline);
    }
    expectToken(cur_contents, TokenType::tt_eof);

    return Pattern{dims, walls, cells, Slice<Action>{actions_ptr, actions_len}};
}

auto writeStructDefinition(FILE *out, StrSlice out_fn, Dims dims) -> void {
    fprintf(out, "struct Pattern_%.*s {\n", STR_ARGS(out_fn));
    for (size_t r = 0; r < dims.height; ++r) {
        for (size_t c = 0; c < dims.width; ++c) {
            fprintf(out, "    Cell &c_%zu_%zu;\n", r, c);
        }
    }
    fprintf(out, "};\n");
    fprintf(out, "\n");
}

auto writePatternMatchCell(FILE *out, PatternCell cell, Location loc) -> void {
    switch (cell.type) {
    case pct_any: {
    } break;
    case pct_number: {
        fprintf(out,
                "    if (pat.c_%zu_%zu.display_type != "
                "CellDisplayType::cdt_value ||\n",
                loc.row, loc.col);
        fprintf(out, "        pat.c_%zu_%zu.type != CellType::ct_number) {\n",
                loc.row, loc.col);
        fprintf(out, "        return false;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    case pct_literal: {
        fprintf(out,
                "    if (pat.c_%zu_%zu.display_type != "
                "CellDisplayType::cdt_value ||\n",
                loc.row, loc.col);
        fprintf(out,
                "        pat.c_%zu_%zu.type != CellType::ct_number || "
                "pat.c_%zu_%zu.number != %d) {\n",
                loc.row, loc.col, loc.row, loc.col, cell.number);
        fprintf(out, "        return false;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    case pct_not_flag: {
        fprintf(out,
                "    if (pat.c_%zu_%zu.display_type == "
                "CellDisplayType::cdt_flag) {\n",
                loc.row, loc.col);
        fprintf(out, "        return false;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    case pct_flag:
    case pct_execute: {
        assert(0 && "Bad pattern");
    } break;
    }
}

auto writeActionCell(FILE *out, PatternCell pre_cond, PatternCell post_cond,
                     Location loc) -> void {
    switch (post_cond.type) {
    case pct_flag: {
        fprintf(out,
                "    if (pat.c_%zu_%zu.display_type == "
                "CellDisplayType::cdt_hidden ||\n",
                loc.row, loc.col);
        fprintf(out,
                "        pat.c_%zu_%zu.display_type == "
                "CellDisplayType::cdt_maybe_flag) {\n",
                loc.row, loc.col);
        fprintf(
            out,
            "        pat.c_%zu_%zu.display_type = CellDisplayType::cdt_flag;\n",
            loc.row, loc.col);
        fprintf(out, "        did_work = true;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    case pct_execute: {
        fprintf(out,
                "    if (pat.c_%zu_%zu.display_type == "
                "CellDisplayType::cdt_maybe_flag ||\n",
                loc.row, loc.col);
        fprintf(out,
                "        pat.c_%zu_%zu.display_type == "
                "CellDisplayType::cdt_hidden) {\n",
                loc.row, loc.col);
        fprintf(out,
                "        // mark as hidden to remove possible maybe_flag\n");
        fprintf(out,
                "        pat.c_%zu_%zu.display_type = "
                "CellDisplayType::cdt_hidden;\n",
                loc.row, loc.col);
        fprintf(out,
                "        uncoverSelfAndNeighbors(grid, Location{row + %zu, col "
                "+ %zu});\n",
                loc.row, loc.col);
        fprintf(out, "        did_work = true;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    case pct_any:
    case pct_number:
    case pct_not_flag:
    case pct_literal: {
        assert(0 && "Bad pattern");
    } break;
    }
}

auto writeCheckPatternFunction(FILE *out, StrSlice out_fn, Pattern pattern)
    -> void {
    fprintf(out,
            "auto checkPattern_%.*s(Grid grid, size_t row, size_t col, "
            "Pattern_%.*s pat) -> bool {\n",
            STR_ARGS(out_fn), STR_ARGS(out_fn));
    fprintf(out, "    // check pattern match\n");
    fprintf(out, "\n");

    Dims dims = pattern.dims;
    for (size_t r = 0; r < dims.height; ++r) {
        for (size_t c = 0; c < dims.width; ++c) {
            PatternCell cell = pattern.cells[r * dims.width + c];
            writePatternMatchCell(out, cell, Location{r, c});
        }
    }

    fprintf(out, "    // pattern has been matched\n");
    fprintf(out, "\n");
    fprintf(out, "    bool did_work = false;\n");
    fprintf(out, "\n");

    for (Action action : pattern.actions) {
        writeActionCell(out, action.pre_cond, action.post_cond, action.loc);
    }

    fprintf(out, "    return did_work;\n");
    fprintf(out, "}\n");
    fprintf(out, "\n");
}

typedef auto(DimsAdj)(Dims dims) -> Dims;
typedef auto(LocAdj)(Dims dims, Location loc) -> Location;

static inline auto idDimAdj(Dims dims) -> Dims { return dims; }
static inline auto dimAdj90(Dims dims) -> Dims {
    return Dims{dims.height, dims.width};
}

static inline auto idLocAdj(Dims dims, Location loc) -> Location { return loc; }

static inline auto locAdj90n(Dims dims, Location loc) -> Location {
    return Location{loc.col, dims.height - 1 - loc.row};
}

static inline auto locAdj180n(Dims dims, Location loc) -> Location {
    return Location{dims.height - 1 - loc.row, dims.width - 1 - loc.col};
}

static inline auto locAdj270n(Dims dims, Location loc) -> Location {
    return Location{dims.width - 1 - loc.col, loc.row};
}

static inline auto locAdj0r(Dims dims, Location loc) -> Location {
    return Location{loc.row, dims.width - 1 - loc.col};
}

static inline auto locAdj90r(Dims dims, Location loc) -> Location {
    return Location{loc.col, loc.row};
}

static inline auto locAdj180r(Dims dims, Location loc) -> Location {
    return Location{dims.height - 1 - loc.row, loc.col};
}

static inline auto locAdj270r(Dims dims, Location loc) -> Location {
    return Location{dims.width - 1 - loc.col, dims.height - 1 - loc.row};
}

template <DimsAdj *DAdj, LocAdj *LAdj>
auto writeFunctionGeneric(FILE *out, StrSlice out_fn, Dims dims, size_t num,
                          char suffix) -> void {
    Dims dims_adj = DAdj(dims);

    fprintf(out,
            "auto %.*s_%zu%c(Grid grid, size_t row, size_t col) -> bool {\n",
            STR_ARGS(out_fn), num, suffix);
    fprintf(out, "    size_t pat_width = %zu;\n", dims_adj.width);
    fprintf(out, "    size_t pat_height = %zu;\n", dims_adj.height);
    fprintf(out, "\n");
    fprintf(out, "    if ((col + pat_width) > grid.dims.width) {\n");
    fprintf(out, "        return false;\n");
    fprintf(out, "    }\n");
    fprintf(out, "    if ((row + pat_height) > grid.dims.height) {\n");
    fprintf(out, "        return false;\n");
    fprintf(out, "    }\n");
    fprintf(out, "\n");

    fprintf(out, "    Pattern_%.*s pat{\n", STR_ARGS(out_fn));
    for (size_t r = 0; r < dims.height; ++r) {
        for (size_t c = 0; c < dims.width; ++c) {
            Location loc_adj = LAdj(dims, Location{r, c});
            fprintf(out, "        grid[row + %zu][col + %zu],\n", loc_adj.row,
                    loc_adj.col);
        }
    }
    fprintf(out, "    };\n");
    fprintf(out, "\n");

    fprintf(out, "    return checkPattern_%.*s(grid, row, col, pat);\n",
            STR_ARGS(out_fn));
    fprintf(out, "};\n");
    fprintf(out, "\n");
}

auto writeFunction_0n(FILE *out, StrSlice out_fn, Dims dims) -> void {
    writeFunctionGeneric<idDimAdj, idLocAdj>(out, out_fn, dims, 0, 'n');
}

auto writeFunction_90n(FILE *out, StrSlice out_fn, Dims dims) -> void {
    writeFunctionGeneric<dimAdj90, locAdj90n>(out, out_fn, dims, 90, 'n');
}

auto writeFunction_180n(FILE *out, StrSlice out_fn, Dims dims) -> void {
    writeFunctionGeneric<idDimAdj, locAdj180n>(out, out_fn, dims, 180, 'n');
}

auto writeFunction_270n(FILE *out, StrSlice out_fn, Dims dims) -> void {
    writeFunctionGeneric<dimAdj90, locAdj270n>(out, out_fn, dims, 270, 'n');
}

auto writeFunction_0r(FILE *out, StrSlice out_fn, Dims dims) -> void {
    writeFunctionGeneric<idDimAdj, locAdj0r>(out, out_fn, dims, 0, 'r');
}

auto writeFunction_90r(FILE *out, StrSlice out_fn, Dims dims) -> void {
    writeFunctionGeneric<dimAdj90, locAdj90r>(out, out_fn, dims, 90, 'r');
}

auto writeFunction_180r(FILE *out, StrSlice out_fn, Dims dims) -> void {
    writeFunctionGeneric<idDimAdj, locAdj180r>(out, out_fn, dims, 180, 'r');
}

auto writeFunction_270r(FILE *out, StrSlice out_fn, Dims dims) -> void {
    writeFunctionGeneric<dimAdj90, locAdj270r>(out, out_fn, dims, 270, 'r');
}

auto writeFunction(FILE *out, StrSlice out_fn, Pattern pattern) -> void {
    fprintf(out, "#pragma once\n");
    fprintf(out, "\n");
    fprintf(out, "#include \"../dirutils.cc\"\n");
    fprintf(out, "#include \"../grid.cc\"\n");
    fprintf(out, "\n");
    fprintf(out, "#include <sys/types.h>\n");
    fprintf(out, "\n");

    Dims dims = pattern.dims;

    writeStructDefinition(out, out_fn, dims);
    writeCheckPatternFunction(out, out_fn, pattern);

    writeFunction_0n(out, out_fn, dims);
    writeFunction_90n(out, out_fn, dims);
    writeFunction_180n(out, out_fn, dims);
    writeFunction_270n(out, out_fn, dims);

    writeFunction_0r(out, out_fn, dims);
    writeFunction_90r(out, out_fn, dims);
    writeFunction_180r(out, out_fn, dims);
    writeFunction_270r(out, out_fn, dims);

    fprintf(out, "auto %.*s(Grid grid, size_t row, size_t col) -> bool {\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_0n = %.*s_0n(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_90n = %.*s_90n(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_180n = %.*s_180n(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_270n = %.*s_270n(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_0r = %.*s_0r(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_90r = %.*s_90r(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_180r = %.*s_180r(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_270r = %.*s_270r(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "\n");
    fprintf(out, "    return did_work_0n || did_work_180n || did_work_90n || "
                 "did_work_270n ||\n");
    fprintf(out, "           did_work_0r || did_work_180r || did_work_90r || "
                 "did_work_270r;\n");
    fprintf(out, "}\n");
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        usage(argv[0]);
        EXIT(1);
    }

    char const *in_name = argv[1];
    FileArgs file_args = getFileArgs(in_name);

    Op<StrSlice> contents_op = getContents(in_name);
    if (!contents_op.valid) {
        fprintf(stderr, "Failed to read pattern file %s\n", in_name);
        EXIT(1);
    }

    Pattern pattern = readPattern(contents_op.get());

    char const *out_name = file_args.out_name;
    FILE *out = fopen(out_name, "w");
    if (out == nullptr) {
        fprintf(stderr, "Failed to open output file %s\n", out_name);
        EXIT(1);
    }

    writeFunction(out, file_args.out_root, pattern);
    fflush(out);
    fclose(out);
}
