#include "parser.h"
#include "lexer.h"

#include <stdlib.h>
#include <string.h>

// --- Helpers ---

static SqlParseResult make_error(SqlErrorCode code, u64 pos) {
    return (SqlParseResult){.err_code = code, .err_pos = pos, .statement = {0}};
}

static char *copy_to_arena(Arena *arena, const char *src, u64 len) {
    char *dst = arena_alloc(arena, len, 1);
    if (dst) {
        memcpy(dst, src, len);
    }
    return dst;
}

static Expr *alloc_expr(Arena *arena) {
    return arena_alloc(arena, sizeof(Expr), _Alignof(Expr));
}

// --- Internal result types ---

typedef struct {
    bool ok;
    SqlErrorCode err;
    u64 err_pos;
    Expr *expr;
} ExprResult;

static ExprResult expr_ok(Expr *e) {
    return (ExprResult){.ok = true, .expr = e};
}

static ExprResult expr_err(SqlErrorCode code, u64 pos) {
    return (ExprResult){.ok = false, .err = code, .err_pos = pos};
}

typedef struct {
    bool ok;
    SqlErrorCode err;
    u64 err_pos;
    Expr **columns;
    u64 column_count;
} SelectListResult;

// --- Expression parser (recursive descent with precedence) ---

static ExprResult parse_or_expr(Arena *arena, Lexer *lexer);

static ExprResult parse_atom(Arena *arena, Lexer *lexer) {
    Token tok = lexer_peek(lexer);

    if (tok.kind == TOK_INTEGER) {
        lexer_next(lexer);
        Expr *e = alloc_expr(arena);
        e->kind = EXPR_LITERAL_INT;
        // Parse integer from token
        s64 val = 0;
        for (u64 i = 0; i < tok.length; i++) {
            val = val * 10 + (tok.start[i] - '0');
        }
        e->literal_int.value = val;
        return expr_ok(e);
    }

    if (tok.kind == TOK_STRING) {
        lexer_next(lexer);
        Expr *e = alloc_expr(arena);
        e->kind = EXPR_LITERAL_STR;
        e->literal_str.value = copy_to_arena(arena, tok.start, tok.length);
        e->literal_str.value_len = tok.length;
        return expr_ok(e);
    }

    if (tok.kind == TOK_UNKNOWN && tok.length > 0 && tok.start[0] == '\'') {
        // Unterminated string — lexer emitted TOK_UNKNOWN starting at the opening quote
        return expr_err(SQL_ERR_UNTERMINATED_STRING, tok.position);
    }

    if (tok.kind == TOK_COUNT) {
        lexer_next(lexer);
        Token lp = lexer_next(lexer);
        if (lp.kind != TOK_LPAREN) {
            return expr_err(SQL_ERR_EXPECTED_LPAREN, lp.position);
        }
        Token star = lexer_next(lexer);
        if (star.kind != TOK_STAR) {
            return expr_err(SQL_ERR_EXPECTED_STAR, star.position);
        }
        Token rp = lexer_next(lexer);
        if (rp.kind != TOK_RPAREN) {
            return expr_err(SQL_ERR_EXPECTED_RPAREN, rp.position);
        }
        Expr *e = alloc_expr(arena);
        e->kind = EXPR_COUNT_STAR;
        return expr_ok(e);
    }

    if (tok.kind == TOK_IDENTIFIER) {
        lexer_next(lexer);
        Expr *e = alloc_expr(arena);
        e->kind = EXPR_COLUMN;
        e->column.name = copy_to_arena(arena, tok.start, tok.length);
        e->column.name_len = tok.length;
        return expr_ok(e);
    }

    if (tok.kind == TOK_LPAREN) {
        lexer_next(lexer);
        ExprResult inner = parse_or_expr(arena, lexer);
        if (!inner.ok) return inner;
        Token rp = lexer_next(lexer);
        if (rp.kind != TOK_RPAREN) {
            return expr_err(SQL_ERR_EXPECTED_RPAREN, rp.position);
        }
        return inner;
    }

    return expr_err(SQL_ERR_EXPECTED_EXPRESSION, tok.position);
}

static ExprResult parse_comparison(Arena *arena, Lexer *lexer) {
    ExprResult left = parse_atom(arena, lexer);
    if (!left.ok) return left;

    Token tok = lexer_peek(lexer);
    BinaryOp op;
    bool has_op = true;

    switch (tok.kind) {
    case TOK_EQ:  op = OP_EQ;  break;
    case TOK_NEQ: op = OP_NEQ; break;
    case TOK_LT:  op = OP_LT;  break;
    case TOK_GT:  op = OP_GT;  break;
    case TOK_LTE: op = OP_LTE; break;
    case TOK_GTE: op = OP_GTE; break;
    default: has_op = false; break;
    }

    if (!has_op) return left;

    lexer_next(lexer); // consume operator
    ExprResult right = parse_atom(arena, lexer);
    if (!right.ok) return right;

    Expr *e = alloc_expr(arena);
    e->kind = EXPR_BINARY;
    e->binary.op = op;
    e->binary.left = left.expr;
    e->binary.right = right.expr;
    return expr_ok(e);
}

