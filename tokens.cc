#pragma once

#include "strslice.cc"
#include "utils.cc"

#include <stdio.h>

auto isDigit(char c) -> bool { return '0' <= c && c <= '9'; }
auto isMinesweeperNumber(int num) -> bool { return 0 <= num && num <= 8; }

enum TokenType {
    tt_any,
    tt_number,
    tt_literal,
    tt_not_flag,
    tt_flag,
    tt_execute,
    tt_space,
    tt_newline,
    tt_wall,
    tt_north,
    tt_east,
    tt_south,
    tt_west,
    tt_eof,
};

struct Token {
    TokenType type;
    StrSlice slice;
    int data;
};

static StrSlice STR_WALL = STR_SLICE("wall");
static StrSlice STR_NORTH = STR_SLICE("north");
static StrSlice STR_EAST = STR_SLICE("east");
static StrSlice STR_SOUTH = STR_SLICE("south");
static StrSlice STR_WEST = STR_SLICE("west");

Token TK_EOF{TokenType::tt_eof, StrSlice{}, EOF};

auto checkSlice(StrSlice &contents, StrSlice needle) -> bool {
    if (startsWith(contents, needle)) {
        contents = contents.slice(needle.len);
        return true;
    }
    return false;
}

auto nextToken(StrSlice &contents) -> Token {
    if (contents.len == 0) {
        return TK_EOF;
    }

    StrSlice next_contents = contents;
    Token token = TK_EOF;

    char c = contents[0];
    switch (c) {
    case 'n': {
        if (checkSlice(next_contents, STR_NORTH)) {
            token = Token{TokenType::tt_north, STR_NORTH};
            break;
        }

        next_contents = contents.slice(1);
        token = Token{TokenType::tt_number, contents.slice(0, 1)};
    } break;
    case 'e': {
        if (checkSlice(next_contents, STR_EAST)) {
            token = Token{TokenType::tt_east, STR_EAST};
            break;
        }

        fprintf(stderr, "Unexpected character '%c'\n", c);
        EXIT(1);
    } break;
    case 's': {
        if (checkSlice(next_contents, STR_SOUTH)) {
            token = Token{TokenType::tt_south, STR_SOUTH};
            break;
        }

        fprintf(stderr, "Unexpected character '%c'\n", c);
        EXIT(1);
    } break;
    case 'w': {
        if (checkSlice(next_contents, STR_WALL)) {
            token = Token{TokenType::tt_wall, STR_WALL};
            break;
        }

        if (checkSlice(next_contents, STR_WEST)) {
            token = Token{TokenType::tt_west, STR_WEST};
            break;
        }

        fprintf(stderr, "Unexpected character '%c'\n", c);
        EXIT(1);
    } break;
    case '_': {
        next_contents = contents.slice(1);
        token = Token{TokenType::tt_any, contents.slice(0, 1)};
    } break;
    case 'f': {
        next_contents = contents.slice(1);
        token = Token{TokenType::tt_not_flag, contents.slice(0, 1)};
    } break;
    case 'F': {
        next_contents = contents.slice(1);
        token = Token{TokenType::tt_flag, contents.slice(0, 1)};
    } break;
    case 'x': {
        next_contents = contents.slice(1);
        token = Token{TokenType::tt_execute, contents.slice(0, 1)};
    } break;
    case ' ': {
        next_contents = contents.slice(1);
        token = Token{TokenType::tt_space, contents.slice(0, 1)};
    } break;
    case '\r': {
        if (contents.len > 1 && contents[1] == '\n') {
            next_contents = contents.slice(2);
            token = Token{TokenType{tt_newline}, contents.slice(0, 2)};
            break;
        }

        next_contents = contents.slice(1);
        token = Token{TokenType{tt_newline}, contents.slice(0, 1)};
    } break;
    case '\n': {
        next_contents = contents.slice(1);
        token = Token{TokenType{tt_newline}, contents.slice(1)};
    } break;
    default: {
        if (isDigit(c)) {
            int num = c - '0';

            size_t idx = 1;
            while (idx < contents.len && isDigit((c = contents[idx]))) {
                num = (10 * num) + (c - '0');
                ++idx;
            }

            next_contents = contents.slice(idx);
            token = Token{TokenType::tt_literal, contents.slice(0, idx), num};
            break;
        }

        fprintf(stderr, "Unexpected character '%c'\n", c);
        EXIT(1);
    } break;
    }

    contents = next_contents;
    return token;
}

auto expectToken(StrSlice &contents, TokenType type) -> Token {
    Token next = nextToken(contents);
    if (next.type != type) {
        fprintf(stderr, "Unexpected token type: %d. Expected %d\n", next.type,
                type);
        EXIT(1);
    }
    return next;
}
