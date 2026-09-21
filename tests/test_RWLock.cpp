// Copyright 2011 Boris Kogan (boris@thekogans.net)
//
// This file is part of libthekogans_util.
//
// libthekogans_util is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libthekogans_util is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libthekogans_util. If not, see <http://www.gnu.org/licenses/>.

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cassert>
#include <cstdlib>
#include <algorithm>
#include <thekogans/util/Thread.h>
#include <CppUnitXLite/CppUnitXLite.cpp>
#include <thekogans/util/RWLock.h>

using namespace thekogans::util;

// Fixture protecting real payloads using your API
struct ValidationFixture {
    RWLock rw_lock;

    // The shared state protected by the lock
    int64_t balance = 1000;
    int64_t verification_checksum = 1000;
};

void RunContentionHarness(int reader_threads, int writer_threads, std::chrono::seconds run_duration) {
    ValidationFixture fixture;
    std::atomic<bool> stop_flag{false};

    // Metrics trackers
    std::atomic<uint64_t> read_ops{0};
    std::atomic<uint64_t> write_ops{0};
    std::atomic<uint64_t> try_read_success{0};
    std::atomic<uint64_t> try_read_fail{0};
    std::atomic<uint64_t> try_write_success{0};
    std::atomic<uint64_t> try_write_fail{0};

    std::vector<std::thread> workers;

    // Spawn Writer group
    for (int i = 0; i < writer_threads; ++i) {
        workers.emplace_back([&]() {
            // Give each thread its own backoff context for TryAcquire retries
            Thread::Backoff backoff;

            while (!stop_flag.load(std::memory_order_relaxed)) {
                bool has_lock = false;

                // Randomly decide between standard Acquire (false) and TryAcquire (false)
                if (std::rand() % 2 == 0) {
                    fixture.rw_lock.Acquire(false);
                    has_lock = true;
                } else {
                    if (fixture.rw_lock.TryAcquire(false)) {
                        has_lock = true;
                        try_write_success.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        try_write_fail.fetch_add(1, std::memory_order_relaxed);
                        backoff.Pause(); // Back off exponentially if TryAcquire failed
                        continue;
                    }
                }

                if (has_lock) {
                    backoff.Reset();

                    // --- CRITICAL SECTION START ---
                    int64_t delta = (std::rand() % 20) - 10;
                    fixture.balance += delta;

                    std::this_thread::yield();

                    fixture.verification_checksum += delta;
                    // --- CRITICAL SECTION END ---

                    fixture.rw_lock.Release(false);
                    write_ops.fetch_add(1, std::memory_order_relaxed);
                }
                std::this_thread::yield();
            }
        });
    }

    // Spawn Reader group
    for (int i = 0; i < reader_threads; ++i) {
        workers.emplace_back([&]() {
            Thread::Backoff backoff;

            while (!stop_flag.load(std::memory_order_relaxed)) {
                bool has_lock = false;

                // Randomly alternate between Acquire (true) and TryAcquire (true)
                if (std::rand() % 2 == 0) {
                    fixture.rw_lock.Acquire(true);
                    has_lock = true;
                } else {
                    if (fixture.rw_lock.TryAcquire(true)) {
                        has_lock = true;
                        try_read_success.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        try_read_fail.fetch_add(1, std::memory_order_relaxed);
                        backoff.Pause();
                        continue;
                    }
                }

                if (has_lock) {
                    backoff.Reset();

                    // --- CRITICAL SECTION START ---
                    int64_t current_balance = fixture.balance;
                    int64_t current_check = fixture.verification_checksum;

                    if (current_balance != current_check) {
                        std::cerr << "\n[CRITICAL FAILURE] Race Condition Detected!"
                                  << "\nReader observed inconsistent state during active lock."
                                  << "\nBalance: " << current_balance
                                  << " | Checksum: " << current_check << std::endl;
                        std::abort();
                    }
                    // --- CRITICAL SECTION END ---

                    fixture.rw_lock.Release(true);
                    read_ops.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    std::cout << "Stress test executing with " << reader_threads
              << " readers and " << writer_threads << " writers (incorporating TryAcquire)..." << std::endl;

    std::this_thread::sleep_for(run_duration);
    stop_flag.store(true);

    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }

    std::cout << "Test completed successfully."
              << "\nTotal Reads (Combined): " << read_ops.load()
              << "\n  -> Via TryAcquire Success: " << try_read_success.load()
              << "\n  -> Via TryAcquire Failures: " << try_read_fail.load()
              << "\nTotal Writes (Combined): " << write_ops.load()
              << "\n  -> Via TryAcquire Success: " << try_write_success.load()
              << "\n  -> Via TryAcquire Failures: " << try_write_fail.load() << std::endl;
}

TEST (thekogans, RWLock) {
    // Seed the randomizer for path variance
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // Fetch the total available hardware threads (cores + hyperthreads)
    unsigned int hw_threads = std::thread::hardware_concurrency();

    // Fallback safety if hardware_concurrency() returns 0
    if (hw_threads == 0) {
        hw_threads = 4;
    }

    std::cout << "=========================================================" << std::endl;
    std::cout << " Detected Hardware Threads: " << hw_threads << std::endl;
    std::cout << "=========================================================" << std::endl;

    // Profile 1: Reader Heavy (Stresses Reader Fallback / TBB fetch_add path)
    // Scales readers up to the hardware limit, keeps writers low.
    unsigned int p1_readers = std::max(hw_threads, 4u);
    unsigned int p1_writers = 2;
    std::cout << "\n[Profile 1] Starting Reader Heavy Profile..." << std::endl;
    RunContentionHarness(p1_readers, p1_writers, std::chrono::seconds(5));

    // Profile 2: Writer Heavy (Stresses WRITER_PENDING & Mutex handover)
    // Scales writers up to the hardware limit, keeps readers low.
    unsigned int p2_readers = 2;
    unsigned int p2_writers = std::max(hw_threads, 4u);
    std::cout << "\n[Profile 2] Starting Writer Heavy Profile..." << std::endl;
    RunContentionHarness(p2_readers, p2_writers, std::chrono::seconds(5));

    // Profile 3: Symmetric Overcommit (Forces heavy OS context switching)
    // Total threads = 2x your total hardware cores. This forces maximum thread-preemption.
    unsigned int p3_readers = hw_threads;
    unsigned int p3_writers = hw_threads;
    std::cout << "\n[Profile 3] Starting Symmetric Overcommit Profile (2x Overcommit)..." << std::endl;
    RunContentionHarness(p3_readers, p3_writers, std::chrono::seconds(5));

    std::cout << "\n=========================================================" << std::endl;
    std::cout << " All scaled stress test profiles completed successfully! " << std::endl;
    std::cout << "=========================================================" << std::endl;
}

TESTMAIN
