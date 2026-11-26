#include <thekogans/util/Path.h>
#include <thekogans/util/TransactedFile.h>
#include <thekogans/util/TransactedFileTypeDictionary.h>
#include <thekogans/util/TransactedFileBTreeAllocator.h>
#include <thekogans/util/TransactedFileBTreeRegistry.h>

using namespace thekogans::util;

// Helper: runs ValidateHeap + CheckForOrphans and returns true if expectedError was observed
bool ExpectHeapError (
        TransactedFile *file,
        std::size_t expectedError) {
    bool sawExpected = false;
    TransactedFile::HeapWalker::ValidateHeap (file,
        [&] (const TransactedFile::Block &, const TransactedFile::Object::BlockInfo &, std::size_t err) -> bool {
            if (err == expectedError) {
                sawExpected = true;
            }
            return true;
        });
    if (!sawExpected) {
        TransactedFile::HeapWalker::CheckForOrphans (file,
            [&] (const TransactedFile::Block &, const TransactedFile::Object::BlockInfo &, std::size_t err) -> bool {
                if (err == expectedError) {
                    sawExpected = true;
                }
                return true;
            });
    }
    return sawExpected;
}

void RunHeapWalkerCorruptionTests (const std::string &baseDir) {
    // Helper lambda to create a valid baseline file with 3 adjacent blocks (b1, b2, b3)
    // and optional secure=true mode.
    struct Baseline {
        TransactedFile::SharedPtr file;
        TransactedFile::AddressType b1{0};
        TransactedFile::AddressType b2{0};
        TransactedFile::AddressType b3{0};
    };
    auto makeBaseline = [&] (const std::string &name, bool secure = false) -> Baseline {
        const std::string path = MakePath (baseDir, name);
        if (Path (path).Exists ()) {
            File::Delete (path);
        }
        Baseline b;
        b.file = TransactedFile::Create (
            HostEndian, path, secure,
            new TransactedFileTypeDictionary,
            new TransactedFileBTreeAllocator,
            new TransactedFileBTreeRegistry,
            true);
        TransactedFile::Transaction tx (b.file.Get ());
        b.b1 = b.file->GetAllocator ()->Alloc (256);
        b.b2 = b.file->GetAllocator ()->Alloc (256);
        b.b3 = b.file->GetAllocator ()->Alloc (256);
        tx.Commit ();
        return b;
    };

    // Case 1: ERROR_FREE_BLOCK_0
    {
        auto b = makeBaseline ("corrupt_free_block_0.db");
        TransactedFile::Block block0 (b.file.Get (), TransactedFile::Block::BLOCK_0_OFFSET);
        block0.Read ();
        block0.SetFree (true);
        block0.Write ();
        THEKOGANS_UTIL_ASSERT (
            ExpectHeapError (b.file.Get (), TransactedFile::HeapWalker::ERROR_FREE_BLOCK_0),
            "Failed to detect ERROR_FREE_BLOCK_0!");
    }

    // Case 2: ERROR_TWO_CONSECUTIVE_FREE_BLOCKS & ERROR_BLOCK_MISSING_FROM_FREE_LIST
    {
        auto b = makeBaseline ("corrupt_consecutive_free.db");
        {
            TransactedFile::Transaction tx (b.file.Get ());
            b.file->GetAllocator ()->Free (b.b1);
            tx.Commit ();
        }
        // Mark adjacent b2 free directly on disk without coalescing or inserting into free list
        TransactedFile::Block blk2 (b.file.Get (), b.b2);
        blk2.Read ();
        blk2.SetFree (true);
        blk2.Write ();
        THEKOGANS_UTIL_ASSERT (
            ExpectHeapError (b.file.Get (), TransactedFile::HeapWalker::ERROR_TWO_CONSECUTIVE_FREE_BLOCKS),
            "Failed to detect ERROR_TWO_CONSECUTIVE_FREE_BLOCKS!");
        THEKOGANS_UTIL_ASSERT (
            ExpectHeapError (b.file.Get (), TransactedFile::HeapWalker::ERROR_BLOCK_MISSING_FROM_FREE_LIST),
            "Failed to detect ERROR_BLOCK_MISSING_FROM_FREE_LIST!");
    }

    // Case 3: ERROR_GARBAGE_IN_PAYLOAD (on secure = true file)
    {
        auto b = makeBaseline ("corrupt_garbage_payload.db", true /* secure */);
        {
            TransactedFile::Transaction tx (b.file.Get ());
            b.file->GetAllocator ()->Free (b.b2);
            tx.Commit ();
        }
        // Scrawl a non-poison byte into freed b2's payload
        TransactedFile::Range range (b.file.Get (), b.b2, 16, false);
        ui8 garbage = 0xDE;
        range.Write (&garbage, 1);
        THEKOGANS_UTIL_ASSERT (
            ExpectHeapError (b.file.Get (), TransactedFile::HeapWalker::ERROR_GARBAGE_IN_PAYLOAD),
            "Failed to detect ERROR_GARBAGE_IN_PAYLOAD!");
    }

    // Case 4: ERROR_FREE_LAST_BLOCK
    {
        auto b = makeBaseline ("corrupt_free_last.db");
        TransactedFile::Block blk3 (b.file.Get (), b.b3);
        blk3.Read ();
        blk3.SetFree (true);
        blk3.Write ();
        THEKOGANS_UTIL_ASSERT (
            ExpectHeapError (b.file.Get (), TransactedFile::HeapWalker::ERROR_FREE_LAST_BLOCK),
            "Failed to detect ERROR_FREE_LAST_BLOCK!");
    }

    // Case 5: ERROR_ORPHAN_BLOCK (allocated block b1/b2/b3 not referenced by any BTree/Object)
    {
        auto b = makeBaseline ("corrupt_orphan_block.db");
        THEKOGANS_UTIL_ASSERT (
            ExpectHeapError (b.file.Get (), TransactedFile::HeapWalker::ERROR_ORPHAN_BLOCK),
            "Failed to detect ERROR_ORPHAN_BLOCK!");
    }

    // Case 6: ERROR_TRUNCATED_BLOCK (shrink underlying OS file mid-block)
    {
        const std::string path = MakePath (baseDir, "corrupt_truncated.db");
        {
            auto b = makeBaseline ("corrupt_truncated.db");
        } // close TransactedFile
        SimpleFile rawFile (HostEndian, path, SimpleFile::ReadWrite);
        rawFile.SetSize (rawFile.GetSize () - 16);
        rawFile.Close ();
        try {
            TransactedFile::SharedPtr reopened = TransactedFile::Open (path);
        }
        catch (const TransactedFile::Block::Exception &ex) {
            THEKOGANS_UTIL_ASSERT (
                ex.GetErrorCode () == TransactedFile::Block::Exception::CORRUPT_FOOTER,
                "Failed to detect ERROR_TRUNCATED_BLOCK!");
        }
    }
}
