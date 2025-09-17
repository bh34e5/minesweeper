#include "arena.cc"
#include "dirutils.cc"
#include "fileutils.cc"
#include "op.cc"
#include "parsing.cc"
#include "strslice.cc"
#include "utils.cc"

#include <assert.h>
#include <stdio.h>

auto writeStructDefinition(FILE *out, StrSlice out_fn, Dims dims) -> void {
    fprintf(out, "struct Pattern_%.*s {\n", STR_ARGS(out_fn));
    for (size_t r = 0; r < dims.height; ++r) {
        for (size_t c = 0; c < dims.width; ++c) {
            fprintf(out, "    Cell *c_%zu_%zu;\n", r, c);
        }
    }
    fprintf(out, "};\n");
    fprintf(out, "\n");
}

auto writePatternMatchCell(FILE *out, PatternCell cell, Location loc) -> void {
    switch (cell.type) {
    case pct_hidden: {
        fprintf(out,
                "    if (pat.c_%zu_%zu->display_type != "
                "CellDisplayType::cdt_hidden) {\n",
                loc.row, loc.col);
        fprintf(out, "        return false;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    case pct_number: {
        // NOTE(bhester): Since we are using the "effective number" to check
        // literals, we want to count a flag as a "number" in the sense that it
        // can't be flagged
        fprintf(out,
                "    if ((pat.c_%zu_%zu->display_type != "
                "CellDisplayType::cdt_value || pat.c_%zu_%zu->type != "
                "CellType::ct_number) && (pat.c_%zu_%zu->display_type != "
                "CellDisplayType::cdt_flag)) {\n",
                loc.row, loc.col, loc.row, loc.col, loc.row, loc.col);
        fprintf(out, "        return false;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    case pct_literal: {
        fprintf(out,
                "    if (pat.c_%zu_%zu->display_type != "
                "CellDisplayType::cdt_value || pat.c_%zu_%zu->type != "
                "CellType::ct_number || pat.c_%zu_%zu->eff_number != %d) {\n",
                loc.row, loc.col, loc.row, loc.col, loc.row, loc.col,
                cell.number);
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

auto writeActionCell(FILE *out, Action action, Location loc) -> void {
    switch (action.action) {
    case act_flag: {
        fprintf(out,
                "    if (pat.c_%zu_%zu->display_type == "
                "CellDisplayType::cdt_hidden || pat.c_%zu_%zu->display_type == "
                "CellDisplayType::cdt_maybe_flag) {\n",
                loc.row, loc.col, loc.row, loc.col);
        fprintf(out, "        flagCell(grid, pat.c_%zu_%zu);\n", loc.row,
                loc.col);
        fprintf(out, "        did_work = true;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    case act_execute: {
        fprintf(
            out,
            "    if (pat.c_%zu_%zu->display_type == "
            "CellDisplayType::cdt_maybe_flag || pat.c_%zu_%zu->display_type == "
            "CellDisplayType::cdt_hidden) {\n",
            loc.row, loc.col, loc.row, loc.col);
        fprintf(out,
                "        // mark as hidden to remove possible maybe_flag\n");
        fprintf(out,
                "        pat.c_%zu_%zu->display_type = "
                "CellDisplayType::cdt_hidden;\n",
                loc.row, loc.col);
        fprintf(out, "        uncoverSelfAndNeighbors(grid, pat.c_%zu_%zu);\n",
                loc.row, loc.col);
        fprintf(out, "        did_work = true;\n");
        fprintf(out, "    }\n");
        fprintf(out, "\n");
    } break;
    }
}

auto writeCheckPatternFunction(FILE *out, StrSlice out_fn, Pattern pattern)
    -> void {
    fprintf(out,
            "auto checkPattern_%.*s(Grid *grid, size_t row, size_t col, "
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
        writeActionCell(out, action, action.loc);
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
            "auto %.*s_%zu%c(Grid *grid, size_t row, size_t col) -> bool {\n",
            STR_ARGS(out_fn), num, suffix);
    fprintf(out, "    size_t pat_width = %zu;\n", dims_adj.width);
    fprintf(out, "    size_t pat_height = %zu;\n", dims_adj.height);
    fprintf(out, "\n");
    fprintf(out, "    if ((col + pat_width) > grid->dims.width) {\n");
    fprintf(out, "        return false;\n");
    fprintf(out, "    }\n");
    fprintf(out, "    if ((row + pat_height) > grid->dims.height) {\n");
    fprintf(out, "        return false;\n");
    fprintf(out, "    }\n");
    fprintf(out, "\n");
    fprintf(out, "    Pattern_%.*s pat{\n", STR_ARGS(out_fn));
    for (size_t r = 0; r < dims.height; ++r) {
        for (size_t c = 0; c < dims.width; ++c) {
            Location loc_adj = LAdj(dims, Location{r, c});
            fprintf(out, "        &(*grid)[row + %zu][col + %zu],\n",
                    loc_adj.row, loc_adj.col);
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

auto writeBody(FILE *out, StrSlice out_fn, Pattern pattern) -> void {
    fprintf(out, "#include \"../grid.h\"\n");
    fprintf(out, "#include \"../solver.h\"\n");
    fprintf(out, "\n");
    fprintf(out, "#include \"../arena.cc\"\n");
    fprintf(out, "#include \"../dirutils.cc\"\n");
    fprintf(out, "#include \"../strslice.cc\"\n");
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

    fprintf(out,
            "auto %.*s(Grid *grid, size_t row, size_t col, void *) -> bool {\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_0n = %.*s_0n(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_90n = %.*s_90n(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_180n = %.*s_180n(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "    bool did_work_270n = %.*s_270n(grid, row, col);\n",
            STR_ARGS(out_fn));
    fprintf(out, "\n");
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
    fprintf(out, "\n");
    fprintf(out, "static StrSlice rule_name = STR_SLICE(\"%.*s\");\n",
            STR_ARGS(out_fn));
    fprintf(out, "\n");
    fprintf(out, "REGISTERER(regRule, arena, solver) {\n");
    fprintf(out,
            "    solver->registerRule(arena, makeRule(&%.*s, rule_name));\n",
            STR_ARGS(out_fn));
    fprintf(out, "}\n");
    fprintf(out, "\n");
    fprintf(out, "DEREGISTERER(deregRule, solver) {\n");
    fprintf(out, "    solver->deregisterRule(rule_name);\n");
    fprintf(out, "}\n");
    fprintf(out, "\n");
    fprintf(out, "RulePlugin plugin{regRule, deregRule};\n");
}

auto usage(char const *path) -> void {
    fprintf(stderr, "%s [filename]\n", path);
    fprintf(stderr, "\n");
    fprintf(stderr, "filename - name of pattern file (.pat) to compile\n");
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        usage(argv[0]);
        EXIT(1);
    }

    Arena arena = makeArena(MEGABYTES(10));

    makeDirAndParentsIfNotExists(&arena, OUT_DIR);

    char const *in_name = argv[1];
    FileArgs file_args = getFileArgs(&arena, in_name);

    Op<StrSlice> contents_op = getContents(&arena, in_name);
    if (!contents_op.valid) {
        fprintf(stderr, "Failed to read pattern file %s\n", in_name);
        EXIT(1);
    }

    Pattern pattern = readPattern(&arena, contents_op.get());

    char const *out_name = file_args.out_name;
    FILE *out = fopen(out_name, "w");
    if (out == nullptr) {
        fprintf(stderr, "Failed to open output file %s\n", out_name);
        EXIT(1);
    }

    writeBody(out, file_args.out_root, pattern);
    fflush(out);
    fclose(out);

    freeArena(&arena);
}
