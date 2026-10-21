#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>
#include <CppUnitXLite/CppUnitXLite.cpp>
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Mutex.h"

using namespace thekogans::util;

TEST (thekogans, MutexTryAcquire) {
    Mutex mutex;
    mutex.Acquire ();

    bool acquired = true;
    std::thread contender ([&] {
        acquired = mutex.TryAcquire ();
    });
    contender.join ();
    mutex.Release ();

    CHECK (!acquired);

    acquired = mutex.TryAcquire ();
    CHECK (acquired);
    if (acquired) {
        mutex.Release ();
    }
}

TEST (thekogans, MutexContentionStress) {
    const unsigned int threadCount = 8;
    const unsigned int iterations = 20000;
    Mutex mutex;
    std::atomic<unsigned int> ready {0};
    std::atomic<bool> start {false};
    std::atomic<bool> insideCriticalSection {false};
    std::atomic<bool> overlapDetected {false};
    std::uint64_t protectedCount = 0;
    std::vector<std::thread> workers;
    workers.reserve (threadCount);

    for (unsigned int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        workers.emplace_back ([&, threadIndex] {
            ready.fetch_add (1, std::memory_order_release);
            while (!start.load (std::memory_order_acquire)) {
                std::this_thread::yield ();
            }

            for (unsigned int iteration = 0; iteration < iterations; ++iteration) {
                bool acquired = false;
                if ((iteration + threadIndex) % 2 == 0) {
                    acquired = mutex.TryAcquire ();
                }

                if (acquired) {
                    LockGuard<Mutex> guard (mutex, false);
                    if (insideCriticalSection.exchange (true)) {
                        overlapDetected.store (true);
                    }
                    else {
                        ++protectedCount;
                        insideCriticalSection.store (false);
                    }
                }
                else {
                    LockGuard<Mutex> guard (mutex);
                    if (insideCriticalSection.exchange (true)) {
                        overlapDetected.store (true);
                    }
                    else {
                        ++protectedCount;
                        insideCriticalSection.store (false);
                    }
                }
            }
        });
    }

    while (ready.load (std::memory_order_acquire) != threadCount) {
        std::this_thread::yield ();
    }
    start.store (true, std::memory_order_release);

    for (std::thread &worker : workers) {
        worker.join ();
    }

    CHECK (!overlapDetected.load ());
    CHECK (protectedCount == static_cast<std::uint64_t> (threadCount) * iterations);
}

TEST (thekogans, RecursiveMutex) {
    Mutex mutex (Mutex::Recursive);
    mutex.Acquire ();
    mutex.Acquire ();
    mutex.Release ();
    mutex.Release ();
}

TESTMAIN