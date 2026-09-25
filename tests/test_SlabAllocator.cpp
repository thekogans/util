#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <cassert>
#include <mutex>
#include <condition_variable>
#include "thekogans/util/SlabAllocator.h"

using namespace thekogans::util;

// --- COMPILATION FOR C++17 ---
// Linux/GCC:  g++ -O3 -std=c++17 -pthread stress_test.cpp -o stress_test
// macOS/Clang: clang++ -O3 -std=c++17 -pthread stress_test.cpp -o stress_test

// Forward declare your SlabAllocator here or include its header
// #include "SlabAllocator.hpp"

struct MockObject {
    uint64_t id;
    uint64_t payload; // 32 bytes total
};

// Configuration Parameters for the Stress Test
constexpr int NUM_PRODUCERS  = 8;
constexpr int NUM_CONSUMERS  = 8;
constexpr int NUM_MIXED_WORKERS = 16;
constexpr int OPS_PER_THREAD = 50000;

using TestAllocator = SlabAllocator<MockObject>;

int main () {
    TestAllocator &allocator = *TestAllocator::Instance ();
    std::cout << "[INFO] Spinning up " << (NUM_PRODUCERS + NUM_CONSUMERS + NUM_MIXED_WORKERS) <<
        " highly-contended worker threads...\n";

    // C++17 Latch replacement mechanics using core synchronization primitives
    std::mutex latch_mtx;
    std::condition_variable latch_cv;
    std::atomic<size_t> ready_threads{0};
    std::atomic<bool> start_gate{false};

    size_t total_threads = NUM_PRODUCERS + NUM_CONSUMERS + NUM_MIXED_WORKERS;

    auto sync_arrive_and_wait = [&] () {
        std::unique_lock<std::mutex> lock (latch_mtx);
        if (++ready_threads == total_threads) {
            start_gate.store (true, std::memory_order_release);
            latch_cv.notify_all();
        }
        else {
            latch_cv.wait (lock, [&] () {
                return start_gate.load(std::memory_order_acquire);
            });
        }
    };

    // Thread-safe pointer buffer pipelines to pass objects from allocations to frees
    // Add a mutex bank to protect the test lanes
    std::vector<std::mutex> pipeline_mutexes (NUM_PRODUCERS);
    std::vector<std::vector<void *>> transfer_pipelines (NUM_PRODUCERS);
    for (auto &queue : transfer_pipelines) {
        queue.reserve (OPS_PER_THREAD);
    }

    std::vector<std::thread> workers;
    std::atomic<bool> producers_finished{false};
    auto start_time = std::chrono::high_resolution_clock::now ();

    // 1. Launch Pure Allocation Producer Threads
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        workers.emplace_back ([&, i] () {
            sync_arrive_and_wait (); // Wait for all threads to align
            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                void *ptr = allocator.Alloc ();
                if (ptr) {
                    new (ptr) MockObject{static_cast<uint64_t> (j), 0};
                    std::lock_guard<std::mutex> lock (pipeline_mutexes[i]);
                    transfer_pipelines[i].push_back (ptr);
                }
            }
        });
    }

    // 2. Launch Pure Deallocation Consumer Threads
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        workers.emplace_back ([&, i] () {
            sync_arrive_and_wait ();
            std::lock_guard<std::mutex> lock (pipeline_mutexes[i]);
            int source_producer = i % NUM_PRODUCERS;
            while (!producers_finished.load (std::memory_order_acquire) || !transfer_pipelines[source_producer].empty ()) {
                if (!transfer_pipelines[source_producer].empty ()) {
                    void *ptr = nullptr;
                    if (!transfer_pipelines[source_producer].empty ()) {
                        ptr = transfer_pipelines[source_producer].back ();
                        transfer_pipelines[source_producer].pop_back ();
                    }
                    if (ptr) {
                        allocator.Free (ptr);
                    }
                }
                else {
                    std::this_thread::yield ();
                }
            }
        });
    }

    // 3. Launch Mixed Chaos Threads (Simultaneous Alloc + Free + Random Shuffling)
    for (int i = 0; i < NUM_MIXED_WORKERS; ++i) {
        workers.emplace_back ([&] () {
            std::mt19937_64 rng (std::random_device{} ());
            std::vector<void *> local_store;
            local_store.reserve (1000);
            sync_arrive_and_wait ();
            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                if (local_store.empty () || (rng () % 100 < 60 && local_store.size () < 900)) {
                    void *ptr = allocator.Alloc ();
                    if (ptr) {
                        new (ptr) MockObject{42, 0};
                        local_store.push_back (ptr);
                    }
                }
                else {
                    size_t target_idx = rng () % local_store.size ();
                    void *ptr = local_store[target_idx];
                    local_store[target_idx] = local_store.back ();
                    local_store.pop_back ();
                    allocator.Free (ptr);
                }
            }
            for (void *ptr : local_store) {
                allocator.Free (ptr);
            }
        });
    }

    // Wait until pure producers finish writing
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        workers[i].join ();
    }

    producers_finished.store (true, std::memory_order_release);

    // Wait for everything else to finish processing
    for (size_t i = NUM_PRODUCERS; i < workers.size (); ++i) {
        workers[i].join ();
    }

    auto end_time = std::chrono::high_resolution_clock::now ();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds> (end_time - start_time).count ();

    std::cout << "\n================ TEST RESULTS ================\n";
    std::cout << "[SUCCESS] Stress test completed execution successfully!\n";
    std::cout << "Processed Execution Time: " << total_duration << " ms\n";
    std::cout << "Total Allocation Loops:    " << (NUM_PRODUCERS + NUM_MIXED_WORKERS) * OPS_PER_THREAD << "\n";
    std::cout << "==============================================\n";

    return 0;
}