static ExprResult parse_not_expr(Arena *arena, Lexer *lexer) {
    Token tok = lexer_peek(lexer);
    if (tok.kind == TOK_NOT) {
        lexer_next(lexer);
        ExprResult inner = parse_not_expr(arena, lexer);
        if (!inner.ok) return inner;
        Expr *e = alloc_expr(arena);
        e->kind = EXPR_UNARY_NOT;
        e->unary_not.operand = inner.expr;
        return expr_ok(e);
    }
    return parse_comparison(arena, lexer);
}

static ExprResult parse_and_expr(Arena *arena, Lexer *lexer) {
    ExprResult left = parse_not_expr(arena, lexer);
    if (!left.ok) return left;

    while (lexer_peek(lexer).kind == TOK_AND) {
        lexer_next(lexer);
        ExprResult right = parse_not_expr(arena, lexer);
        if (!right.ok) return right;
        Expr *e = alloc_expr(arena);
        e->kind = EXPR_BINARY;
        e->binary.op = OP_AND;
        e->binary.left = left.expr;
        e->binary.right = right.expr;
        left = expr_ok(e);
    }

    return left;
}

static ExprResult parse_or_expr(Arena *arena, Lexer *lexer) {
    ExprResult left = parse_and_expr(arena, lexer);
    if (!left.ok) return left;

    while (lexer_peek(lexer).kind == TOK_OR) {
        lexer_next(lexer);
        ExprResult right = parse_and_expr(arena, lexer);
        if (!right.ok) return right;
        Expr *e = alloc_expr(arena);
        e->kind = EXPR_BINARY;
        e->binary.op = OP_OR;
        e->binary.left = left.expr;
        e->binary.right = right.expr;
        left = expr_ok(e);
    }

    return left;
}

// --- Select list parser ---

static SelectListResult parse_select_list(Arena *arena, Lexer *lexer) {
    Token peek = lexer_peek(lexer);

    if (peek.kind == TOK_COUNT) {
        ExprResult er = parse_atom(arena, lexer);
        if (!er.ok) {
            return (SelectListResult){
                .ok = false, .err = er.err, .err_pos = er.err_pos};
        }
        Expr **columns = arena_alloc(arena, sizeof(Expr *), _Alignof(Expr *));
        columns[0] = er.expr;
        return (SelectListResult){
            .ok = true, .columns = columns, .column_count = 1};
    }

    if (peek.kind == TOK_IDENTIFIER) {
        // Two-pass: count columns, then allocate and parse
        u64 saved_pos = lexer->position;
        u64 count = 0;

        while (true) {
            Token t = lexer_next(lexer);
            if (t.kind != TOK_IDENTIFIER) break;
            count++;
            t = lexer_peek(lexer);
            if (t.kind != TOK_COMMA) break;
            lexer_next(lexer); // consume comma
        }

        lexer->position = saved_pos;

        Expr **columns = arena_alloc(arena, count * sizeof(Expr *), _Alignof(Expr *));

        for (u64 i = 0; i < count; i++) {
            Token t = lexer_next(lexer);
            Expr *e = alloc_expr(arena);
            e->kind = EXPR_COLUMN;
            e->column.name = copy_to_arena(arena, t.start, t.length);
            e->column.name_len = t.length;
            columns[i] = e;

            if (i + 1 < count) {
                lexer_next(lexer); // consume comma
            }
        }

        return (SelectListResult){
            .ok = true, .columns = columns, .column_count = count};
    }

    return (SelectListResult){
        .ok = false,
        .err = SQL_ERR_UNEXPECTED_TOKEN,
        .err_pos = peek.position};
}

// --- Clause parsers ---

static ExprResult parse_where(Arena *arena, Lexer *lexer) {
    return parse_or_expr(arena, lexer);
}

typedef struct {
    bool ok;
    SqlErrorCode err;
    u64 err_pos;
    OrderByItem *items;
    u64 count;
} OrderByResult;

