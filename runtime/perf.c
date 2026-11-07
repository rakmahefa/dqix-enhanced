#define _POSIX_C_SOURCE 200809L
#include "perf.h"
#include "version.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#define PERF_MKDIR(path) _mkdir(path)
#define PERF_UNLINK(path) _unlink(path)
#define PERF_ACCESS(path) _access(path, 0)
#define PERF_PID() ((long)_getpid())
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define PERF_MKDIR(path) mkdir(path, 0755)
#define PERF_UNLINK(path) unlink(path)
#define PERF_ACCESS(path) access(path, F_OK)
#define PERF_PID() ((long)getpid())
#endif

#define PERF_DIR "perf_logs"
#define PERF_KEEP_PREVIOUS 9
#define PERF_DEFAULT_FRAME_THRESHOLD_US 16600u
#define PERF_DEFAULT_SCOPE_THRESHOLD_US 1000u
#define PERF_DEFAULT_SAMPLE_EVERY 60u
#define PERF_DEFAULT_MAX_MB 32u

static FILE* g_file;
static int g_initialized;
static int g_enabled = 1;
static int g_stopped_at_limit;
static int g_scene_override;
static uint64_t g_session_start_us;
static uint64_t g_frame_start_us;
static uint64_t g_frame_threshold_us = PERF_DEFAULT_FRAME_THRESHOLD_US;
static uint64_t g_scope_threshold_us = PERF_DEFAULT_SCOPE_THRESHOLD_US;
static uint64_t g_sample_every = PERF_DEFAULT_SAMPLE_EVERY;
static uint64_t g_file_limit_bytes = (uint64_t)PERF_DEFAULT_MAX_MB * 1024u * 1024u;
static char g_scene[64] = "boot";
static char g_file_path[512];
static volatile int g_lock;

static PerfCounters g_total;
static PerfCounters g_frame_baseline;
static uint64_t g_current_frame;

static void lock_file(void) {
    while (__atomic_test_and_set(&g_lock, __ATOMIC_ACQUIRE)) { }
}

static void unlock_file(void) {
    __atomic_clear(&g_lock, __ATOMIC_RELEASE);
}

static uint64_t monotonic_us(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    static int frequency_ready;
    LARGE_INTEGER counter;
    if (!frequency_ready) {
        QueryPerformanceFrequency(&frequency);
        frequency_ready = 1;
    }
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000ULL) / (uint64_t)frequency.QuadPart);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return (uint64_t)time(NULL) * 1000000ULL;
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
#endif
}

static uint64_t parse_env_u64(const char* name, uint64_t fallback, uint64_t min_value, uint64_t max_value) {
    const char* value = getenv(name);
    char* end = NULL;
    unsigned long long parsed;
    if (!value || !*value) return fallback;
    errno = 0;
    parsed = strtoull(value, &end, 10);
    if (errno || end == value || *end != '\0') return fallback;
    if (parsed < min_value) return min_value;
    if (parsed > max_value) return max_value;
    return (uint64_t)parsed;
}

static int env_is_false(const char* name) {
    const char* value = getenv(name);
    return value && (!strcmp(value, "0") || !strcmp(value, "false") ||
                     !strcmp(value, "off") || !strcmp(value, "no"));
}

static void counter_add(uint64_t* counter, uint64_t amount) {
    __atomic_fetch_add(counter, amount, __ATOMIC_RELAXED);
}

static uint64_t counter_load(const uint64_t* counter) {
    return __atomic_load_n(counter, __ATOMIC_RELAXED);
}

static PerfCounters counters_snapshot(const PerfCounters* source) {
    PerfCounters value;
#define PERF_COUNTER_COPY(field) value.field = counter_load(&source->field)
    PERF_COUNTER_COPY(memory_lookups);
    PERF_COUNTER_COPY(memory_lookups_arm9);
    PERF_COUNTER_COPY(memory_lookups_arm7);
    PERF_COUNTER_COPY(emulated_calls);
    PERF_COUNTER_COPY(emulated_calls_arm9);
    PERF_COUNTER_COPY(emulated_calls_arm7);
    PERF_COUNTER_COPY(allocations);
    PERF_COUNTER_COPY(allocated_bytes);
#undef PERF_COUNTER_COPY
    value.memory_lookups = value.memory_lookups_arm9 + value.memory_lookups_arm7;
    value.emulated_calls = value.emulated_calls_arm9 + value.emulated_calls_arm7;
    return value;
}

