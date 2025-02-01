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

#if !defined (__thekogans_util_BlockAllocator_h)
#define __thekogans_util_BlockAllocator_h

#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/IntrusiveList.h"

namespace thekogans {
    namespace util {

        /// \struct BlockAllocator BlockAllocator.h thekogans/util/BlockAllocator.h
        ///
        /// \brief
        /// BlockAllocator

        struct _LIB_THEKOGANS_UTIL_DECL BlockAllocator : public Allocator {
            /// \brief
            /// Declare \see{DynamicCreatable} boilerplate.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE (BlockAllocator)

        private:
            /// \brief
            /// Forward declaration of Page.
            struct Page;
            /// \brief
            /// Alias for IntrusiveList<Page>.
            using PageList = IntrusiveList<Page>;
            struct Page : public PageList::Node {
                const std::size_t blockSize;
                const std::size_t blocksPerPage;
                std::size_t blockCount;
                union Block {
                    Block *next;
                    ui8 block[1];
                } *freeBlock;
                ui8 blocks[1];

                Page (std::size_t blockSize_,
                    std::size_t blocksPerPage_) :
                    blockSize (blockSize_),
                    blocksPerPage (blocksPerPage_),
                    blockCount (0),
                    freeBlock (nullptr) {}

                static std::size_t Size (
                        std::size_t blockSize,
                        std::size_t blocksPerPage) {
                    return sizeof (Page) - sizeof (blocks) + blockSize * blocksPerPage;
                }

                inline bool IsEmpty () const {
                    return blockCount == 0;
                }
                inline bool IsFull () const {
                    return blockCount == blocksPerPage;
                }

                inline void *Alloc () {
                    if (freeBlock != nullptr) {
                        Block *block = freeBlock;
                        freeBlock = freeBlock->next;
                        ++blockCount;
                        return block->block;
                    }
                    else if (!IsFull ()) {
                        return blocks + blockSize * blockCount++;
                    }
                    // We are allocating from a full page!
                    assert (0);
                    return 0;
                }

                inline void Free (void *ptr) {
                    Block *block = (Block *)ptr;
                    block->next = freeBlock;
                    freeBlock = block;
                    --blockCount;
                }

                inline bool IsValidPtr (void *ptr) {
                    Block *block = (Block *)ptr;
                    return
                        // Verify that the given pointer points to the
                        // beginning of an block.
                        block->block >= blocks && block->block < blocks + blocksPerPage * blockSize &&
                        (std::ptrdiff_t)((const std::size_t)block->block -
                            (const std::size_t)blocks) % blockSize == 0;
                }
            };

            /// \brief
            /// Bloc size.
            const std::size_t blockSize;
            /// \brief
            /// BlockAllocator minimum blocks in page.
            std::size_t blocksPerPage;
            /// \brief
            /// Pages need to be aligned on a page size boundary.
            Allocator::SharedPtr allocator;
            /// \brief
            /// Current number of blocks on the heap.
            std::size_t blockCount;
            /// \brief
            /// Full pages.
            PageList fullPages;
            /// \brief
            /// Partial pages.
            PageList partialPages;
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            enum {
                DEFAULT_BLOCKS_PER_PAGE = 256
            };

            /// \brief
            /// ctor.
            /// \param[in] blocksPerPage_ BlockAllocator minimum blocks in page.
            /// NOTE: The heap uses an \see{AlignedAllocator} to allocate its pages.
            /// To maximize memory efficiency, any given page may contain
            /// more or less blocks then any other (depends on alignment. See
            /// AlignedAllocator.h). Therefore you cannot dictate the exact
            /// count of blocks per page, only the minimum.
            /// \param[in] allocator_ Page allocator.
            BlockAllocator (
                std::size_t blockSize_,
                std::size_t blocksPerPage_ = DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ());
            /// \brief
            /// dtor. Remove the heap from the registrty.
            virtual ~BlockAllocator ();

            /// \brief
            /// Return true if the given pointer is one of ours.
            /// \param[in] ptr Pointer to check.
            /// \return true == we own the pointer, false == the pointer is not one of ours.
            bool IsValidPtr (void *ptr) noexcept;

            /// \brief
            /// Allocate a block.
            /// NOTE: BlockAllocator policy is to return nullptr if size == 0.
            /// if size > 0 and an error occurs, BlockAllocator will throw an exception.
            /// \param[in] size Size of block to allocate.
            /// \return Pointer to the allocated block (nullptr if size == 0).
            virtual void *Alloc (std::size_t /*size*/) override;
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// NOTE: BlockAllocator policy is to do nothing if ptr == nullptr.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Same size parameter previously passed in to Alloc.
            virtual void Free (
                void *ptr,
                std::size_t /*size*/) override;

            struct _LIB_THEKOGANS_UTIL_DECL Pool : public Singleton<Pool> {
            private:
                using Map = std::map<std::size_t, Allocator::SharedPtr>;
                Map map;
                SpinLock spinLock;

            public:
                Allocator::SharedPtr GetBlockAllocator (
                    std::size_t blockSize,
                    std::size_t blocksPerPage = DEFAULT_BLOCKS_PER_PAGE,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            };

        private:
            /// \brief
            /// Return first partially allocated page (presumably for allocation).
            /// If no partially allocated pages left, allocate a new one.
            /// \return Pointer to partialPages.head
            Page *GetPage ();
            /// \brief
            /// Get the Page to which a given pointer belongs.
            /// \param[in] ptr Pointer whose page we are asked to return.
            /// \return Page for a given pointer, or 0 if the pointer is not ours.
            Page *GetPage (void *ptr) const;

            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BlockAllocator)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_BlockAllocator_h)
