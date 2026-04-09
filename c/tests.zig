const std = @import("std");
const testing = std.testing;
const sql = @cImport({
    @cInclude("parser.h");
});

const Expr = sql.Expr;

fn parse(input: [*:0]const u8) sql.SqlParseResult {
    var arena = sql.arena_create(4096);
    return sql.parse_statement(&arena, input);
}

fn expectEnum(expected: c_int, actual: c_uint) !void {
    try testing.expectEqual(@as(c_uint, @intCast(expected)), actual);
}

fn strEql(ptr: [*c]const u8, len: u64, expected: []const u8) !void {
    try testing.expectEqualStrings(expected, ptr[0..len]);
}

fn s(r: sql.SqlParseResult) sql.SelectStmt {
    return r.statement.unnamed_0.select;
}

// --- Basic SELECT ---

test "count all" {
    const r = parse("SELECT COUNT(*) FROM users");
    try expectEnum(sql.SQL_OK, r.err_code);
    try expectEnum(sql.STMT_SELECT, r.statement.kind);
    const sel = s(r);
    try testing.expectEqual(@as(u64, 1), sel.column_count);
    try expectEnum(sql.EXPR_COUNT_STAR, sel.columns[0].*.kind);
    try strEql(sel.table_name, sel.table_name_len, "users");
    try testing.expectEqual(@as([*c]Expr, null), sel.where_clause);
    try testing.expectEqual(@as([*c]sql.OrderByItem, null), sel.order_by);
    try testing.expectEqual(@as([*c]Expr, null), sel.limit);
}

test "select column" {
    const r = parse("SELECT name FROM users");
    try expectEnum(sql.SQL_OK, r.err_code);
    const sel = s(r);
    try testing.expectEqual(@as(u64, 1), sel.column_count);
    const col = sel.columns[0].*;
    try expectEnum(sql.EXPR_COLUMN, col.kind);
    try strEql(col.unnamed_0.column.name, col.unnamed_0.column.name_len, "name");
    try strEql(sel.table_name, sel.table_name_len, "users");
}

test "select multi column" {
    const r = parse("SELECT a, b FROM t");
    try expectEnum(sql.SQL_OK, r.err_code);
    const sel = s(r);
    try testing.expectEqual(@as(u64, 2), sel.column_count);
    try expectEnum(sql.EXPR_COLUMN, sel.columns[0].*.kind);
    try strEql(sel.columns[0].*.unnamed_0.column.name, sel.columns[0].*.unnamed_0.column.name_len, "a");
    try expectEnum(sql.EXPR_COLUMN, sel.columns[1].*.kind);
    try strEql(sel.columns[1].*.unnamed_0.column.name, sel.columns[1].*.unnamed_0.column.name_len, "b");
    try strEql(sel.table_name, sel.table_name_len, "t");
}

test "case insensitive" {
    const r = parse("select count(*) from USERS");
    try expectEnum(sql.SQL_OK, r.err_code);
    const sel = s(r);
    try testing.expectEqual(@as(u64, 1), sel.column_count);
    try expectEnum(sql.EXPR_COUNT_STAR, sel.columns[0].*.kind);
    try strEql(sel.table_name, sel.table_name_len, "USERS");
}

// --- Error cases ---

test "empty input" {
    try expectEnum(sql.SQL_ERR_EMPTY_INPUT, parse("").err_code);
}

test "missing from" {
    try expectEnum(sql.SQL_ERR_EXPECTED_FROM, parse("SELECT name users").err_code);
}

test "missing table" {
    try expectEnum(sql.SQL_ERR_EXPECTED_TABLE_NAME, parse("SELECT name FROM").err_code);
}

test "bad count syntax" {
    try expectEnum(sql.SQL_ERR_EXPECTED_LPAREN, parse("SELECT COUNT FROM t").err_code);
}

// --- WHERE clause ---

test "where eq" {
    const r = parse("SELECT a FROM t WHERE x = 1");
    try expectEnum(sql.SQL_OK, r.err_code);
    const w = s(r).where_clause[0];
    try expectEnum(sql.EXPR_BINARY, w.kind);
    try expectEnum(sql.OP_EQ, w.unnamed_0.binary.op);
    const lhs = w.unnamed_0.binary.left[0];
    try expectEnum(sql.EXPR_COLUMN, lhs.kind);
    try strEql(lhs.unnamed_0.column.name, lhs.unnamed_0.column.name_len, "x");
    const rhs = w.unnamed_0.binary.right[0];
    try expectEnum(sql.EXPR_LITERAL_INT, rhs.kind);
    try testing.expectEqual(@as(i64, 1), rhs.unnamed_0.literal_int.value);
}

