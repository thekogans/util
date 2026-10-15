#include <atomic>
#include <thread>
#include <CppUnitXLite/CppUnitXLite.cpp>
#include "thekogans/util/Thread2.h"

using namespace thekogans::util;

TEST (thekogans, Thread2Join) {
    std::atomic<bool> release {false};
    Thread2 worker ([&] {
        while (!release.load (std::memory_order_acquire)) {
            std::this_thread::yield ();
        }
    });

    CHECK (!worker.Join (TimeSpec::FromMilliseconds (1)));
    release.store (true, std::memory_order_release);
    CHECK (worker.Join (TimeSpec::FromSeconds (2)));
}

TEST (thekogans, Thread2DetachCancelsJoin) {
    std::atomic<bool> started {false};
    std::atomic<bool> release {false};
    Thread2 worker ([&] {
        started.store (true, std::memory_order_release);
        while (!release.load (std::memory_order_acquire)) {
            std::this_thread::yield ();
        }
    });

    while (!started.load (std::memory_order_acquire)) {
        std::this_thread::yield ();
    }

    std::atomic<bool> joinResult {true};
    std::thread joiner ([&] {
        joinResult.store (worker.Join (), std::memory_order_release);
    });

    worker.Detach ();
    release.store (true, std::memory_order_release);
    joiner.join ();

    CHECK (!joinResult.load (std::memory_order_acquire));
}

TESTMAIN