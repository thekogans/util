#include <iostream>
#include <vector>
#include <thread>
#include <cassert>
#include <chrono>
#include <thekogans/util/ScopedSlabAllocator.h>

using namespace thekogans::util;

struct EphemeralModuleContext {
    struct alignas(256) MockWorkItem {
        ui64 taskId;
        char cargo[128];
    };
    using ModuleAllocator = ScopedSlabAllocator<MockWorkItem>;
    ModuleAllocator allocator;
    std::vector<std::thread> workers;

    EphemeralModuleContext (
            std::size_t threadCount,
            std::size_t loopsPerThread) {
        workers.reserve (threadCount);
        for (std::size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back (
                [this, loopsPerThread, i] () {
                    std::vector<void *> localSlots;
                    localSlots.reserve (loopsPerThread);
                    // Phase 1: Hammer allocations across cooperating threads
                    for (std::size_t loop = 0; loop < loopsPerThread; ++loop) {
                        void *ptr = allocator.Alloc ();
                        if (ptr != nullptr) {
                            // Write to the memory to guarantee physical page faults occur
                            MockWorkItem *item = new (ptr) MockWorkItem ();
                            item->taskId = (i << 32) | loop;
                            localSlots.push_back (ptr);
                        }
                    }
                    // Phase 2: Mixed latency release
                    for (void *ptr : localSlots) {
                        allocator.Free (ptr);
                    }
                });
        }
    }

    // The Destructor: Join worker threads and instantly flush all page maps
    ~EphemeralModuleContext () {
        for (auto &worker : workers) {
            if (worker.joinable ()) {
                worker.join ();
            }
        }
        // Allocator destructor triggers implicitly here, vaporizing virtual memory
    }
};

int main () {
    std::cout << "[INFO] Initializing ScopedSlabAllocator Lifecycle Stress Test..." << std::endl;
    constexpr std::size_t ITERATIONS = 100; // Volatile lifecycle churn loops
    constexpr std::size_t THREADS = 32;     // Target concurrency limit
    constexpr std::size_t LOOPS = 1000;     // Allocations per lifecycle loop
    try {
        auto startTime = std::chrono::high_resolution_clock::now();
        for (std::size_t cycle = 0; cycle < ITERATIONS; ++cycle) {
            if (cycle % 10 == 0) {
                std::cout << "[RUNNING] Completed " << cycle << "/" << ITERATIONS << " module flushes..." << std::endl;
            }
            // Allocate, execute, and destroy the entire module architecture context
            {
                EphemeralModuleContext module(THREADS, LOOPS);
                // Context goes out of scope here: threads are joined, pages are unmapped!
            }
        }
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        std::cout << "\n================ LIFECYCLE TEST RESULTS ================" << std::endl;
        std::cout << "[SUCCESS] Generated and destroyed " << ITERATIONS << " module lifecycles successfully!" << std::endl;
        std::cout << "Total Allocation/Free cycles managed: " << (ITERATIONS * THREADS * LOOPS) << std::endl;
        std::cout << "Processed Execution Time:             " << duration << " ms" << std::endl;
        std::cout << "========================================================\n" << std::endl;

    }
    catch (const std::exception& e) {
        std::cout << "[FATAL CORRUPTION ERROR]: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
