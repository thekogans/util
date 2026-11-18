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

    static constexpr const char * const errorStrings[] = {
        "ERROR_NONE",
        "ERROR_FREE_BLOCK_0",
        "ERROR_TWO_CONSECUTIVE_FREE_BLOCKS",
        "ERROR_GARBAGE_IN_PAYLOAD",
        "ERROR_FREE_LAST_BLOCK",
        "ERROR_FREE_COUNT_MISMATCH",
        "ERROR_BLOCK_MISSING_FROM_FREE_LIST",
        "ERROR_ORPHAN_BLOCK"
    };

    using Callback = std::function<bool (const Block &block, std::size_t errorCode)>;
    void ValidateHeap (
        TransactedFile *file,
        Callback callback);
    void CheckForOrphans (
        TransactedFile *file,
        Callback callback);
};
