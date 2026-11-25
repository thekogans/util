#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <random>

void RunMultiThreadedTortureTest (const std::string &dbPath) {
    static constexpr std::size_t NUM_THREADS = 8;
    static constexpr std::size_t OPS_PER_THREAD = 2500;
    static constexpr std::size_t NUM_CYCLES = 20;

    // 1. Single-threaded database creation
    {
        TransactedFile::SharedPtr file = TransactedFile::Create (
            HostEndian,
            dbPath,
            false,
            new TransactedFileTypeDictionary,
            new TransactedFileBTreeAllocator,
            new TransactedFileBTreeRegistry,
            true);
    }

    // Track ground-truth oracle state on the test side
    std::map<std::string, std::string> expectedState;

    for (std::size_t cycle = 0; cycle < NUM_CYCLES; ++cycle) {
        TransactedFile::SharedPtr file = TransactedFile::Open (dbPath);
        TransactedFile::Transaction transaction (file);

        std::vector<std::thread> workers;
        std::atomic<bool> startFlag{false};
        std::mutex oracleMutex;

        for (std::size_t t = 0; t < NUM_THREADS; ++t) {
            workers.emplace_back ([&, t] () {
                // Spin-wait so all threads slam the registry simultaneously
                while (!startFlag.load (std::memory_order_acquire)) {}

                std::mt19937_64 rng (cycle * 1000 + t);
                for (std::size_t op = 0; op < OPS_PER_THREAD; ++op) {
                    // Mix hot colliding keys (0..99) and wide keys (0..9999)
                    std::size_t keyId = (op % 3 == 0) ? (rng () % 100) : (rng () % 10000);
                    std::string key = "key_" + std::to_string (keyId);
                    int action = rng () % 10;

                    if (action < 5) {
                        // Insert / Overwrite
                        std::string payload (32 + (rng () % 512), static_cast<char> ('A' + (t % 26)));
                        auto obj = /* create your test TransactedFile::Object with payload */;
                        if (file->GetRegistry ()->InsertValue (key, obj)) {
                            std::lock_guard<std::mutex> lock (oracleMutex);
                            expectedState[key] = payload;
                        }
                    }
                    else if (action < 8) {
                        // Concurrent Read
                        auto obj = file->GetRegistry ()->GetValue (key);
                        // Validate object integrity if non-null
                    }
                    else {
                        // Remove
                        file->GetRegistry ()->RemoveValue (key);
                        std::lock_guard<std::mutex> lock (oracleMutex);
                        expectedState.erase (key);
                    }
                }
            });
        }

        // Unleash the angry swarm
        startFlag.store (true, std::memory_order_release);
        for (auto &worker : workers) {
            worker.join ();
        }

        // Single-threaded transaction resolution (every 4th cycle tests Abort!)
        if (cycle % 4 == 3) {
            transaction.Abort (); // Or let transaction unwind without Commit()
        }
        else {
            transaction.Commit (cycle % 2 == 0); // Alternate clearCache = true / false
        }
    }

    // Cold reopen & full verification against expectedState
    TransactedFile::SharedPtr verifyFile = TransactedFile::Open (HostEndian, dbPath);
    for (const auto &[key, expectedPayload] : expectedState) {
        auto obj = verifyFile->GetRegistry ()->GetValue (key);
        THEKOGANS_UTIL_ASSERT (obj != nullptr, "Missing committed key!");
        // Assert payload matches expectedPayload
    }
}


