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

/// \struct TransactedFile::HeapWalker TransactedFileHeapWalker.h thekogans/util/TransactedFileHeapWalker.h
///
/// \brief
struct _LIB_THEKOGANS_UTIL_DECL HeapWalker {
    static constexpr std::size_t ERROR_NONE = 0;
    static constexpr std::size_t ERROR_FREE_BLOCK_0 = 1;
    static constexpr std::size_t ERROR_TWO_CONSECUTIVE_FREE_BLOCKS = 2;
    static constexpr std::size_t ERROR_GARBAGE_IN_PAYLOAD = 3;
    static constexpr std::size_t ERROR_FREE_LAST_BLOCK = 4;
    static constexpr std::size_t ERROR_FREE_COUNT_MISMATCH = 5;
    static constexpr std::size_t ERROR_BLOCK_MISSING_FROM_FREE_LIST = 6;
    static constexpr std::size_t ERROR_ORPHAN_BLOCK = 7;
    static constexpr std::size_t ERROR_TRUNCATED_BLOCK = 8;
    static constexpr std::size_t ERROR_DUPLICATE_BLOCK_REFERENCE = 9;
    static constexpr std::size_t ERROR_DANGLING_BLOCK_REFERENCE = 10;
    static constexpr std::size_t ERROR_DANGLING_WEAK_REFERENCE = 11;
    static constexpr std::size_t ERROR_ORPHAN_WEAK_REFERENCE = 12;

    static constexpr const char * const errorStrings[] = {
        "ERROR_NONE",
        "ERROR_FREE_BLOCK_0",
        "ERROR_TWO_CONSECUTIVE_FREE_BLOCKS",
        "ERROR_GARBAGE_IN_PAYLOAD",
        "ERROR_FREE_LAST_BLOCK",
        "ERROR_FREE_COUNT_MISMATCH",
        "ERROR_BLOCK_MISSING_FROM_FREE_LIST",
        "ERROR_ORPHAN_BLOCK",
        "ERROR_TRUNCATED_BLOCK",
        "ERROR_DUPLICATE_BLOCK_REFERENCE",
        "ERROR_DANGLING_BLOCK_REFERENCE",
        "ERROR_DANGLING_WEAK_REFERENCE",
        "ERROR_ORPHAN_WEAK_REFERENCE"
    };

    using Callback = std::function<bool (const Block &block, std::size_t errorCode)>;

    static void ValidateHeap (
        TransactedFile *file,
        Callback callback);
    static void CheckForOrphans (
        TransactedFile *file,
        Callback callback);

    struct Stats {
        SizeType arenaSize{0};
        std::size_t totalBlocks{0};
        std::size_t pageStraddlingBlocks{0};
        SizeType pageStraddlingPayloadBytes{0};
        SizeType pageStraddlingBlockBytes{0};
        std::size_t allocatedBlocks{0};
        SizeType allocatedPayloadBytes{0};
        SizeType allocatedBlockBytes{0};
        std::size_t freeBlocks{0};
        SizeType freePayloadBytes{0};
        SizeType freeBlockBytes{0};
        SizeType largestFreePayload{0};
        SizeType smallestFreePayload{std::numeric_limits<SizeType>::max ()};
        // Buckets: <=64, <=256, <=1024, <=4096, <=16384, >16384
        std::array<std::pair<std::size_t, TransactedFile::SizeType>, 6> freeSizeHistogram{};

        void Clear () {
            arenaSize = 0;
            totalBlocks = 0;
            pageStraddlingBlocks = 0;
            pageStraddlingPayloadBytes = 0;
            pageStraddlingBlockBytes = 0;
            allocatedBlocks = 0;
            allocatedPayloadBytes = 0;
            allocatedBlockBytes = 0;
            freeBlocks = 0;
            freePayloadBytes = 0;
            freeBlockBytes = 0;
            largestFreePayload = 0;
            smallestFreePayload = std::numeric_limits<SizeType>::max ();
            freeSizeHistogram[0] = {0, 0};
            freeSizeHistogram[1] = {0, 0};
            freeSizeHistogram[2] = {0, 0};
            freeSizeHistogram[3] = {0, 0};
            freeSizeHistogram[4] = {0, 0};
            freeSizeHistogram[5] = {0, 0};
        }

        f64 GetFragmentationRatio () const noexcept {
            return freePayloadBytes == 0 ?
                0.0 :
                1.0 - (static_cast<f64> (largestFreePayload) / static_cast<f64> (freePayloadBytes));
        }
    };
    static void GetStats (
        TransactedFile *file,
        Stats &stats);
};