test "where string" {
    const r = parse("SELECT a FROM t WHERE name = 'alice'");
    try expectEnum(sql.SQL_OK, r.err_code);
    const rhs = s(r).where_clause[0].unnamed_0.binary.right[0];
    try expectEnum(sql.EXPR_LITERAL_STR, rhs.kind);
    try strEql(rhs.unnamed_0.literal_str.value, rhs.unnamed_0.literal_str.value_len, "alice");
}

test "where and" {
    const r = parse("SELECT a FROM t WHERE x = 1 AND y > 2");
    try expectEnum(sql.SQL_OK, r.err_code);
    const w = s(r).where_clause[0];
    try expectEnum(sql.EXPR_BINARY, w.kind);
    try expectEnum(sql.OP_AND, w.unnamed_0.binary.op);
    try expectEnum(sql.EXPR_BINARY, w.unnamed_0.binary.left[0].kind);
    try expectEnum(sql.OP_EQ, w.unnamed_0.binary.left[0].unnamed_0.binary.op);
    try expectEnum(sql.EXPR_BINARY, w.unnamed_0.binary.right[0].kind);
    try expectEnum(sql.OP_GT, w.unnamed_0.binary.right[0].unnamed_0.binary.op);
}

test "where or" {
    const r = parse("SELECT a FROM t WHERE x = 1 OR y = 2");
    try expectEnum(sql.SQL_OK, r.err_code);
    try expectEnum(sql.OP_OR, s(r).where_clause[0].unnamed_0.binary.op);
}

test "where precedence" {
    const r = parse("SELECT a FROM t WHERE a = 1 OR b = 2 AND c = 3");
    try expectEnum(sql.SQL_OK, r.err_code);
    const w = s(r).where_clause[0];
    try expectEnum(sql.OP_OR, w.unnamed_0.binary.op);
    try expectEnum(sql.OP_AND, w.unnamed_0.binary.right[0].unnamed_0.binary.op);
}

test "where not" {
    const r = parse("SELECT a FROM t WHERE NOT x = 1");
    try expectEnum(sql.SQL_OK, r.err_code);
    const w = s(r).where_clause[0];
    try expectEnum(sql.EXPR_UNARY_NOT, w.kind);
    const operand = w.unnamed_0.unary_not.operand[0];
    try expectEnum(sql.EXPR_BINARY, operand.kind);
    try expectEnum(sql.OP_EQ, operand.unnamed_0.binary.op);
}

test "where parens" {
    const r = parse("SELECT a FROM t WHERE (a = 1 OR b = 2) AND c = 3");
    try expectEnum(sql.SQL_OK, r.err_code);
    const w = s(r).where_clause[0];
    try expectEnum(sql.OP_AND, w.unnamed_0.binary.op);
    try expectEnum(sql.OP_OR, w.unnamed_0.binary.left[0].unnamed_0.binary.op);
}

test "where all ops" {
    const cases = [_]struct { input: [*:0]const u8, expected: c_int }{
        .{ .input = "SELECT a FROM t WHERE x = 1", .expected = sql.OP_EQ },
        .{ .input = "SELECT a FROM t WHERE x != 1", .expected = sql.OP_NEQ },
        .{ .input = "SELECT a FROM t WHERE x < 1", .expected = sql.OP_LT },
        .{ .input = "SELECT a FROM t WHERE x > 1", .expected = sql.OP_GT },
        .{ .input = "SELECT a FROM t WHERE x <= 1", .expected = sql.OP_LTE },
        .{ .input = "SELECT a FROM t WHERE x >= 1", .expected = sql.OP_GTE },
    };
    for (cases) |case| {
        var arena = sql.arena_create(4096);
        const r = sql.parse_statement(&arena, case.input);
        try expectEnum(sql.SQL_OK, r.err_code);
        try expectEnum(case.expected, r.statement.unnamed_0.select.where_clause[0].unnamed_0.binary.op);
        sql.arena_destroy(&arena);
    }
}

// --- ORDER BY ---

test "order by asc" {
    const r = parse("SELECT a FROM t ORDER BY a");
    try expectEnum(sql.SQL_OK, r.err_code);
    const sel = s(r);
    try testing.expect(sel.order_by != null);
    try testing.expectEqual(@as(u64, 1), sel.order_by_count);
    try expectEnum(sql.EXPR_COLUMN, sel.order_by[0].expr[0].kind);
    try expectEnum(sql.ORDER_ASC, sel.order_by[0].direction);
}

test "order by desc" {
    const r = parse("SELECT a FROM t ORDER BY a DESC");
    try expectEnum(sql.SQL_OK, r.err_code);
    const sel = s(r);
    try testing.expectEqual(@as(u64, 1), sel.order_by_count);
    try expectEnum(sql.ORDER_DESC, sel.order_by[0].direction);
}

