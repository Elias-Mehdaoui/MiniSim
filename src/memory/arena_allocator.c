#include <sys/mman.h>
#include <unistd.h>

#include <arena_allocator.h>

arena_t *arena_create(const size_t capacity) {
    if (UNLIKELY(capacity <= 0))
        return nullptr;

    const size_t otd = arena_align_up(sizeof(arena_t), DEFAULT_ALIGNEMENT);
    if (UNLIKELY(capacity > SIZE_MAX - otd))
        return nullptr;

    const size_t total_size = otd + capacity;
    void *ptr = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (UNLIKELY(ptr == MAP_FAILED))
        return nullptr;

    arena_t *arena = ptr;
    arena->ptr = (uint8_t *)ptr + otd;
    arena->offset = 0;
    arena->capacity = capacity;
    arena->total_mapped_size = total_size;

    return arena;
}

void arena_destroy(arena_t *arena) {
    if (LIKELY(arena != nullptr))
        munmap(arena, arena->total_mapped_size);
}

void *arena_allocate_aligned(arena_t *arena, const size_t size, const size_t alignement) {
    const size_t aligned_offset = arena_align_up(arena->offset, alignement);
    const size_t new_offset = aligned_offset + size;
    if (UNLIKELY(new_offset > arena->capacity))
        return nullptr;

    void *ptr = arena->ptr + aligned_offset;
    arena->offset = new_offset;

    return ptr;
}

void *arena_allocate(arena_t *arena, const size_t size) {
    return arena_allocate_aligned(arena, size, DEFAULT_ALIGNEMENT);
}

void *arena_allocate_array(arena_t *arena, const size_t size, const size_t len) {
    size_t total_size;
    if (UNLIKELY(__builtin_mul_overflow(size, len, &total_size)))
        return nullptr;

    return arena_allocate_aligned(arena, total_size, DEFAULT_ALIGNEMENT);
}