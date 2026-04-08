#pragma once

#include "spbint.h"

#define SQL_EXPORT __attribute__((visibility("default")))

typedef struct {
    u8 *buf;
    u64 capacity;
    u64 offset;
} Arena;

SQL_EXPORT Arena arena_create(u64 capacity);
SQL_EXPORT void *arena_alloc(Arena *arena, u64 size, u64 align);
SQL_EXPORT void arena_destroy(Arena *arena);
