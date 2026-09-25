#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <cassert>
#include <cstddef>
#include <new>
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

template <
    typename T,
    std::size_t Capacity = 1024>
class LockFreeSPSCQueue {
private:
    // Force the ring buffer template matrix to enforce power-of-2 geometry.
    // This allows us to use bitwise AND indexing masks instead of expensive modulo (%) arithmetic!
    static_assert ((Capacity & (Capacity - 1)) == 0, "Capacity must be a strict power of 2.");
    static constexpr std::size_t IndexMask = Capacity - 1;

    // We hardcode the system's detected line configuration to stop false sharing completely.
    static constexpr std::size_t CacheLineSize = 256; // Perfect M5 baseline matching!

    // The raw pointer slot buffer storage
    T *buffer[Capacity];

    // THE HARDWARE TRAP DEFENSE:
    // The producer writes to the tail, and the consumer writes to the head.
    // We isolate them onto completely separate physical 256-byte cache lines
    // so they never trigger cross-core interconnect invalidations while running side-by-side!
    alignas (CacheLineSize) std::atomic<std::size_t> tail{0};
    alignas (CacheLineSize) std::atomic<std::size_t> head{0};

public:
    LockFreeSPSCQueue () noexcept {
        for (std::size_t i = 0; i < Capacity; ++i) {
            buffer[i] = nullptr;
        }
    }

    // Strict non-copyable semantics
    LockFreeSPSCQueue (const LockFreeSPSCQueue &) = delete;
    LockFreeSPSCQueue &operator = (const LockFreeSPSCQueue &) = delete;

    /// \brief Push a pointer into the queue (Producer Thread hot path)
    bool Push (T *ptr) noexcept {
        const std::size_t current_tail = tail.load (std::memory_order_relaxed);
        const std::size_t current_head = head.load (std::memory_order_acquire);
        // Check if the ring buffer is completely full
        if ((current_tail - current_head) >= Capacity) {
            return false;
        }
        // Map the sliding index to the buffer footprint via zero-overhead bitwise mask
        buffer[current_tail & IndexMask] = ptr;
        // Release barrier ensures the slot write is visible *before* the tail index increments
        tail.store (current_tail + 1, std::memory_order_release);
        return true;
    }

    /// \brief Pop a pointer out of the queue (Consumer Thread hot path)
    bool Pop (T *&ptr) noexcept {
        const std::size_t current_head = head.load (std::memory_order_relaxed);
        const std::size_t current_tail = tail.load (std::memory_order_acquire);
        // Check if the queue is completely empty
        if (current_head == current_tail) {
            return false;
        }
        ptr = buffer[current_head & IndexMask];
        // Release barrier guarantees the item lookup is finished *before* the slot opens back up
        head.store (current_head + 1, std::memory_order_release);
        return true;
    }

    inline bool Empty () const noexcept {
        return head.load (std::memory_order_relaxed) == tail.load (std::memory_order_relaxed);
    }
};

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
            latch_cv.notify_all ();
        }
        else {
            latch_cv.wait (lock, [&] () {
                return start_gate.load (std::memory_order_acquire);
            });
        }
    };

    // Thread-safe pointer buffer pipelines to pass objects from allocations to frees
    // Add a mutex bank to protect the test lanes
    std::vector<LockFreeSPSCQueue<void>> pipelines (NUM_PRODUCERS);
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
                    if (!pipelines[i].Push (ptr)) {
                        allocator.Free (ptr);
                    }
                }
            }
        });
    }

    // 2. Launch Pure Deallocation Consumer Threads
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        workers.emplace_back ([&, i] () {
            sync_arrive_and_wait ();
            int source_producer = i % NUM_PRODUCERS;
            LockFreeSPSCQueue<void> &pipeline = pipelines[source_producer];
            void *ptr = nullptr;
            while (!producers_finished.load (std::memory_order_acquire) || !pipeline.Empty ()) {
                if (pipeline.Pop (ptr)) {
                    if (ptr) {
                        allocator.Free (ptr); // Freeing happens locally, 100% un-synchronized!
                    }
                }
                else {
                    // High-velocity back-off: since we are entirely lock-free, a tiny hint
                    // keeps the CPU from aggressively melting the core execution slots
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

    std::cout << "================ TEST RESULTS ================\n";
    std::cout << "[SUCCESS] Stress test completed execution successfully!\n";
    std::cout << "Processed Execution Time: " << total_duration << " ms\n";
    std::cout << "Total Allocation Loops:    " << (NUM_PRODUCERS + NUM_MIXED_WORKERS) * OPS_PER_THREAD << "\n";
    std::cout << "==============================================\n";

    return 0;
}
