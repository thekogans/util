#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <random>
#include <thekogans/util/TransactedFile.h>
#include <thekogans/util/TransactedFileTypeDictionary.h>
#include <thekogans/util/TransactedFileBTreeAllocator.h>
#include <thekogans/util/TransactedFileBTreeRegistry.h>
#include <thekogans/util/TransactedFileObjects.h>

using namespace thekogans::util;

/*
================================================================================
TRANSACTEDFILE ALLOCATOR VALIDATION MATRIX (PHASES 1 - 4)
================================================================================

GLOBAL INVARIANT:
- Open file with secure = true.
- Run HeapWalker::ValidateHeap (verifyHeap) after EVERY Alloc / Realloc / Free.
- Run HeapWalker::GetStats (dumpStats) at the end of each phase.

--------------------------------------------------------------------------------
PHASE 1: SIZE CLAMPING, PAGE BOUNDARIES & MULTI-PAGE STRADDLES
--------------------------------------------------------------------------------
1A. Minimum Clamping:
    - Alloc(0) -> returns 0.
    - Alloc(1), Alloc(MIN_USER_DATA_SIZE - 1), Alloc(MIN_USER_DATA_SIZE)
      -> all succeed, Block::GetSize() >= MIN_USER_DATA_SIZE.
    - Free all three -> verifyHeap.

1B. Page-Boundary & Spacer ('prev') Branches:
    - Query pageSize = file->GetPageSize(), overhead = Block::SIZE.
    - Compute offset into current page: pageOff = file->GetSizeEx() % pageSize.
    - Allocate filler leaving rem = pageSize - overhead - 8 (< MIN_BLOCK_SIZE),
      then Alloc(64) -> exercises small-tail rounding / page advance.
    - Allocate filler leaving rem = MIN_BLOCK_SIZE + 16 (>= MIN_BLOCK_SIZE),
      then Alloc(rem + 128) -> exercises spacer ('prev') block insertion.

1C. Multi-Page Straddles:
    - Alloc(pageSize - overhead)      [exact 1-page block]
    - Alloc(pageSize - overhead + 1)  [2-page straddle]
    - Alloc(pageSize * 4 + 33)        [5-page straddle]
    - Write distinct byte patterns to each payload, verifyHeap,
      then Free all Phase 1 blocks in reverse order -> verifyHeap.

--------------------------------------------------------------------------------
PHASE 2: ALL 4 COALESCING TOPOLOGIES + TAIL SHRINK()
--------------------------------------------------------------------------------
2A. Isolated Free:
    - Allocate contiguous [A0][A1][A2][A3] (256B each).
    - Free(A1) -> [A0][Free A1][A2][A3] (payload wiped).

2B. Forward Coalesce:
    - From [A0][Free A1][A2][A3], allocate fresh [B0][B1][B2][B3] at tail.
    - Free(B2) -> [B0][B1][Free B2][B3].
    - Free(B1) -> merges forward with B2 -> [B0][Free B1+B2][B3].

2C. Backward Coalesce:
    - From [B0][Free B1+B2][B3], Free(B3) -> merges backward into B1+B2.
      (First allocate [B4] after B3 so B3 isn't at the tail.)

2D. Double-Sided Coalesce:
    - Allocate contiguous [C0][C1][C2][C3][C4] (256B each).
    - Free(C1), Free(C3), then Free(C2)
      -> C2 merges left with C1 and right with C3 into one free block.

2E. Tail Collapse & file->Shrink():
    - Allocate contiguous [T0][T1][T2] at the end of the arena (T2.IsLast()).
    - Record sizeBefore = file->GetSizeEx().
    - Free(T1) (middle block free).
    - Free(T2) -> collapses T1 + T2 off tail and calls file->Shrink().
    - Assert file->GetSizeEx() < sizeBefore && verifyHeap.
    - Free remaining Phase 2 blocks so arena is clean for Phase 3.

--------------------------------------------------------------------------------
PHASE 3: REALLOC (IN-PLACE GROW, MOVE-GROW, SHRINK-SPLIT, DATA INTEGRITY)
--------------------------------------------------------------------------------
3A. In-Place Grow (into adjacent free next block):
    - Allocate contiguous [R0 (256B)][R1 (1024B)][R2 (256B)].
    - Fill R0 with 0x5A, Free(R1).
    - newOff = Realloc(R0, 768, true)
      -> Assert newOff == R0, first 256 bytes == 0x5A, verifyHeap.

3B. Move-Grow (blocked by allocated next block R2):
    - newOff = Realloc(R0, 2048, true)
      -> Assert newOff != R0, first 768 bytes intact, old R0 wiped, verifyHeap.

3C. Shrink-Realloc (needsShrink threshold: < 50% && slack >= MIN_BLOCK_SIZE):
    - R0 = Realloc(newOff, 128, true)
      -> Assert slack > 1.8 KB reclaimed into free list, first 128 bytes intact,
         verifyHeap.
    - Free(R0), Free(R2) -> verifyHeap.

--------------------------------------------------------------------------------
PHASE 4: FREE-LIST B-TREE MULTI-LEVEL CHURN & FULL ARENA COLLAPSE
--------------------------------------------------------------------------------
4A. Bulk Allocation:
    - Allocate 1,500 blocks of deterministic pseudo-random sizes (16B .. 2048B),
      saving {offset, size} in a vector.

4B. Even-Index Free Pass (Peak Fragmentation & Multi-Level Splits):
    - Free indices 0, 2, 4, ... (750 non-adjacent holes).
    - Run verifyHeap and dumpStats (record peak freeBlocks & fragmentation).

4C. Shuffled Odd-Index Free Pass (Cascading Coalesces & Tree Collapses):
    - Deterministically shuffle odd indices 1, 3, 5, ... and Free all 750.
    - Run verifyHeap every 100 frees.

4D. Final Collapse Audit:
    - Run verifyHeap and dumpStats.
    - Assert freeBlocks == 0 (or minimal bootstrap state) and arenaSize
      collapsed back down to initial footprint.
================================================================================
 */

