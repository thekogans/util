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
#include "thekogans/util/Types.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/IntrusiveList.h"

namespace thekogans {
    namespace util {

        /// \struct BlockAllocator BlockAllocator.h thekogans/util/BlockAllocator.h
        ///
        /// \brief
        /// BlockAllocator is an adapter class. It takes a regular \see{Allocator} and
        /// turns it in to a fixed block allocator. Each block allocated by BlockAllocator
        /// is the same size. This makes BlockAllocator::Alloc and BlockAllocator::Free
        /// run in O(1). Like all other allocators BlockAllocator is thread safe.

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
            /// \struct BlockAllocator::Page BlockAllocator.h thekogans/util/BlockAllocator.h
            ///
            /// \brief
            /// Blocks are allocated from pages. Each page has the
            /// following layout:
            ///
            /// +-----------------------------------------------------+\n
            /// | Page data | Block 0 | ... | Block blocksPerPage - 1 |\n
            /// +-----------------------------------------------------+
            struct Page : public PageList::Node {
                /// \brief
                /// Block size.
                const std::size_t blockSize;
                /// \brief
                /// Number of blocks per page.
                const std::size_t blocksPerPage;
                /// \brief
                /// Number of allocated blocks.
                std::size_t blockCount;
                /// \union BlockAllocator::Page::Block BlockAllocator.h thekogans/util/BlockAllocator.h
                ///
                /// \brief
                union Block {
                    /// \brief
                    /// Pointer to next free block.
                    Block *next;
                    /// \brief
                    /// Block data.
                    ui8 block[1];
                /// \brief
                /// Pointer to first free block.a
                } *freeBlock;
                /// \brief
                /// Array of blocks following the page.
                ui8 blocks[1];

                /// \brief
                /// ctor.
                /// \param[in] blockSize_ Block size.
                /// \param[in] blocksPerPage_ Number of blocks per page.
                Page (std::size_t blockSize_,
                    std::size_t blocksPerPage_) :
                    blockSize (blockSize_),
                    blocksPerPage (blocksPerPage_),
                    blockCount (0),
                    freeBlock (nullptr) {}

                /// \brief
                /// Given a block size and blocks per page, return
                /// the size of the allocated page in bytes.
                /// \param[in] blockSize Block size.
                /// \param[in] blocksPerPage Number of blocks per page.
                /// \return Size of allocated page in bytes.
                static std::size_t Size (
                        std::size_t blockSize,
                        std::size_t blocksPerPage) {
                    return sizeof (Page) - sizeof (blocks) + blockSize * blocksPerPage;
                }

                /// \brief
                /// Return true if page is empty.
                /// \return true if page is empty.
                inline bool IsEmpty () const {
                    return blockCount == 0;
                }
                /// \brief
                /// Return true if page is full.
                /// \return true if page is full.
                inline bool IsFull () const {
                    return blockCount == blocksPerPage;
                }

                /// \brief
                /// Allocate a block.
                /// \return Pointer to block.
                void *Alloc ();
                /// \brief
                /// Free a previously allocated block.
                /// \param[in] ptr Previously allocated block.
                void Free (void *ptr);

                /// \brief
                /// Return true if the given pointer is one of ours.
                /// \param[in] ptr Pointer to check.
                /// \return true == we own the pointer, false == the pointer is not one of ours.
                bool IsValidPtr (void *ptr);
            };

            /// \brief
            /// Block size.
            const std::size_t blockSize;
            /// \brief
            /// Minimum blocks in page.
            std::size_t blocksPerPage;
            /// \brief
            /// Page allocator.
            Allocator::SharedPtr allocator;
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
                /// \brief
                /// Default number of blocks per page.
                DEFAULT_BLOCKS_PER_PAGE = 256
            };

            /// \brief
            /// ctor.
            /// \param[in] blockSize_ Block size.
            /// \param[in] blocksPerPage_ Minimum blocks per page.
            /// \param[in] allocator_ \see{Allocator} used to allocate pages.
            BlockAllocator (
                std::size_t blockSize_,
                std::size_t blocksPerPage_ = DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ()) :
                blockSize (blockSize_ >= sizeof (Page::Block) ? blockSize_ : sizeof (Page::Block)),
                blocksPerPage (blocksPerPage_ > 0 ? blocksPerPage_ : 1),
                allocator (allocator_ != nullptr ? allocator_ : DefaultAllocator::Instance ()) {}
            /// \brief
            /// dtor.
            virtual ~BlockAllocator ();

            /// \brief
            /// Return true if the given pointer is one of ours.
            /// \param[in] ptr Pointer to check.
            /// \return true == we own the pointer, false == the pointer is not one of ours.
            bool IsValidPtr (void *ptr) noexcept;

            /// \brief
            /// Allocate a block.
            /// \param[in] size Size of block to allocate (ignored).
            /// \return Pointer to the allocated block.
            virtual void *Alloc (std::size_t /*size*/) override;
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// NOTE: BlockAllocator policy is to do nothing if ptr == nullptr.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Size of block to free (ignored).
            virtual void Free (
                void *ptr,
                std::size_t /*size*/) override;

            /// \struct BlockAllocator::Pool BlockAllocator.h thekogans/util/BlockAllocator.h
            ///
            /// \brief
            /// Use Pool to recycle and reuse block allocators.
            struct _LIB_THEKOGANS_UTIL_DECL Pool : public Singleton<Pool> {
            private:
                /// \brief
                /// BlockAllocator map type (keyed on block size).
                using Map = std::map<std::size_t, Allocator::SharedPtr>;
                /// \brief
                /// BlockAllocator map.
                Map map;
                /// \brief
                /// Synchronization lock.
                SpinLock spinLock;

            public:
                /// \brief
                /// Given a block size, return a matching block allocator.
                /// If we don't have one, create it.
                /// \param[in] blockSize Block size.
                /// \param[in] blocksPerPage Minimum blocks per page.
                /// \param[in] allocator \see{Allocator} used to allocate pages.
                /// \return BlockAllocator matching the given block size.
                Allocator::SharedPtr GetBlockAllocator (
                    std::size_t blockSize,
                    std::size_t blocksPerPage = DEFAULT_BLOCKS_PER_PAGE,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            };

        private:
            /// \brief
            /// Return first partially allocated page (presumably for allocation).
            /// If no partially allocated pages left, allocate a new one.
            /// \return Pointer to partialPages.head.
            Page *GetPage ();
            /// \brief
            /// Get the Page to which a given pointer belongs.
            /// \param[in] ptr Pointer whose page we are asked to return.
            /// \return Page for a given pointer, or nullptr if the pointer is not ours.
            Page *GetPage (void *ptr) const;

            /// \brief
            /// BlockAllocator is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BlockAllocator)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_BlockAllocator_h)