static PerfCounters counters_delta(const PerfCounters* end, const PerfCounters* start) {
    PerfCounters value;
    value.memory_lookups = end->memory_lookups - start->memory_lookups;
    value.memory_lookups_arm9 = end->memory_lookups_arm9 - start->memory_lookups_arm9;
    value.memory_lookups_arm7 = end->memory_lookups_arm7 - start->memory_lookups_arm7;
    value.emulated_calls = end->emulated_calls - start->emulated_calls;
    value.emulated_calls_arm9 = end->emulated_calls_arm9 - start->emulated_calls_arm9;
    value.emulated_calls_arm7 = end->emulated_calls_arm7 - start->emulated_calls_arm7;
    value.allocations = end->allocations - start->allocations;
    value.allocated_bytes = end->allocated_bytes - start->allocated_bytes;
    return value;
}

void perf_note_memory_lookup(int cpu_id) {
    if (!g_initialized || !g_enabled) return;
    if (cpu_id == 7) counter_add(&g_total.memory_lookups_arm7, 1);
    else counter_add(&g_total.memory_lookups_arm9, 1);
}

void perf_note_emulated_call(int cpu_id) {
    if (!g_initialized || !g_enabled) return;
    if (cpu_id == 7) counter_add(&g_total.emulated_calls_arm7, 1);
    else counter_add(&g_total.emulated_calls_arm9, 1);
}

void perf_note_allocation(size_t bytes) {
    if (!g_initialized || !g_enabled) return;
    counter_add(&g_total.allocations, 1);
    counter_add(&g_total.allocated_bytes, (uint64_t)bytes);
}

void* perf_malloc(size_t size) {
    void* pointer = malloc(size);
    if (pointer) perf_note_allocation(size);
    return pointer;
}

void* perf_calloc(size_t count, size_t size) {
    void* pointer = calloc(count, size);
    if (pointer) {
        uint64_t bytes = (count && size > SIZE_MAX / count) ? 0 : (uint64_t)count * (uint64_t)size;
        perf_note_allocation((size_t)bytes);
    }
    return pointer;
}

void* perf_realloc(void* pointer, size_t size) {
    void* result = realloc(pointer, size);
    if (result && size) perf_note_allocation(size);
    return result;
}

void perf_free(void* pointer) {
    free(pointer);
}

static void copy_scene(char destination[64], const char* scene) {
    size_t n;
    if (!scene || !*scene) scene = "unknown";
    n = strlen(scene);
    if (n > 63) n = 63;
    memcpy(destination, scene, n);
    destination[n] = '\0';
}

void perf_set_scene(const char* scene) {
    if (!g_initialized || !g_enabled || g_scene_override) return;
    lock_file();
    copy_scene(g_scene, scene);
    unlock_file();
}

static void csv_string(FILE* file, const char* value) {
    const unsigned char* p = (const unsigned char*)(value ? value : "");
    fputc('"', file);
    while (*p) {
        if (*p == '"') fputc('"', file);
        if (*p == '\r' || *p == '\n') fputc(' ', file);
        else fputc(*p, file);
        ++p;
    }
    fputc('"', file);
}

static void write_event_locked(const char* type, uint64_t frame, uint64_t frame_us,
                               uint64_t scope_us, const char* scope,
                               const PerfCounters* counters, const char* reason) {
    uint64_t now = monotonic_us();
    if (!g_file || g_stopped_at_limit) return;
    fprintf(g_file, "%s,%llu,%llu,%llu,%llu,",
            type,
            (unsigned long long)frame,
            (unsigned long long)(now - g_session_start_us),
            (unsigned long long)frame_us,
            (unsigned long long)scope_us);
    csv_string(g_file, scope);
    fputc(',', g_file);
    csv_string(g_file, g_scene);
    fprintf(g_file, ",%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,",
            (unsigned long long)counters->memory_lookups,
            (unsigned long long)counters->memory_lookups_arm9,
            (unsigned long long)counters->memory_lookups_arm7,
            (unsigned long long)counters->emulated_calls,
            (unsigned long long)counters->emulated_calls_arm9,
            (unsigned long long)counters->emulated_calls_arm7,
            (unsigned long long)counters->allocations,
            (unsigned long long)counters->allocated_bytes);
    csv_string(g_file, reason);
    fputc('\n', g_file);
    fflush(g_file);
    {
        long position = ftell(g_file);
        if (position >= 0 && (uint64_t)position >= g_file_limit_bytes) {
            fclose(g_file);
            g_file = NULL;
            g_stopped_at_limit = 1;
            fprintf(stderr, "[perf] session log reached its %llu MiB limit; logging stopped\n",
                    (unsigned long long)(g_file_limit_bytes / (1024u * 1024u)));
        }
    }
}