namespace {
    auto verifyHeap = [] (
            TransactedFile *file,
            const char *stepName) {
        TransactedFile::HeapWalker::ValidateHeap (file,
            [&] (const TransactedFile::Block &block, const TransactedFile::Object::BlockInfo &info, std::size_t errorCode) -> bool {
                THEKOGANS_UTIL_ASSERT (false,
                    "[%s] %s @%llu (size=%llu, %s)",
                    stepName,
                    TransactedFile::HeapWalker::errorStrings[errorCode],
                    block.GetOffset (),
                    block.GetSize (),
                    block.IsFree () ? "free" : "allocated");
                return false;
            });
    };

    auto dumpStats = [] (
            TransactedFile *file,
            const char *stepName) {
        TransactedFile::HeapWalker::Stats stats;
        TransactedFile::HeapWalker::GetStats (file, stats);
        std::cout <<
            "[" << stepName << "] PageSize: " << file->GetPageSize () << "\n" <<
            "  arenaSize = " << stats.arenaSize << "\n" <<
            "  totalBlocks = " << stats.totalBlocks << "\n" <<
            "  pageStraddlingBlocks = " << stats.pageStraddlingBlocks << "\n" <<
            "  pageStraddlingPayloadBytes = " << stats.pageStraddlingPayloadBytes << "\n" <<
            "  pageStraddlingBlockBytes = " << stats.pageStraddlingBlockBytes << "\n" <<
            "  allocatedBlocks = " << stats.allocatedBlocks << "\n" <<
            "  allocatedPayloadBytes = " << stats.allocatedPayloadBytes << "\n" <<
            "  allocatedBlockBytes = " << stats.allocatedBlockBytes << "\n" <<
            "  freeBlocks = " << stats.freeBlocks << "\n" <<
            "  freePayloadBytes = " << stats.freePayloadBytes << "\n" <<
            "  freeBlockBytes = " << stats.freeBlockBytes << "\n" <<
            "  largestFreePayload = " << stats.largestFreePayload << "\n" <<
            "  smallestFreePayload = " << stats.smallestFreePayload << "\n" <<
            "  Free payload histogram {count, size}:\n" <<
            "    [<=64] = {" << stats.freeSizeHistogram[0].first << ", " << stats.freeSizeHistogram[0].second << "}\n" <<
            "    [<=256] = {" << stats.freeSizeHistogram[1].first << ", " << stats.freeSizeHistogram[1].second << "}\n" <<
            "    [<=1024] = {" << stats.freeSizeHistogram[2].first << ", " << stats.freeSizeHistogram[2].second << "}\n" <<
            "    [<=4096] = {" << stats.freeSizeHistogram[3].first << ", " << stats.freeSizeHistogram[3].second << "}\n" <<
            "    [<=16384] = {" << stats.freeSizeHistogram[4].first << ", " << stats.freeSizeHistogram[4].second << "}\n" <<
            "    [>16384] = {" << stats.freeSizeHistogram[5].first << ", " << stats.freeSizeHistogram[5].second << "}\n";
    };

    auto formatItem = [] (ToStringConvertible::SharedPtr item) -> std::string {
        if (item != nullptr) {
            return item->ToString ();
        }
        return "<non-convertible>";
    };
}

