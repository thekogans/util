#include <thekogans/util/Path.h>
#include <thekogans/util/TransactedFile.h>
#include <thekogans/util/TransactedFileTypeDictionary.h>
#include <thekogans/util/TransactedFileBTreeAllocator.h>
#include <thekogans/util/TransactedFileBTreeRegistry.h>

using namespace thekogans::util;

void RunAllocatorEdgeCaseTests (const std::string &baseDir) {
    // 1. Grow-Then-Drain: forces TransactedFileBTreeAllocatorBTree to split many
    //    internal nodes, then frees 95% of them to populate header.freeNodeOffset != 0,
    //    followed by small allocations that exercise freeNodeOffset reuse!
    {
        const std::string path = MakePath (baseDir, "edge_grow_drain.db");
        if (Path (path).Exists ()) {
            File::Delete (path);
        }
        TransactedFile::SharedPtr file = TransactedFile::Create (
            HostEndian, path, false,
            new TransactedFileTypeDictionary,
            new TransactedFileBTreeAllocator,
            new TransactedFileBTreeRegistry,
            true);

        std::vector<TransactedFile::AddressType> offsets;
        {
            TransactedFile::Transaction tx (file.Get ());
            // Allocate 600 distinct sizes (64 .. 2460 in 4-byte steps) so every size
            // creates a distinct key in TransactedFileBTreeAllocatorBTree
            for (std::size_t i = 0; i < 600; ++i) {
                TransactedFile::SizeType sz = 64 + i * 4;
                TransactedFile::AddressType off = file->GetAllocator ()->Alloc (sz);
                THEKOGANS_UTIL_ASSERT (off != 0, "Alloc failed during grow phase!");
                offsets.push_back (off);
            }
            tx.Commit ();
        }
        {
            TransactedFile::Transaction tx (file.Get ());
            // Free 570 of the 600 blocks in checkerboard/reverse order to force
            // allocator B-tree node merges -> pushes free nodes onto header.freeNodeOffset
            for (std::size_t i = 0; i < 570; ++i) {
                file->GetAllocator ()->Free (offsets[i]);
            }
            tx.Commit ();
        }
        {
            TransactedFile::Transaction tx (file.Get ());
            // Allocate 120 small blocks (<= nodeBlockSize) to exercise header.freeNodeOffset != 0
            // reuse path, then free them so no orphans remain
            std::vector<TransactedFile::AddressType> reused;
            for (std::size_t i = 0; i < 120; ++i) {
                TransactedFile::AddressType off = file->GetAllocator ()->Alloc (64);
                THEKOGANS_UTIL_ASSERT (off != 0, "Alloc failed during freeNodeOffset reuse phase!");
                reused.push_back (off);
            }
            for (auto off : reused) {
                file->GetAllocator ()->Free (off);
            }
            // Free remaining 30 blocks (including tail blocks -> exercises ShrinkFile!)
            for (std::size_t i = 570; i < 600; ++i) {
                file->GetAllocator ()->Free (offsets[i]);
            }
            tx.Commit ();
        }

        std::size_t errorCount = 0;
        TransactedFile::HeapWalker::ValidateHeap (file.Get (),
            [&] (const TransactedFile::Block &, const TransactedFile::Object::BlockInfo &, std::size_t err) -> bool {
                if (err != TransactedFile::HeapWalker::ERROR_NONE) {
                    ++errorCount;
                }
                return true;
            });
        THEKOGANS_UTIL_ASSERT (errorCount == 0, "ValidateHeap failed after Grow-Then-Drain!");
    }

    // 2. Small-Page (4096 B) Straddle, Filler-Block & Page-Boundary Round-Up
    {
        const std::string path = MakePath (baseDir, "edge_small_page.db");
        if (Path (path).Exists ()) {
            File::Delete (path);
        }
        // Pass pageSize = 4096 (adjust constructor overload if pageSize is passed via Create)
        TransactedFile::SharedPtr file = TransactedFile::Create (
            HostEndian, path, false,
            new TransactedFileTypeDictionary,
            new TransactedFileBTreeAllocator,
            new TransactedFileBTreeRegistry,
            true,
            32, 8, 12);

        std::vector<TransactedFile::AddressType> blocks;
        {
            TransactedFile::Transaction tx (file.Get ());
            // Allocate sizes that leave 1 .. MIN_BLOCK_SIZE - 1 bytes at page end
            // (exercises round-up to page boundary) and sizes that straddle 4096-byte pages
            for (std::size_t i = 0; i < 64; ++i) {
                TransactedFile::SizeType sz = 3900 + (i % 128);
                blocks.push_back (file->GetAllocator ()->Alloc (sz));
            }
            // Free every other block to create free holes that straddle page boundaries,
            // then allocate medium blocks inside those holes (exercises filler-block split!)
            for (std::size_t i = 0; i < blocks.size (); i += 2) {
                file->GetAllocator ()->Free (blocks[i]);
                blocks[i] = 0;
            }
            for (std::size_t i = 0; i < 32; ++i) {
                blocks.push_back (file->GetAllocator ()->Alloc (1800 + (i % 64)));
            }
            tx.Commit ();
        }

        TransactedFile::HeapWalker::Stats stats;
        TransactedFile::HeapWalker::GetStats (file.Get (), stats);
        THEKOGANS_UTIL_ASSERT (stats.totalBlocks > 0, "Expected non-empty heap stats!");

        {
            // Clean up all allocated blocks
            TransactedFile::Transaction cleanupTx (file.Get ());
            for (auto off : blocks) {
                if (off != 0) {
                    file->GetAllocator ()->Free (off);
                }
            }
            cleanupTx.Commit ();
        }

        std::size_t errorCount = 0;
        TransactedFile::HeapWalker::ValidateHeap (file.Get (),
            [&] (const TransactedFile::Block &, const TransactedFile::Object::BlockInfo &, std::size_t err) -> bool {
                if (err != TransactedFile::HeapWalker::ERROR_NONE) {
                    ++errorCount;
                }
                return true;
            });
        THEKOGANS_UTIL_ASSERT (errorCount == 0, "ValidateHeap failed after small-page straddle test!");
    }

    // 3. Secure Mode (secure = true): exercises ALLOC_POISON & FREE_POISON scrubbing
    {
        const std::string path = MakePath (baseDir, "edge_secure.db");
        if (Path (path).Exists ()) {
            File::Delete (path);
        }
        TransactedFile::SharedPtr file = TransactedFile::Create (
            HostEndian, path, true /* secure */,
            new TransactedFileTypeDictionary,
            new TransactedFileBTreeAllocator,
            new TransactedFileBTreeRegistry,
            true);

        TransactedFile::AddressType b1 = 0;
        TransactedFile::AddressType b2 = 0;
        TransactedFile::AddressType b3 = 0;
        {
            TransactedFile::Transaction tx (file.Get ());
            b1 = file->GetAllocator ()->Alloc (512);
            b2 = file->GetAllocator ()->Alloc (512);
            b3 = file->GetAllocator ()->Alloc (512);
            // Write non-zero user payload to b2, then free b2 (leaves middle free block)
            {
                TransactedFile::Range r (file.Get (), b2, 512, false);
                std::vector<ui8> pattern (512, 0x5A);
                r.Write (pattern.data (), pattern.size ());
            }
            file->GetAllocator ()->Free (b2);
            tx.Commit ();
        }

        std::size_t errorCount = 0;
        TransactedFile::HeapWalker::ValidateHeap (file.Get (),
            [&] (const TransactedFile::Block &, const TransactedFile::Object::BlockInfo &, std::size_t err) -> bool {
                if (err != TransactedFile::HeapWalker::ERROR_NONE) {
                    ++errorCount;
                }
                return true;
            });
        THEKOGANS_UTIL_ASSERT (errorCount == 0, "Secure file failed ValidateHeap!");

        {
            TransactedFile::Transaction cleanupTx (file.Get ());
            file->GetAllocator ()->Free (b1);
            file->GetAllocator ()->Free (b3);
            cleanupTx.Commit ();
        }
    }
}
