#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>
#include <CppUnitXLite/CppUnitXLite.cpp>
#include "thekogans/util/Condition.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Mutex.h"

using namespace thekogans::util;

TEST (thekogans, ConditionTimeout) {
    Mutex mutex;
    Condition condition;
    LockGuard<Mutex> guard (mutex);

    CHECK (!condition.Wait (mutex, TimeSpec::FromMilliseconds (2)));
}

TEST (thekogans, ConditionSignalStress) {
    const std::size_t threadCount = 8;
    const std::size_t itemCount = 20000;
    Mutex mutex;
    Condition condition;
    std::size_t pending = 0;
    std::size_t consumed = 0;
    bool stop = false;
    std::vector<std::thread> workers;
    workers.reserve (threadCount);

    for (std::size_t threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        workers.emplace_back ([&] {
            LockGuard<Mutex> guard (mutex);
            while (!stop || pending != 0) {
                while (pending == 0 && !stop) {
                    condition.Wait (mutex);
                }
                if (pending != 0) {
                    --pending;
                    ++consumed;
                }
            }
        });
    }

    for (std::size_t item = 0; item < itemCount; ++item) {
        LockGuard<Mutex> guard (mutex);
        ++pending;
        condition.Signal ();
    }

    {
        LockGuard<Mutex> guard (mutex);
        stop = true;
        condition.SignalAll ();
    }

    for (std::thread &worker : workers) {
        worker.join ();
    }

    CHECK (pending == 0);
    CHECK (consumed == itemCount);
}

TESTMAIN