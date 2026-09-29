// ============================================================================
// tools/f1_smoke_consumer/main.cpp — F1 packaging smoke consumer
// (TRACKED template; the driver materializes this file into an
// out-of-tree consumer project under .generated-temp at run time).
//
// Consumes the INSTALLED qiven-foundation package through find_package:
// installs the diag service with a sink path from argv, proves the
// one-writer law (a second install() while one instance is live is a
// typed failure — through the installed library, not the repo build),
// emits one event at EVERY severity, checks health/crash-ring
// observability, shuts down with flushed accounting and writes a marker
// file the driver asserts. --overhead N mode times N emits with
// QueryPerformanceCounter (p50/p99/mean) for the static-vs-shared
// comparison.
// ============================================================================

#include <qiven/diag/service.hpp>

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{

int fail(const char* what) noexcept
{
    std::fprintf(stderr, "[FAIL] f1-consumer: %s\n", what);
    return 2;
}

bool write_marker(const char* path, const char* text) noexcept
{
    std::FILE* f = std::fopen(path, "wb");
    if (f == nullptr)
        return false;
    std::fputs(text, f);
    std::fclose(f);
    return true;
}

int smoke_run(const char* sink, const char* marker) noexcept
{
    qiven::diag::service_config cfg;
    cfg.file.path             = sink;
    cfg.file.size_bound_bytes = 1u << 20;
    cfg.file.generations      = 2;

    const qiven::diag::install_result first = qiven::diag::install(cfg);
    if (!first.ok || !first.host_emitter.valid())
        return fail("first install() failed");
    if (first.failure != nullptr)
        return fail("first install() reported a typed failure");

    // one-writer law THROUGH the installed package: a second install
    // while one instance is live fails typed
    const qiven::diag::install_result second = qiven::diag::install(cfg);
    if (second.ok)
        return fail("second install() unexpectedly succeeded");
    if (second.failure == nullptr || std::strstr(second.failure, "already installed") == nullptr)
        return fail("second install() did not fail typed");

    static const qiven::diag::severity levels[] = {
        qiven::diag::severity::trace, qiven::diag::severity::debug,
        qiven::diag::severity::info,  qiven::diag::severity::warn,
        qiven::diag::severity::error, qiven::diag::severity::critical,
    };
    for (unsigned i = 0; i < 6; ++i)
    {
        first.host_emitter.emit(levels[i], qiven::diag::event_id { 100 + i },
                                "f1 packaging smoke event");
    }

    const qiven::diag::health_snapshot snap = qiven::diag::health();
    if (snap.general_emitted < 5 || snap.critical_emitted < 1)
        return fail("health() did not observe the emits");
    if (!snap.writer_alive)
        return fail("writer not alive after install");
    if (snap.sink_write_failures != 0)
        return fail("sink write failures observed");

    const qiven::diag::crash_ring* ring = qiven::diag::crash_history();
    if (ring == nullptr)
        return fail("crash_history() null while installed");
    const qiven::diag::crash_ring::snapshot ring_snap = ring->capture_newest(8);
    if (ring_snap.count < 6)
        return fail("crash ring snapshot missing emitted events");

    const qiven::diag::shutdown_result shut = qiven::diag::shutdown();
    if (!shut.writer_flushed || shut.leftover_general != 0 || shut.leftover_critical != 0)
        return fail("shutdown() did not flush");
    if (shut.drained < 6)
        return fail("shutdown() drained fewer events than emitted");
    if (qiven::diag::health().writer_alive)
        return fail("writer alive after shutdown");

    char text[160];
    std::snprintf(text, sizeof(text), "ok=1 emitted=6 drained=%llu critical=%llu\n",
                  static_cast<unsigned long long>(shut.drained),
                  static_cast<unsigned long long>(snap.critical_emitted));
    if (!write_marker(marker, text))
        return fail("marker write failed");
    std::printf("f1-consumer: smoke ok (emitted=6 drained=%llu)\n",
                static_cast<unsigned long long>(shut.drained));
    return 0;
}

int overhead_run(unsigned count, const char* sink, const char* marker) noexcept
{
    qiven::diag::service_config cfg;
    cfg.file.path             = sink;
    cfg.file.size_bound_bytes = 1u << 20;
    cfg.file.generations      = 2;

    const qiven::diag::install_result inst = qiven::diag::install(cfg);
    if (!inst.ok)
        return fail("install() failed (overhead mode)");
    const qiven::diag::emitter em = inst.host_emitter;

    for (unsigned i = 0; i < 2000; ++i) // warm lanes/writer before timing
        em.emit(qiven::diag::severity::info, qiven::diag::event_id { 7 }, "warmup");

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    std::vector<double> per_emit_ns(count);
    for (unsigned i = 0; i < count; ++i)
    {
        LARGE_INTEGER before, after;
        QueryPerformanceCounter(&before);
        em.emit(qiven::diag::severity::info, qiven::diag::event_id { 7 }, "overhead probe");
        QueryPerformanceCounter(&after);
        per_emit_ns[i] = static_cast<double>(after.QuadPart - before.QuadPart) * 1e9
                         / static_cast<double>(frequency.QuadPart);
    }
    std::sort(per_emit_ns.begin(), per_emit_ns.end());
    const double p50 = per_emit_ns[count / 2];
    const double p99 = per_emit_ns[(count * 99) / 100];
    double total = 0.0;
    for (double v : per_emit_ns)
        total += v;
    std::printf("OVERHEAD n=%u p50=%.1fns p99=%.1fns mean=%.1fns\n", count, p50, p99,
                total / static_cast<double>(count));
    std::fflush(stdout);

    const qiven::diag::shutdown_result shut = qiven::diag::shutdown();
    if (!shut.writer_flushed)
        return fail("shutdown() did not flush (overhead mode)");
    if (!write_marker(marker, "ok=1 overhead=1\n"))
        return fail("marker write failed");
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "--overhead") == 0)
    {
        if (argc != 5)
            return fail("usage: f1-consumer --overhead <n> <sink> <marker>");
        const unsigned count = static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10));
        if (count == 0)
            return fail("--overhead count must be positive");
        return overhead_run(count, argv[3], argv[4]);
    }
    if (argc != 3)
        return fail("usage: f1-consumer <sink> <marker>");
    return smoke_run(argv[1], argv[2]);
}
