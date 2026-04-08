#pragma once

#include "spbint.h"

typedef enum {
    TOK_SELECT,
    TOK_FROM,
    TOK_COUNT,
    TOK_WHERE,
    TOK_ORDER,
    TOK_BY,
    TOK_LIMIT,
    TOK_ASC,
    TOK_DESC,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_IDENTIFIER,
    TOK_INTEGER,
    TOK_STRING,
    TOK_STAR,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_SEMICOLON,
    TOK_EQ,
    TOK_NEQ,
    TOK_LT,
    TOK_GT,
    TOK_LTE,
    TOK_GTE,
    TOK_EOF,
    TOK_UNKNOWN
} TokenKind;

typedef struct {
    TokenKind kind;
    const char *start;
    u64 length;
    u64 position;
} Token;

typedef struct {
    const char *source;
    u64 position;
} Lexer;

Lexer lexer_create(const char *source);
Token lexer_next(Lexer *lexer);
Token lexer_peek(Lexer *lexer);
