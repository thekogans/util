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
#include "thekogans/util/RandomSeekSerializer.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/BlockAllocator.h"
#include "thekogans/util/AlignedAllocator.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        /// \struct PageMap PageMap.h thekogans/util/PageMap.h
        ///
        /// \brief
        /// PageMap gives you the ability to work with virtual address spacess, potentialy
        /// astronomical (10^19, 10^38...) in size, given the somewhat limited resources
        /// of today's and tomorrow's hosts. PageMap divides the given address space in to
        /// sequential, contiguous, nonoverlapping pages. The pages are maintained in groups
        /// called segments. Segments are organized as leaf nodes in a fixed depth, mutiway
        /// tree. Given a particular parameterization of an address space, PageMap will
        /// calculate all other parameters needed for efficient tree traversal. PageMap's
        /// entire reason for being, as far as the user is concerned, is to provide on demand
        /// access to pages given any valid address space address (GetPage). It does this by
        /// lazily wiring in pages as the need arises. Given an address, PageMap walks the
        /// tree looking for the correponding page. That tree walk is constant for every page
        /// and is dependent on the particular parameterization of the address space. As pages
        /// accumulate, eventually memory will become an issue. PageMap provides an API to
        /// maintain internal page cache (Clear). PageMap maintains a cache of last accessed
        /// page (lastGetPagePage), promoting locality of reference by optimizing away tree
        /// walks for requests with sufficiently close addresses. Pages maintain a dirty flag
        /// which is used by the Log and Flush methods to move pages back and forth to and from
        /// the bitSource. Finally, Shrink is used to clip pages outside the new address space
        /// size. With just three simple knobs (bitsPerSegment, bitsPerLevel and bitsPerPage)
        /// you can tune the address space very precisely given your particular performance
        /// requirements. Specifying exactly how many levels deep to make the multiway tree,
        /// which directly affects tree walk performance, and by extension GetPage performance.
        ///
        /// Ex:
        ///
        /// AddressType = ui32
        /// bitsPerAddress = 32
        /// bitsPerSegment = 20
        /// bitsPerLevel = 6
        /// bitsPerPage = 16
        ///
        /// msb                                                                        lsb
        /// |----------------------------- bitsPerAddress ------------------------------|
        /// |                         |----------------- bitsPerSegment ----------------|
        /// +------------+------------+-----------+-------------------------------------+
        /// |bitsPerLevel|            |           |------------ bitsPerPage ------------|
        /// +------------+------------+-----------+-------------------------------------+
        /// 31           26           19          15                                    0
        ///
        /// levelCount = (bitsPerAddress - bitsPerSegment) / bitsPerLevel; (32 - 20) / 6 = 2
        /// nodesPerInternal = 1 << bitsPerLevel; 1 << 6 = 64
        /// pageSize = 1 << bitsPerPage; 1 << 16 = 64KB
        /// pagesPerSegment = 1 << (bitsPerSegment - bitsPerPage); 1 << (20 - 16) = 16
        ///
        /// This will cover the entire 32 bit address space with 2 levels of 1MB segments,
        /// each containing 16 64KB pages.
        ///
        /// Ex:
        ///
        /// AddressType = ui64
        /// bitsPerAddress = 64
        /// bitsPerSegment = 32
        /// bitsPerLevel = 8
        /// bitsPerPage = 20
        ///
        /// msb                                                                                                      lsb
        /// |------------------------------------------- bitsPerAddress ----------------------------------------------|
        /// |                                                    |------------------ bitsPerSegment ------------------|
        /// +------------+---------------------------------------+---------------------+------------------------------+
        /// |bitsPerLevel|                                       |                     |--------- bitsPerPage --------|
        /// +------------+---------------------------------------+---------------------+------------------------------+
        /// 63           56                                      31                    19                             0
        ///
        /// levelCount = (bitsPerAddress - bitsPerSegment) / bitsPerLevel; (64 - 32) / 8 = 4
        /// nodesPerInternal = 1 << bitsPerLevel; 1 << 8 = 256
        /// pageSize = 1 << bitsPerPage; 1 << 20 = 1MB
        /// pagesPerSegment = 1 << (bitsPerSegment - bitsPerPage); 1 << (32 - 20) = 4K
        ///
        /// This will cover the entire 64 bit address space with 4 levels of 4GB segments,
        /// each containing 4K of 1MB pages.
        ///
        /// Ex:
        ///
        /// AddressType = ui64
        /// bitsPerAddress = 64
        /// bitsPerSegment = 34
        /// bitsPerLevel = 10
        /// bitsPerPage = 22
        ///
        /// msb                                                                                                      lsb
        /// |------------------------------------------- bitsPerAddress ----------------------------------------------|
        /// |                                                |-------------------- bitsPerSegment --------------------|
        /// +------------+-----------------------------------+---------------------+----------------------------------+
        /// |bitsPerLevel|                                   |                     |----------- bitsPerPage ----------|
        /// +------------+-----------------------------------+---------------------+----------------------------------+
        /// 63           54                                  33                    21                                 0
        ///
        /// levelCount = (bitsPerAddress - bitsPerSegment) / bitsPerLevel; (64 - 34) / 10 = 3
        /// nodesPerInternal = 1 << bitsPerLevel; 1 << 10 = 1K
        /// pageSize = 1 << bitsPerPage; 1 << 22 = 4MB
        /// pagesPerSegment = 1 << (bitsPerSegment - bitsPerPage); 1 << (34 - 22) = 4K
        ///
        /// This will cover the entire 64 bit address space with 3 levels of 16GB segments,
        /// each containing 4K of 4MB pages.
        ///
        /// Ex:
        ///
        /// AddressType = ui128
        /// bitsPerAddress = 128
        /// bitsPerSegment = 32
        /// bitsPerLevel = 12
        /// bitsPerPage = 22
        ///
        /// msb                                                                                                         lsb
        /// |--------------------------------...----------- bitsPerAddress ----------------------------------------------|
        /// |                                                       |------------------ bitsPerSegment ------------------|
        /// +------------+-------------------...--------------------+------------------+---------------------------------+
        /// |bitsPerLevel|                                          |                  |---------- bitsPerPage ----------|
        /// +------------+-------------------...--------------------+------------------+---------------------------------+
        /// 127          116                                        31                 21                                0
        ///
        /// levelCount = (bitsPerAddress - bitsPerSegment) / bitsPerLevel; (128 - 32) / 12 = 8
        /// nodesPerInternal = 1 << bitsPerLevel; 1 << 12 = 4K
        /// pageSize = 1 << bitsPerPage; 1 << 22 = 4MB
        /// pagesPerSegment = 1 << (bitsPerSegment - bitsPerPage); 1 << (32 - 22) = 1K
        ///
        /// This will cover the entire 128 bit address space with 8 levels of 4GB segments,
        /// each containing 1K of 4MB pages.
        template<typename T>
        struct PageMap : public RefCounted {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (PageMap)

            using AddressType = T;

        private:
            RandomSeekSerializer &bitSource;
            const std::size_t bitsPerAddress;
            const std::size_t bitsPerSegment;
            const std::size_t bitsPerLevel;
            const std::size_t bitsPerPage;
            const std::size_t levelCount;
            const std::size_t nodesPerInternal;
            const std::size_t pageSize;
            const std::size_t pagesPerSegment;
            const std::size_t internalSize;
            const std::size_t segmentSize;
            const std::size_t levelShift;
            const AddressType levelMask;
            const AddressType segmentMask;
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
            struct Segment;

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

                /// \brief
                /// PageMap to which this page belongs.
                PageMap &pageMap;
                /// \brief
                /// Page index in \see{Segment::pages}.
                const std::size_t index;
                /// \brief
                /// Page offset.
                const AddressType offset;
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
                        AddressType offset_) :
                        pageMap (pageMap_),
                        index (index_),
                        offset (offset_),
                        data ((ui8 *)pageMap.pageAllocator.Alloc (pageMap.pageSize)),
                        dirty (false) {
                    pageMap.bitSource.Seek (offset, SEEK_SET);
                    std::size_t countRead = pageMap.bitSource.Read (data, pageMap.pageSize);
                    SecureZeroMemory (data + countRead, pageMap.pageSize - countRead);
                }
                /// \brief
                /// dtor.
                virtual ~Page () {
                    pageMap.pageAllocator.Free (data, pageMap.pageSize);
                }

            private:
                /// \brief
                /// If dirty, write page to log.
                /// \param[in] log \see{RandomSeekSerializer} to write to.
                void Log (RandomSeekSerializer &log) {
                    if (dirty) {
                        log << offset;
                        log.Write (data, pageMap.pageSize);
                        dirty = false;
                    }
                }
                /// \brief
                /// If dirty, write page to it's source.
                void Flush () {
                    if (dirty) {
                        pageMap.bitSource.Seek (offset, SEEK_SET);
                        pageMap.bitSource.Write (data, pageMap.pageSize);
                        dirty = false;
                    }
                }
                /// \brief
                /// Clip the page to the new size.
                /// \param[in] newize Size to clip the page to.
                /// \return true == the page was completely clipped.
                /// false == the page was partially clipped.
                bool Shrink (AddressType newSize) {
                    if (offset < newSize) {
                        AddressType consumed = newSize - offset;
                        if (consumed < pageMap.pageSize) {
                            // Pages don't maintain internal lengths. All pages are
                            // pageMap.pageSize long (with potentially the last one
                            // being less). If this is the last page, we clear that
                            // part which falls outside the new address space size.
                            SecureZeroMemory (data + consumed, pageMap.pageSize - consumed);
                            dirty = true;
                        }
                        return false;
                    }
                    return true;
                }

                /// \brief
                /// Needs access to private methods.
                friend struct Segment;

                /// \brief
                /// Page is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Page)
            };

            static const std::size_t FLAGS_CLEAR_DIRTY = 1;
            static const std::size_t FLAGS_CLEAR_CLEAN = 2;

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
            struct Node : public NodeList::Node {
                /// \brief
                /// \see{PageMap} this node belongs to.
                PageMap &pageMap;
                /// \brief
                /// Node index in \see{Internal::nodes}.
                std::size_t index;

                /// \brief
                /// ctor.
                /// \param[in] pageMap_ \see{PageMap} this node belongs to.
                /// \param[in] index_ Node index in \see{Internal::nodes}.
                Node (
                    PageMap &pageMap_,
                    std::size_t index_) :
                    pageMap (pageMap_),
                    index (index_) {}

                /// \brief
                /// Delete pages.
                /// \param[in] flags
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (std::size_t flags) = 0;
                /// \brief
                /// Write dirty pages to log.
                /// \param[in] log \see{RandomSeekSerializer} to write to.
                virtual void Log (
                    RandomSeekSerializer &log,
                    bool clearCache = false) = 0;
                /// \brief
                /// Write dirty pages to their source.
                virtual void Flush (bool clearCache = false) = 0;
                /// \brief
                /// Delete all pages whose offset > newSize.
                /// \param[in] newSize New size to clip the address space to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a page was encoutered whose offset was < newSize, stop iterating.
                virtual bool Shrink (AddressType newSize) = 0;

                virtual void Harakiri () = 0;
            };

            /// \struct PageMap::Segment PageMap.h thekogans/util/PageMap.h
            ///
            /// \brief
            /// Leaf node organizing address space \see{Page}s.
            struct Segment : public Node {
                /// \brief
                /// An array of \see{Page}s tilling the segment.
                Page **pages;
                /// \brief
                /// \see{IntrusiveList} of linked \see{Page}s.
                PageList pageList;

                /// \brief
                /// ctor.
                /// \param[in] pageMap
                /// \param[in] index Segment index in \see{Internal::nodes}.
                Segment (
                        PageMap &pageMap,
                        std::size_t index) :
                        Node (pageMap, index),
                        pages ((Page **)(this + 1)) {
                    SecureZeroMemory (pages, pageMap.pagesPerSegment * sizeof (Page *));
                }
                /// \brief
                /// dtor.
                virtual ~Segment () {
                    pageList.for_each (
                        [] (typename PageList::Callback::argument_type page) ->
                                typename PageList::Callback::result_type {
                            page->Release ();
                            return true;
                        }
                    );
                }

                /// \brief
                /// Delete pages.
                /// \param[in] flags
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (std::size_t flags) override {
                    pageList.for_each (
                        [this, flags] (typename PageList::Callback::argument_type page) ->
                                typename PageList::Callback::result_type {
                            if (((flags & FLAGS_CLEAR_DIRTY) && page->dirty) ||
                                    ((flags & FLAGS_CLEAR_CLEAN) && !page->dirty)) {
                                DeletePage (page);
                            }
                            return true;
                        }
                    );
                    return pageList.empty ();
                }
                /// \brief
                /// Write dirty pages to log.
                /// \param[in] log \see{RandomSeekSerializer} to write to.
                virtual void Log (
                        RandomSeekSerializer &log,
                        bool clearCache = false) override {
                    pageList.for_each (
                        [this, &log, clearCache] (typename PageList::Callback::argument_type page) ->
                                typename PageList::Callback::result_type {
                            page->Log (log);
                            if (clearCache) {
                                DeletePage (page);
                            }
                            return true;
                        }
                    );
                }
                /// \brief
                /// Write dirty pages to their source.
                virtual void Flush (bool clearCache = false) override {
                    pageList.for_each (
                        [this, clearCache] (typename PageList::Callback::argument_type page) ->
                                typename PageList::Callback::result_type {
                            page->Flush ();
                            if (clearCache) {
                                DeletePage (page);
                            }
                            return true;
                        }
                    );
                }
                /// \brief
                /// Delete all pages whose offset > newSize.
                /// \param[in] newSize New size to clip the address space to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a page was encoutered whose offset was < newSize, stop iterating.
                virtual bool Shrink (AddressType newSize) override {
                    pageList.for_each (
                        [this, newSize] (typename PageList::Callback::argument_type page) ->
                                typename PageList::Callback::result_type {
                            if (page->Shrink (newSize)) {
                                DeletePage (page);
                                return true;
                            }
                            return false;
                        },
                        true
                    );
                    return pageList.empty ();
                }

                virtual void Harakiri () override {
                    this->~Segment ();
                    this->pageMap.segmentAllocator.Free (this, this->pageMap.segmentSize);
                }

                /// \brief
                /// Return the \see{Page} @index. Create if null.
                /// \param[in] pageIndex Page index in the pages array.
                /// \param[in] pageOffset Page offset (multiple of pageSize).
                /// \return The new page.
                Page *GetPage (
                        ui32 pageIndex,
                        AddressType pageOffset) {
                    if (pages[pageIndex] == nullptr) {
                        // We don't align the page boundary as it's a fairly complex
                        // structure with internal machinery that's hidden from view
                        // (vptr tables...).
                        Page *page = new Page (this->pageMap, pageIndex, pageOffset);
                        page->AddRef ();
                        pages[pageIndex] = page;
                        // Insert the new page in to the ordered (on index) page list.
                        // A quick optimization to check if it's the first or last page
                        // potentially saving us a list walk...
                        if (pageList.empty () || pageList.tail->index < page->index) {
                            // ...it is. First or last is the same push_back.
                            pageList.push_back (page);
                        }
                        else {
                            // ...otherwise walk the list. The page will go in the middle somewhere.
                            pageList.for_each (
                                [this, page] (typename PageList::Callback::argument_type page_) ->
                                        typename PageList::Callback::result_type {
                                    if (page_->index > page->index) {
                                        pageList.insert (page, page_);
                                        return false;
                                    }
                                    return true;
                                }
                            );
                        }
                    }
                    return pages[pageIndex];
                }

                static std::size_t Size (std::size_t pagesPerSegment) {
                    return sizeof (Segment) + pagesPerSegment * sizeof (Page *);
                }
                static Node *Alloc (
                        PageMap &pageMap,
                        std::size_t index) {
                    return new (
                        pageMap.segmentAllocator.Alloc (
                            pageMap.segmentSize)) Segment (pageMap, index);
                }

            private:
                void DeletePage (Page *page) {
                    pages[page->index] = nullptr;
                    pageList.erase (page);
                    page->Release ();
                }

                /// \brief
                /// Segment is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Segment)
            };

            /// \struct PageMap::Internal PageMap.h thekogans/util/PageMap.h
            ///
            /// \brief
            /// Internal structure node.
            struct Internal : public Node {
                /// \brief
                /// Child nodes.
                Node **nodes;
                /// \brief
                /// \see{IntrusiveList} of \see{Node}s.
                NodeList nodeList;

                /// \brief
                /// ctor.
                /// \param[in] index Internal index in nodes.
                Internal (
                        PageMap &pageMap,
                        std::size_t index) :
                        Node (pageMap, index),
                        nodes ((Node **)(this + 1)) {
                    SecureZeroMemory (nodes, pageMap.nodesPerInternal * sizeof (Node *));
                }
                /// \brief
                /// dtor.
                virtual ~Internal () {
                    nodeList.for_each (
                        [] (typename NodeList::Callback::argument_type node) ->
                                typename NodeList::Callback::result_type {
                            node->Harakiri ();
                            return true;
                        }
                    );
                }

                /// \brief
                /// Delete pages.
                /// \param[in] flags
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (std::size_t flags) override {
                    nodeList.for_each (
                        [this, flags] (typename NodeList::Callback::argument_type node) ->
                                typename NodeList::Callback::result_type {
                            if (node->Clear (flags)) {
                                DeleteNode (node);
                            }
                            return true;
                        }
                    );
                    return nodeList.empty ();
                }
                /// \brief
                /// Write dirty pages to log.
                /// \param[in] log \see{RandomSeekSerializer} to write to.
                /// \param[in] clearCache true == Delete the page cache after.
                virtual void Log (
                        RandomSeekSerializer &log,
                        bool clearCache = false) override {
                    nodeList.for_each (
                        [this, &log, clearCache] (typename NodeList::Callback::argument_type node) ->
                                typename NodeList::Callback::result_type {
                            node->Log (log, clearCache);
                            if (clearCache) {
                                DeleteNode (node);
                            }
                            return true;
                        }
                    );
                }
                /// \brief
                /// Write dirty pages to bitSource.
                /// \param[in] clearCache true == Delete the page cache after.
                virtual void Flush (bool clearCache = false) override {
                    nodeList.for_each (
                        [this, clearCache] (typename NodeList::Callback::argument_type node) ->
                                typename NodeList::Callback::result_type {
                            node->Flush (clearCache);
                            if (clearCache) {
                                DeleteNode (node);
                            }
                            return true;
                        }
                    );
                }
                /// \brief
                /// Delete all pages whose offset > newSize.
                /// \param[in] newSize New size to clip the address space to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a page was encoutered whose offset was < newSize, stop iterating.
                virtual bool Shrink (AddressType newSize) override {
                    nodeList.for_each (
                        [this, newSize] (typename NodeList::Callback::argument_type node) ->
                                typename NodeList::Callback::result_type {
                            if (node->Shrink (newSize)) {
                                DeleteNode (node);
                                return true;
                            }
                            return false;
                        },
                        true
                    );
                    return nodeList.empty ();
                }

                virtual void Harakiri () override {
                    this->~Internal ();
                    this->pageMap.internalAllocator.Free (this, this->pageMap.internalSize);
                }

                /// \brief
                /// Return either an \see{Internal} scaffolding node
                /// or a \see{Segment} leaf node. Create if null.
                /// \param[in] index Index of node to return.
                /// \param[in] segment If null, true == create \see{Segment},
                /// otherwise create \see{Internal}
                /// \retrun \see{Segment} or \see{Internal} node @index.
                /// NOTE: Unlike pages which are shared outside the PageMap
                /// api, nodes are used internally only by GetPage which
                /// uses them and lets them go. It's therefore unnecessary
                /// overhead to return a SharedPtr. A raw pointer will do
                /// just fine.
                Node *GetNode (
                        std::size_t index,
                        bool segment = false) {
                    if (nodes[index] == nullptr) {
                        Node *node = segment ?
                            Segment::Alloc (this->pageMap, index) :
                            Internal::Alloc (this->pageMap, index);
                        nodes[index] = node;
                        if (nodeList.empty () || nodeList.tail->index < node->index) {
                            nodeList.push_back (node);
                        }
                        else {
                            nodeList.for_each (
                                [this, node] (typename NodeList::Callback::argument_type node_) ->
                                        typename NodeList::Callback::result_type {
                                    if (node_->index > node->index) {
                                        nodeList.insert (node, node_);
                                        return false;
                                    }
                                    return true;
                                }
                            );
                        }
                    }
                    return nodes[index];
                }

                static std::size_t Size (std::size_t nodesPerInternal) {
                    return sizeof (Internal) + nodesPerInternal * sizeof (Node *);
                }
                static Node *Alloc (
                        PageMap &pageMap,
                        std::size_t index) {
                    return new (
                        pageMap.segmentAllocator.Alloc (
                            pageMap.internalSize)) Internal (pageMap, index);
                }

            private:
                void DeleteNode (Node *node) {
                    nodes[node->index] = nullptr;
                    nodeList.erase (node);
                    node->Harakiri ();
                }

                /// \brief
                /// Internal is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Internal)
            } *root;
            /// \brief
            /// Last accessed page offset.
            /// This is the page offset of the last call to \see{GetPage (offset)}.
            /// (offset & ~(pageSize - 1))
            AddressType lastGetPageOffset;
            /// \brief
            /// Last accessed page cache promoting locality of refernce.
            typename Page::SharedPtr lastGetPagePage;

        public:
            static const std::size_t DEFAULT_PAGE_ALIGNMENT = 4096;
            static const std::size_t DEFAULT_INTERNAL_NODES_PER_PAGE = 16;
            static const std::size_t DEFAULT_SEGMENT_NODES_PER_PAGE = 8;

            PageMap (
                RandomSeekSerializer &bitSource_,
                std::size_t bitsPerSegment_,
                std::size_t bitsPerLevel_,
                std::size_t bitsPerPage_,
                std::size_t pageAlignment = DEFAULT_PAGE_ALIGNMENT,
                std::size_t internalNodesPerPage = DEFAULT_INTERNAL_NODES_PER_PAGE,
                std::size_t segmentNodesPerPage = DEFAULT_SEGMENT_NODES_PER_PAGE,
                util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                bitSource (bitSource_),
                bitsPerAddress (sizeof (AddressType) * CHAR_BIT),
                bitsPerSegment (bitsPerSegment_),
                bitsPerLevel (bitsPerLevel_),
                bitsPerPage (bitsPerPage_),
                levelCount ((bitsPerAddress - bitsPerSegment) / bitsPerLevel),
                nodesPerInternal (1 << bitsPerLevel),
                pageSize (1 << bitsPerPage),
                pagesPerSegment (1 << (bitsPerSegment - bitsPerPage)),
                internalSize (Internal::Size (nodesPerInternal)),
                segmentSize (Segment::Size (pagesPerSegment)),
                levelShift (bitsPerAddress - bitsPerLevel),
                levelMask ((((AddressType)1 << bitsPerLevel) - 1) << levelShift),
                segmentMask (((AddressType)1 << bitsPerSegment) - 1),
                internalAllocator (internalSize, internalNodesPerPage, allocator),
                segmentAllocator (segmentSize, segmentNodesPerPage, allocator),
                pageAllocator (pageAlignment, allocator),
                root ((Internal *)Internal::Alloc (*this, 0)),
                lastGetPageOffset (NOFFS) {}
            ~PageMap () {
                root->Harakiri ();
            }

            /// \brief
            /// Return the page size.
            /// \return Page size.
            inline std::size_t GetPageSize () const {
                return pageSize;
            }

            /// \brief
            /// Return the \see{Page} that contains the given offset.
            /// \param[in] offset Offset whose page to return.
            /// \return \see{Page} that contains the given offset.
            typename Page::SharedPtr GetPage (AddressType offset) {
                AddressType pageOffset = offset & ~(pageSize - 1);
                if (lastGetPageOffset != pageOffset) {
                    Node *node = root;
                    // Begging the compiler to put these in to registers.
                    std::size_t levelShift_ = levelShift;
                    AddressType levelMask_ = levelMask;
                    for (std::size_t levelCount_ = levelCount; levelCount_-- != 0;
                            levelShift_ -= bitsPerLevel, levelMask_ >>= bitsPerLevel) {
                        node = ((Internal *)node)->GetNode (
                            (pageOffset & levelMask_) >> levelShift_, levelCount_ == 0);
                    }
                    // Cache the result so that we can reuse it if the next
                    // call to GetPage is sufficiently close to this one
                    // (locality of reference).
                    lastGetPageOffset = pageOffset;
                    lastGetPagePage.Reset (
                        ((Segment *)node)->GetPage (
                            (pageOffset & segmentMask) >> bitsPerPage, pageOffset));
                }
                return lastGetPagePage;
            }
            /// \brief
            /// Delete pages.
            /// \param[in] flags Combination of FLAGS_CLEAR_CLEAN and FLAGS_CLEAR_DIRTY.
            void Clear (std::size_t flags) {
                root->Clear (flags);
                if (lastGetPagePage != nullptr &&
                        (((flags & FLAGS_CLEAR_DIRTY) && lastGetPagePage->dirty) ||
                            ((flags & FLAGS_CLEAR_CLEAN) && !lastGetPagePage->dirty))) {
                    lastGetPageOffset = NOFFS;
                    lastGetPagePage.Reset ();
                }
            }
            /// \brief
            /// Write dirty pages to log and optionaly clear the page cache.
            /// \param[in] log \see{RandomSeekSerializer} to write to.
            /// \param[in] clearCache true == Delete the page cache after.
            void Log (
                    RandomSeekSerializer &log,
                    bool clearCache = false) {
                root->Log (log, clearCache);
                if (clearCache) {
                    lastGetPageOffset = NOFFS;
                    lastGetPagePage.Reset ();
                }
            }
            /// \brief
            /// Write dirty pages to their source and optionaly clear the page cache.
            /// \param[in] clearCache true == Delete the page cache after.
            void Flush (bool clearCache = false) {
                root->Flush (clearCache);
                if (clearCache) {
                    lastGetPageOffset = NOFFS;
                    lastGetPagePage.Reset ();
                }
            }
            /// \brief
            /// Delete all pages whose offset > newSize.
            /// \param[in] newSize New size to clip the address space to.
            void Shrink (AddressType newSize) {
                root->Shrink (newSize);
                // If newSize is <= lastGetPageOffset, lastGetPagePage
                // will have been deleted by root->Shrink.
                if (lastGetPageOffset >= newSize) {
                    lastGetPageOffset = NOFFS;
                    lastGetPagePage.Reset ();
                }
            }

            /// \brief
            /// PageMap is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (PageMap)
        };

        /// \brief
        /// Alias for PageMap<ui32>.
        using PageMap32 = PageMap<ui32>;
        /// \brief
        /// Alias for PageMap<ui64>.
        using PageMap64 = PageMap<ui64>;
        /// \brief
        /// Alias for PageMap<ui128>.
        using PageMap128 = PageMap<ui128>;

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_PageMap_h