static OrderByResult parse_order_by(Arena *arena, Lexer *lexer) {
    Token by_tok = lexer_next(lexer);
    if (by_tok.kind != TOK_BY) {
        return (OrderByResult){
            .ok = false, .err = SQL_ERR_EXPECTED_BY, .err_pos = by_tok.position};
    }

    // Two-pass: count items, then allocate and parse
    u64 saved_pos = lexer->position;
    u64 count = 0;

    while (true) {
        Token t = lexer_peek(lexer);
        if (t.kind != TOK_IDENTIFIER && t.kind != TOK_INTEGER &&
            t.kind != TOK_LPAREN && t.kind != TOK_COUNT) {
            break;
        }
        ExprResult skip = parse_or_expr(arena, lexer);
        if (!skip.ok) break;
        count++;
        Token dir = lexer_peek(lexer);
        if (dir.kind == TOK_ASC || dir.kind == TOK_DESC) {
            lexer_next(lexer);
        }
        if (lexer_peek(lexer).kind != TOK_COMMA) break;
        lexer_next(lexer); // consume comma
    }

    if (count == 0) {
        Token t = lexer_peek(lexer);
        return (OrderByResult){
            .ok = false, .err = SQL_ERR_EXPECTED_ORDER_EXPR, .err_pos = t.position};
    }

    lexer->position = saved_pos;

    OrderByItem *items = arena_alloc(arena, count * sizeof(OrderByItem), _Alignof(OrderByItem));

    for (u64 i = 0; i < count; i++) {
        ExprResult er = parse_or_expr(arena, lexer);
        if (!er.ok) {
            return (OrderByResult){
                .ok = false, .err = er.err, .err_pos = er.err_pos};
        }
        items[i].expr = er.expr;
        items[i].direction = ORDER_ASC;

        Token dir = lexer_peek(lexer);
        if (dir.kind == TOK_ASC) {
            lexer_next(lexer);
            items[i].direction = ORDER_ASC;
        } else if (dir.kind == TOK_DESC) {
            lexer_next(lexer);
            items[i].direction = ORDER_DESC;
        }

        if (i + 1 < count) {
            lexer_next(lexer); // consume comma
        }
    }

    return (OrderByResult){
        .ok = true, .items = items, .count = count};
}

// --- Main entry point ---

SqlParseResult parse_statement(Arena *arena, const char *text) {
    Lexer lexer = lexer_create(text);

    Token tok = lexer_peek(&lexer);
    if (tok.kind == TOK_EOF) {
        return make_error(SQL_ERR_EMPTY_INPUT, 0);
    }

    tok = lexer_next(&lexer);
    if (tok.kind != TOK_SELECT) {
        return make_error(SQL_ERR_UNEXPECTED_TOKEN, tok.position);
    }

    SelectListResult slr = parse_select_list(arena, &lexer);
    if (!slr.ok) {
        return make_error(slr.err, slr.err_pos);
    }

    tok = lexer_next(&lexer);
    if (tok.kind != TOK_FROM) {
        return make_error(SQL_ERR_EXPECTED_FROM, tok.position);
    }

    tok = lexer_next(&lexer);
    if (tok.kind != TOK_IDENTIFIER) {
        return make_error(SQL_ERR_EXPECTED_TABLE_NAME, tok.position);
    }

    char *table_name = copy_to_arena(arena, tok.start, tok.length);
    u64 table_name_len = tok.length;

    // Optional WHERE
    Expr *where_clause = NULL;
    if (lexer_peek(&lexer).kind == TOK_WHERE) {
        lexer_next(&lexer);
        ExprResult wr = parse_where(arena, &lexer);
        if (!wr.ok) {
            return make_error(wr.err, wr.err_pos);
        }
        where_clause = wr.expr;
    }

    // Optional ORDER BY
    OrderByItem *order_by = NULL;
    u64 order_by_count = 0;
    if (lexer_peek(&lexer).kind == TOK_ORDER) {
        lexer_next(&lexer);
        OrderByResult obr = parse_order_by(arena, &lexer);
        if (!obr.ok) {
            return make_error(obr.err, obr.err_pos);
        }
        order_by = obr.items;
        order_by_count = obr.count;
    }

    // Optional LIMIT
    Expr *limit = NULL;
    if (lexer_peek(&lexer).kind == TOK_LIMIT) {
        lexer_next(&lexer);
        ExprResult lr = parse_or_expr(arena, &lexer);
        if (!lr.ok) {
            return make_error(lr.err, lr.err_pos);
        }
        limit = lr.expr;
    }

    // Optional semicolon
    if (lexer_peek(&lexer).kind == TOK_SEMICOLON) {
        lexer_next(&lexer);
    }

    return (SqlParseResult){
        .err_code = SQL_OK,
        .err_pos = 0,
        .statement = {
            .kind = STMT_SELECT,
            .select = {
                .columns = slr.columns,
                .column_count = slr.column_count,
                .table_name = table_name,
                .table_name_len = table_name_len,
                .where_clause = where_clause,
                .order_by = order_by,
                .order_by_count = order_by_count,
                .limit = limit,
            }}};
}