void RunAllocatorTest (const std::string &dbPath) {
    {
        const char *stepName = "Phase 0 - TransactedFile::Create";
        TransactedFile::SharedPtr file = TransactedFile::Create (
            HostEndian,
            dbPath,
            true,
            new TransactedFileTypeDictionary,
            new TransactedFileBTreeAllocator,
            new TransactedFileBTreeRegistry,
            true);
        verifyHeap (file.Get (), stepName);
        dumpStats (file.Get (), stepName);
    }
    {
        TransactedFile::SharedPtr file = TransactedFile::Open (dbPath);
        TransactedFileBTreeAllocator::SharedPtr allocator = file->GetAllocator ();
        const std::size_t pageSize = file->GetPageSize ();
        const std::size_t pageMask = file->GetPageMask ();
        const std::size_t overhead = TransactedFile::Block::SIZE;
        {
            const char *stepName = "Phase 1.1 - Minimum Clamping";
            TransactedFile::Transaction transaction (file.Get ());
            TransactedFile::AddressType offset_0 = allocator->Alloc (0);
            THEKOGANS_UTIL_ASSERT (offset_0 == 0, "offset_0");
            TransactedFile::AddressType offset_1 = allocator->Alloc (1);
            THEKOGANS_UTIL_ASSERT (
                offset_1 != 0 && TransactedFile::Block::GetSize (file.Get (), offset_1) >= TransactedFile::MIN_USER_DATA_SIZE,
                "offset_1");
            TransactedFile::AddressType offset_min_minus_1 = allocator->Alloc (TransactedFile::MIN_USER_DATA_SIZE - 1);
            THEKOGANS_UTIL_ASSERT (
                offset_min_minus_1 != 0 && TransactedFile::Block::GetSize (file.Get (), offset_min_minus_1) >= TransactedFile::MIN_USER_DATA_SIZE,
                "offset_min_minus_1");
            TransactedFile::AddressType offset_min = allocator->Alloc (TransactedFile::MIN_USER_DATA_SIZE);
            THEKOGANS_UTIL_ASSERT (
                offset_min != 0 && TransactedFile::Block::GetSize (file.Get (), offset_min) >= TransactedFile::MIN_USER_DATA_SIZE,
                "offset_min");
            allocator->Free (offset_0);
            allocator->Free (offset_1);
            allocator->Free (offset_min_minus_1);
            allocator->Free (offset_min);
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        {
            const char *stepName = "Phase 1.1.1 - Allocate every free block.";
            TransactedFile::Transaction transaction (file.Get ());
            std::vector<std::pair<TransactedFile::AddressType, TransactedFile::SizeType>> freeList;
            while (allocator->GetFreeList (freeList) > 0) {
                for (const auto &[offset, size] : freeList) {
                    TransactedFile::AddressType offset_free = allocator->Alloc (size);
                    THEKOGANS_UTIL_ASSERT (offset_free != 0 && offset_free == offset, "offset_free");
                }
                freeList.clear ();
            }
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        {
            const char *stepName = "Phase 1.1.2 - Allocate page filling block.";
            TransactedFile::Transaction transaction (file.Get ());
            TransactedFile::SizeType remainder = pageSize - (file->GetSizeEx () & pageMask);
            if (remainder < TransactedFile::MIN_BLOCK_SIZE) {
                remainder += pageSize;
            }
            TransactedFile::AddressType offset_remainder = allocator->Alloc (remainder - TransactedFile::Block::SIZE);
            THEKOGANS_UTIL_ASSERT (offset_remainder != 0, "offset_remainder");
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        {
            const char *stepName = "Phase 1.2 - Exact Page Boundary & Spacer Insertion.";
            TransactedFile::Transaction transaction (file.Get ());
            TransactedFile::AddressType offset_absorb = allocator->Alloc (pageSize - TransactedFile::Block::SIZE - 8);
            THEKOGANS_UTIL_ASSERT (
                offset_absorb != 0 && TransactedFile::Block::GetSize (file.Get (), offset_absorb) == pageSize - TransactedFile::Block::SIZE,
                "offset_absorb");
            TransactedFile::AddressType offset_aligned = allocator->Alloc (64);
            THEKOGANS_UTIL_ASSERT (
                offset_aligned != 0 && (offset_aligned & pageMask) == TransactedFile::Block::HEADER_SIZE,
                "offset_aligned");
            TransactedFile::SizeType remainder = pageSize - (file->GetSizeEx () & pageMask);
            TransactedFile::AddressType offset_remainder = allocator->Alloc (remainder - overhead - TransactedFile::MIN_BLOCK_SIZE - 16);
            THEKOGANS_UTIL_ASSERT (
                offset_remainder != 0 &&
                offset_remainder == offset_aligned +
                TransactedFile::Block::GetSize (file.Get (), offset_aligned) +
                TransactedFile::Block::SIZE,
                "offset_remainder");
            TransactedFile::AddressType offset_aligned2 = allocator->Alloc (128);
            THEKOGANS_UTIL_ASSERT (
                offset_aligned2 != 0 && (offset_aligned2 & pageMask) == TransactedFile::Block::HEADER_SIZE,
                "offset_aligned2");
            allocator->Free (offset_absorb);
            allocator->Free (offset_aligned);
            allocator->Free (offset_remainder);
            allocator->Free (offset_aligned2);
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        TransactedFile::AddressType offset_page_size;
        TransactedFile::AddressType offset_page_size_plus_1;
        TransactedFile::AddressType offset_page_size_times_4_plus_33;
        {
            const char *stepName = "Phase 1.3.1 - Multi-Page Straddles.";
            TransactedFile::Transaction transaction (file.Get ());
            {
                offset_page_size = allocator->Alloc (pageSize - overhead);
                THEKOGANS_UTIL_ASSERT (
                    offset_page_size != 0 && (offset_page_size & pageMask) == TransactedFile::Block::HEADER_SIZE,
                    "offset_page_size");
                TransactedFile::Block block (file.Get (), offset_page_size);
                block.Read ();
                THEKOGANS_UTIL_ASSERT (
                    !block.IsStraddlingPage (),
                    "!block.IsStraddlingPage ()");
                TransactedFile::Range range (file.Get (), offset_page_size, block.GetSize (), false);
                range.Seek (
                    SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable (), '1'),
                    SEEK_CUR);
            }
            {
                offset_page_size_plus_1 = allocator->Alloc (pageSize - overhead + 1);
                THEKOGANS_UTIL_ASSERT (
                    offset_page_size_plus_1 != 0,
                    "offset_page_size_plus_1");
                TransactedFile::Block block (file.Get (), offset_page_size_plus_1);
                block.Read ();
                THEKOGANS_UTIL_ASSERT (
                    block.IsStraddlingPage (),
                    "block.IsStraddlingPage ()");
                TransactedFile::Range range (file.Get (), offset_page_size_plus_1, block.GetSize (), false);
                range.Seek (
                    SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable (), '2'),
                    SEEK_CUR);
            }
            {
                offset_page_size_times_4_plus_33 = allocator->Alloc (pageSize * 4 + 33);
                THEKOGANS_UTIL_ASSERT (
                    offset_page_size_times_4_plus_33 != 0,
                    "offset_page_size_times_4_plus_33");
                TransactedFile::Block block (file.Get (), offset_page_size_times_4_plus_33);
                block.Read ();
                THEKOGANS_UTIL_ASSERT (
                    block.IsStraddlingPage (),
                    "block.IsStraddlingPage ()");
                TransactedFile::Range range (file.Get (), offset_page_size_times_4_plus_33, block.GetSize (), false);
                range.Seek (
                    SecureZeroMemory (range.GetDataPtr (), range.GetDataAvailable (), '3'),
                    SEEK_CUR);
            }
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        {
            const char *stepName = "Phase 1.3.2 - Multi-Page Straddles.";
            TransactedFile::Transaction transaction (file.Get ());
            allocator->Free (offset_page_size_times_4_plus_33);
            allocator->Free (offset_page_size_plus_1);
            allocator->Free (offset_page_size);
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
    }
    {
        TransactedFile::SharedPtr file = TransactedFile::Open (dbPath);
        TransactedFileBTreeAllocator::SharedPtr allocator = file->GetAllocator ();
        TransactedFile::AddressType offset_a0;
        TransactedFile::AddressType offset_a2;
        TransactedFile::AddressType offset_a3;
        {
            const char *stepName = "Phase 2.1 - Isolated Free";
            TransactedFile::Transaction transaction (file.Get ());
            offset_a0 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_a0 != 0, "offset_a0");
            TransactedFile::AddressType offset_a1 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_a1 != 0, "offset_a1");
            offset_a2 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_a2 != 0, "offset_a2");
            offset_a3 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_a3 != 0, "offset_a3");
            allocator->Free (offset_a1);
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        {
            const char *stepName = "Phase 2.1.1 - Isolated Free Cleanup";
            TransactedFile::Transaction transaction (file.Get ());
            allocator->Free (offset_a2);
            allocator->Free (offset_a3);
            allocator->Free (offset_a0);
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        TransactedFile::AddressType offset_b0;
        TransactedFile::AddressType offset_b3;
        {
            const char *stepName = "Phase 2.2 - Forward Coalesce";
            TransactedFile::Transaction transaction (file.Get ());
            offset_b0 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_b0 != 0, "offset_b0");
            TransactedFile::AddressType offset_b1 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_b1 != 0, "offset_b1");
            TransactedFile::AddressType offset_b2 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_b2 != 0, "offset_b2");
            offset_b3 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_b3 != 0, "offset_b3");
            allocator->Free (offset_b2);
            allocator->Free (offset_b1);
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        TransactedFile::AddressType offset_b4;
        {
            const char *stepName = "Phase 2.3 - Backward Coalesce";
            TransactedFile::Transaction transaction (file.Get ());
            offset_b4 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_b4 != 0, "offset_b4");
            allocator->Free (offset_b3);
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        {
            const char *stepName = "Phase 2.3.1 - Backward Coalesce Cleanup";
            TransactedFile::Transaction transaction (file.Get ());
            allocator->Free (offset_b0);
            allocator->Free (offset_b4);
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        TransactedFile::AddressType offset_c0;
        TransactedFile::AddressType offset_c4;
        {
            const char *stepName = "Phase 2.4 - Double-Sided Coalesce";
            TransactedFile::Transaction transaction (file.Get ());
            offset_c0 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_c0 != 0, "offset_c0");
            TransactedFile::AddressType offset_c1 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_c1 != 0, "offset_c1");
            TransactedFile::AddressType offset_c2 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_c2 != 0, "offset_c2");
            TransactedFile::AddressType offset_c3 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_c3 != 0, "offset_c3");
            offset_c4 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_c4 != 0, "offset_c4");
            allocator->Free (offset_c1);
            allocator->Free (offset_c3);
            allocator->Free (offset_c2);
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        {
            const char *stepName = "Phase 2.4.1 - Double-Sided Coalesce Cleanup";
            TransactedFile::Transaction transaction (file.Get ());
            allocator->Free (offset_c0);
            allocator->Free (offset_c4);
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
        {
            const char *stepName = "Phase 2.5 - Tail Collapse & file->Shrink()";
            TransactedFile::Transaction transaction (file.Get ());
            TransactedFile::AddressType offset_t0 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_t0 != 0, "offset_t0");
            TransactedFile::AddressType offset_t1 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_t1 != 0, "offset_t1");
            TransactedFile::AddressType offset_t2 = allocator->Alloc (256);
            THEKOGANS_UTIL_ASSERT (offset_t2 != 0, "offset_t2");
            allocator->Free (offset_t1);
            allocator->Free (offset_t2);
            allocator->Free (offset_t0);
            transaction.Commit ();
            verifyHeap (file.Get (), stepName);
            dumpStats (file.Get (), stepName);
        }
    }
    // 3A. In-Place Grow (into adjacent free next block):
    //     - Allocate contiguous [R0 (256B)][R1 (1024B)][R2 (256B)].
    //     - Fill R0 with 0x5A, Free(R1).
    //     - newOff = Realloc(R0, 768, true)
    //       -> Assert newOff == R0, first 256 bytes == 0x5A, verifyHeap.
    // 3B. Move-Grow (blocked by allocated next block R2):
    //     - newOff = Realloc(R0, 2048, true)
    //       -> Assert newOff != R0, first 768 bytes intact, old R0 wiped, verifyHeap.
    //
    // 3C. Shrink-Realloc (needsShrink threshold: < 50% && slack >= MIN_BLOCK_SIZE):
    //     - R0 = Realloc(newOff, 128, true)
    //       -> Assert slack > 1.8 KB reclaimed into free list, first 128 bytes intact,
    //          verifyHeap.
    //     - Free(R0), Free(R2) -> verifyHeap.

    // =========================================================================
    // PHASE 3: REALLOC VALIDATION MATRIX
    // =========================================================================
    // 1. Degenerate & Boundary Cases:
    //    - Realloc(0, newSize)         -> Delegates to Alloc(newSize); payload stamped ALLOC_POISON (0xCD).
    //    - Realloc(offset, 0)          -> Delegates to Free(offset) and returns 0; freed span stamped FREE_POISON (0xDD).
    //    - Realloc(offset, sameSize)   -> No-op returning identical offset; existing payload preserved intact.
    //    - Invalid / already-free off  -> Throws expected structural/unallocated block exception.
    //
    // 2. In-Place Growth (Interior Block, next.IsFree() && available >= size):
    //    - Sub-32-byte growth          -> 0 < (size - oldSize) < Block::SIZE; verifies absorbed bookend scrub runs
    //                                     BEFORE next.Write() so split header is never overwritten by 0x00/0xCE.
    //    - Split remainder >= MIN_SIZE -> Splits next cleanly; new free block stamped FREE_POISON & inserted in B-tree.
    //    - Remainder < MIN_BLOCK_SIZE  -> Absorbs entire next block; size rounded up to available; no splinter block.
    //    - Exact fit (available==size) -> Consumes next completely; zero remainder block created.
    //    - Payload & Poison check      -> [0, oldSize) preserved byte-for-byte; [oldSize, size) stamped REALLOC_POISON (0xCE).
    //
    // 3. In-Place Tail Growth (block.IsLast() == true, oldSize < size):
    //    - File expansion              -> Calls file->Grow(size - oldSize); block remains IsLast() == true.
    //    - Payload & Poison check      -> [0, oldSize) preserved; [oldSize, size) stamped REALLOC_POISON (0xCE).
    //
    // 4. Out-of-Place Relocation (next allocated, or next free with available < size):
    //    - moveData == true            -> Allocates new block, copies [0, oldSize), stamps [oldSize, size) REALLOC_POISON (0xCE).
    //    - moveData == false           -> Allocates new block, leaves full [0, size) stamped ALLOC_POISON (0xCD).
    //    - Old block reclamation       -> Frees & coalesces old block (with prev/next if free); stamped FREE_POISON (0xDD).
    //
    // 5. In-Place Shrink (oldSize > size):
    //    - Remainder < MIN_BLOCK_SIZE  -> No split; block size unchanged; trailing slack stamped REALLOC_POISON (0xCE).
    //    - Last block (IsLast == true) -> Remainder >= MIN_BLOCK_SIZE; calls file->Shrink(remainder); tail scrubbed.
    //    - Interior + next allocated   -> Carves standalone free block of (remainder - Block::SIZE), stamped FREE_POISON,
    //                                     and inserts into free-list B-tree.
    //    - Interior + next free        -> Removes old next from B-tree, coalesces (remainder + next) into one free block,
    //                                     stamps combined span FREE_POISON, and inserts merged key into B-tree.
    //    - Payload check               -> [0, size) preserved byte-for-byte across all shrink paths.
    //
    // 6. Page-Boundary Straddling Transitions (Test at 4 KB page size):
    //    - Non-straddling -> Straddling -> Grows block across page boundary; Range back-buffer preserves payload & bookends.
    //    - Straddling -> Non-straddling -> Shrinks straddling block inside single page; Range direct-view preserves payload.
    //    - Straddling -> Straddling     -> Multi-page block grow/shrink/relocate preserves every byte across page seams.
    //
    // 7. Post-Transition Invariant Gate (Run after every sub-case):
    //    - HeapWalker::ValidateHeap    -> Zero corrupt bookends; every free block payload matches FREE_POISON (0xDD).
    //    - HeapWalker::GetStats        -> allocatedBlockBytes + freeBlockBytes == arenaSize; freeBlocks == B-tree count.
    // =========================================================================
    {
        const char *stepName = "Phase 3";
        TransactedFile::SharedPtr file = TransactedFile::Open (dbPath);
    }
    {
        const char *stepName = "Phase 4";
        TransactedFile::SharedPtr file = TransactedFile::Open (dbPath);
    }
}

