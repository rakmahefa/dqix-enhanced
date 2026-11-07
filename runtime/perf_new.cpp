#include "perf.h"

#include <cstdlib>
#include <new>

/*
 * Keep the game runtime's C++ allocations visible to the same counters as
 * malloc/calloc/realloc. The implementation delegates to the C allocator to
 * preserve the normal allocation/deallocation pairing.
 */
void* operator new(std::size_t size) {
    const std::size_t actual = size ? size : 1;
    void* pointer = std::malloc(actual);
    if (!pointer) throw std::bad_alloc();
    perf_note_allocation(size);
    return pointer;
}

void* operator new[](std::size_t size) {
    const std::size_t actual = size ? size : 1;
    void* pointer = std::malloc(actual);
    if (!pointer) throw std::bad_alloc();
    perf_note_allocation(size);
    return pointer;
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}
