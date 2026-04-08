#pragma once

#include "spbint.h"
#include "arena.h"

#include <stdbool.h>

typedef enum {
    SQL_OK = 0,
    SQL_ERR_UNEXPECTED_TOKEN,
    SQL_ERR_EXPECTED_FROM,
    SQL_ERR_EXPECTED_TABLE_NAME,
    SQL_ERR_EXPECTED_LPAREN,
    SQL_ERR_EXPECTED_RPAREN,
    SQL_ERR_EXPECTED_STAR,
    SQL_ERR_EMPTY_INPUT,
    SQL_ERR_EXPECTED_EXPRESSION,
    SQL_ERR_UNTERMINATED_STRING,
    SQL_ERR_EXPECTED_BY,
    SQL_ERR_EXPECTED_ORDER_EXPR
} SqlErrorCode;

typedef enum {
    EXPR_COLUMN,
    EXPR_COUNT_STAR,
    EXPR_LITERAL_INT,
    EXPR_LITERAL_STR,
    EXPR_BINARY,
    EXPR_UNARY_NOT
} ExprKind;

typedef enum {
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LTE, OP_GTE,
    OP_AND, OP_OR
} BinaryOp;

typedef struct Expr Expr;
struct Expr {
    ExprKind kind;
    union {
        struct { const char *name; u64 name_len; } column;
        struct { s64 value; } literal_int;
        struct { const char *value; u64 value_len; } literal_str;
        struct { BinaryOp op; Expr *left; Expr *right; } binary;
        struct { Expr *operand; } unary_not;
    };
};

typedef enum { STMT_SELECT } StmtKind;

typedef enum { ORDER_ASC, ORDER_DESC } OrderDirection;

typedef struct {
    Expr *expr;
    OrderDirection direction;
} OrderByItem;

typedef struct {
    Expr **columns;
    u64 column_count;
    const char *table_name;
    u64 table_name_len;
    Expr *where_clause;
    OrderByItem *order_by;
    u64 order_by_count;
    Expr *limit;
} SelectStmt;

typedef struct {
    StmtKind kind;
    union {
        SelectStmt select;
    };
} SqlStmt;

typedef struct {
    SqlErrorCode err_code;
    u64 err_pos;
    SqlStmt statement;
} SqlParseResult;

#ifdef __has_c_attribute
  #if __has_c_attribute(nodiscard)
    #define SQL_NODISCARD [[nodiscard]]
  #else
    #define SQL_NODISCARD
  #endif
#else
  #define SQL_NODISCARD
#endif

SQL_NODISCARD SQL_EXPORT SqlParseResult parse_statement(Arena *arena, const char *text);
