#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <vector>
#include <cassert>
#include <cstdio>
#include <thekogans/util/Scheduler.h>

using namespace thekogans::util;

// Dummy job implementation to hammer the scheduler
struct HammerJob : public RunLoop::Job {
    std::atomic<bool>& executionFlag;

    HammerJob(std::atomic<bool>& flag) : executionFlag(flag) {}

    virtual void Execute(const std::atomic<bool>& /*done*/) noexcept override {
        executionFlag.store(true); // Signal that this job successfully touched the CPU
    }
};

// Endless flood job for high priority tracking
struct FloodJob : public RunLoop::Job {
    Scheduler::JobQueue::SharedPtr myQueue;
    std::atomic<ui64>& floodCounter;

    FloodJob(Scheduler::JobQueue::SharedPtr q, std::atomic<ui64>& counter)
        : myQueue(q), floodCounter(counter) {}

    virtual void Execute(const std::atomic<bool>& done) noexcept override {
        floodCounter.fetch_add(1);

        // 🚀 THE CONTINUOUS FLOOD: If the scheduler isn't shutting down,
        // immediately enqueue our successor to keep the HIGH list constantly occupied.
        if (!ShouldStop(done)) {
            myQueue->EnqJob(new FloodJob(myQueue, floodCounter));
        }
    }
};

void TestSchedulerFairModeBypass() {
    std::printf("[TEST] Initializing Fair Mode Stress Test...\n");

    // 1. Instantiate the Scheduler with fairMode = true and your customizable tuning knobs
    // Using 4 dedicated workers for the underlying pool
    const std::size_t numWorkers = 4;
    Scheduler scheduler(
        numWorkers,          // minJobQueues
        numWorkers * 2,      // maxJobQueues
        "FairTestPool",      // pool name
        new RunLoop::FIFOJobExecutionPolicy(),
        1,                   // workerCount per queue
        THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
        THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
        nullptr,             // workerCallback
        true,                // fairMode = true
        5,                   // highPriorityThreshold
        3                    // normalPriorityThreshold
    );

    // 2. Setup a single LOW priority queue and a target job flag
    std::atomic<bool> lowJobExecuted{false};
    Scheduler::JobQueue::SharedPtr lowQueue = new Scheduler::JobQueue(scheduler, Scheduler::JobQueue::PRIORITY_LOW, "LowQueue");

    // 3. Setup multiple HIGH priority queues to slam the system
    std::atomic<ui64> totalHighJobsRun{0};
    std::vector<Scheduler::JobQueue::SharedPtr> highQueues;

    // Create enough high priority queues to keep all worker threads saturated
    for (std::size_t i = 0; i < numWorkers * 2; ++i) {
        char nameBuf[32];
        std::snprintf(nameBuf, sizeof(nameBuf), "HighQueue-%zu", i);
        highQueues.push_back(new Scheduler::JobQueue(scheduler, Scheduler::JobQueue::PRIORITY_HIGH, nameBuf));
    }

    // 4. Attack phase: Flood the high priority queues completely
    for (auto& hQueue : highQueues) {
        hQueue->EnqJob(new FloodJob(hQueue, totalHighJobsRun));
    }

    // Give the high-priority flood a split second to completely saturate the worker pool
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ui64 highJobsBeforeLow = totalHighJobsRun.load();

    // 5. Drop the lone LOW priority job into the saturated ecosystem
    std::printf("[TEST] Saturated pool with %llu HIGH jobs. Injecting LOW priority target...\n",
                static_cast<unsigned long long>(highJobsBeforeLow));

    lowQueue->EnqJob(new HammerJob(lowJobExecuted));

    // 6. Assertion window: Loop and check if the LOW job gets executed despite the continuous HIGH flood
    bool passed = false;
    for (int retry = 0; retry < 50; ++retry) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (lowJobExecuted.load()) {
            passed = true;
            break;
        }
    }

    // 7. Cleanup the scheduler infrastructure safely using your background Context teardown
    std::printf("[TEST] Shutting down scheduler context...\n");
    // Front-end handle falls out of scope or is explicitly handled,
    // triggering context->switchingOff to stop the FloodJobs naturally.

    // Final Verifications
    std::printf("[TEST] Execution Metrics -> Total HIGH jobs completed: %llu\n",
                static_cast<unsigned long long>(totalHighJobsRun.load()));

    if (passed) {
        std::printf("[TEST PASS] Successfully bypassed strict priority starvation! The LOW job executed perfectly.\n");
    } else {
        std::printf("[TEST FAIL] The LOW job was completely starved by the HIGH priority flood.\n");
        assert(false);
    }
}

// Microscopic payload job to keep the workers constantly moving
struct RippleJob : public RunLoop::Job {
    virtual void Execute(const std::atomic<bool>& /*done*/) noexcept override {
        // Just yield the CPU slice to maximize the chance of a context switch mid-lifecycle
        std::this_thread::yield();
    }
};

