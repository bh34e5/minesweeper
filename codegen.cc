#include "dirutils.cc"
#include "op.cc"
#include "slice.cc"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef Slice<char const> StrSlice;

auto str_slice(char const *s) -> StrSlice { return StrSlice{s, strlen(s)}; }
auto str_slice(char const *s, size_t len) -> StrSlice {
    return StrSlice{s, len};
}

#define STR_SLICE(s) (str_slice(s, sizeof(s) - 1))
#define STR_ARGS(ss) (int)ss.len, ss.ptr

static StrSlice PAT_SUFFIX = STR_SLICE(".pat");
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

auto endsWith(StrSlice haystack, StrSlice needle) -> bool {
    if (haystack.len < needle.len) {
        return false;
    }

    size_t offset = haystack.len - needle.len;
    for (size_t i = 0; i < needle.len; ++i) {
        if (haystack[offset + i] != needle[i]) {
            return false;
        }
    }

    return true;
}

auto getOutName(char const *in_name) -> char const * {
    StrSlice in_slice = str_slice(in_name);

    if (!endsWith(in_slice, PAT_SUFFIX)) {
        fprintf(stderr, "Invalid in file %s\n", in_name);
        exit(1);
    }

    size_t in_root_len = in_slice.len - PAT_SUFFIX.len;
    StrSlice in_root = in_slice.slice(0, in_root_len);

    size_t out_len = in_root.len + OUT_PREFIX.len + OUT_SUFFIX.len;

    char *ptr = new char[out_len + 1]; // len + 1 for null terminator
    snprintf(ptr, out_len + 1, "%.*s%.*s%.*s", STR_ARGS(OUT_PREFIX),
             STR_ARGS(in_root), STR_ARGS(OUT_SUFFIX));

    return ptr;
}

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

struct Shape {
    Dims dims;
    Slice<PatternCell> cells;
};

struct Action {
    PatternCell pre_cond;
    PatternCell post_cond;
    Location loc;
};

struct Pattern {
    Shape shape;
    Slice<Action> actions;
};

enum TokenType {
    tt_any,
    tt_number,
    tt_literal,
    tt_not_flag,
    tt_flag,
    tt_execute,
    tt_newline,
    tt_eof,
};

struct Token {
    TokenType type;
    char data;
};

struct TokenResponse {
    StrSlice next_slice;
    Token token;
};

auto isMinesweeperDigit(char c) -> bool { return '0' <= c && c <= '8'; }

auto nextToken(StrSlice contents) -> TokenResponse {
    if (contents.len == 0) {
        return TokenResponse{contents, Token{TokenType::tt_eof, EOF}};
    }

    char c = contents[0];
    switch (c) {
    case 'n': {
        return TokenResponse{contents.slice(1), Token{TokenType::tt_number}};
    } break;
    case '_': {
        return TokenResponse{contents.slice(1), Token{TokenType::tt_any}};
    } break;
    case 'f': {
        return TokenResponse{contents.slice(1), Token{TokenType::tt_not_flag}};
    } break;
    case 'F': {
        return TokenResponse{contents.slice(1), Token{TokenType::tt_flag}};
    } break;
    case 'x': {
        return TokenResponse{contents.slice(1), Token{TokenType::tt_execute}};
    } break;
    case '\r': {
        if (contents.len > 1 && contents[1] == '\n') {
            return TokenResponse{contents.slice(2),
                                 Token{TokenType{tt_newline}}};
        }
        return TokenResponse{contents.slice(1), Token{TokenType{tt_newline}}};
    } break;
    case '\n': {
        return TokenResponse{contents.slice(1), Token{TokenType{tt_newline}}};
    } break;
    default: {
        if (isMinesweeperDigit(c)) {
            return TokenResponse{contents.slice(1),
                                 Token{TokenType::tt_literal, c}};
        }

        fprintf(stderr, "Unexpected character '%c'\n", c);
        exit(1);
    } break;
    }
}