void RunMultiThreadedTortureTest (const std::string &dbPath) {
    static constexpr std::size_t NUM_THREADS = 64;
    static constexpr std::size_t OPS_PER_THREAD = 2500;
    static constexpr std::size_t NUM_CYCLES = 20;

    // 1. Single-threaded database creation
    {
        TransactedFile::SharedPtr file = TransactedFile::Create (
            HostEndian,
            dbPath,
            true,
            new TransactedFileTypeDictionary,
            new TransactedFileBTreeAllocator,
            new TransactedFileBTreeRegistry (true, 64),
            true);
    }

    // Track ground-truth oracle state on the test side
    std::map<std::string, std::string> expectedState;
    std::map<std::string, std::string> cycleState;

    for (std::size_t cycle = 0; cycle < NUM_CYCLES; ++cycle) {
        TransactedFile::SharedPtr file = TransactedFile::Open (dbPath);
        TransactedFile::Transaction transaction (file.Get ());

        std::vector<std::thread> workers;
        std::atomic<bool> startFlag{false};
        std::mutex oracleMutex;

        cycleState = expectedState;

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
                        auto obj = TransactedFile::Object::Create<StringObject> (file.Get (), 0, payload);
                        std::lock_guard<std::mutex> lock (oracleMutex);
                        if (file->GetRegistry ()->InsertValue (key, obj)) {
                            cycleState[key] = payload;
                        }
                    }
                    else if (action < 8) {
                        // Concurrent Read
                        auto obj = file->GetRegistry ()->GetValue (key);
                        // Validate object integrity if non-null
                    }
                    else {
                        // Remove
                        std::lock_guard<std::mutex> lock (oracleMutex);
                        file->GetRegistry ()->RemoveValue (key);
                        cycleState.erase (key);
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
        if (cycle % 4 != 3) {
            transaction.Commit (cycle % 2 == 0); // Alternate clearCache = true / false
            expectedState = cycleState;
        }
    }

    // Cold reopen & full verification against expectedState
    TransactedFile::SharedPtr verifyFile = TransactedFile::Open (dbPath);
    TransactedFileBTree::SharedPtr registry = verifyFile->GetRegistry ();
    for (TransactedFileBTree::Iterator it = registry->FindFirst (); !it.IsFinished (); it.Next ()) {
        TransactedFileBTree::Key::SharedPtr key = it.GetKey ();
        Serializable::SharedPtr value = it.GetValue ();
        std::cout << "Key: " << formatItem (key) << ", Value: " << formatItem (value) << "\n";
    }
    TransactedFile::HeapWalker::ValidateHeap (verifyFile.Get (),
        [] (const TransactedFile::Block &block, const TransactedFile::Object::BlockInfo &info, std::size_t errorCode) -> bool {
            std::cout << TransactedFile::HeapWalker::errorStrings[errorCode] << " @" <<
                block.GetOffset () << ", " << block.GetSize () << ", " << (block.IsFree () ? "free" : "allocated") << "\n";
            return true;
        });
    struct OrphanInfo {
        std::size_t count{0};
        std::size_t size{0};
    };
    HashedStringMapType<OrphanInfo> orphansMap;
    TransactedFile::HeapWalker::CheckForOrphans (verifyFile.Get (),
        [&orphansMap] (const TransactedFile::Block &block, const TransactedFile::Object::BlockInfo &info, std::size_t errorCode) -> bool {
            TransactedFile::ObjectRange orphanRange (block.GetFile (), block.GetOffset ());
            OrphanInfo &orphanInfo = orphansMap[orphanRange.context.type];
            ++orphanInfo.count;
            orphanInfo.size += block.GetSize ();
            return true;
        });
    for (const auto &[key, orphanInfo] : orphansMap) {
        std::cout << key.c_str () << " count: " << orphanInfo.count << " size: " << orphanInfo.size << "\n";
    }
    for (const auto &[key, expectedPayload] : expectedState) {
        auto obj = verifyFile->GetRegistry ()->GetValue (key);
        THEKOGANS_UTIL_ASSERT (obj != nullptr, "Missing committed key %s!", key.c_str ());
        // Assert payload matches expectedPayload
    }
    std::cout << "done\n";
}