void RunTransactedFileTortureTest (const std::string &dbPath) {
    static constexpr std::size_t NUM_THREADS = 8;
    static constexpr std::size_t OPS_PER_THREAD = 2000;
    static constexpr std::size_t NUM_CYCLES = 16;

    // 1. Single-threaded database initialization
    {
        TransactedFile::SharedPtr file = TransactedFile::Create (
            HostEndian,
            dbPath,
            false,
            new TransactedFileTypeDictionary,
            new TransactedFileBTreeAllocator,
            new TransactedFileBTreeRegistry,
            true);
    }

    // Per-thread ground-truth oracle (disjoint key prefixes eliminate oracle races)
    std::vector<std::map<std::string, std::string>> threadOracles (NUM_THREADS);

    for (std::size_t cycle = 0; cycle < NUM_CYCLES; ++cycle) {
        TransactedFile::SharedPtr file = TransactedFile::Open (dbPath);
        TransactedFileBTreeRegistry::SharedPtr registry = file->GetRegistry ();

        // Snapshot oracle before transaction in case this cycle aborts
        auto preCycleOracles = threadOracles;

        {
            TransactedFile::Transaction transaction (file);

            // Ensure two B-trees exist for cross-tree Clonable isolation testing
            TransactedFileBTree::SharedPtr treeA = registry->GetBTree ("treeA");
            if (treeA == nullptr) {
                treeA = TransactedFile::Object::Create<TransactedFileBTree> (
                    file, 0, StringKey::TYPE, StringValue::TYPE, false);
                registry->InsertBTree ("treeA", treeA);
            }
            TransactedFileBTree::SharedPtr treeB = registry->GetBTree ("treeB");
            if (treeB == nullptr) {
                treeB = TransactedFile::Object::Create<TransactedFileBTree> (
                    file, 0, StringKey::TYPE, StringValue::TYPE, false);
                registry->InsertBTree ("treeB", treeB);
            }

            std::atomic<bool> startFlag{false};
            std::vector<std::thread> workers;

            for (std::size_t t = 0; t < NUM_THREADS; ++t) {
                workers.emplace_back ([&, t] () {
                    while (!startFlag.load (std::memory_order_acquire)) {}

                    std::mt19937_64 rng (cycle * 10000 + t);
                    auto &localOracle = threadOracles[t];

                    // Reusable external key & clonable value to test copy-on-insert isolation
                    StringKey::SharedPtr reusableKey = new StringKey ();
                    ClonableStringValue::SharedPtr reusableVal = new ClonableStringValue ();

                    for (std::size_t op = 0; op < OPS_PER_THREAD; ++op) {
                        std::size_t keyNum = rng () % 500;
                        std::string keyStr = "t" + std::to_string (t) + "_k" + std::to_string (keyNum);
                        int action = rng () % 10;

                        if (action < 4) {
                            // Insert same reusable key & ClonableStringValue into BOTH trees,
                            // then immediately mutate caller's copies to verify isolation!
                            std::string payload = "val_" + std::to_string (cycle) + "_" + std::to_string (op);
                            reusableKey->value = keyStr;
                            reusableVal->value = payload;

                            TransactedFileBTree::Iterator itA;
                            TransactedFileBTree::Iterator itB;
                            treeA->Remove (reusableKey);
                            treeB->Remove (reusableKey);
                            bool insA = treeA->Insert (reusableKey, reusableVal, itA);
                            bool insB = treeB->Insert (reusableKey, reusableVal, itB);

                            // Poison caller's external handles immediately!
                            reusableKey->value = "POISONED_KEY";
                            reusableVal->value = "POISONED_VALUE";

                            THEKOGANS_UTIL_ASSERT (insA && insB, "Insert failed!");
                            localOracle[keyStr] = payload;
                        }
                        else if (action < 6) {
                            // Zero-copy move insert with non-clonable StringValue into treeA
                            std::string moveKeyStr = keyStr + "_mv";
                            std::string payload = "move_" + std::to_string (op);
                            StringKey::SharedPtr k = new StringKey (moveKeyStr);
                            StringValue::SharedPtr v = new StringValue (payload);
                            TransactedFileBTree::Iterator itA;
                            treeA->Remove (k);
                            if (treeA->Insert (k, v, itA)) {
                                // Note: v was moved into treeA (not cloned), so we do NOT mutate v here
                                localOracle[moveKeyStr] = payload;
                            }
                        }
                        else if (action < 8) {
                            // Cross-thread concurrent read across ANY thread's key space
                            std::size_t targetThread = rng () % NUM_THREADS;
                            std::size_t targetKey = rng () % 500;
                            StringKey::SharedPtr probe = new StringKey (
                                "t" + std::to_string (targetThread) + "_k" + std::to_string (targetKey));
                            TransactedFileBTree::Iterator it;
                            if (treeA->Find (probe, it)) {
                                StringValue::SharedPtr val = it.GetValue ();
                                THEKOGANS_UTIL_ASSERT (
                                    val != nullptr && val->value != "POISONED_VALUE",
                                    "Observed poisoned/corrupt value during concurrent read!");
                            }
                        }
                        else {
                            // Remove from both trees
                            reusableKey->value = keyStr;
                            treeA->Remove (reusableKey);
                            treeB->Remove (reusableKey);
                            reusableKey->value = "POISONED_KEY";
                            localOracle.erase (keyStr);
                        }
                    }
                });
            }

            // Release the swarm
            startFlag.store (true, std::memory_order_release);
            for (auto &w : workers) {
                w.join ();
            }

            // Every 4th cycle tests multi-threaded Abort rollback!
            if (cycle % 4 == 3) {
                transaction.Abort ();
                threadOracles = preCycleOracles;
            }
            else {
                transaction.Commit (cycle % 2 == 0);
            }
        }
    }

    // 2. Cold reopen and verify 100% of treeA and treeB against ground-truth oracles
    TransactedFile::SharedPtr verifyFile = TransactedFile::Open (HostEndian, dbPath);
    TransactedFileBTreeRegistry::SharedPtr verifyReg = verifyFile->GetRegistry ();
    TransactedFileBTree::SharedPtr verifyTreeA = verifyReg->GetBTree ("treeA");
    TransactedFileBTree::SharedPtr verifyTreeB = verifyReg->GetBTree ("treeB");

    for (std::size_t t = 0; t < NUM_THREADS; ++t) {
        for (const auto &[k, expectedVal] : threadOracles[t]) {
            StringKey::SharedPtr key = new StringKey (k);
            TransactedFileBTree::Iterator itA;
            THEKOGANS_UTIL_ASSERT (verifyTreeA->Find (key, itA), "Missing key in treeA!");
            StringValue::SharedPtr valA = itA.GetValue ();
            THEKOGANS_UTIL_ASSERT (valA != nullptr && valA->value == expectedVal, "Value mismatch in treeA!");

            // Keys without "_mv" suffix must also exist identically in treeB
            if (k.find ("_mv") == std::string::npos) {
                TransactedFileBTree::Iterator itB;
                THEKOGANS_UTIL_ASSERT (verifyTreeB->Find (key, itB), "Missing key in treeB!");
                StringValue::SharedPtr valB = itB.GetValue ();
                THEKOGANS_UTIL_ASSERT (valB != nullptr && valB->value == expectedVal, "Value mismatch in treeB!");
            }
        }
    }
}