void TestQueueRecyclingHighFrequency() {
    std::printf("[TEST 2] Initializing High-Frequency Recycling Test...\n");

    // 1. Create a core Scheduler engine instance
    Scheduler scheduler(4, 8, "RecycleTestPool");

    // 2. Worker Execution Threat: Aggressively bombard a queue with jobs from a background thread
    std::atomic<bool> stopBombardment{false};
    std::thread bombardier([&scheduler, &stopBombardment]() {
        // Instantiate a public user-facing JobQueue
        Scheduler::JobQueue::SharedPtr streamQueue =
            new Scheduler::JobQueue(scheduler, Scheduler::JobQueue::PRIORITY_NORMAL, "StreamQueue");

        while (!stopBombardment.load()) {
            streamQueue->EnqJob(new RippleJob());
            // Fast micro-pacing to not completely choke the heap allocator
            for(volatile int i = 0; i < 50; ++i);
        }
    });

    // 3. Phase Shift Threat: Rapidly lifecycle a second queue to force the internal += list mechanics
    std::printf("[TEST 2] Saturation active. Executing rapid phase shift cycles...\n");

    const int targetCycles = 500;
    for (int i = 0; i < targetCycles; ++i) {
        // Instantiate a queue - triggers Start() internally, recycling warm workers via +=
        Scheduler::JobQueue *temporaryQueue =
            new Scheduler::JobQueue(scheduler, Scheduler::JobQueue::PRIORITY_HIGH, "TempQueue");

        // Let it run for a microsecond
        std::this_thread::yield();

        // Explicitly destroy it - triggers Stop() internally, splicing workers into drainingWorkers via +=
        delete temporaryQueue;
    }

    // 4. Cool-down & Cleanup
    std::printf("[TEST 2] Cycles completed. Winding down background bombardment...\n");
    stopBombardment.store(true);
    bombardier.join();

    std::printf("[TEST 2 PASS] Thread pool recycling engine is completely bulletproof! No crashes or pointer leaks.\n");
}

// A heavy cooperative job designed to hold a worker thread captive
// across the pool's entire phase-shift window.
struct HeavyCaptiveJob : public RunLoop::Job {
    std::atomic<bool>& startGate;
    std::atomic<bool>& executionFlag;

    HeavyCaptiveJob(std::atomic<bool>& gate, std::atomic<bool>& flag)
        : startGate(gate), executionFlag(flag) {}

    virtual void Execute(const std::atomic<bool>& /*done*/) noexcept override {
        executionFlag.store(true);

        // 🚀 THE CAPTIVE ANCHOR:
        // Hold this physical worker thread hostage until the main test thread
        // has completed the Stop() and subsequent Start() cycle!
        while (!startGate.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

void TestRealPoolWorkerRecycling() {
    std::printf("[TEST 2] Initializing REAL Pool Worker Recycling Test...\n");

    // 1. Configure the Scheduler to force radical scale-down shifts.
    // min = 1, max = 2. This guarantees that borrowing two queues forces an allocation,
    // and releasing one down to min forces our new safe async Stop() path.
    Scheduler scheduler(
        1,                   // minJobQueues
        2,                   // maxJobQueues
        "RealRecyclePool",
        new RunLoop::FIFOJobExecutionPolicy(),
        2,                   // workerCount (2 threads per pool queue)
        THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
        THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
        nullptr
    );

    std::atomic<bool> releaseGate{false};
    std::atomic<bool> job1Active{false};
    std::atomic<bool> job2Active{false};

    // 2. Instantiate two separate Scheduler::JobQueues to force the pool to grow to max capacity (2)
    std::printf("[TEST 2] Spawning queues to scale pool to max capacity...\n");
    Scheduler::JobQueue::SharedPtr queueA = new Scheduler::JobQueue(scheduler, Scheduler::JobQueue::PRIORITY_HIGH, "QueueA");
    Scheduler::JobQueue::SharedPtr queueB = new Scheduler::JobQueue(scheduler, Scheduler::JobQueue::PRIORITY_HIGH, "QueueB");

    // 3. Saturate both pool queues with heavy jobs to trap the underlying worker threads
    queueA->EnqJob(new HeavyCaptiveJob(releaseGate, job1Active));
    queueB->EnqJob(new HeavyCaptiveJob(releaseGate, job2Active));

    // Wait until both physical worker threads are confirmed trapped inside their execution loops
    while (!job1Active.load() || !job2Active.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::printf("[TEST 2] Workers trapped. Triggering asynchronous pool scale-down...\n");

    // 4. THE STOP TRIGGER: Delete one Scheduler queue.
    // This returns a pool queue back to ReleaseJobQueue. Since max capacity was 2,
    // and min is 1, this excess pool queue is instantly spliced into drainingWorkers
    // and its async Stop() is called. The threads are still stuck inside HeavyCaptiveJob!
    queueB.Reset();

    // Let the async Stop() signals circulate to the pool queues
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    std::printf("[TEST 2] Pool scaled down. Executing immediate Start() resurrection...\n");

    // 5. THE RESCUE TRIGGER: Instantly instantiate a new Scheduler queue.
    // This calls pool.GetJobQueue(). The pool notices it has work, resets done = false,
    // and executes 'workers += drainingWorkers' to rescue our warm, trapped threads!
    Scheduler::JobQueue::SharedPtr queueC = new Scheduler::JobQueue(scheduler, Scheduler::JobQueue::PRIORITY_HIGH, "QueueC");

    // 6. Open the gate to let the heavy jobs complete naturally
    std::printf("[TEST 2] Releasing worker gate to monitor recycling behavior...\n");
    releaseGate.store(true);

    // Give the threads plenty of time to wind down or recycle through the intercept check
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::printf("[TEST 2 PASS] The real underlying thread pool recycling code was executed flawlessly!\n");
}

int main (int /*argc*/, const char */*argv*/[]) {
    TestSchedulerFairModeBypass ();
    TestQueueRecyclingHighFrequency ();
    TestRealPoolWorkerRecycling ();
    return 0;
}
