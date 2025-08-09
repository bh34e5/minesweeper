#pragma once

#include "strslice.cc"

#include <stdio.h>
#include <stdlib.h>

auto isMinesweeperDigit(char c) -> bool { return '0' <= c && c <= '8'; }

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
