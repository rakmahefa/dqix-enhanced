#ifndef DQIX_PERF_RAII_HPP
#define DQIX_PERF_RAII_HPP

#include "perf.h"

/*
 * C++ scope helper. The emulator runtime is primarily C, so its C call sites
 * use perf_scope_begin()/perf_scope_end(); C++ modules can use this wrapper.
 */
class ScopedPerfLogger final {
public:
    explicit ScopedPerfLogger(const char* name) noexcept {
        perf_scope_begin(&scope_, name);
    }

    ~ScopedPerfLogger() noexcept {
        perf_scope_end(&scope_);
    }

    ScopedPerfLogger(const ScopedPerfLogger&) = delete;
    ScopedPerfLogger& operator=(const ScopedPerfLogger&) = delete;
    ScopedPerfLogger(ScopedPerfLogger&&) = delete;
    ScopedPerfLogger& operator=(ScopedPerfLogger&&) = delete;

private:
    PerfScope scope_;
};

#endif