typedef struct OldLog {
    char path[512];
    time_t modified;
} OldLog;

static void consider_log(OldLog kept[PERF_KEEP_PREVIOUS], unsigned* count,
                         const char* path, time_t modified) {
    unsigned i, oldest = 0;
    if (*count < PERF_KEEP_PREVIOUS) {
        snprintf(kept[*count].path, sizeof kept[*count].path, "%s", path);
        kept[*count].modified = modified;
        ++*count;
        return;
    }
    for (i = 1; i < PERF_KEEP_PREVIOUS; ++i)
        if (kept[i].modified < kept[oldest].modified) oldest = i;
    if (modified > kept[oldest].modified) {
        PERF_UNLINK(kept[oldest].path);
        snprintf(kept[oldest].path, sizeof kept[oldest].path, "%s", path);
        kept[oldest].modified = modified;
    } else {
        PERF_UNLINK(path);
    }
}

static void cleanup_old_logs(void) {
    OldLog kept[PERF_KEEP_PREVIOUS];
    unsigned count = 0;
#ifdef _WIN32
    struct _finddata_t entry;
    intptr_t handle = _findfirst(PERF_DIR "\\session_*.csv", &entry);
    if (handle == -1) return;
    do {
        char path[512];
        struct _stat st;
        if (entry.attrib & _A_SUBDIR) continue;
        snprintf(path, sizeof path, "%s\\%s", PERF_DIR, entry.name);
        if (_stat(path, &st) == 0) consider_log(kept, &count, path, st.st_mtime);
    } while (_findnext(handle, &entry) == 0);
    _findclose(handle);
#else
    DIR* directory = opendir(PERF_DIR);
    struct dirent* entry;
    if (!directory) return;
    while ((entry = readdir(directory)) != NULL) {
        char path[512];
        struct stat st;
        size_t n = strlen(entry->d_name);
        if (strncmp(entry->d_name, "session_", 8) != 0 || n < 12 ||
            strcmp(entry->d_name + n - 4, ".csv") != 0)
            continue;
        snprintf(path, sizeof path, "%s/%s", PERF_DIR, entry->d_name);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            consider_log(kept, &count, path, st.st_mtime);
    }
    closedir(directory);
#endif
}

static void make_timestamp(char buffer[32], time_t when) {
    struct tm tm_value;
#ifdef _WIN32
    localtime_s(&tm_value, &when);
#else
    localtime_r(&when, &tm_value);
#endif
    strftime(buffer, 32, "%Y%m%d_%H%M%S", &tm_value);
}

