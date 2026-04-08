#include "lexer.h"

#include <ctype.h>
#include <stdbool.h>

static void skip_whitespace(Lexer *lexer) {
    while (lexer->source[lexer->position] != '\0' &&
           isspace((unsigned char)lexer->source[lexer->position])) {
        lexer->position++;
    }
}

static bool keyword_matches(const char *start, u64 len, const char *keyword,
                            u64 keyword_len) {
    if (len != keyword_len) {
        return false;
    }
    for (u64 i = 0; i < len; i++) {
        if (tolower((unsigned char)start[i]) != keyword[i]) {
            return false;
        }
    }
    return true;
}

static TokenKind match_keyword(const char *start, u64 len) {
    if (keyword_matches(start, len, "select", 6)) return TOK_SELECT;
    if (keyword_matches(start, len, "from", 4))   return TOK_FROM;
    if (keyword_matches(start, len, "count", 5))  return TOK_COUNT;
    if (keyword_matches(start, len, "where", 5))  return TOK_WHERE;
    if (keyword_matches(start, len, "order", 5))  return TOK_ORDER;
    if (keyword_matches(start, len, "by", 2))     return TOK_BY;
    if (keyword_matches(start, len, "limit", 5))  return TOK_LIMIT;
    if (keyword_matches(start, len, "asc", 3))    return TOK_ASC;
    if (keyword_matches(start, len, "desc", 4))   return TOK_DESC;
    if (keyword_matches(start, len, "and", 3))    return TOK_AND;
    if (keyword_matches(start, len, "or", 2))     return TOK_OR;
    if (keyword_matches(start, len, "not", 3))    return TOK_NOT;
    if (keyword_matches(start, len, "create", 6)) return TOK_CREATE;
    if (keyword_matches(start, len, "table", 5))  return TOK_TABLE;
    return TOK_IDENTIFIER;
}

Lexer lexer_create(const char *source) {
    return (Lexer){.source = source, .position = 0};
}

Token lexer_next(Lexer *lexer) {
    skip_whitespace(lexer);

    u64 start_pos = lexer->position;
    char c = lexer->source[start_pos];

    if (c == '\0') {
        return (Token){TOK_EOF, &lexer->source[start_pos], 0, start_pos};
    }

    if (c == '*') {
        lexer->position++;
        return (Token){TOK_STAR, &lexer->source[start_pos], 1, start_pos};
    }
    if (c == '(') {
        lexer->position++;
        return (Token){TOK_LPAREN, &lexer->source[start_pos], 1, start_pos};
    }
    if (c == ')') {
        lexer->position++;
        return (Token){TOK_RPAREN, &lexer->source[start_pos], 1, start_pos};
    }
    if (c == ',') {
        lexer->position++;
        return (Token){TOK_COMMA, &lexer->source[start_pos], 1, start_pos};
    }
    if (c == ';') {
        lexer->position++;
        return (Token){TOK_SEMICOLON, &lexer->source[start_pos], 1, start_pos};
    }
    if (c == '=') {
        lexer->position++;
        return (Token){TOK_EQ, &lexer->source[start_pos], 1, start_pos};
    }
    if (c == '<') {
        lexer->position++;
        if (lexer->source[lexer->position] == '=') {
            lexer->position++;
            return (Token){TOK_LTE, &lexer->source[start_pos], 2, start_pos};
        }
        return (Token){TOK_LT, &lexer->source[start_pos], 1, start_pos};
    }
    if (c == '>') {
        lexer->position++;
        if (lexer->source[lexer->position] == '=') {
            lexer->position++;
            return (Token){TOK_GTE, &lexer->source[start_pos], 2, start_pos};
        }
        return (Token){TOK_GT, &lexer->source[start_pos], 1, start_pos};
    }
    if (c == '!') {
        lexer->position++;
        if (lexer->source[lexer->position] == '=') {
            lexer->position++;
            return (Token){TOK_NEQ, &lexer->source[start_pos], 2, start_pos};
        }
        return (Token){TOK_UNKNOWN, &lexer->source[start_pos], 1, start_pos};
    }
    if (c == '\'') {
        lexer->position++; // skip opening quote
        u64 content_start = lexer->position;
        while (lexer->source[lexer->position] != '\0') {
            if (lexer->source[lexer->position] == '\'') {
                if (lexer->source[lexer->position + 1] == '\'') {
                    lexer->position += 2; // escaped quote
                    continue;
                }
                break;
            }
            lexer->position++;
        }
        if (lexer->source[lexer->position] == '\0') {
            return (Token){TOK_UNKNOWN, &lexer->source[start_pos], lexer->position - start_pos, start_pos};
        }
        u64 content_len = lexer->position - content_start;
        lexer->position++; // skip closing quote
        return (Token){TOK_STRING, &lexer->source[content_start], content_len, start_pos};
    }
    if (isdigit((unsigned char)c)) {
        while (isdigit((unsigned char)lexer->source[lexer->position])) {
            lexer->position++;
        }
        u64 len = lexer->position - start_pos;
        return (Token){TOK_INTEGER, &lexer->source[start_pos], len, start_pos};
    }

    if (isalpha((unsigned char)c) || c == '_') {
        while (isalnum((unsigned char)lexer->source[lexer->position]) ||
               lexer->source[lexer->position] == '_') {
            lexer->position++;
        }
        u64 len = lexer->position - start_pos;
        TokenKind kind = match_keyword(&lexer->source[start_pos], len);
        return (Token){kind, &lexer->source[start_pos], len, start_pos};
    }

    lexer->position++;
    return (Token){TOK_UNKNOWN, &lexer->source[start_pos], 1, start_pos};
}

Token lexer_peek(Lexer *lexer) {
    u64 saved = lexer->position;
    Token tok = lexer_next(lexer);
    lexer->position = saved;
    return tok;
}