void RunTransactedFileTortureTest (const std::string &dbPath) {
    static constexpr std::size_t NUM_THREADS = 64;
    static constexpr std::size_t OPS_PER_THREAD = 2000;
    static constexpr std::size_t NUM_CYCLES = 16;
    static constexpr std::size_t MAX_WORKING_SET_COUNT = 16;

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
            TransactedFile::Transaction transaction (file.Get ());

            // Ensure two B-trees exist for cross-tree Clonable isolation testing
            TransactedFileBTree::SharedPtr treeA = registry->GetValue ("treeA");
            if (treeA == nullptr) {
                treeA = TransactedFile::Object::Create<TransactedFileBTree> (
                    file.Get (), 0, StringKey::GetContext (), SerializableHeader (), false);
                registry->InsertValue ("treeA", treeA);
            }
            TransactedFileBTree::SharedPtr treeB = registry->GetValue ("treeB");
            if (treeB == nullptr) {
                treeB = TransactedFile::Object::Create<TransactedFileBTree> (
                    file.Get (), 0, StringKey::GetContext (), SerializableHeader (), false);
                registry->InsertValue ("treeB", treeB);
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
                            reusableKey->key = keyStr;
                            reusableVal->value = payload;

                            treeA->Remove (*reusableKey);
                            treeB->Remove (*reusableKey);
                            treeA->Insert (reusableKey, reusableVal);
                            treeB->Insert (reusableKey, reusableVal);

                            // Poison caller's external handles immediately!
                            reusableKey->key = "POISONED_KEY";
                            reusableVal->value = "POISONED_VALUE";

                            localOracle[keyStr] = payload;
                        }
                        else if (action < 6) {
                            // Zero-copy move insert with non-clonable StringValue into treeA
                            std::string moveKeyStr = keyStr + "_mv";
                            std::string payload = "move_" + std::to_string (op);
                            StringKey::SharedPtr k = new StringKey (moveKeyStr);
                            StringValue::SharedPtr v = new StringValue (payload);
                            treeA->Remove (*k);
                            if (treeA->Insert (k, v)) {
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
                            StringValue::SharedPtr val = treeA->Find (*probe);
                            if (val != nullptr) {
                                THEKOGANS_UTIL_ASSERT (
                                    val != nullptr && val->value != "POISONED_VALUE",
                                    "Observed poisoned/corrupt value during concurrent read!");
                            }
                        }
                        else {
                            // Remove from both trees
                            reusableKey->key = keyStr;
                            treeA->Remove (*reusableKey);
                            treeB->Remove (*reusableKey);
                            reusableKey->key = "POISONED_KEY";
                            localOracle.erase (keyStr);
                        }
                    }
                    if (file->GetWorkingSetCount () >= MAX_WORKING_SET_COUNT) {
                        file->ClearPageCache ();
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
                threadOracles = preCycleOracles;
            }
            else {
                transaction.Commit (cycle % 2 == 0);
            }
        }
    }

    // 2. Cold reopen and verify 100% of treeA and treeB against ground-truth oracles
    TransactedFile::SharedPtr verifyFile = TransactedFile::Open (dbPath);
    TransactedFileBTree::SharedPtr registry = verifyFile->GetRegistry ();
    for (TransactedFileBTree::Iterator it = registry->FindFirst (); !it.IsFinished (); it.Next ()) {
        TransactedFileBTree::Key::SharedPtr key = it.GetKey ();
        Serializable::SharedPtr value = it.GetValue ();
        std::cout << "Key: " << formatItem (key) << ", Value: " << formatItem (value) << "\n";
    }
    TransactedFile::HeapWalker::ValidateHeap (verifyFile.Get (),
        [] (const TransactedFile::Block &block, const TransactedFile::Object::BlockInfo &info, std::size_t errorCode) -> bool {
            std::cout << TransactedFile::HeapWalker::errorStrings[errorCode] << " @" <<
                block.GetOffset () << ", " << block.GetSize () << ", " << (block.IsFree () ? "free" : "allocated") << "\n";
            return true;
        });
    struct OrphanInfo {
        std::size_t count{0};
        std::size_t size{0};
    };
    HashedStringMapType<OrphanInfo> orphansMap;
    TransactedFile::HeapWalker::CheckForOrphans (verifyFile.Get (),
        [&orphansMap] (const TransactedFile::Block &block, const TransactedFile::Object::BlockInfo &info, std::size_t errorCode) -> bool {
            TransactedFile::ObjectRange orphanRange (block.GetFile (), block.GetOffset ());
            OrphanInfo &orphanInfo = orphansMap[orphanRange.context.type];
            ++orphanInfo.count;
            orphanInfo.size += block.GetSize ();
            return true;
        });
    for (const auto &[key, orphanInfo] : orphansMap) {
        std::cout << key.c_str () << " count: " << orphanInfo.count << " size: " << orphanInfo.size << "\n";
    }

    TransactedFileBTreeRegistry::SharedPtr verifyReg = verifyFile->GetRegistry ();
    TransactedFileBTree::SharedPtr verifyTreeA = verifyReg->GetValue ("treeA");
    TransactedFileBTree::SharedPtr verifyTreeB = verifyReg->GetValue ("treeB");

    for (std::size_t t = 0; t < NUM_THREADS; ++t) {
        for (const auto &[k, expectedVal] : threadOracles[t]) {
            StringKey::SharedPtr key = new StringKey (k);
            StringValue::SharedPtr valA = verifyTreeA->Find (*key);
            THEKOGANS_UTIL_ASSERT (valA != nullptr, "Missing key in treeA!");
            THEKOGANS_UTIL_ASSERT (valA != nullptr && valA->value == expectedVal, "Value mismatch in treeA!");

            // Keys without "_mv" suffix must also exist identically in treeB
            if (k.find ("_mv") == std::string::npos) {
                StringValue::SharedPtr valB = verifyTreeB->Find (*key);
                THEKOGANS_UTIL_ASSERT (valB != nullptr, "Missing key in treeB!");
                THEKOGANS_UTIL_ASSERT (valB != nullptr && valB->value == expectedVal, "Value mismatch in treeB!");
            }
        }
    }
    std::cout << "done\n";
}

