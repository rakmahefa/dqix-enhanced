#include "perf.h"
#include "perf_raii.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include <dirent.h>
#include <sys/stat.h>

static void seed_old_logs() {
    mkdir("perf_logs", 0755);
    for (int i = 0; i < 12; ++i) {
        char path[128];
        std::snprintf(path, sizeof path, "perf_logs/session_old%02d.csv", i);
        std::ofstream old(path);
        old << "old";
    }
}

static std::string latest_log() {
    DIR* directory = opendir("perf_logs");
    assert(directory != nullptr);
    std::string found;
    dirent* entry;
    while ((entry = readdir(directory)) != nullptr) {
        std::string name(entry->d_name);
        if (name.compare(0, 8, "session_") == 0 &&
            name.size() >= 4 && name.substr(name.size() - 4) == ".csv" &&
            name.find("session_old") != 0) {
            found = "perf_logs/" + name;
            break;
        }
    }
    closedir(directory);
    return found;
}

int main() {
    setenv("DQIX_PERF_FRAME_THRESHOLD_US", "0", 1);
    setenv("DQIX_PERF_SCOPE_THRESHOLD_US", "0", 1);
    setenv("DQIX_PERF_SAMPLE_EVERY", "1", 1);
    setenv("DQIX_PERF_MAX_MB", "1", 1);
    seed_old_logs();

    perf_init();
    perf_set_scene("test-scene");
    perf_note_memory_lookup(9);
    perf_note_memory_lookup(7);
    perf_note_emulated_call(7);

    void* allocation = perf_malloc(64);
    assert(allocation != nullptr);
    allocation = perf_realloc(allocation, 128);
    assert(allocation != nullptr);
    perf_free(allocation);

    {
        ScopedPerfLogger scope("test.scoped");
        perf_note_memory_lookup(9);
        int* value = new int(7);
        assert(*value == 7);
        delete value;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    perf_frame_end(42);
    perf_shutdown();

    const std::string path = latest_log();
    assert(!path.empty());
    std::ifstream input(path.c_str());
    assert(input.good());
    const std::string contents((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());

    assert(contents.find("# build_version=") != std::string::npos);
    assert(contents.find("# git_commit=") != std::string::npos);
    assert(contents.find("memory_lookups_arm9") != std::string::npos);
    assert(contents.find("frame,42,") != std::string::npos);
    assert(contents.find("scope,") != std::string::npos);
    assert(contents.find("test.scoped") != std::string::npos);
    assert(contents.find("test-scene") != std::string::npos);

    DIR* directory = opendir("perf_logs");
    assert(directory != nullptr);
    unsigned logs = 0;
    dirent* entry;
    while ((entry = readdir(directory)) != nullptr) {
        std::string name(entry->d_name);
        if (name.compare(0, 8, "session_") == 0 &&
            name.size() >= 4 && name.substr(name.size() - 4) == ".csv")
            ++logs;
    }
    closedir(directory);
    assert(logs <= 10);

    std::cout << "performance logger, RAII scope, counters and retention: passed\n";
    return 0;
}