auto readPattern(StrSlice contents) -> Pattern {
    size_t width = 0;
    size_t height = 0;

    // read once to get dimensions and assert matching dimensions

    bool first_part = true;
    bool first_row = true;
    size_t part_height = 0;
    size_t row_width = 0;

    StrSlice cur_contents = contents;
    while (cur_contents.len > 0) {
        TokenResponse token_resp = nextToken(cur_contents);
        if (token_resp.token.type == TokenType::tt_newline ||
            token_resp.token.type == TokenType::tt_eof) {
            if (row_width == 0) {
                if (first_part) {
                    if (part_height == 0) {
                        fprintf(stderr, "Invalid dimensions (%d)\n", __LINE__);
                        exit(1);
                    }

                    height = part_height;
                    first_part = false;
                } else {
                    if (part_height != height) {
                        fprintf(stderr, "Invalid dimensions (%d)\n", __LINE__);
                        exit(1);
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
                        exit(1);
                    }
                }

                row_width = 0;
            }
        } else {
            ++row_width;
        }

        cur_contents = token_resp.next_slice;
    }

    // read again to fill in pattern info

    Dims dims{width, height};
    PatternCell *cells_ptr = new PatternCell[dims.area()];
    Action *actions_ptr = new Action[dims.area()];

    size_t actions_len = 0;

    Slice<PatternCell> cells{cells_ptr, dims.area()};

    if (cells_ptr == nullptr || actions_ptr == nullptr) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    cur_contents = contents;
    for (size_t r = 0; r < dims.height; ++r) {
        for (size_t c = 0; c < dims.width; ++c) {
            PatternCell &in_cell = cells[r * dims.width + c];

            TokenResponse token_resp = nextToken(cur_contents);
            switch (token_resp.token.type) {
            case tt_any: {
                in_cell = PatternCell{PatternCellType::pct_any};
            } break;
            case tt_number: {
                in_cell = PatternCell{PatternCellType::pct_number};
            } break;
            case tt_literal: {
                unsigned char number = token_resp.token.data - '0';
                in_cell = PatternCell{PatternCellType::pct_literal, number};
            } break;
            case tt_not_flag: {
                in_cell = PatternCell{PatternCellType::pct_not_flag};
            } break;
            case tt_flag:
            case tt_execute:
            case tt_newline:
            case tt_eof: {
                fprintf(stderr, "Unexpected token type\n");
                exit(1);
            } break;
            }
            cur_contents = token_resp.next_slice;
        }
        // read new line
        cur_contents = nextToken(cur_contents).next_slice;
    }

    // read new line
    cur_contents = nextToken(cur_contents).next_slice;

    for (size_t r = 0; r < dims.height; ++r) {
        for (size_t c = 0; c < dims.width; ++c) {
            PatternCell in_cell = cells[r * dims.width + c];
            PatternCell out_cell{};

            TokenResponse token_resp = nextToken(cur_contents);
            switch (token_resp.token.type) {
            case tt_any: {
                out_cell = PatternCell{PatternCellType::pct_any};
            } break;
            case tt_number: {
                out_cell = PatternCell{PatternCellType::pct_number};
            } break;
            case tt_literal: {
                unsigned char number = token_resp.token.data - '0';
                out_cell = PatternCell{PatternCellType::pct_literal, number};
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
            case tt_newline:
            case tt_eof: {
                fprintf(stderr, "Unexpected token type\n");
                exit(1);
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
                    exit(1);
                }
            } break;
            }
            cur_contents = token_resp.next_slice;
        }
        // read new line
        cur_contents = nextToken(cur_contents).next_slice;
    }
    assert(nextToken(cur_contents).token.type == TokenType::tt_eof);

    return Pattern{Shape{dims, cells}, Slice<Action>{actions_ptr, actions_len}};
}

auto writeFunctionHeader(FILE *out, StrSlice out_fn, Dims dims, size_t num,
                         char suffix) -> void {
    fprintf(out,
            "auto %.*s_%zu%c(Grid grid, size_t row, size_t col) -> bool {\n",
            STR_ARGS(out_fn), num, suffix);
    fprintf(out, "    size_t pat_width = %zu;\n", dims.width);
    fprintf(out, "    size_t pat_height = %zu;\n", dims.height);
    fprintf(out, "\n");
    fprintf(out, "    if ((col + pat_width) > grid.dims.width) {\n");
    fprintf(out, "        return false;\n");
    fprintf(out, "    }\n");
    fprintf(out, "    if ((row + pat_height) > grid.dims.height) {\n");
    fprintf(out, "        return false;\n");
    fprintf(out, "    }\n");
    fprintf(out, "\n");

    for (size_t r = 0; r < dims.height; ++r) {
        for (size_t c = 0; c < dims.width; ++c) {
            fprintf(out, "    Cell &c_%zu_%zu = grid[row + %zu][col + %zu];\n",
                    r, c, r, c);
        }
    }
}

