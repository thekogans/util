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

#if !defined (__thekogans_util_PageMap_h)
#define __thekogans_util_PageMap_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/RandomSeekSerializer.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/BlockAllocator.h"
#include "thekogans/util/AlignedAllocator.h"
#include "thekogans/util/SecureAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct PageMap PageMap.h thekogans/util/PageMap.h
        ///
        /// \brief
        ///
        /// Ex:
        ///
        /// DEFAULT_BITS_PER_OFFSET = 64
        /// DEFAULT_BITS_PER_SEGMENT = 32
        /// DEFAULT_BITS_PER_LEVEL = 8
        /// DEFAULT_BITS_PER_PAGE = 20
        ///
        /// |------------------------------------------- bitsPerOffset -----------------------------------------------|
        /// |                                                    |------------------ bitsPerSegment ------------------|
        /// +------------+---------------------------------------+-----------------------+----------------------------+
        /// |bitsPerLevel|                                       |                       |-------- bitsPerPage -------|
        /// +------------+---------------------------------------+-----------------------+----------------------------+
        /// 0            7                                      31                      45                           63
        ///
        /// This will cover the entire 64 bit address space with 4 levels of 4GB segments, each containing 4K of 1MB pages.
        ///
        /// levelCount = (bitsPerOffset - bitsPerSegment) / bitsPerLevel; (64 - 32) / 8 = 4
        /// nodesPerInternal = 1 << bitsPerLevel; 1 << 8 = 256
        /// pageSize = 1 << bitsPerPage; 1 << 20 = 1MB
        /// pagesPerSegment = 1 << (bitsPerSegment - bitsPerPage); 1 << (32 - 20) = 4K
        ///
        /// Ex:
        ///
        /// DEFAULT_BITS_PER_OFFSET = 128
        /// DEFAULT_BITS_PER_SEGMENT = 32
        /// DEFAULT_BITS_PER_LEVEL = 12
        /// DEFAULT_BITS_PER_PAGE = 22
        ///
        /// |--------------------------------...----------- bitsPerOffset -----------------------------------------------|
        /// |                                                       |------------------ bitsPerSegment ------------------|
        /// +------------+-------------------...--------------------+-----------------------+----------------------------+
        /// |bitsPerLevel|                                          |                       |-------- bitsPerPage -------|
        /// +------------+-------------------...--------------------+-----------------------+----------------------------+
        /// 0           11                                         95                     105                          127
        ///
        /// This will cover the entire 128 bit address space with 8 levels of 4GB segments, each containing 1K of 4MB pages.
        ///
        /// levelCount = (bitsPerOffset - bitsPerSegment) / bitsPerLevel; (128 - 32) / 12 = 8
        /// nodesPerInternal = 1 << bitsPerLevel; 1 << 12 = 4K
        /// pageSize = 1 << bitsPerPage; 1 << 22 = 4MB
        /// pagesPerSegment = 1 << (bitsPerSegment - bitsPerPage); 1 << (32 - 22) = 1K
        struct _LIB_THEKOGANS_UTIL_DECL PageMap {
            using BaseType = ui64;

            RandomSeekSerializer &serializer;
            const std::size_t bitsPerOffset;
            const std::size_t bitsPerSegment;
            const std::size_t bitsPerLevel;
            const std::size_t bitsPerPage;
            const std::size_t levelCount;
            const std::size_t nodesPerInternal;
            const std::size_t pageSize;
            const std::size_t pagesPerSegment;

        private:
            const std::size_t levelShift;
            const BaseType levelMask;
            const BaseType segmentMask;
            const std::size_t internalSize;
            const std::size_t segmentSize;
            BlockAllocator internalAllocator;
            BlockAllocator segmentAllocator;
            AlignedAllocator pageAllocator;

        public:
            /// \brief
            /// Forward declaration of \see{Page} needed by \see{PageList}.
            struct Page;

        private:
            /// \brief
            /// Alias for \see{IntrusiveList}<Page>.
            using PageList = IntrusiveList<Page>;

        public:
            /// \struct PageMap::Page PageMap.h thekogans/util/PageMap.h
            ///
            /// \brief
            /// Page tiles the address space providing incremental, sparse
            /// access to the data.
            struct Page :
                    public RefCounted,
                    public PageList::Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Page)
                /// \brief
                /// Page has a private heap.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                PageMap &pageMap;
                /// \brief
                /// Page index in \see{Segment::pages}.
                const std::size_t index;
                /// \brief
                /// Page offset.
                const BaseType offset;
                /// \brief
                /// Page data.
                ui8 *data;
                /// \brief
                /// true == modified.
                bool dirty;

                /// \brief
                /// ctor.
                /// \param[in] pageMap_ PageMap managing this Page.
                /// \param[in] index_ Page index in \see{Segment::pages}.
                /// \param[in] offset_ Page offset.
                Page (
                    PageMap &pageMap_,
                    std::size_t index_,
                    BaseType offset_);
                /// \brief
                /// dtor.
                virtual ~Page ();

                /// \brief
                /// Write dirty pages to log.
                /// \param[in] log \see{RandomSeekSerializer} to write to.
                void Log (RandomSeekSerializer &log);
                /// \brief
                /// Write dirty pages to their source.
                void Flush ();

                /// \brief
                /// Page is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Page)
            };

        private:
            /// \brief
            /// Forward declaration of \see{Node} needed by NodeList.
            struct Node;
            /// \brief
            /// Alias for \see{IntrusiveList}<Node>.
            using NodeList = IntrusiveList<Node>;

            /// \struct PageMap::Node PageMap.h thekogans/util/PageMap.h
            ///
            /// \brief
            struct Node :
                    public RefCounted,
                    public NodeList::Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Node)

                PageMap &pageMap;
                /// \brief
                /// Node index in \see{Internal::nodes}.
                std::size_t index;

                /// \brief
                /// ctor.
                /// \param[in] index_ Node index in \see{Internal::nodes}.
                Node (
                    PageMap &pageMap_,
                    std::size_t index_) :
                    pageMap (pageMap_),
                    index (index_) {}

                /// \brief
                /// Delete pages.
                /// \param[in] all true == clear all, false == dirty only.
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (bool all = false) = 0;
                /// \brief
                /// Write dirty pages to log.
                /// \param[in] log \see{RandomSeekSerializer} to write to.
                virtual void Log (RandomSeekSerializer &log) = 0;
                /// \brief
                /// Write dirty pages to their source.
                virtual void Flush () = 0;
                /// \brief
                /// Delete all pages whose offset > newSize.
                /// \param[in] newSize New size to clip the address space to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a page was encoutered whose offset was < newSize, stop iterating.
                virtual bool Shrink (BaseType newSize) = 0;
            };

            /// \struct PageMap::Segment PageMap.h thekogans/util/PageMap.h
            ///
            /// \brief
            /// Leaf node representing a 4GB chunk of the file.
            struct Segment : public Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Segment)

                /// \brief
                /// \see{Pages} tilling the segment.
                Page::SharedPtr *pages;
                /// \brief
                /// \see{IntrusiveList} of linked \see{Page}s.
                PageList pageList;

                /// \brief
                /// ctor.
                /// \param[in] pageMap
                /// \param[in] index Segment index in \see{Internal::nodes}.
                Segment (
                    PageMap &pageMap,
                    std::size_t index);
                /// \brief
                /// dtor.
                virtual ~Segment ();

                /// \brief
                /// Delete pages.
                /// \param[in] all true == clear all, false == dirty only.
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (bool all = false) override;
                /// \brief
                /// Write dirty pages to log.
                /// \param[in] log \see{RandomSeekSerializer} to write to.
                virtual void Log (RandomSeekSerializer &log) override;
                /// \brief
                /// Write dirty pages to their source.
                virtual void Flush () override;
                /// \brief
                /// Delete all pages whose offset > newSize.
                /// \param[in] newSize New size to clip the address space to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a page was encoutered whose offset was < newSize, stop iterating.
                virtual bool Shrink (BaseType newSize) override;

                /// \brief
                /// Return the \see{Page} @index. Create if null.
                /// \param[in] pageIndex Page index in the pages array.
                /// \param[in] pageOffset Page offset (multiple of pageSize).
                /// \param[in] serializer \see{RandomSeekSerializer} to read the page data from.
                /// \return The new page.
                Page::SharedPtr GetPage (
                    ui32 pageIndex,
                    BaseType pageOffset);

                virtual void Harakiri () override;

                static std::size_t Size (std::size_t pagesPerSegment);
                static Node *Alloc (
                    PageMap &pageMap,
                    std::size_t index);

                /// \brief
                /// Segment is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Segment)
            };

            /// \struct PageMap::Internal PageMap.h thekogans/util/PageMap.h
            ///
            /// \brief
            /// Internal structure node representing 4G of 4GB segments.
            struct Internal : public Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Internal)

                /// \brief
                /// These are the internal 4G of 4GB segments.
                Node::SharedPtr *nodes;
                /// \brief
                /// \see{IntrusiveList} of \see{Node}s.
                NodeList nodeList;

                /// \brief
                /// ctor.
                /// \param[in] index Internal index in nodes.
                Internal (
                    PageMap &pageMap,
                    std::size_t index);
                /// \brief
                /// dtor.
                virtual ~Internal ();

                /// \brief
                /// Delete pages.
                /// \param[in] all true == clear all, false == dirty only.
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (bool all = false) override;
                /// \brief
                /// Write dirty pages to log.
                /// \param[in] log \see{RandomSeekSerializer} to write to.
                virtual void Log (RandomSeekSerializer &log) override;
                /// \brief
                /// Write dirty pages to their source.
                virtual void Flush () override;
                /// \brief
                /// Delete all pages whose offset > newSize.
                /// \param[in] newSize New size to clip the address space to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a page was encoutered whose offset was < newSize, stop iterating.
                virtual bool Shrink (BaseType newSize) override;

                /// \brief
                /// Return (posibly creating) the \see{Page} that contains the given offset.
                /// \param[in] offset Offset of the page in the file.
                /// \return \see{Page} that contains the given offset.
                Page::SharedPtr GetPage (BaseType offset);

                /// \brief
                /// Return either an \see{Internal} scaffolding node
                /// or a \see{Segment} leaf node. Create if null.
                /// \param[in] index Index of node to return.
                /// \param[in] segment If null, true == create \see{Segment},
                /// otherwise create \see{Internal}
                /// \retrun \see{Segment} or \see{Internal} node @index.
                /// NOTE: Unlike pages which are shared outside the file api,
                /// nodes are used internally only by GetPage below which
                /// uses them and lets them go. It's therefore unnecessary
                /// overhead to return a SharedPtr. A raw pointer will do
                /// just fine.
                Node *GetNode (
                    std::size_t index,
                    bool segment = false);

                virtual void Harakiri () override;

                static std::size_t Size (std::size_t nodesPerInternal);
                static Node *Alloc (
                    PageMap &pageMap,
                    std::size_t index);

                /// \brief
                /// Internal is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Internal)
            } root;
            /// \brief
            /// Last accessed page offset.
            /// This is the page offset of the last call to \see{GetPage (offset)}.
            /// (offset & ~(pageSize - 1))
            BaseType lastGetPageOffset;
            /// \brief
            /// Last accessed page cache promoting locality of refernce.
            Page::SharedPtr lastGetPage;

        public:
            static const std::size_t DEFAULT_BITS_PER_OFFSET = 64;
            static const std::size_t DEFAULT_BITS_PER_SEGMENT = 32;
            static const std::size_t DEFAULT_BITS_PER_LEVEL = 8;
            static const std::size_t DEFAULT_BITS_PER_PAGE = 20;
            static const std::size_t DEFAULT_PAGE_ALIGNMENT = 4096;

            PageMap (
                RandomSeekSerializer &serializer_,
                std::size_t bitsPerOffset_ = DEFAULT_BITS_PER_OFFSET,
                std::size_t bitsPerSegment_ = DEFAULT_BITS_PER_SEGMENT,
                std::size_t bitsPerLevel_ = DEFAULT_BITS_PER_LEVEL,
                std::size_t bitsPerPage_ = DEFAULT_BITS_PER_PAGE,
                std::size_t pageAlignment = DEFAULT_PAGE_ALIGNMENT) :
                serializer (serializer_),
                bitsPerOffset (bitsPerOffset_),
                bitsPerSegment (bitsPerSegment_),
                bitsPerLevel (bitsPerLevel_),
                bitsPerPage (bitsPerPage_),
                levelCount ((bitsPerOffset - bitsPerSegment) / bitsPerLevel),
                nodesPerInternal (1 << bitsPerLevel),
                pageSize (1 << bitsPerPage),
                pagesPerSegment (1 << (bitsPerSegment - bitsPerPage)),
                levelShift (bitsPerOffset - bitsPerLevel),
                levelMask (BitMask (bitsPerLevel) << levelShift),
                segmentMask (BitMask (bitsPerSegment)),
                internalSize (Internal::Size (nodesPerInternal)),
                segmentSize (Segment::Size (pagesPerSegment)),
                internalAllocator (internalSize),
                segmentAllocator (segmentSize),
                pageAllocator (pageAlignment),
                root (*this, 0),
                lastGetPageOffset (NOFFS) {}

            /// \brief
            /// Return the \see{Page} that contains the given offset.
            /// \param[in] offset Offset whose page to return.
            /// \return \see{Page} that contains the given offset.
            Page::SharedPtr GetPage (BaseType offset);

            /// \brief
            /// Delete pages.
            /// \param[in] all true == clear all, false == dirty only.
            void Clear (bool all = false);
            /// \brief
            /// Write dirty pages to log.
            /// \param[in] log \see{RandomSeekSerializer} to write to.
            void Log (RandomSeekSerializer &log);
            /// \brief
            /// Write dirty pages to their source.
            void Flush ();
            /// \brief
            /// Delete all pages whose offset > newSize.
            /// \param[in] newSize New size to clip the address space to.
            void Shrink (BaseType newSize);

        private:
            static BaseType BitMask (std::size_t count) {
                BaseType mask = 0;
                while (count--) {
                    mask <<= 1;
                    ++mask;
                }
                return mask;
            }

            /// \brief
            /// PageMap is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (PageMap)
        };

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_PageMap_h
