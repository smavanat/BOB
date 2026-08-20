#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct Arena_t Arena;

size_t align_up(size_t value, size_t alignment);
uint8_t init_arena(Arena *arena, size_t capacity);
void destroy_arena(Arena *arena);
void *arena_alloc(Arena *arena, size_t size, size_t alignment);
void arena_clear(Arena *arena);

#endif //ARENA_H

#ifdef ARENA_IMPLEMENTATION
#include <stdlib.h>
#include <assert.h>
#include <stdalign.h>

struct Arena_t{
    void *memory;
    size_t capacity;
    size_t offset;
};

#define MIN_ALIGNMENT alignof(max_align_t)

size_t align_up(size_t value, size_t alignment) {
    //Assert that alignments are powers of 2
    assert(alignment != 0);
    assert((alignment & (alignment - 1)) == 0);
    return (value + alignment - 1) & ~(alignment - 1);
}

uint8_t init_arena(Arena *arena, size_t capacity) {
    arena->capacity = capacity;
    arena->offset = 0;
    arena->memory = malloc(capacity);

    return arena->memory != NULL;
}

void destroy_arena(Arena *arena) {
    if(arena->memory) free(arena->memory);
    *arena = (Arena){0};
}

void* arena_alloc(Arena *arena, size_t size, size_t alignment) {
    //Assert that alignments are powers of 2:
    assert(alignment != 0);
    assert((alignment & (alignment - 1)) == 0);

    uintptr_t current = (uintptr_t)arena->memory + arena->offset;
    uintptr_t offset = align_up(current, alignment);
    offset -= (uintptr_t)(arena->memory);

    if(offset + size > arena->capacity) return NULL; //Out of memory

    void *ptr = (void *)((uintptr_t)arena->memory + offset);
    arena->offset = offset + size;

    return ptr;
}

void arena_clear(Arena *arena) {
    arena->offset = 0;
}

#endif //ARENA_IMPLEMENTATION