void TestTransactedFileBTreeIterators (const std::string &dbPath) {
    TransactedFile::SharedPtr file = TransactedFile::Create (
        HostEndian,
        dbPath,
        false,
        new TransactedFileTypeDictionary,
        new TransactedFileBTreeAllocator,
        new TransactedFileBTreeRegistry,
        true);

    std::vector<std::string> expected;
    TransactedFile::Transaction transaction (file.Get ());
    // Use a small entriesPerNode (e.g., 4) so ~60 keys force a 3-4 level B-tree
    // with internal separator keys, partial leaf fills, and multi-level parent stacks.
    TransactedFileBTree::SharedPtr btree =
        TransactedFile::Object::Create<TransactedFileBTree> (
            file.Get (), 0, StringKey::GetContext (), ClonableStringValue::GetContext (), false, 4);
    // Insert keys across prefixes: "a_00".."a_19", "b_00".."b_19", "d_00".."d_19"
    // Intentionally omit "c_*" to test missing-key LowerBound / UpperBound!
    for (const char *prefix : {"a_", "b_", "d_"}) {
        for (int i = 0; i < 20; ++i) {
            char buf[16];
            std::snprintf (buf, sizeof (buf), "%s%02d", prefix, i);
            expected.emplace_back (buf);
            btree->Insert (
                new StringKey (buf),
                new ClonableStringValue (std::string ("val_") + buf));
        }
    }
    transaction.Commit ();

    // 1. Full forward scan: [Begin(), End())
    {
        std::vector<std::string> actual;
        for (TransactedFileBTree::Iterator it = btree->Begin (), end = btree->End ();
                it != end; ++it) {
            actual.push_back (it.GetKey ()->ToString ());
        }
        assert (actual == expected);
    }

    // 2. Full reverse scan: [RBegin(), End())
    {
        std::vector<std::string> actual;
        for (TransactedFileBTree::Iterator it = btree->RBegin (), end = btree->End ();
                it != end; --it) {
            actual.push_back (it.GetKey ()->ToString ());
        }
        std::vector<std::string> revExpected (expected.rbegin (), expected.rend ());
        assert (actual == revExpected);
    }

    // 3. Single-prefix forward & reverse scans: FindFirst("b_") / FindLast("b_")
    {
        std::vector<std::string> actualFwd;
        for (TransactedFileBTree::Iterator it = btree->FindFirst (new StringKey ("b_"));
                !it.IsFinished (); it.Next ()) {
            actualFwd.push_back (it.GetKey ()->ToString ());
        }
        assert (actualFwd.size () == 20 && actualFwd.front () == "b_00" && actualFwd.back () == "b_19");

        std::vector<std::string> actualRev;
        for (TransactedFileBTree::Iterator it = btree->FindLast (new StringKey ("b_"));
                !it.IsFinished (); it.Prev ()) {
            actualRev.push_back (it.GetKey ()->ToString ());
        }
        assert (actualRev.size () == 20 && actualRev.front () == "b_19" && actualRev.back () == "b_00");
    }

    // 4. Half-open range [LowerBound("b_"), UpperBound("d_")) -> all 40 "b_*" and "d_*" keys
    {
        std::vector<std::string> actual;
        for (TransactedFileBTree::Iterator first = btree->LowerBound (new StringKey ("b_")),
                last = btree->UpperBound (new StringKey ("d_"));
                first != last; ++first) {
            actual.push_back (first.GetKey ()->ToString ());
        }
        assert (actual.size () == 40 && actual.front () == "b_00" && actual.back () == "d_19");
    }

    // 5. Missing-key endpoints: [LowerBound("b_10"), UpperBound("c_")) -> "b_10".."b_19" (stops cleanly at "d_00")
    {
        std::vector<std::string> actual;
        for (TransactedFileBTree::Iterator first = btree->LowerBound (new StringKey ("b_10")),
                last = btree->UpperBound (new StringKey ("c_"));
                first != last; ++first) {
            actual.push_back (first.GetKey ()->ToString ());
        }
        assert (actual.size () == 10 && actual.front () == "b_10" && actual.back () == "b_19");
    }

    // 6. Empty range on missing prefix: [LowerBound("c_"), UpperBound("c_")) -> 0 iterations
    {
        std::size_t count = 0;
        for (TransactedFileBTree::Iterator first = btree->LowerBound (new StringKey ("c_")),
                last = btree->UpperBound (new StringKey ("c_"));
                first != last; ++first) {
            ++count;
        }
        assert (count == 0);
    }

    // 7. Out-of-bounds endpoints: [LowerBound("z_"), End()) and [Begin(), UpperBound("0_")) -> 0 iterations
    {
        assert (btree->LowerBound (new StringKey ("z_")) == btree->End ());
        assert (btree->UpperBound (new StringKey ("0_")) == btree->Begin ());
    }
}

