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

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
    #endif // !defined (_WINDOWS_)
    #include <windows.h>
#else // defined (TOOLCHAIN_OS_Windows)
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <string>
#include "thekogans/util/internal.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/SharedAllocator.h"

namespace thekogans {
    namespace util {

        SharedAllocator::~SharedAllocator () {
        #if defined (TOOLCHAIN_OS_Windows)
            if (secure) {
                VirtualUnlock (base, (SIZE_T)size);
            }
            UnmapViewOfFile (base);
            CloseHandle (handle);
        #else // defined (TOOLCHAIN_OS_Windows)
            if (secure) {
                munlock (base, size);
            }
            munmap (base, size);
            close (handle);
            if (owner) {
                shm_unlink (name);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void *SharedAllocator::Alloc (std::size_t size) {
            if (size > 0) {
                LockGuard<Lock> guard (lock);
                if (size < Block::SMALLEST_BLOCK_SIZE) {
                    size = Block::SMALLEST_BLOCK_SIZE;
                }
                for (Block *prev = 0, *block = GetBlockFromOffset (header->freeList);
                        block != 0;
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
                        if (prev != 0) {
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
            return 0;
        }

        void SharedAllocator::Free (
                void *ptr,
                std::size_t size) {
            if (ptr != 0) {
                LockGuard<Lock> guard (lock);
                Block *blockToFree = ValidatePtr (ptr);
                if (blockToFree != 0) {
                    Block *prev = 0;
                    // The freeList chain is sorted on offset. Find the
                    // place in the chain where this block belongs and
                    // see if it needs to be coalesced to either or both
                    // of it's neighbors.
                    for (Block *block = GetBlockFromOffset (header->freeList);
                            block != 0;
                            prev = block, block = GetBlockFromOffset (block->next)) {
                        if (block > blockToFree) {
                            if (prev != 0) {
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
                    if (prev == 0) {
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

        THEKOGANS_UTIL_HANDLE SharedAllocator::CreateSharedRegion () {
            THEKOGANS_UTIL_HANDLE handle;
            if (name != 0 && size > 0) {
            #if defined (TOOLCHAIN_OS_Windows)
                handle = CreateFileMapping (
                    INVALID_HANDLE_VALUE, 0, PAGE_READWRITE,
                    THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (size, 0),
                    THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (size, 1), name);
                if (handle == 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                // ftruncate fails if there was an existing shared region.
                // Since we are being asked to create a new one, delete the
                // old one and recreate.
                handle = shm_open (name, O_RDWR, S_IRUSR | S_IWUSR);
                if (handle != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    close (handle);
                    shm_unlink (name);
                }
                handle = shm_open (name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
                if (handle != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    if (FTRUNCATE_FUNC (handle, size) == -1) {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        close (handle);
                        shm_unlink (name);
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
            return handle;
        }

        THEKOGANS_UTIL_HANDLE SharedAllocator::OpenSharedRegion () {
            THEKOGANS_UTIL_HANDLE handle;
            if (name != 0 && size > 0) {
            #if defined (TOOLCHAIN_OS_Windows)
                handle = OpenFileMapping (FILE_MAP_ALL_ACCESS, FALSE, name);
                // Don't you just love Microsoft and all their inconsistencies?
                if (handle == 0) {
            #else // defined (TOOLCHAIN_OS_Windows)
                handle = shm_open (name, O_RDWR, S_IRUSR | S_IWUSR);
                if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
            #endif // defined (TOOLCHAIN_OS_Windows)
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
            return handle;
        }

    #if !defined (TOOLCHAIN_OS_Windows)
        namespace {
            bool LockRegion (
                    ui8 *base,
                    ui64 size) {
                if (mlock (base, size) == 0) {
                #if defined (MADV_DONTDUMP)
                    if (madvise (base, size, MADV_DONTDUMP) == 0) {
                #endif // defined (MADV_DONTDUMP)
                        return true;
                #if defined (MADV_DONTDUMP)
                    }
                    else {
                        munlock (base, size);
                    }
                #endif // defined (MADV_DONTDUMP)
                }
                return false;
            }
        }
    #endif // !defined (TOOLCHAIN_OS_Windows)

        ui8 *SharedAllocator::MapSharedRegion () {
            ui8 *base = 0;
        #if defined (TOOLCHAIN_OS_Windows)
            base = (ui8 *)MapViewOfFile (handle, FILE_MAP_ALL_ACCESS, 0, 0, (SIZE_T)size);
            if (base == 0 || (secure && !VirtualLock (base, (SIZE_T)size))) {
                // Since we are throwing an exception during
                // initialization, our dtor will never be call.
                // Clean up the previously successful
                // CreateSharedRegion/OpenSharedRegion.
                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                if (base != 0) {
                    UnmapViewOfFile (base);
                }
                CloseHandle (handle);
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            base = (ui8 *)mmap (0, size, PROT_READ | PROT_WRITE, MAP_SHARED, handle, 0);
            if (base == MAP_FAILED || (secure && !LockRegion (base, size))) {
                // Since we are throwing an exception during
                // initialization, our dtor will never be call.
                // Clean up the previously successful
                // CreateSharedRegion/OpenSharedRegion.
                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                if (base != MAP_FAILED) {
                    munmap (base, size);
                }
                close (handle);
                if (owner) {
                    shm_unlink (name);
                }
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
            return base;
        }

    } // namespace util
} // namespace thekogans
