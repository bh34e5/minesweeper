#pragma once

#include "dirutils.cc"
#include "slice.cc"
#include "tokens.cc"
#include "utils.cc"

#include <stdio.h>
#include <sys/types.h>

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

auto readWalls(StrSlice &cur_contents, Walls &walls) -> void {
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

        Token number_tok = expectToken(cur_contents, TokenType::tt_literal);
        assert(target != nullptr && "Unset target");
        *target = number_tok.data;

        next = nextToken(cur_contents);
    } while (next.type != TokenType::tt_eof &&
             next.type != TokenType::tt_newline);

    expectToken(cur_contents, TokenType::tt_newline);
}

auto readPattern(StrSlice contents) -> Pattern {
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
    StrSlice pattern_start = contents;

    cur_contents = contents;
    while (cur_contents.len > 0) {
        Token token_resp = nextToken(cur_contents);
        switch (token_resp.type) {
        case tt_wall: {
            if (!first_char) {
                fprintf(stderr, "Invalid wall directive\n");
                EXIT(1);
            }

            readWalls(cur_contents, walls);

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
