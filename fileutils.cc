#pragma once

#include "arena.cc"
#include "op.cc"
#include "strslice.cc"
#include "utils.cc"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

static StrSlice PAT_SUFFIX = STR_SLICE(".pat");
static StrSlice OUT_DIR = STR_SLICE("generated/");
static StrSlice OUT_PREFIX = STR_SLICE("pat_");
static StrSlice OUT_SUFFIX = STR_SLICE(".cc");

auto getContents(Arena &arena, char const *filename) -> Op<StrSlice> {
    FILE *f = fopen(filename, "r");
    if (f == nullptr) {
        return Op<StrSlice>::empty();
    }

    fseek(f, 0, SEEK_END);
    size_t file_len = ftell(f);
    fseek(f, 0, SEEK_SET);

    size_t mark = arena.len;
    char *ptr = arena.pushTN<char>(file_len);

    size_t nread = fread(ptr, file_len, 1, f);
    if (nread != 1) {
        arena.reset(mark);
        return Op<StrSlice>::empty();
    }

    fclose(f);

    return StrSlice{ptr, file_len};
}

auto getContentsZ(Arena &arena, char const *filename) -> char const * {
    FILE *f = fopen(filename, "r");
    if (f == nullptr) {
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    size_t file_len = ftell(f);
    fseek(f, 0, SEEK_SET);

    size_t mark = arena.len;
    char *ptr = arena.pushTN<char>(file_len + 1);

    size_t nread = fread(ptr, file_len, 1, f);
    if (nread != 1) {
        arena.reset(mark);
        return nullptr;
    }
    ptr[file_len] = '\0';

    fclose(f);

    return ptr;
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

auto getFileArgs(Arena &arena, char const *in_name) -> FileArgs {
    StrSlice in_slice = strSlice(in_name);

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

    char *ptr = arena.pushTN<char>(out_len + 1); // len + 1 for null terminator
    snprintf(ptr, out_len + 1, "%.*s%.*s%.*s%.*s", STR_ARGS(OUT_DIR),
             STR_ARGS(OUT_PREFIX), STR_ARGS(in_root), STR_ARGS(OUT_SUFFIX));

    StrSlice out_slice{ptr, out_len};
    StrSlice out_root =
        out_slice.slice(OUT_DIR.len, out_slice.len - OUT_SUFFIX.len);

    return FileArgs{in_slice,  in_root,  in_prefix, in_suffix,
                    out_slice, out_root, ptr};
}

auto makeDirIfNotExists(Arena &arena, StrSlice dir_name) -> void {
    auto marker = arena.mark();
    char const *name_str = toZString(arena, dir_name);

    struct stat stat_struct {};
    int ret = stat(name_str, &stat_struct);
    if (ret != 0 || ((stat_struct.st_mode & S_IFDIR) == 0)) {
        if (mkdir(name_str, 0755) != 0) {
            fprintf(stderr, "Failed to create directory: %s\n", name_str);
            EXIT(1);
        }
        if (chmod(name_str, 0755) != 0) {
            fprintf(stderr, "Failed to ensure directory permissions: %s\n",
                    name_str);
            EXIT(1);
        }
    }
}

auto makeDirAndParentsIfNotExists(Arena &arena, StrSlice dir_name) -> void {
    auto pattern_op = Op<size_t>::empty();
    auto pattern_it = PatternIterator{dir_name, STR_SLICE("/")};
    while ((pattern_op = pattern_it.next()).valid) {
        size_t idx = pattern_op.get();
        makeDirIfNotExists(arena, dir_name.slice(0, idx));
    }
}