test "order by multi" {
    const r = parse("SELECT a FROM t ORDER BY a ASC, b DESC");
    try expectEnum(sql.SQL_OK, r.err_code);
    const sel = s(r);
    try testing.expectEqual(@as(u64, 2), sel.order_by_count);
    try expectEnum(sql.ORDER_ASC, sel.order_by[0].direction);
    try expectEnum(sql.ORDER_DESC, sel.order_by[1].direction);
}

// --- LIMIT ---

test "limit" {
    const r = parse("SELECT a FROM t LIMIT 10");
    try expectEnum(sql.SQL_OK, r.err_code);
    const sel = s(r);
    try testing.expect(sel.limit != null);
    try expectEnum(sql.EXPR_LITERAL_INT, sel.limit[0].kind);
    try testing.expectEqual(@as(i64, 10), sel.limit[0].unnamed_0.literal_int.value);
}

test "no limit" {
    const r = parse("SELECT a FROM t");
    try expectEnum(sql.SQL_OK, r.err_code);
    try testing.expectEqual(@as([*c]Expr, null), s(r).limit);
}

// --- Combined ---

test "full query" {
    const r = parse("SELECT a, b FROM t WHERE a > 1 ORDER BY b DESC LIMIT 5");
    try expectEnum(sql.SQL_OK, r.err_code);
    const sel = s(r);
    try testing.expectEqual(@as(u64, 2), sel.column_count);
    try testing.expect(sel.where_clause != null);
    try expectEnum(sql.OP_GT, sel.where_clause[0].unnamed_0.binary.op);
    try testing.expectEqual(@as(u64, 1), sel.order_by_count);
    try expectEnum(sql.ORDER_DESC, sel.order_by[0].direction);
    try testing.expect(sel.limit != null);
    try testing.expectEqual(@as(i64, 5), sel.limit[0].unnamed_0.literal_int.value);
}

test "semicolon" {
    const r = parse("SELECT a FROM t;");
    try expectEnum(sql.SQL_OK, r.err_code);
    try testing.expectEqual(@as(u64, 1), s(r).column_count);
}

// --- More error cases ---

test "where missing expr" {
    try expectEnum(sql.SQL_ERR_EXPECTED_EXPRESSION, parse("SELECT a FROM t WHERE").err_code);
}

test "unterminated string" {
    try expectEnum(sql.SQL_ERR_UNTERMINATED_STRING, parse("SELECT a FROM t WHERE x = 'abc").err_code);
}

test "order missing by" {
    try expectEnum(sql.SQL_ERR_EXPECTED_BY, parse("SELECT a FROM t ORDER a").err_code);
}

// --- CREATE TABLE ---

fn ct(r: sql.SqlParseResult) sql.CreateTableStmt {
    return r.statement.unnamed_0.create_table;
}

test "create table basic" {
    const r = parse("CREATE TABLE users (id INT, name VARCHAR(255))");
    try expectEnum(sql.SQL_OK, r.err_code);
    try expectEnum(sql.STMT_CREATE_TABLE, r.statement.kind);
    const stmt = ct(r);
    try strEql(stmt.table_name, stmt.table_name_len, "users");
    try testing.expectEqual(@as(u64, 2), stmt.column_count);

    try strEql(stmt.columns[0].name, stmt.columns[0].name_len, "id");
    try strEql(stmt.columns[0].type_name, stmt.columns[0].type_name_len, "INT");

    try strEql(stmt.columns[1].name, stmt.columns[1].name_len, "name");
    try strEql(stmt.columns[1].type_name, stmt.columns[1].type_name_len, "VARCHAR(255)");
}

test "create table single column" {
    const r = parse("CREATE TABLE t (id INT)");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try strEql(stmt.table_name, stmt.table_name_len, "t");
    try testing.expectEqual(@as(u64, 1), stmt.column_count);
    try strEql(stmt.columns[0].name, stmt.columns[0].name_len, "id");
    try strEql(stmt.columns[0].type_name, stmt.columns[0].type_name_len, "INT");
}

test "create table many columns" {
    const r = parse("CREATE TABLE t (a INT, b TEXT, c BOOLEAN)");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try testing.expectEqual(@as(u64, 3), stmt.column_count);
    try strEql(stmt.columns[0].name, stmt.columns[0].name_len, "a");
    try strEql(stmt.columns[0].type_name, stmt.columns[0].type_name_len, "INT");
    try strEql(stmt.columns[1].name, stmt.columns[1].name_len, "b");
    try strEql(stmt.columns[1].type_name, stmt.columns[1].type_name_len, "TEXT");
    try strEql(stmt.columns[2].name, stmt.columns[2].name_len, "c");
    try strEql(stmt.columns[2].type_name, stmt.columns[2].type_name_len, "BOOLEAN");
}

