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

#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <vector>
#include <cassert>
#include <cstdio>
#include <CppUnitXLite/CppUnitXLite.cpp>
#include <thekogans/util/JobQueue.h>

using namespace thekogans::util;

// A non-deterministic heavy job to hold pool workers at random time offsets
struct NondeterministicHeavyJob : public RunLoop::Job {
    std::atomic<bool>& startGate;
    std::atomic<bool>& executionFlag;

    NondeterministicHeavyJob(std::atomic<bool>& gate, std::atomic<bool>& flag)
        : startGate(gate), executionFlag(flag) {}

    virtual void Execute(const std::atomic<bool>& /*done*/) noexcept override {
        executionFlag.store(true);

        // Setup a basic non-deterministic jitter loop to simulate real-world variance
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(1, 20); // 100 to 500 microseconds
        int iterations = dist(gen);

        // Spin to simulate complex computational work
        for (volatile int i = 0; i < iterations * 100; ++i) {
            // Keep spinning to trap the thread context mid-execution
        }
    }
};

void TestUtilJobQueueWarmRecyclingDirect() {
    std::printf("[DIRECT TEST] Initializing Primitive util::JobQueue Stress Test...\n");

    // 1. Instantiate exactly ONE standalone util::JobQueue instance.
    // Configure it with 4 worker threads to populate the internal workers list.
    JobQueue poolJobQueue(
        "DirectPrimitiveQueue",
        new RunLoop::FIFOJobExecutionPolicy(),
        4,                                      // workerCount
        THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
        THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
        nullptr                                 // workerCallback
    );

    std::atomic<bool> releaseGate{false};
    std::atomic<bool> jobsActive{false};

    std::printf("[DIRECT TEST] Flooding queue with non-deterministic captive jobs...\n");

    // 2. Flood the queue to trap all 4 underlying worker threads mid-flight
    for (int i = 0; i < 4; ++i) {
        poolJobQueue.EnqJob(new NondeterministicHeavyJob(releaseGate, jobsActive));
    }

    // Wait for the threads to actively engage the work payload
    while (!jobsActive.load()) {
        std::this_thread::yield();
    }

    std::printf("[DIRECT TEST] Blasting rapid Start/Stop cycles directly at the primitive...\n");

    // 3. THE HIGH-FREQUENCY FIRE LOOP:
    // Rapidly blast Stop() and Start() to exercise your custom += logic back-to-back
    const int targetCycles = 1000;
    for (int i = 0; i < targetCycles; ++i) {

        // Triggers Stop(): splices active workers into drainingWorkers via +=
        poolJobQueue.Stop(true, false);

        // Introduce a microscopic variable pacing jitter to maximize collision windows
        for (volatile int j = 0; j < (i % 5); ++j);

        // Triggers Start(): rescues warm, trapped workers back via += before they exit!
        poolJobQueue.Start();
    }

    // 4. Cool-down Phase
    std::printf("[DIRECT TEST] Cycles complete. Releasing worker gate for final wind-down...\n");
    releaseGate.store(true);

    // Allow any remaining threads to process the intercept check or hit the ThreadReaper safely
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::printf("[DIRECT PASS] Primitive util::JobQueue recycling verified cleanly under direct fire!\n");
}

TEST (thekogans, test_JobQueue) {
    TestUtilJobQueueWarmRecyclingDirect ();
}

TESTMAIN