auto writePatternMatchCell(FILE *out, PatternCell cell, Location loc) -> void {
    switch (cell.type) {
    case pct_any: {
    } break;
    case pct_number: {
        fprintf(out,
                "    if (c_%zu_%zu.display_type != "
                "CellDisplayType::cdt_value ||\n",
                loc.row, loc.col);
        fprintf(out, "        c_%zu_%zu.type != CellType::ct_number) {\n",
                loc.row, loc.col);
        fprintf(out, "        return false;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    case pct_literal: {
        fprintf(out,
                "    if (c_%zu_%zu.display_type != "
                "CellDisplayType::cdt_value ||\n",
                loc.row, loc.col);
        fprintf(out,
                "        c_%zu_%zu.type != CellType::ct_number || "
                "c_%zu_%zu.number != %d) {\n",
                loc.row, loc.col, loc.row, loc.col, cell.number);
        fprintf(out, "        return false;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    case pct_not_flag: {
        fprintf(out,
                "    if (c_%zu_%zu.display_type == "
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
                "    if (c_%zu_%zu.display_type == "
                "CellDisplayType::cdt_hidden ||\n",
                loc.row, loc.col);
        fprintf(out,
                "        c_%zu_%zu.display_type == "
                "CellDisplayType::cdt_maybe_flag) {\n",
                loc.row, loc.col);
        fprintf(out,
                "        c_%zu_%zu.display_type = CellDisplayType::cdt_flag;\n",
                loc.row, loc.col);
        fprintf(out, "        did_work = true;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    case pct_execute: {
        fprintf(out,
                "    if (c_%zu_%zu.display_type == "
                "CellDisplayType::cdt_maybe_flag ||\n",
                loc.row, loc.col);
        fprintf(out,
                "        c_%zu_%zu.display_type == "
                "CellDisplayType::cdt_hidden) {\n",
                loc.row, loc.col);
        fprintf(out,
                "        // mark as hidden to remove possible maybe_flag\n");
        fprintf(out,
                "        c_%zu_%zu.display_type = "
                "CellDisplayType::cdt_hidden;\n",
                loc.row, loc.col);
        fprintf(out,
                "        uncoverSelfAndNeighbors(grid, Location{row + %zu, "
                "col + %zu});\n",
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

auto idDimAdj(Dims dims) -> Dims { return dims; }
auto dimAdj90(Dims dims) -> Dims { return Dims{dims.height, dims.width}; }

auto idLocAdj(Dims dims, Location loc) -> Location { return loc; }

auto locAdj90n(Dims dims, Location loc) -> Location {
    return Location{loc.col, dims.height - 1 - loc.row};
}

auto locAdj180n(Dims dims, Location loc) -> Location {
    return Location{dims.height - 1 - loc.row, dims.width - 1 - loc.col};
}

auto locAdj270n(Dims dims, Location loc) -> Location {
    return Location{dims.width - 1 - loc.col, loc.row};
}

auto locAdj0r(Dims dims, Location loc) -> Location {
    return Location{loc.row, dims.width - 1 - loc.col};
}

auto locAdj90r(Dims dims, Location loc) -> Location {
    return Location{loc.col, loc.row};
}

auto locAdj180r(Dims dims, Location loc) -> Location {
    return Location{dims.height - 1 - loc.row, loc.col};
}

auto locAdj270r(Dims dims, Location loc) -> Location {
    return Location{dims.width - 1 - loc.col, dims.height - 1 - loc.row};
}

typedef auto(DimsAdj)(Dims dims) -> Dims;
typedef auto(LocAdj)(Dims dims, Location loc) -> Location;

auto writeFunctionGeneric(FILE *out, StrSlice out_fn, Pattern pattern,
                          size_t num, char suffix, DimsAdj *dims_adjuster,
                          LocAdj *loc_adjuster) -> void {
    Dims dims = pattern.shape.dims;
    Dims dims_adj = dims_adjuster(dims);

    writeFunctionHeader(out, out_fn, dims_adj, num, suffix);

    fprintf(out, "\n");
    fprintf(out, "    // check pattern match\n");
    fprintf(out, "\n");

    for (size_t r = 0; r < dims.height; ++r) {
        for (size_t c = 0; c < dims.width; ++c) {
            Location loc_adj = loc_adjuster(dims, Location{r, c});
            PatternCell cell = pattern.shape.cells[r * dims.width + c];
            writePatternMatchCell(out, cell, loc_adj);
        }
    }

    fprintf(out, "    // pattern has been matched\n");
    fprintf(out, "\n");
    fprintf(out, "    bool did_work = false;\n");
    fprintf(out, "\n");

    for (Action action : pattern.actions) {
        Location loc_adj = loc_adjuster(dims, action.loc);
        writeActionCell(out, action.pre_cond, action.post_cond, loc_adj);
    }

    fprintf(out, "    return did_work;\n");
    fprintf(out, "}\n");
    fprintf(out, "\n");
}

auto writeFunction_0n(FILE *out, StrSlice out_fn, Pattern pattern) -> void {
    writeFunctionGeneric(out, out_fn, pattern, 0, 'n', &idDimAdj, &idLocAdj);
}

auto writeFunction_90n(FILE *out, StrSlice out_fn, Pattern pattern) -> void {
    writeFunctionGeneric(out, out_fn, pattern, 90, 'n', &dimAdj90, &locAdj90n);
}

auto writeFunction_180n(FILE *out, StrSlice out_fn, Pattern pattern) -> void {
    writeFunctionGeneric(out, out_fn, pattern, 180, 'n', &idDimAdj,
                         &locAdj180n);
}

auto writeFunction_270n(FILE *out, StrSlice out_fn, Pattern pattern) -> void {
    writeFunctionGeneric(out, out_fn, pattern, 270, 'n', &dimAdj90,
                         &locAdj270n);
}

auto writeFunction_0r(FILE *out, StrSlice out_fn, Pattern pattern) -> void {
    writeFunctionGeneric(out, out_fn, pattern, 0, 'r', &idDimAdj, &locAdj0r);
}

auto writeFunction_90r(FILE *out, StrSlice out_fn, Pattern pattern) -> void {
    writeFunctionGeneric(out, out_fn, pattern, 90, 'r', &dimAdj90, &locAdj90r);
}

auto writeFunction_180r(FILE *out, StrSlice out_fn, Pattern pattern) -> void {
    writeFunctionGeneric(out, out_fn, pattern, 180, 'r', &idDimAdj,
                         &locAdj180r);
}

auto writeFunction_270r(FILE *out, StrSlice out_fn, Pattern pattern) -> void {
    writeFunctionGeneric(out, out_fn, pattern, 270, 'r', &dimAdj90,
                         &locAdj270r);
}

auto writeFunction(FILE *out, StrSlice out_fn, Pattern pattern) -> void {
    fprintf(out, "#pragma once\n");
    fprintf(out, "\n");
    fprintf(out, "#include \"dirutils.cc\"\n");
    fprintf(out, "#include \"grid.cc\"\n");
    fprintf(out, "\n");
    fprintf(out, "#include <sys/types.h>\n");
    fprintf(out, "\n");

    writeFunction_0n(out, out_fn, pattern);
    writeFunction_90n(out, out_fn, pattern);
    writeFunction_180n(out, out_fn, pattern);
    writeFunction_270n(out, out_fn, pattern);

    writeFunction_0r(out, out_fn, pattern);
    writeFunction_90r(out, out_fn, pattern);
    writeFunction_180r(out, out_fn, pattern);
    writeFunction_270r(out, out_fn, pattern);

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
        exit(1);
    }

    char const *in_name = argv[1];
    char const *out_name = getOutName(in_name);

    StrSlice out_name_slice = str_slice(out_name);
    StrSlice out_fn =
        out_name_slice.slice(0, out_name_slice.len - OUT_SUFFIX.len);

    Op<StrSlice> contents_op = getContents(in_name);
    if (!contents_op.valid) {
        fprintf(stderr, "Failed to read pattern file %s\n", in_name);
        exit(1);
    }

    Pattern pattern = readPattern(contents_op.get());

    FILE *out = fopen(out_name, "w");
    if (out == nullptr) {
        fprintf(stderr, "Failed to open output file %s\n", out_name);
        exit(1);
    }

    writeFunction(out, out_fn, pattern);
    fflush(out);
    fclose(out);
}
