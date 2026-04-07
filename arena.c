#include "arena.h"

#include <stdlib.h>
#include <string.h>

Arena arena_create(u64 capacity) {
    u8 *buf = malloc(capacity);
    return (Arena){.buf = buf, .capacity = capacity, .offset = 0};
}

void *arena_alloc(Arena *arena, u64 size, u64 align) {
    u64 aligned = (arena->offset + align - 1) & ~(align - 1);
    if (aligned + size > arena->capacity) {
        return NULL;
    }
    arena->offset = aligned + size;
    return arena->buf + aligned;
}

void arena_destroy(Arena *arena) {
    free(arena->buf);
    memset(arena, 0, sizeof(*arena));
}