void TestTransactedFileBTreeIteratorEdgeCases (const std::string &dbPath) {
    TransactedFile::SharedPtr file = TransactedFile::Create (
        HostEndian,
        dbPath,
        false,
        new TransactedFileTypeDictionary,
        new TransactedFileBTreeAllocator,
        new TransactedFileBTreeRegistry,
        true);

    // 1. Empty B-tree: every lookup must immediately return End() / IsFinished()
    {
        TransactedFile::Transaction transaction (file.Get ());
        TransactedFileBTree::SharedPtr emptyTree =
            TransactedFile::Object::Create<TransactedFileBTree> (
                file.Get (), 0, StringKey::GetContext (), ClonableStringValue::GetContext (), false, 4);
        transaction.Commit ();

        assert (emptyTree->Begin () == emptyTree->End ());
        assert (emptyTree->RBegin () == emptyTree->End ());
        assert (emptyTree->FindFirst (new StringKey ("a")).IsFinished ());
        assert (emptyTree->FindLast (new StringKey ("a")).IsFinished ());
        assert (emptyTree->LowerBound (new StringKey ("a")) == emptyTree->End ());
        assert (emptyTree->UpperBound (new StringKey ("a")) == emptyTree->End ());

        // Calling Next() / Prev() on a finished iterator must remain a safe no-op
        TransactedFileBTree::Iterator it = emptyTree->End ();
        assert (!it.Next ());
        assert (!it.Prev ());
        assert (it.IsFinished ());
    }

    // 2. Single-entry B-tree (root leaf with count == 1): boundary transitions in both directions
    {
        TransactedFile::Transaction transaction (file.Get ());
        TransactedFileBTree::SharedPtr singleTree =
            TransactedFile::Object::Create<TransactedFileBTree> (
                file.Get (), 0, StringKey::GetContext (), ClonableStringValue::GetContext (), false, 4);
        singleTree->Insert (new StringKey ("only_key"), new ClonableStringValue ("only_val"));
        transaction.Commit ();

        TransactedFileBTree::Iterator fwd = singleTree->Begin ();
        assert (!fwd.IsFinished ());
        assert (fwd.GetKey ()->ToString () == "only_key");
        assert (!fwd.Next ());
        assert (fwd.IsFinished ());
        assert (fwd == singleTree->End ());

        TransactedFileBTree::Iterator rev = singleTree->RBegin ();
        assert (!rev.IsFinished ());
        assert (rev.GetKey ()->ToString () == "only_key");
        assert (!rev.Prev ());
        assert (rev.IsFinished ());
        assert (rev == singleTree->End ());

        // LowerBound / UpperBound bracketing the single element
        assert (singleTree->LowerBound (new StringKey ("a")) == singleTree->Begin ());
        assert (singleTree->LowerBound (new StringKey ("only_key")) == singleTree->Begin ());
        assert (singleTree->LowerBound (new StringKey ("z")) == singleTree->End ());
        assert (singleTree->UpperBound (new StringKey ("a")) == singleTree->Begin ());
        assert (singleTree->UpperBound (new StringKey ("only_key")) == singleTree->End ());
    }

    // 3. Multi-level tree with mixed Next() <-> Prev() direction reversals across internal & leaf boundaries
    {
        std::vector<std::string> keys;
        TransactedFileBTree::SharedPtr tree;
        {
            TransactedFile::Transaction transaction (file.Get ());
            tree = TransactedFile::Object::Create<TransactedFileBTree> (
                file.Get (), 0, StringKey::GetContext (), ClonableStringValue::GetContext (), false, 4);
            for (int i = 0; i < 50; ++i) {
                char buf[16];
                std::snprintf (buf, sizeof (buf), "k_%03d", i);
                keys.emplace_back (buf);
                tree->Insert (new StringKey (buf), new ClonableStringValue (buf));
            }
            transaction.Commit ();
        }

        // Walk forward 25 steps, reverse 15 steps, walk forward to end
        TransactedFileBTree::Iterator it = tree->Begin ();
        std::size_t idx = 0;
        assert (it.GetKey ()->ToString () == keys[idx]);

        for (int step = 0; step < 25; ++step) {
            assert (it.Next (true));
            ++idx;
            assert (it.GetKey ()->ToString () == keys[idx]);
        }
        for (int step = 0; step < 15; ++step) {
            assert (it.Prev (true));
            --idx;
            assert (it.GetKey ()->ToString () == keys[idx]);
        }
        while (idx + 1 < keys.size ()) {
            assert (it.Next (true));
            ++idx;
            assert (it.GetKey ()->ToString () == keys[idx]);
        }
        assert (!it.Next (true));
        assert (it.IsFinished ());

        // 4. Delete half the keys (forcing node underflows, borrows, merges, and root collapse),
        // then verify full forward, reverse, and range scans match the surviving keys 1-for-1
        std::vector<std::string> surviving;
        {
            TransactedFile::Transaction transaction (file.Get ());
            for (std::size_t i = 0; i < keys.size (); ++i) {
                if (i % 2 == 0) {
                    tree->Remove (StringKey (keys[i]));
                }
                else {
                    surviving.push_back (keys[i]);
                }
            }
            transaction.Commit ();
        }

        std::vector<std::string> fwdSurviving;
        for (auto f = tree->Begin (), e = tree->End (); f != e; ++f) {
            fwdSurviving.push_back (f.GetKey ()->ToString ());
        }
        assert (fwdSurviving == surviving);

        std::vector<std::string> revSurviving;
        for (auto r = tree->RBegin (), e = tree->End (); r != e; --r) {
            revSurviving.push_back (r.GetKey ()->ToString ());
        }
        std::vector<std::string> expectedRev (surviving.rbegin (), surviving.rend ());
        assert (revSurviving == expectedRev);

        // Verify range scan across deleted keys: [LowerBound("k_010"), UpperBound("k_030"))
        // (note: both "k_010" and "k_030" were deleted, leaving odd keys "k_011".."k_029")
        std::vector<std::string> rangeKeys;
        for (auto first = tree->LowerBound (new StringKey ("k_010")),
                last = tree->UpperBound (new StringKey ("k_030"));
                first != last; ++first) {
            rangeKeys.push_back (first.GetKey ()->ToString ());
        }
        assert (rangeKeys.size () == 10 &&
            rangeKeys.front () == "k_011" &&
            rangeKeys.back () == "k_029");
    }
}

int main (
        int argc,
        const char *argv[]) {
    #if defined (THEKOGANS_UTIL_TYPE_Static)
        StaticInit ();
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)
        //RunAllocatorTest ("test.db");
        //RunMultiThreadedTortureTest ("test.db");
        RunTransactedFileTortureTest ("test.db");
        //TestTransactedFileBTreeIterators ("test.db");
        //TestTransactedFileBTreeIteratorEdgeCases ("test.db");
    return 0;
}
