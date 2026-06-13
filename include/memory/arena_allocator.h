#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LIKELY(x) __builtin_expect(!!(x), 1)

#define DEFAULT_ALIGNEMENT 64

typedef struct {
    uint8_t *ptr;			  // 8 bytes
    size_t offset;			  // 8 bytes
    size_t capacity;		  // 8 bytes
    size_t total_mapped_size; // 8 bytes
} __attribute__((aligned(32))) arena_t;

ALWAYS_INLINE bool is_power_of_two(const size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

ALWAYS_INLINE size_t arena_align_up(const size_t num, const size_t align) {
    assert(is_power_of_two(align) && "Alignment need to be a power of 2");
    return (num + align - 1) & ~(align - 1);
}

__attribute__((cold)) arena_t *arena_create(size_t capacity) __attribute__((warn_unused_result));

__attribute__((cold)) void arena_destroy(arena_t *arena);

ALWAYS_INLINE void arena_reset(arena_t *arena) {
    if (UNLIKELY(arena == nullptr))
        return;

    arena->offset = 0;
}

ALWAYS_INLINE size_t arena_get_capacity(const arena_t *arena) {
    if (UNLIKELY(arena == nullptr))
        return 0;

    return arena->capacity;
}

ALWAYS_INLINE size_t arena_get_used(const arena_t *arena) {
    if (UNLIKELY(arena == nullptr))
        return 0;

    return arena->offset;
}

void *arena_allocate_aligned(arena_t *arena, size_t size, size_t alignement);

void *arena_allocate(arena_t *arena, size_t size);

void *arena_allocate_array(arena_t *arena, size_t size, size_t len);

#endif // ARENA_ALLOCATOR_H