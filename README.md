# SQL Parser

A SQL parser written in C, built with Zig.

## Prerequisites

- [Zig](https://ziglang.org/download/) (0.16.0-dev or later)

## Build

```sh
zig build
```

Build outputs:
- `zig-out/lib/libsqlparser.so` - shared library
- `zig-out/bin/sql_tests` - test binary

## Run tests

```sh
zig build test --summary all
```
