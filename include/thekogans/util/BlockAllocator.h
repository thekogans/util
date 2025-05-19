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

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/DefaultAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct BlockAllocator BlockAllocator.h thekogans/util/BlockAllocator.h
        ///
        /// \brief
        /// BlockAllocator is an adapter class. It takes a regular \see{Allocator} and
        /// turns it in to a fixed block allocator. Each block allocated by BlockAllocator
        /// is the same size. This makes BlockAllocator::Alloc and BlockAllocator::Free
        /// run in amortized O(1). Like all other allocators BlockAllocator is thread safe.
        /// BlockAllocator was created to expose the benefits of \see{Heap} to objects
        /// don't know their size at compile time. At run time when object size is known,
        /// use BlockAllocator::Pool to create/get a BlockAllocator for that object.

        struct _LIB_THEKOGANS_UTIL_DECL BlockAllocator : public Allocator {
            /// \brief
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (BlockAllocator)
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
                /// A convenient union to make dereferencing the structured data easier.
                union Block {
                    /// \brief
                    /// Pointer to next free block.
                    Block *next;
                    /// \brief
                    /// Block data.
                    ui8 block[1];
                /// \brief
                /// Pointer to first free block.
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

        public:
            /// \brief
            /// Minimum block size.
            static const std::size_t MIN_BLOCK_SIZE = sizeof (Page::Block);
            /// \brief
            /// Default number of blocks per page.
            static const std::size_t DEFAULT_BLOCKS_PER_PAGE = 256;

            /// \brief
            /// ctor.
            /// \param[in] blockSize_ Block size.
            /// \param[in] blocksPerPage_ Minimum blocks per page.
            /// \param[in] allocator_ \see{Allocator} used to allocate pages.
            BlockAllocator (
                std::size_t blockSize_,
                std::size_t blocksPerPage_ = DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ());
            /// \brief
            /// dtor.
            virtual ~BlockAllocator ();

            /// \brief
            inline std::size_t GetBlockSize () const {
                return blockSize;
            }
            /// \brief
            inline std::size_t GetBlocksPerPage () const {
                return blocksPerPage;
            }
            /// \brief
            inline Allocator::SharedPtr GetAllocator () const {
                return allocator;
            }

            /// \brief
            /// Return true if the given pointer is one of ours.
            /// \param[in] ptr Pointer to check.
            /// \return true == we own the pointer, false == the pointer is not one of ours.
            bool IsValidPtr (void *ptr) noexcept;

            /// \brief
            /// Allocate a block.
            /// \param[in] size Size of block to allocate (checked if <= blockSize).
            /// \return Pointer to the allocated block.
            virtual void *Alloc (std::size_t size) override;
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// NOTE: BlockAllocator policy is to do nothing if ptr == nullptr.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Size of block to free (checked if <= blockSize).
            virtual void Free (
                void *ptr,
                std::size_t size) override;

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
