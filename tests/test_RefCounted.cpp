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
#include <mutex>
#include <algorithm>
#include <CppUnitXLite/CppUnitXLite.cpp>
#include "thekogans/util/RefCounted.h"

using namespace thekogans::util;

// 1. Derive directly from your genuine production RefCounted mixin block
struct StressTestObject : public RefCounted {
    THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (StressTestObject)

    // Canary variables used to ensure memory barriers don't allow stale data visibility
    uint64_t alignment_canary = 0x55AA55AA66BB66BBULL;
    std::string data_payload = "OpenSSL System Verification Target Payload String";

    std::atomic<bool> harakiri_triggered{false};

    // 2. Re-implement Harakiri to hook into our test verification framework
    virtual void Harakiri() override {
        bool already_dead = harakiri_triggered.exchange(true, std::memory_order_relaxed);
        if (already_dead) {
            std::cerr << "[CRITICAL FAILURE] Double Harakiri invocation detected!" << std::endl;
            std::abort();
        }

        // Validate that memory ordering barriers completely protected payload fields from tearing
        if (alignment_canary != 0x55AA55AA66BB66BBULL || data_payload.empty()) {
            std::cerr << "[CRITICAL FAILURE] Memory visibility cache line corruption detected during Harakiri!" << std::endl;
            std::abort();
        }

        // Perform the true destructive operation since we are overriding base behavior
        delete this;
    }
};

void RunVerificationSession(std::chrono::seconds runtime) {
    std::atomic<bool> keep_running{true};
    std::atomic<uint64_t> total_cycles{0};
    std::atomic<uint64_t> total_upgrades{0};

    // Communication vectors to pass WeakPtr handles across threads.
    std::vector<StressTestObject::WeakPtr> global_weak_channels;
    std::mutex channel_lock;

    // A. Lifecycle Thread: Constantly churns objects inside SharedPtr scope bounds.
    std::thread generator([&]() {
        while (keep_running.load(std::memory_order_relaxed)) {
            StressTestObject::SharedPtr master_sp(new StressTestObject());
            StressTestObject::WeakPtr worker_wp(master_sp);
            {
                std::lock_guard<std::mutex> lock(channel_lock);
                global_weak_channels.push_back(worker_wp);
            }
            // Provide a tiny race window for concurrent workers to scramble to upgrade.
            std::this_thread::sleep_for(std::chrono::microseconds(15 + (std::rand() % 25)));
            {
                std::lock_guard<std::mutex> lock(channel_lock);
                if (!global_weak_channels.empty()) {
                    global_weak_channels.erase(global_weak_channels.begin());
                }
            }
            // master_sp drops out of scope right here, natively invoking
            // ReleaseSharedRef loop and testing the acq_rel boundary on Harakiri execution!
            total_cycles.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // B. Worker Racing Pool: Constantly extracts WeakPtr handles and executes GetSharedPtr().
    std::vector<std::thread> workers;
    unsigned int core_overcommit = std::max(std::thread::hardware_concurrency() * 2, 4u);

    for (unsigned int i = 0; i < core_overcommit; ++i) {
        workers.emplace_back([&]() {
            while (keep_running.load(std::memory_order_relaxed)) {
                StressTestObject::WeakPtr active_weak;
                {
                    std::lock_guard<std::mutex> lock(channel_lock);
                    if (!global_weak_channels.empty()) {
                        active_weak = global_weak_channels.at(std::rand() % global_weak_channels.size());
                    }
                }
                if (active_weak != nullptr) {
                    // STRESS POINT: Execute template upgrading method loop.
                    // This exercises lock-free LockObject atomic compare_exchange logic.
                    StressTestObject::SharedPtr active_shared = active_weak.GetSharedPtr();
                    if (active_shared != nullptr) {
                        // If LockObject acq_rel memory barrier works, this target is guaranteed safe.
                        if (active_shared->alignment_canary != 0x55AA55AA66BB66BBULL) {
                            std::cerr << "[CRITICAL FAILURE] Promoted pointer exposed un-synchronized cache data!" << std::endl;
                            std::abort();
                        }
                        total_upgrades.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                std::this_thread::yield();
            }
        });
    }

    std::cout << "Executing verification harness via " << core_overcommit << " threads..." << std::endl;
    std::this_thread::sleep_for(runtime);

    // Tear down execution gracefully
    keep_running.store(false);
    generator.join();
    for (auto& worker : workers) {
        if (worker.joinable()) worker.join();
    }

    std::cout << "\n========================================================"
              << "\n Verification Harness Session Completed Successfully"
              << "\n -> Lifecycles Processed:  " << total_cycles.load()
              << "\n -> Weak->Shared Upgrades: " << total_upgrades.load()
              << "\n========================================================" << std::endl;
}

TEST (thekogans, test_RefCounted) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    // Execute a standard 5-second platform verification sweep
    RunVerificationSession(std::chrono::seconds(5));
}

TESTMAIN