#include <string>
#include <list>
#include <iostream>
#include <thekogans/util/Types.h>
#include <thekogans/util/Constants.h>
#include <thekogans/util/LoggerMgr.h>
#include <thekogans/util/ChildProcess.h>
#include <thekogans/util/SystemInfo.h>
#include <thekogans/util/StringUtils.h>
#include <thekogans/util/MainRunLoop.h>
#include "thekogans/transactedfiletest/Options.h"
#include "thekogans/transactedfiletest/Version.h"

using namespace thekogans;

namespace {
    std::string GetLevelsList (const std::string &separator) {
        std::string logLevelList;
        {
            std::list<util::ui32> levels;
            util::LoggerMgr::GetLevels (levels);
            if (!levels.empty ()) {
                std::list<util::ui32>::const_iterator it = levels.begin ();
                logLevelList = util::LoggerMgr::levelTostring (*it++);
                for (std::list<util::ui32>::const_iterator end = levels.end (); it != end; ++it) {
                    logLevelList += separator + util::LoggerMgr::levelTostring (*it);
                }
            }
            else {
                logLevelList = "No LoggerMgr levels defined.";
            }
        }
        return logLevelList;
    }

    std::string GetDecorationsList (const std::string &separator) {
        std::string decorationsList;
        {
            std::list<util::ui32> decorations;
            util::LoggerMgr::GetDecorations (decorations);
            if (!decorations.empty ()) {
                std::list<util::ui32>::const_iterator it = decorations.begin ();
                decorationsList = util::LoggerMgr::decorationsTostring (*it++);
                for (std::list<util::ui32>::const_iterator end = decorations.end (); it != end; ++it) {
                    decorationsList += separator + util::LoggerMgr::decorationsTostring (*it);
                }
            }
            else {
                decorationsList = "No LoggerMgr decorations defined.";
            }
        }
        return decorationsList;
    }
}

int main (
        int argc,
        const char *argv[]) {
    #if defined (THEKOGANS_UTIL_TYPE_Static)
        util::StaticInit ();
        // FIXME: Add other library (crypto, stream, packet) static initialization here.
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)
    // FIXME: Application options go here.
    transactedfiletest::Options::Instance ()->Parse (argc, argv, "hvldcfamok");
    if (transactedfiletest::Options::Instance ()->help) {
        std::cout << util::FormatString (
            "%s [-h] [-v] [-l:'%s'] [-d:'%s'] [-c] [-f:'path'] [-a[:archive count]] [-m:max size] [-o] [-k:'path']\n\n"
            "h - Display this help message.\n"
            "v - Display version information.\n"
            "l - Set logging level.\n"
            "d - Set log entry decorations. (If using more than one, wrap in \" and separate by |).\n"
            "c - Log to console.\n"
            "f - Log to file (path - Path of log file).\n"
            "a - Archive file log (archive count - Number of archives before dropping the oldest).\n"
            "m - Max log file size before archiving.\n"
            "o - Log to debugger console.\n"
            "k - Use lock file to prevent multiple instances (path - Path to lock file).\n",
            // FIXME: Application options go here.
            util::SystemInfo::Instance ()->GetProcessPath ().c_str (),
            GetLevelsList (" | ").c_str (),
            GetDecorationsList (" | ").c_str ());
            // FIXME: Application options go here.
    }
    else if (transactedfiletest::Options::Instance ()->version) {
        std::cout << util::FormatString (
            "%s - %s\n"
            "%s - %s\n",
            THEKOGANS_UTIL,
            util::GetVersion ().ToString ().c_str (),
            util::SystemInfo::Instance ()->GetProcessPath ().c_str (),
            transactedfiletest::GetVersion ().ToString ().c_str ());
    }
    else {
        THEKOGANS_UTIL_TRY {
            struct App {
                App () {
                    THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER;
                    THEKOGANS_UTIL_LOG_INFO ("%s starting.\n",
                        util::SystemInfo::Instance ()->GetProcessPath ().c_str ());
                    util::LockFile lockFile (transactedfiletest::Options::Instance ()->lockFilePath);
                    // FIXME: Application startup code goes here.
                }
                ~App () {
                    THEKOGANS_UTIL_LOG_INFO ("%s exiting.\n",
                        util::SystemInfo::Instance ()->GetProcessPath ().c_str ());
                    // FIXME: Application shutdown code goes here.
                }
                void Run () {
                    util::MainRunLoop::Instance ()->Start ();
                }
            } app;
            app.Run ();
        }
        THEKOGANS_UTIL_CATCH_AND_LOG
    }
    return 0;
}
