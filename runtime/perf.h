#ifndef DQIX_PERF_H
#define DQIX_PERF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PerfCounters {
    uint64_t memory_lookups;
    uint64_t memory_lookups_arm9;
    uint64_t memory_lookups_arm7;
    uint64_t emulated_calls;
    uint64_t emulated_calls_arm9;
    uint64_t emulated_calls_arm7;
    uint64_t allocations;
    uint64_t allocated_bytes;
} PerfCounters;

/* Stack-allocated state used by the C API and the C++ RAII wrapper. */
typedef struct PerfScope {
    uint64_t started_us;
    PerfCounters start;
    char name[64];
    int active;
} PerfScope;

void perf_init(void);
void perf_shutdown(void);
void perf_set_scene(const char* scene);
void perf_frame_end(uint64_t completed_frame);
void perf_reset_frame_clock(void);

void perf_note_memory_lookup(int cpu_id);
void perf_note_emulated_call(int cpu_id);
void perf_note_allocation(size_t bytes);
char* perf_strdup(const char* text);

void perf_scope_begin(PerfScope* scope, const char* name);
void perf_scope_end(PerfScope* scope);

/* Allocation wrappers used by the C runtime's malloc-family macros. */
void* perf_malloc(size_t size);
void* perf_calloc(size_t count, size_t size);
void* perf_realloc(void* pointer, size_t size);
void perf_free(void* pointer);

#ifdef __cplusplus
}
#endif

#endif
