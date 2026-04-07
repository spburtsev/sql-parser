#include "munit/munit.h"
#include "parser.h"

// --- Existing tests (updated for Expr** access) ---

static MunitResult test_count_all(const MunitParameter params[],
                                  void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT COUNT(*) FROM users");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_int(r.statement.kind, ==, STMT_SELECT);
    munit_assert_uint64(r.statement.select.column_count, ==, 1);
    munit_assert_int(r.statement.select.columns[0]->kind, ==, EXPR_COUNT_STAR);
    munit_assert_uint64(r.statement.select.table_name_len, ==, 5);
    munit_assert_memory_equal(5, r.statement.select.table_name, "users");
    munit_assert_null(r.statement.select.where_clause);
    munit_assert_null(r.statement.select.order_by);
    munit_assert_null(r.statement.select.limit);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_select_column(const MunitParameter params[],
                                      void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT name FROM users");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_int(r.statement.kind, ==, STMT_SELECT);
    munit_assert_uint64(r.statement.select.column_count, ==, 1);
    munit_assert_int(r.statement.select.columns[0]->kind, ==, EXPR_COLUMN);
    munit_assert_uint64(r.statement.select.columns[0]->column.name_len, ==, 4);
    munit_assert_memory_equal(4, r.statement.select.columns[0]->column.name, "name");
    munit_assert_uint64(r.statement.select.table_name_len, ==, 5);
    munit_assert_memory_equal(5, r.statement.select.table_name, "users");

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_select_multi_column(const MunitParameter params[],
                                            void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a, b FROM t");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_uint64(r.statement.select.column_count, ==, 2);
    munit_assert_int(r.statement.select.columns[0]->kind, ==, EXPR_COLUMN);
    munit_assert_uint64(r.statement.select.columns[0]->column.name_len, ==, 1);
    munit_assert_memory_equal(1, r.statement.select.columns[0]->column.name, "a");
    munit_assert_int(r.statement.select.columns[1]->kind, ==, EXPR_COLUMN);
    munit_assert_uint64(r.statement.select.columns[1]->column.name_len, ==, 1);
    munit_assert_memory_equal(1, r.statement.select.columns[1]->column.name, "b");
    munit_assert_uint64(r.statement.select.table_name_len, ==, 1);
    munit_assert_memory_equal(1, r.statement.select.table_name, "t");

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_case_insensitive(const MunitParameter params[],
                                         void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "select count(*) from USERS");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_uint64(r.statement.select.column_count, ==, 1);
    munit_assert_int(r.statement.select.columns[0]->kind, ==, EXPR_COUNT_STAR);
    munit_assert_uint64(r.statement.select.table_name_len, ==, 5);
    munit_assert_memory_equal(5, r.statement.select.table_name, "USERS");

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_empty_input(const MunitParameter params[],
                                    void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "");

    munit_assert_int(r.err_code, ==, SQL_ERR_EMPTY_INPUT);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_missing_from(const MunitParameter params[],
                                     void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT name users");

    munit_assert_int(r.err_code, ==, SQL_ERR_EXPECTED_FROM);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_missing_table(const MunitParameter params[],
                                      void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT name FROM");

    munit_assert_int(r.err_code, ==, SQL_ERR_EXPECTED_TABLE_NAME);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_bad_count_syntax(const MunitParameter params[],
                                         void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT COUNT FROM t");

    munit_assert_int(r.err_code, ==, SQL_ERR_EXPECTED_LPAREN);

    arena_destroy(&arena);
    return MUNIT_OK;
}

// --- WHERE clause tests ---

static MunitResult test_where_eq(const MunitParameter params[],
                                 void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t WHERE x = 1");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_not_null(r.statement.select.where_clause);

    Expr *w = r.statement.select.where_clause;
    munit_assert_int(w->kind, ==, EXPR_BINARY);
    munit_assert_int(w->binary.op, ==, OP_EQ);
    munit_assert_int(w->binary.left->kind, ==, EXPR_COLUMN);
    munit_assert_uint64(w->binary.left->column.name_len, ==, 1);
    munit_assert_memory_equal(1, w->binary.left->column.name, "x");
    munit_assert_int(w->binary.right->kind, ==, EXPR_LITERAL_INT);
    munit_assert_int64(w->binary.right->literal_int.value, ==, 1);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_where_string(const MunitParameter params[],
                                     void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t WHERE name = 'alice'");

    munit_assert_int(r.err_code, ==, SQL_OK);
    Expr *rhs = r.statement.select.where_clause->binary.right;
    munit_assert_int(rhs->kind, ==, EXPR_LITERAL_STR);
    munit_assert_uint64(rhs->literal_str.value_len, ==, 5);
    munit_assert_memory_equal(5, rhs->literal_str.value, "alice");

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_where_and(const MunitParameter params[],
                                  void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t WHERE x = 1 AND y > 2");

    munit_assert_int(r.err_code, ==, SQL_OK);
    Expr *w = r.statement.select.where_clause;
    munit_assert_int(w->kind, ==, EXPR_BINARY);
    munit_assert_int(w->binary.op, ==, OP_AND);
    munit_assert_int(w->binary.left->kind, ==, EXPR_BINARY);
    munit_assert_int(w->binary.left->binary.op, ==, OP_EQ);
    munit_assert_int(w->binary.right->kind, ==, EXPR_BINARY);
    munit_assert_int(w->binary.right->binary.op, ==, OP_GT);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_where_or(const MunitParameter params[],
                                 void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t WHERE x = 1 OR y = 2");

    munit_assert_int(r.err_code, ==, SQL_OK);
    Expr *w = r.statement.select.where_clause;
    munit_assert_int(w->kind, ==, EXPR_BINARY);
    munit_assert_int(w->binary.op, ==, OP_OR);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_where_precedence(const MunitParameter params[],
                                         void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena,
        "SELECT a FROM t WHERE a = 1 OR b = 2 AND c = 3");

    munit_assert_int(r.err_code, ==, SQL_OK);
    Expr *w = r.statement.select.where_clause;
    munit_assert_int(w->kind, ==, EXPR_BINARY);
    munit_assert_int(w->binary.op, ==, OP_OR);
    munit_assert_int(w->binary.right->kind, ==, EXPR_BINARY);
    munit_assert_int(w->binary.right->binary.op, ==, OP_AND);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_where_not(const MunitParameter params[],
                                  void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t WHERE NOT x = 1");

    munit_assert_int(r.err_code, ==, SQL_OK);
    Expr *w = r.statement.select.where_clause;
    munit_assert_int(w->kind, ==, EXPR_UNARY_NOT);
    munit_assert_int(w->unary_not.operand->kind, ==, EXPR_BINARY);
    munit_assert_int(w->unary_not.operand->binary.op, ==, OP_EQ);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_where_parens(const MunitParameter params[],
                                     void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena,
        "SELECT a FROM t WHERE (a = 1 OR b = 2) AND c = 3");

    munit_assert_int(r.err_code, ==, SQL_OK);
    Expr *w = r.statement.select.where_clause;
    munit_assert_int(w->kind, ==, EXPR_BINARY);
    munit_assert_int(w->binary.op, ==, OP_AND);
    munit_assert_int(w->binary.left->kind, ==, EXPR_BINARY);
    munit_assert_int(w->binary.left->binary.op, ==, OP_OR);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_where_all_ops(const MunitParameter params[],
                                      void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    struct { const char *sql; BinaryOp expected_op; } cases[] = {
        {"SELECT a FROM t WHERE x = 1",  OP_EQ},
        {"SELECT a FROM t WHERE x != 1", OP_NEQ},
        {"SELECT a FROM t WHERE x < 1",  OP_LT},
        {"SELECT a FROM t WHERE x > 1",  OP_GT},
        {"SELECT a FROM t WHERE x <= 1", OP_LTE},
        {"SELECT a FROM t WHERE x >= 1", OP_GTE},
    };

    for (u64 i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Arena a = arena_create(4096);
        SqlParseResult r = parse_statement(&a, cases[i].sql);
        munit_assert_int(r.err_code, ==, SQL_OK);
        munit_assert_int(r.statement.select.where_clause->binary.op, ==, cases[i].expected_op);
        arena_destroy(&a);
    }

    return MUNIT_OK;
}

// --- ORDER BY tests ---

static MunitResult test_order_by_asc(const MunitParameter params[],
                                     void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t ORDER BY a");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_not_null(r.statement.select.order_by);
    munit_assert_uint64(r.statement.select.order_by_count, ==, 1);
    munit_assert_int(r.statement.select.order_by[0].expr->kind, ==, EXPR_COLUMN);
    munit_assert_int(r.statement.select.order_by[0].direction, ==, ORDER_ASC);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_order_by_desc(const MunitParameter params[],
                                      void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t ORDER BY a DESC");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_uint64(r.statement.select.order_by_count, ==, 1);
    munit_assert_int(r.statement.select.order_by[0].direction, ==, ORDER_DESC);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_order_by_multi(const MunitParameter params[],
                                       void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t ORDER BY a ASC, b DESC");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_uint64(r.statement.select.order_by_count, ==, 2);
    munit_assert_int(r.statement.select.order_by[0].direction, ==, ORDER_ASC);
    munit_assert_int(r.statement.select.order_by[1].direction, ==, ORDER_DESC);

    arena_destroy(&arena);
    return MUNIT_OK;
}

// --- LIMIT tests ---

static MunitResult test_limit(const MunitParameter params[],
                              void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t LIMIT 10");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_not_null(r.statement.select.limit);
    munit_assert_int(r.statement.select.limit->kind, ==, EXPR_LITERAL_INT);
    munit_assert_int64(r.statement.select.limit->literal_int.value, ==, 10);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_no_limit(const MunitParameter params[],
                                 void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_null(r.statement.select.limit);

    arena_destroy(&arena);
    return MUNIT_OK;
}

// --- Combined tests ---

static MunitResult test_full_query(const MunitParameter params[],
                                   void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena,
        "SELECT a, b FROM t WHERE a > 1 ORDER BY b DESC LIMIT 5");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_uint64(r.statement.select.column_count, ==, 2);
    munit_assert_not_null(r.statement.select.where_clause);
    munit_assert_int(r.statement.select.where_clause->binary.op, ==, OP_GT);
    munit_assert_uint64(r.statement.select.order_by_count, ==, 1);
    munit_assert_int(r.statement.select.order_by[0].direction, ==, ORDER_DESC);
    munit_assert_not_null(r.statement.select.limit);
    munit_assert_int64(r.statement.select.limit->literal_int.value, ==, 5);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_semicolon(const MunitParameter params[],
                                  void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t;");

    munit_assert_int(r.err_code, ==, SQL_OK);
    munit_assert_uint64(r.statement.select.column_count, ==, 1);

    arena_destroy(&arena);
    return MUNIT_OK;
}

// --- Error tests ---

static MunitResult test_where_missing_expr(const MunitParameter params[],
                                           void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t WHERE");

    munit_assert_int(r.err_code, ==, SQL_ERR_EXPECTED_EXPRESSION);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_unterminated_string(const MunitParameter params[],
                                            void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t WHERE x = 'abc");

    munit_assert_int(r.err_code, ==, SQL_ERR_UNTERMINATED_STRING);

    arena_destroy(&arena);
    return MUNIT_OK;
}

static MunitResult test_order_missing_by(const MunitParameter params[],
                                         void *user_data_or_fixture) {
    (void)params;
    (void)user_data_or_fixture;

    Arena arena = arena_create(4096);
    SqlParseResult r = parse_statement(&arena, "SELECT a FROM t ORDER a");

    munit_assert_int(r.err_code, ==, SQL_ERR_EXPECTED_BY);

    arena_destroy(&arena);
    return MUNIT_OK;
}

// --- Test suite ---

MunitTest tests[] = {
    {"/count-all", test_count_all, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/select-column", test_select_column, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/select-multi-column", test_select_multi_column, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/case-insensitive", test_case_insensitive, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/empty-input", test_empty_input, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/missing-from", test_missing_from, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/missing-table", test_missing_table, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/bad-count-syntax", test_bad_count_syntax, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/where-eq", test_where_eq, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/where-string", test_where_string, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/where-and", test_where_and, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/where-or", test_where_or, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/where-precedence", test_where_precedence, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/where-not", test_where_not, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/where-parens", test_where_parens, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/where-all-ops", test_where_all_ops, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/order-by-asc", test_order_by_asc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/order-by-desc", test_order_by_desc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/order-by-multi", test_order_by_multi, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/limit", test_limit, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/no-limit", test_no_limit, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/full-query", test_full_query, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/semicolon", test_semicolon, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/where-missing-expr", test_where_missing_expr, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/unterminated-string", test_unterminated_string, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/order-missing-by", test_order_missing_by, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {
    .prefix = "/sql-parser",
    .tests = tests,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
};

int main(int argc, char *const argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