test "create table with semicolon" {
    const r = parse("CREATE TABLE t (x INT);");
    try expectEnum(sql.SQL_OK, r.err_code);
    try expectEnum(sql.STMT_CREATE_TABLE, r.statement.kind);
}

test "create table case insensitive" {
    const r = parse("create table Users (Id int)");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try strEql(stmt.table_name, stmt.table_name_len, "Users");
    try strEql(stmt.columns[0].name, stmt.columns[0].name_len, "Id");
    try strEql(stmt.columns[0].type_name, stmt.columns[0].type_name_len, "int");
}

test "create table decimal type" {
    const r = parse("CREATE TABLE t (price DECIMAL(10,2))");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try strEql(stmt.columns[0].type_name, stmt.columns[0].type_name_len, "DECIMAL(10,2)");
}

test "create table missing table keyword" {
    try expectEnum(sql.SQL_ERR_EXPECTED_TABLE, parse("CREATE users (id INT)").err_code);
}

test "create table missing table name" {
    try expectEnum(sql.SQL_ERR_EXPECTED_TABLE_NAME, parse("CREATE TABLE (id INT)").err_code);
}

test "create table missing lparen" {
    try expectEnum(sql.SQL_ERR_EXPECTED_LPAREN, parse("CREATE TABLE t id INT)").err_code);
}

test "create table missing column type" {
    try expectEnum(sql.SQL_ERR_EXPECTED_COLUMN_TYPE, parse("CREATE TABLE t (id)").err_code);
}

test "create table missing rparen" {
    try expectEnum(sql.SQL_ERR_EXPECTED_RPAREN, parse("CREATE TABLE t (id INT").err_code);
}

// --- Column constraints ---

test "create table not null" {
    const r = parse("CREATE TABLE t (id INT NOT NULL)");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try testing.expectEqual(@as(u64, 1), stmt.column_count);
    try strEql(stmt.columns[0].type_name, stmt.columns[0].type_name_len, "INT");
    try testing.expect(stmt.columns[0].not_null);
    try testing.expect(!stmt.columns[0].is_primary_key);
    try testing.expect(!stmt.columns[0].is_unique);
}

test "create table explicit null" {
    const r = parse("CREATE TABLE t (id INT NULL)");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try testing.expect(!stmt.columns[0].not_null);
}

test "create table primary key" {
    const r = parse("CREATE TABLE t (id INT PRIMARY KEY)");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try testing.expect(stmt.columns[0].is_primary_key);
    try testing.expect(!stmt.columns[0].not_null);
}

test "create table unique" {
    const r = parse("CREATE TABLE t (email VARCHAR(255) UNIQUE)");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try strEql(stmt.columns[0].type_name, stmt.columns[0].type_name_len, "VARCHAR(255)");
    try testing.expect(stmt.columns[0].is_unique);
}

test "create table default int" {
    const r = parse("CREATE TABLE t (age INT DEFAULT 0)");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try expectEnum(sql.DEFAULT_INT, stmt.columns[0].default_value.kind);
    try testing.expectEqual(@as(i64, 0), stmt.columns[0].default_value.unnamed_0.int_value);
}

test "create table default string" {
    const r = parse("CREATE TABLE t (status TEXT DEFAULT 'active')");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try expectEnum(sql.DEFAULT_STR, stmt.columns[0].default_value.kind);
    try strEql(
        stmt.columns[0].default_value.unnamed_0.str_value.value,
        stmt.columns[0].default_value.unnamed_0.str_value.value_len,
        "active",
    );
}

test "create table multiple modifiers" {
    const r = parse("CREATE TABLE t (id INT NOT NULL PRIMARY KEY)");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try testing.expect(stmt.columns[0].not_null);
    try testing.expect(stmt.columns[0].is_primary_key);
}

test "create table mixed columns with constraints" {
    const r = parse("CREATE TABLE t (id INT NOT NULL PRIMARY KEY, name TEXT NOT NULL, age INT DEFAULT 0)");
    try expectEnum(sql.SQL_OK, r.err_code);
    const stmt = ct(r);
    try testing.expectEqual(@as(u64, 3), stmt.column_count);

    try testing.expect(stmt.columns[0].not_null);
    try testing.expect(stmt.columns[0].is_primary_key);

    try testing.expect(stmt.columns[1].not_null);
    try testing.expect(!stmt.columns[1].is_primary_key);

    try testing.expect(!stmt.columns[2].not_null);
    try expectEnum(sql.DEFAULT_INT, stmt.columns[2].default_value.kind);
    try testing.expectEqual(@as(i64, 0), stmt.columns[2].default_value.unnamed_0.int_value);
}
