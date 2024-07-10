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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#else // defined (TOOLCHAIN_OS_Windows)
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <string>
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/SharedAllocator.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (SharedAllocator)

        void *SharedAllocator::Alloc (std::size_t size) {
            if (size > 0) {
                LockGuard<StorageSpinLock> guard (lock);
                if (size < Block::SMALLEST_BLOCK_SIZE) {
                    size = Block::SMALLEST_BLOCK_SIZE;
                }
                for (Block *prev = nullptr, *block = GetBlockFromOffset (header->freeList);
                        block != nullptr;
                        prev = block, block = GetBlockFromOffset (block->next)) {
                    if (block->size >= size) {
                        ui64 remainder = block->size - size;
                        Block *freeBlock;
                        if (remainder >= Block::FREE_BLOCK_SIZE) {
                            freeBlock = new (block->data + size) Block (remainder, block->next);
                            block->size = size;
                        }
                        else {
                            freeBlock = GetBlockFromOffset (block->next);
                        }
                        if (prev != nullptr) {
                            prev->next = GetOffsetFromBlock (freeBlock);
                        }
                        else {
                            header->freeList = GetOffsetFromBlock (freeBlock);
                        }
                        return block->data;
                    }
                }
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_ENOMEM);
            }
            return nullptr;
        }

        void SharedAllocator::Free (
                void *ptr,
                std::size_t size) {
            if (ptr != nullptr) {
                LockGuard<StorageSpinLock> guard (lock);
                Block *blockToFree = ValidatePtr (ptr);
                if (blockToFree != nullptr) {
                    Block *prev = nullptr;
                    // The freeList chain is sorted on offset. Find the
                    // place in the chain where this block belongs and
                    // see if it needs to be coalesced to either or both
                    // of it's neighbors.
                    for (Block *block = GetBlockFromOffset (header->freeList);
                            block != nullptr;
                            prev = block, block = GetBlockFromOffset (block->next)) {
                        if (block > blockToFree) {
                            if (prev != nullptr) {
                                if (GetNextBlock (prev) == blockToFree) {
                                    prev->size += GetTrueBlockSize (blockToFree);
                                    if (GetNextBlock (blockToFree) == block) {
                                        prev->next = block->next;
                                        prev->size += GetTrueBlockSize (block);
                                    }
                                }
                                else if (GetNextBlock (blockToFree) == block) {
                                    prev->next = GetOffsetFromBlock (blockToFree);
                                    blockToFree->next = block->next;
                                    blockToFree->size += GetTrueBlockSize (block);
                                }
                                else {
                                    prev->next = GetOffsetFromBlock (blockToFree);
                                    blockToFree->next = GetOffsetFromBlock (block);
                                }
                            }
                            else {
                                header->freeList = GetOffsetFromBlock (blockToFree);
                                if (GetNextBlock (blockToFree) == block) {
                                    blockToFree->next = block->next;
                                    blockToFree->size += GetTrueBlockSize (block);
                                }
                                else {
                                    blockToFree->next = GetOffsetFromBlock (block);
                                }
                            }
                            return;
                        }
                    }
                    if (prev == nullptr) {
                        // First and only block in the list.
                        header->freeList = GetOffsetFromBlock (blockToFree);
                        blockToFree->next = 0;
                    }
                    else {
                        // Last block in the list.
                        if (GetNextBlock (prev) == blockToFree) {
                            prev->size += GetTrueBlockSize (blockToFree);
                        }
                        else {
                            prev->next = GetOffsetFromBlock (blockToFree);
                            blockToFree->next = 0;
                        }
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
        }

        void SharedAllocator::SetRootObject (void *rootObject) {
            if (rootObject == nullptr || ValidatePtr (rootObject) != nullptr) {
                LockGuard<StorageSpinLock> guard (lock);
                header->rootObject = rootObject != nullptr ? GetOffsetFromPtr (rootObject) : 0;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void *SharedAllocator::GetRootObject () {
            LockGuard<StorageSpinLock> guard (lock);
            return header->rootObject != 0 ? GetPtrFromOffset (header->rootObject) : nullptr;
        }

    } // namespace util
} // namespace thekogans