void perf_init(void) {
    const char* commit;
    const char* version = DQIX_VERSION;
    char timestamp[32], wall_time[64], path[512];
    time_t wall_now;
    unsigned suffix;
    uint64_t max_mb;

    if (g_initialized) return;
    g_initialized = 1;
    g_enabled = !env_is_false("DQIX_PERF");
    g_frame_threshold_us = parse_env_u64("DQIX_PERF_FRAME_THRESHOLD_US",
                                         PERF_DEFAULT_FRAME_THRESHOLD_US, 0, 60000000u);
    g_scope_threshold_us = parse_env_u64("DQIX_PERF_SCOPE_THRESHOLD_US",
                                         PERF_DEFAULT_SCOPE_THRESHOLD_US, 0, 60000000u);
    g_sample_every = parse_env_u64("DQIX_PERF_SAMPLE_EVERY",
                                   PERF_DEFAULT_SAMPLE_EVERY, 1, 10000000u);
    max_mb = parse_env_u64("DQIX_PERF_MAX_MB", PERF_DEFAULT_MAX_MB, 1, 256);
    g_file_limit_bytes = max_mb * 1024u * 1024u;

    commit = getenv("DQIX_GIT_COMMIT");
#ifdef DQIX_GIT_COMMIT
    if (!commit || !*commit) commit = DQIX_GIT_COMMIT;
#endif
    if (!commit || !*commit) commit = "unknown";
    if (env_is_false("DQIX_PERF")) return;

    if (PERF_MKDIR(PERF_DIR) != 0 && errno != EEXIST) {
        fprintf(stderr, "[perf] cannot create %s: %s; performance logging disabled\n",
                PERF_DIR, strerror(errno));
        g_enabled = 0;
        return;
    }
    cleanup_old_logs();

    wall_now = time(NULL);
    make_timestamp(timestamp, wall_now);
    {
        struct tm tm_value;
#ifdef _WIN32
        localtime_s(&tm_value, &wall_now);
#else
        localtime_r(&wall_now, &tm_value);
#endif
        strftime(wall_time, sizeof wall_time, "%Y-%m-%dT%H:%M:%S%z", &tm_value);
    }

    for (suffix = 0; suffix < 1000; ++suffix) {
        if (suffix == 0)
            snprintf(path, sizeof path, "%s/session_%s_%ld.csv", PERF_DIR, timestamp, PERF_PID());
        else
            snprintf(path, sizeof path, "%s/session_%s_%ld_%u.csv", PERF_DIR, timestamp, PERF_PID(), suffix);
        if (PERF_ACCESS(path) != 0) {
            g_file = fopen(path, "wb");
            if (g_file) break;
        }
    }
    if (!g_file) {
        fprintf(stderr, "[perf] cannot create a session CSV in %s; performance logging disabled\n", PERF_DIR);
        g_enabled = 0;
        return;
    }

    snprintf(g_file_path, sizeof g_file_path, "%s", path);
    g_session_start_us = monotonic_us();
    g_frame_start_us = g_session_start_us;
    {
        const char* scene = getenv("DQIX_PERF_SCENE");
        g_scene_override = scene && *scene;
        copy_scene(g_scene, g_scene_override ? scene : "loading");
    }

    fprintf(g_file,
            "# DQIX performance log\n"
            "# session_start=%s\n"
            "# build_version=%s\n"
            "# git_commit=%s\n"
            "# clock=monotonic_microseconds\n"
            "# frame_threshold_us=%llu\n"
            "# scope_threshold_us=%llu\n"
            "# sample_every_frames=%llu\n"
            "# max_file_mb=%llu\n",
            wall_time, version, commit,
            (unsigned long long)g_frame_threshold_us,
            (unsigned long long)g_scope_threshold_us,
            (unsigned long long)g_sample_every,
            (unsigned long long)max_mb);
    fprintf(g_file,
            "event_type,frame,timestamp_us,frame_time_us,scope_duration_us,scope,game_state,"
            "memory_lookups,memory_lookups_arm9,memory_lookups_arm7,"
            "emulated_calls,emulated_calls_arm9,emulated_calls_arm7,"
            "allocations,allocated_bytes,reason\n");
    fflush(g_file);
    fprintf(stderr, "[perf] writing performance data to %s\n", g_file_path);
}

void perf_frame_end(uint64_t completed_frame) {
    uint64_t now, frame_us;
    PerfCounters current, counters;
    if (!g_initialized || !g_enabled) return;
    now = monotonic_us();
    frame_us = now >= g_frame_start_us ? now - g_frame_start_us : 0;
    current = counters_snapshot(&g_total);
    counters = counters_delta(&current, &g_frame_baseline);
    g_frame_baseline = current;
    __atomic_store_n(&g_current_frame, completed_frame + 1, __ATOMIC_RELAXED);
    int slow = frame_us >= g_frame_threshold_us;
    int periodic = (completed_frame % g_sample_every) == 0;
    g_frame_start_us = now;

    if (!slow && !periodic) return;
    lock_file();
    if (g_file) {
        write_event_locked("frame", completed_frame, frame_us, 0, "", &counters,
                           slow ? "slow_frame" : "periodic_sample");
    }
    unlock_file();
}

void perf_reset_frame_clock(void) {
    if (!g_initialized || !g_enabled) return;
    g_frame_start_us = monotonic_us();
    g_frame_baseline = counters_snapshot(&g_total);
}

void perf_scope_begin(PerfScope* scope, const char* name) {
    if (!scope) return;
    memset(scope, 0, sizeof *scope);
    if (!g_initialized || !g_enabled) return;
    scope->started_us = monotonic_us();
    scope->start = counters_snapshot(&g_total);
    if (name) snprintf(scope->name, sizeof scope->name, "%s", name);
    scope->active = 1;
}

void perf_scope_end(PerfScope* scope) {
    uint64_t now, duration;
    PerfCounters current, delta;
    if (!scope || !scope->active) return;
    scope->active = 0;
    now = monotonic_us();
    duration = now >= scope->started_us ? now - scope->started_us : 0;
    if (!g_initialized || !g_enabled || duration < g_scope_threshold_us) return;
    current = counters_snapshot(&g_total);
    delta = counters_delta(&current, &scope->start);

    lock_file();
    if (g_file) {
        write_event_locked("scope", __atomic_load_n(&g_current_frame, __ATOMIC_RELAXED),
                           0, duration, scope->name, &delta, "slow_scope");
    }
    unlock_file();
}

void perf_shutdown(void) {
    lock_file();
    if (g_file) {
        fflush(g_file);
        fclose(g_file);
        g_file = NULL;
    }
    unlock_file();
}
