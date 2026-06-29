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
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
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
        /// called segments. Segments are organized as leaf nodes in a fixed depth, multiway
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
        /// PageMap is designed to be used either embeded behind an API that controls access
        /// from multiple threads or as a standalone thread safe object by pasing in different
        /// types for Lock template parameter.
        ///
        /// Ex:
        ///
        /// AddressType = ui32
        /// bitsPerAddress = 32
        /// bitsPerSegment = 32
        /// bitsPerLevel = 0
        /// bitsPerPage = 16
        ///
        /// msb                                                                        lsb
        /// |----------------------------- bitsPerAddress ------------------------------|
        /// |----------------------------- bitsPerSegment ------------------------------|
        /// +-------------------------------------+-------------------------------------+
        /// |                                     |------------ bitsPerPage ------------|
        /// +-------------------------------------+-------------------------------------+
        /// 31                                    15                                    0
        ///
        /// levelCount = bitsPerLevel == 0 = 0
        /// pageSize = 1 << bitsPerPage; 1 << 16 = 64KB
        /// pagesPerSegment = 1 << (bitsPerSegment - bitsPerPage); 1 << (32 - 16) = 64K
        ///
        /// This will cover the entire 32 bit address space with 1 4GB segment,
        /// containing 64K 64KB pages.
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
        /// |----------levels---------|----------------- bitsPerSegment ----------------|
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
        /// |-----------------------levels-----------------------|------------------ bitsPerSegment ------------------|
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
        /// |---------------------levels---------------------|-------------------- bitsPerSegment --------------------|
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
        /// |--------------levels------------...--------------------|------------------ bitsPerSegment ------------------|
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
        ///
        /// \tparam T The type that will represent addresses.
        /// \tparam bitsPerAddress Narrow the address space to this many bits (default as wide as AddressType).
        /// \tparam Lock Any of the standard locks to use for synchronization.
        template<
            typename T,
            std::size_t bitsPerAddress = BitWidth<T>::value,
            typename Lock = SpinLock>
        struct PageMap : public RefCounted {
            /// \brief
            /// Rename T to something more appropriate that's
            /// visible outside the template.
            using AddressType = T;

            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (PageMap)

            /// \brief
            /// Forward declaration for \see{Page}.
            struct Page;

        private:
            /// \brief
            /// Inverse address mask for quick rejection test in GetPage.
            const AddressType inverseAddressMask;
            /// \brief
            /// This is the very last valid address space offset (before it overflows).
            const AddressType lastValidOffset;
            /// \brief
            /// \see{Segment} size in bits.
            const std::size_t bitsPerSegment;
            /// \brief
            /// Number of children per \see{Internal} node in bits.
            const std::size_t bitsPerLevel;
            /// \brief
            /// \see{Page} size in bits.
            const std::size_t bitsPerPage;
            /// \brief
            /// Number of children per \see{Internal} node in bytes.
            const std::size_t nodesPerInternal;
            /// \brief
            /// \see{Page} size in bytes.
            const std::size_t pageSize;
            /// \brief
            /// Number of \see{Page}s per \see{Segment}.
            const std::size_t pagesPerSegment;
            /// \brief
            /// \see{Internal} nodes are allocated from a custom
            /// \see{BlockAllocator}. Cache it's size to speed up
            /// allocations.
            const std::size_t internalSize;
            /// \brief
            /// \see{Segment} nodes are allocated from a custom
            /// \see{BlockAllocator}. Cache it's size to speed up
            /// allocations.
            const std::size_t segmentSize;
            /// \brief
            /// Level bit shift count to prime the address
            /// disassembly engine in preparation for a tree
            /// walk in \see{GetPage}.
            const std::size_t levelShift;
            /// \brief
            /// Level mask to work with the levelShift above.
            const AddressType levelMask;
            /// \brief
            /// \see{Segment} mask to use by the address disassembly
            /// engine as a final \see{Page} address resolution step
            /// in \see{GetPage}.
            const AddressType segmentMask;
            /// \brief
            /// \see{BlockAllocator} for allocating \see{Internal} nodes.
            BlockAllocator internalAllocator;
            /// \brief
            /// \see{BlockAllocator} for allocating \see{Segment} nodes.
            BlockAllocator segmentAllocator;
            /// \brief
            /// \see{AlignedAllocator} for \see{Page::data}.
            AlignedAllocator pageAllocator;
            /// \brief
            /// Alias for \see{IntrusiveList}<Page>.
            using PageList = IntrusiveList<Page>;
            /// \brief
            /// PageList provides direct access to pages without
            /// the need to walk the tree. Because the link between
            /// a page and the list is made durring critical path
            /// execution (GetPage), it's cost must be minimal.
            /// The pages are therefore unsorted.
            PageList pageList;

        public:
            /// \brief
            /// \see{Page} cache management flags.
            /// If this flag is passed to \see{Clear},
            /// will result in all \see{Page}s with
            /// dirty == true, being purged from the cache.
            static const std::size_t FLAGS_CLEAR_DIRTY = 1;
            /// \brief
            /// If this flag is passed to \see{Clear},
            /// will result in all \see{Page}s with
            /// dirty == false, being purged from the cache.
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
            /// Base class for \see{Page} and \see{Parent} (and by extension
            /// \see{Segment} and \see{Internal}) classes. Defines the API
            /// used by PageMap to interact with and maintain the \see{Page}
            /// tree.
            struct Node : public NodeList::Node {
                /// \brief
                /// Backpointer to \see{PageMap}.
                PageMap &pageMap;
                /// \brief
                /// Node index in \see{Parent::children}.
                const std::size_t index;

                /// \brief
                /// ctor.
                /// \param[in] pageMap_ Backpointer to \see{PageMap}.
                /// \param[in] index_ Node index in \see{Parent::children}.
                Node (
                    PageMap &pageMap_,
                    std::size_t index_) :
                    pageMap (pageMap_),
                    index (index_) {}

                /// \brief
                /// Return true if the node is empty (has no children).
                /// \return true == the node is empty.
                virtual bool IsEmpty () const = 0;

                /// \brief
                /// Delete pages.
                /// \param[in] flags Combination of FLAGS_CLEAR_DIRTY and FLAGS_CLEAR_CLEAN.
                /// \return true == the node is empty,
                /// false == the node has pages remaining.
                virtual bool Clear (std::size_t flags) = 0;

                /// \brief
                /// Write dirty pages to log.
                /// \param[in] log \see{RandomSeekSerializer} to write to.
                virtual void Log (RandomSeekSerializer &log) = 0;

                /// \brief
                /// Write dirty pages to their source.
                /// \param[in] bitSink \see{RandomSeekSerializer} where \see{Page} bits go.
                /// \param[in] clearCache true == Delete cache after fluh.
                virtual void Flush (
                    RandomSeekSerializer &bitSink,
                    bool clearCache = false) = 0;

                /// \brief
                /// Delete all pages whose offset > newSize.
                /// \param[in] newSize New size to clip the address space to.
                /// \return true == node was completely clipped (it is empty).
                /// false == node was partialy clipped (it has pages).
                virtual bool Shrink (AddressType newSize) = 0;

                /// \brief
                /// Kill yourself. Since different node types come from differernt
                /// custom \see{BlockAllocator}s, each node type will know which
                /// allocator to use for it's own demise.
                virtual void Release () = 0;
            };

        public:
            /// \struct PageMap::Page PageMap.h thekogans/util/PageMap.h
            ///
            /// \brief
            /// Page tiles the address space providing incremental, sparse
            /// access to the data.
            struct Page :
                    public virtual RefCounted,
                    public Node,
                    public PageList::Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Page)
                /// \brief
                /// Page has a private heap.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

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
                /// \param[in] pageMap Backpointer to \see{PageMap}.
                /// \param[in] index Page index in \see{Parent::children}.
                /// \param[in] offset_ Page offset in the address space
                /// (multiple of pageMap.pageSize).
                /// \param[in] bitSource Optional \see{RandomSeekSerializer}
                /// where \see{Page} bits come from. If this PageMap is associated
                /// with a \see{RandomSeekSerializer}, it will read it's bits from it.
                Page (
                        PageMap &pageMap,
                        std::size_t index,
                        AddressType offset_,
                        RandomSeekSerializer *bitSource) :
                        Node (pageMap, index),
                        offset (offset_),
                        data ((ui8 *)pageMap.pageAllocator.Alloc (pageMap.pageSize)),
                        dirty (false) {
                    std::size_t countRead = 0;
                    if (bitSource != nullptr) {
                        bitSource->Seek (offset, SEEK_SET);
                        countRead = bitSource->Read (data, pageMap.pageSize);
                    }
                    SecureZeroMemory (data + countRead, pageMap.pageSize - countRead);
                    pageMap.pageList.push_back (this);
                }
                /// \brief
                /// dtor.
                virtual ~Page () {
                    this->pageMap.pageAllocator.Free (data, this->pageMap.pageSize);
                    this->pageMap.pageList.erase (this);
                }

                // Node
                /// \brief
                /// This part of the Node interface is meant for the \see{Parent}
                /// derivative and doesn't apply to us. We return false.
                /// \return false.
                virtual bool IsEmpty () const override {
                    return false;
                }

                /// \brief
                /// Return true if page should be deleted based on the given
                /// flags and the dirty state.
                /// \param[in] flags Combination of FLAGS_CLEAR_DIRTY and FLAGS_CLEAR_CLEAN.
                /// \return true == page shoul be deleted,
                /// false == page does not match the given criteria.
                virtual bool Clear (std::size_t flags) override {
                    return ((flags & FLAGS_CLEAR_DIRTY) && dirty) ||
                        ((flags & FLAGS_CLEAR_CLEAN) && !dirty);
                }

                /// \brief
                /// If dirty, write page to log.
                /// \param[in] log \see{RandomSeekSerializer} to write to.
                virtual void Log (RandomSeekSerializer &log) override {
                    if (dirty) {
                        log << offset;
                        log.Write (data, this->pageMap.pageSize);
                        // NOTE: We don't set dirty = false here.
                    }
                }

                /// \brief
                /// If dirty, write page to it's source and make clean.
                /// \param[in] bitSink \see{RandomSeekSerializer} where \see{Page} bits go.
                virtual void Flush (
                        RandomSeekSerializer &bitSink,
                        bool /*clearCache*/ = false) override {
                    if (dirty) {
                        bitSink.Seek (offset, SEEK_SET);
                        bitSink.Write (data, this->pageMap.pageSize);
                        dirty = false;
                    }
                }

                /// \brief
                /// Clip the page to the new size.
                /// \param[in] newize Size to clip the page to.
                /// \return true == the page was completely clipped.
                /// false == the page was partially clipped.
                virtual bool Shrink (AddressType newSize) override {
                    if (offset < newSize) {
                        AddressType consumed = newSize - offset;
                        if (consumed < this->pageMap.pageSize) {
                            // Pages don't maintain internal lengths. All pages are
                            // pageMap.pageSize long (with potentially the last one
                            // being less). If this is the last page, we clear that
                            // part which falls outside the new address space size.
                            SecureZeroMemory (data + consumed, this->pageMap.pageSize - consumed);
                            dirty = true;
                        }
                        return false;
                    }
                    return true;
                }

                /// \brief
                /// Release the hold on the page reference (kill yourself).
                virtual void Release () override {
                    RefCounted::Release ();
                }

                /// \brief
                /// Allocate a \see{Page}.
                /// \param[in] pageMap Backpointer to \see{PageMap}.
                /// \param[in] index \see{Parent} node index in \see{Parent::children}.
                /// \param[in] offset Page offset in the address space
                /// (multiple of pageMap.pageSize).
                /// \param[in] bitSource Optional \see{RandomSeekSerializer}
                /// where \see{Page} bits come from. If this PageMap is associated
                /// with a \see{RandomSeekSerializer}, it will read it's bits from it.
                /// \return The new page node.
                static Node *Alloc (
                        PageMap &pageMap,
                        std::size_t index,
                        AddressType offset,
                        RandomSeekSerializer *bitSource) {
                    Page *page =  new Page (pageMap, index, offset, bitSource);
                    page->AddRef ();
                    return page;
                }

                /// \brief
                /// Page is neither copy or move constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (Page)
            };

        private:
            /// \struct PageMap::Parent PageMap.h thekogans/util/PageMap.h
            ///
            /// \brief
            /// Base for \see{Segment} and \see{Internal} nodes. Abstracts out
            /// all the common code between the two classes.
            struct Parent : public Node {
                /// \brief
                /// Array of our children directly following us. It is only used
                /// in GetChild and for that we pay the full freight including all
                /// the empty ones. It's used in \see{PageMap::GetPage} in the heart
                /// of the address disassembly engine. The fact that we can tell if a
                /// child exists just by indexing in to an array is worth all the cost.
                Node **children;
                /// \brief
                /// \see{IntrusiveList} of \see{Node}s. A linked list is much better
                /// suited to everyday chores like tree walking. We therefore maintain
                /// a list even at the cost of making sure that the nodes are inserted
                /// in order (sorted on index).
                NodeList childList;

                /// \brief
                /// ctor.
                /// \param[in] pageMap Backpointer to \see{PageMap}.
                /// \param[in] index Backpointer to Parent::children.
                /// \param[in] childCount Number of child Node * following this node.
                Parent (PageMap &pageMap,
                        std::size_t index,
                        std::size_t childCount) :
                        Node (pageMap, index),
                        children ((Node **)(this + 1)) {
                    SecureZeroMemory (children, childCount * sizeof (Node *));
                }
                /// \brief
                /// dtor.
                virtual ~Parent () {
                    childList.clear (
                        [this] (typename NodeList::Callback::argument_type child) ->
                                typename NodeList::Callback::result_type {
                            children[child->index] = nullptr;
                            child->Release ();
                            return true;
                        }
                    );
                }

                // Node
                /// \brief
                /// Return true if the node is empty (has no children).
                /// \return true == the node is empty.
                virtual bool IsEmpty () const override {
                    return childList.empty ();
                }

                /// \brief
                /// Delete pages.
                /// \param[in] flags Combination of FLAGS_CLEAR_DIRTY and FLAGS_CLEAR_CLEAN.
                /// \return IsEmpty ().
                virtual bool Clear (std::size_t flags) override {
                    childList.for_each (
                        [this, flags] (typename NodeList::Callback::argument_type child) ->
                                typename NodeList::Callback::result_type {
                            if (child->Clear (flags)) {
                                DeleteChild (child);
                            }
                            return true;
                        }
                    );
                    return IsEmpty ();
                }

                /// \brief
                /// Write dirty pages to log.
                /// \param[in] log \see{RandomSeekSerializer} to write to.
                virtual void Log (RandomSeekSerializer &log) override {
                    childList.for_each (
                        [&log] (typename NodeList::Callback::argument_type child) ->
                                typename NodeList::Callback::result_type {
                            child->Log (log);
                            return true;
                        }
                    );
                }

                /// \brief
                /// Write dirty pages to bitSink.
                /// \param[in] bitSink \see{RandomSeekSerializer} where \see{Page} bits go.
                /// \param[in] clearCache true == Delete the page cache after.
                virtual void Flush (
                        RandomSeekSerializer &bitSink,
                        bool clearCache = false) override {
                    childList.for_each (
                        [this, &bitSink, clearCache] (typename NodeList::Callback::argument_type child) ->
                                typename NodeList::Callback::result_type {
                            child->Flush (bitSink, clearCache);
                            if (clearCache) {
                                DeleteChild (child);
                            }
                            return true;
                        }
                    );
                }

                /// \brief
                /// Delete all pages whose offset > newSize.
                /// \param[in] newSize New size to clip the address space to.
                /// \return IsEmpty ().
                virtual bool Shrink (AddressType newSize) override {
                    childList.for_each (
                        [this, newSize] (typename NodeList::Callback::argument_type child) ->
                                typename NodeList::Callback::result_type {
                            if (child->Shrink (newSize)) {
                                DeleteChild (child);
                                return true;
                            }
                            return false;
                        },
                        true
                    );
                    return IsEmpty ();
                }

                /// \brief
                /// Retrieve the child @index, create if nullptr.
                /// \param[in] index Child index to retrieve.
                /// \param[in] factory If null, Factory to create the child.
                /// \return Child @ the given index.
                Node *GetChild (
                        std::size_t index,
                        std::function<Node *()> factory) {
                    if (children[index] == nullptr) {
                        Node *child = factory ();
                        children[index] = child;
                        if (childList.empty () || childList.tail->index < child->index) {
                            childList.push_back (child);
                        }
                        else {
                            childList.for_each (
                                [this, child] (typename NodeList::Callback::argument_type child_) ->
                                        typename NodeList::Callback::result_type {
                                    if (child->index < child_->index) {
                                        childList.insert (child, child_);
                                        return false;
                                    }
                                    return true;
                                }
                            );
                        }
                    }
                    return children[index];
                }

            private:
                /// \brief
                /// Delete the given child.
                /// \param[in] child \see{Node} to delete.
                void DeleteChild (Node *child) {
                    children[child->index] = nullptr;
                    childList.erase (child);
                    child->Release ();
                }

                /// \brief
                /// Parent is neither copy or move constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (Parent)
            };

            /// \struct PageMap::Segment PageMap.h thekogans/util/PageMap.h
            ///
            /// \brief
            /// Leaf node organizing address space \see{Page}s.
            struct Segment : public Parent {
                /// \brief
                /// ctor.
                /// \param[in] pageMap Backpointer to \see{PageMap}.
                /// \param[in] index Index in \see{Parent::children}.
                Segment (
                    PageMap &pageMap,
                    std::size_t index) :
                    Parent (pageMap, index, pageMap.pagesPerSegment) {}

                /// \brief
                /// Return the size of the segment node. They're all the same size, hence static.
                /// \return Size of the segment node.
                static std::size_t Size (std::size_t pagesPerSegment) {
                    return sizeof (Segment) + pagesPerSegment * sizeof (Page *);
                }

                /// \brief
                /// Allocate a segment leaf using a custom \see{BlockAllocator}.
                /// \param[in] pageMap Backpointer to \see{PageMap}.
                /// \param[in] index Index in \see{Parent::children}.
                /// \return The new segment node.
                static Node *Alloc (
                        PageMap &pageMap,
                        std::size_t index) {
                    return new (
                        pageMap.segmentAllocator.Alloc (
                            pageMap.segmentSize)) Segment (pageMap, index);
                }

                // Node
                /// \brief
                /// Kill yourself.
                virtual void Release () override {
                    this->~Segment ();
                    this->pageMap.segmentAllocator.Free (this, this->pageMap.segmentSize);
                }

                /// \brief
                /// Segment is neither copy or move constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (Segment)
            };

            /// \struct PageMap::Internal PageMap.h thekogans/util/PageMap.h
            ///
            /// \brief
            /// Internal structure node.
            struct Internal : public Parent {
                /// \brief
                /// ctor.
                /// \param[in] pageMap Backpointer to \see{PageMap}.
                /// \param[in] index Index in \see{Parent::children}.
                Internal (
                    PageMap &pageMap,
                    std::size_t index) :
                    Parent (pageMap, index, pageMap.nodesPerInternal) {}

                /// \brief
                /// Return the size of the internal node. They're all the same size, hence static.
                /// \return Size of the internal node.
                static std::size_t Size (std::size_t nodesPerInternal) {
                    return sizeof (Internal) + nodesPerInternal * sizeof (Node *);
                }

                /// \brief
                /// Allocate an internl structure node using a custom \see{BlockAllocator}.
                /// \param[in] pageMap Backpointer to \see{PageMap}.
                /// \param[in] index Index in \see{Parent::children}.
                /// \return The new internal node.
                static Node *Alloc (
                        PageMap &pageMap,
                        std::size_t index) {
                    return new (
                        pageMap.segmentAllocator.Alloc (
                            pageMap.internalSize)) Internal (pageMap, index);
                }

                // Node
                /// \brief
                /// Kill yourself.
                virtual void Release () override {
                    this->~Internal ();
                    this->pageMap.internalAllocator.Free (this, this->pageMap.internalSize);
                }

                /// \brief
                /// Internal is neither copy or move constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (Internal)
            };
            /// \brief
            /// The root of the tree.
            Node *root;
            /// \brief
            /// Last accessed page offset.
            /// This is the page offset of the last call to \see{GetPage (offset)}.
            /// (offset & ~(pageSize - 1))
            AddressType lastGetPageOffset;
            /// \brief
            /// Last accessed page cache promoting locality of refernce.
            typename Page::SharedPtr lastGetPagePage;
            /// \brief
            /// Synchronization lock.
            Lock lock;

        public:
            /// \brief
            /// Default \see{Page} alignment.
            static const std::size_t DEFAULT_PAGE_ALIGNMENT = 4096;
            /// \brief
            /// Default number of \see{Internal} nodes per \see{BlockAllocator} page.
            static const std::size_t DEFAULT_INTERNAL_NODES_PER_PAGE = 16;
            /// \brief
            /// Default number of \see{Segment} nodes per \see{BlockAllocator} page.
            static const std::size_t DEFAULT_SEGMENT_NODES_PER_PAGE = 8;

            /// \brief
            /// ctor.
            /// \param[in] bitsPerSegment_ How many address bits represent a segment.
            /// \param[in] bitsPerLevel_ How many address bits represent a level.
            /// \param[in] bitsPerPage_ How many address bits represent a page.
            /// \param[in] pageAlignment Page alignment.
            /// \param[in] internalNodesPerPage Number of \see{Internal} nodes per \see{BlockAllocator} page.
            /// \param[in] segmentNodesPerPage Number of \see{Segment} nodes per \see{BlockAllocator} page.
            /// \param[in] allocator \see{Allocator} to use to create \see{BlockAllocator} pages.
            PageMap (
                    std::size_t bitsPerSegment_,
                    std::size_t bitsPerLevel_,
                    std::size_t bitsPerPage_,
                    std::size_t pageAlignment = DEFAULT_PAGE_ALIGNMENT,
                    std::size_t internalNodesPerPage = DEFAULT_INTERNAL_NODES_PER_PAGE,
                    std::size_t segmentNodesPerPage = DEFAULT_SEGMENT_NODES_PER_PAGE,
                    util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                    inverseAddressMask (
                        bitsPerAddress < BitWidth<AddressType>::value ?
                            ~(((AddressType)1 << bitsPerAddress) - 1) : 0),
                    lastValidOffset (~inverseAddressMask),
                    bitsPerSegment (bitsPerSegment_),
                    bitsPerLevel (bitsPerLevel_),
                    bitsPerPage (bitsPerPage_),
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
                    root (nullptr),
                    lastGetPageOffset (NOFFS) {
                // Validate input.
                if (bitsPerAddress == 0 || bitsPerAddress > BitWidth<AddressType>::value ||
                        bitsPerSegment == 0 || bitsPerSegment > bitsPerAddress ||
                        /*bitsPerLevel == 0 ||*/ bitsPerLevel > (bitsPerAddress - bitsPerSegment) ||
                        bitsPerPage == 0 || bitsPerPage > bitsPerSegment ||
                        !IsPowerOf2 (pageAlignment) ||
                        internalNodesPerPage == 0 || segmentNodesPerPage == 0 ||
                        allocator == nullptr) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
            /// \brief
            /// dtor.
            ~PageMap () {
                if (root != nullptr) {
                    root->Release ();
                }
            }

            /// \brief
            /// Return the last valid address space offset.
            /// \return lastValidOffset.
            inline AddressType GetLastValidOffset () const {
                return lastValidOffset;
            }

            /// \brief
            /// Return the page size.
            /// \return pageSize.
            inline std::size_t GetPageSize () const {
                return pageSize;
            }

            /// \brief
            /// Return the \see{Page} that contains the given offset.
            /// \param[in] offset Offset whose page to return.
            /// \param[in] bitSource Optional \see{RandomSeekSerializer}
            /// where \see{Page} bits come from.
            /// \return \see{Page} that contains the given offset.
            typename Page::SharedPtr GetPage (
                    AddressType offset,
                    RandomSeekSerializer *bitSource) {
                // Quick bounds check.
                if ((offset & inverseAddressMask) != 0) {
                    return nullptr;
                }
                offset &= ~(pageSize - 1);
                LockGuard<Lock> guard (lock);
                if (lastGetPageOffset != offset) {
                    Node *node = GetRoot ();
                    // This is the address dissassembly engine used to
                    // break up the address (offset) in service of a virtual
                    // tree walk.
                    // Begging the compiler to put these in to registers.
                    std::size_t levelShift_ = levelShift;
                    AddressType levelMask_ = levelMask;
                    // We begin the tree walk with the root node and take a
                    // bite off the levels portion of the address at every loop
                    // step, as we step internal structure nodes on our way
                    // to visit the final segment node that will have our page.
                    // NOTE: This algorithm is sensitive to the corner case
                    // where one segment covers the entire address space. In
                    // that case GetRoot above will return that segment and
                    // this while loop will not be executed.
                    while (levelMask_ != 0) {
                        std::size_t index = (offset & levelMask_) >> levelShift_;
                        levelShift_ -= bitsPerLevel;
                        levelMask_ >>= bitsPerLevel;
                        node = ((Internal *)node)->GetChild (index,
                            [this, index, levelMask_] () -> Node * {
                                return levelMask_ == 0 ?
                                    Segment::Alloc (*this, index) :
                                    Internal::Alloc (*this, index);
                            }
                        );
                    }
                    lastGetPageOffset = offset;
                    // Since tree leafs are segments, ask the one we got for the
                    // page correponding to the given address.
                    // Cache the result so that we can reuse it if the next
                    // call to GetPage is sufficiently close to this one
                    // (locality of reference).
                    std::size_t index = (offset & segmentMask) >> bitsPerPage;
                    lastGetPagePage.Reset (
                        (Page *)((Segment *)node)->GetChild (index,
                            [this, index, offset, bitSource] () -> Node * {
                                return Page::Alloc (*this, index, offset, bitSource);
                            }
                        )
                    );
                }
                return lastGetPagePage;
            }

            /// \brief
            /// Delete pages.
            /// \param[in] flags Combination of FLAGS_CLEAR_DIRTY and FLAGS_CLEAR_CLEAN.
            void Clear (std::size_t flags) {
                LockGuard<Lock> guard (lock);
                if (root != nullptr) {
                    root->Clear (flags);
                    DeleteRoot (lastGetPagePage != nullptr && lastGetPagePage->Clear (flags));
                }
            }

            /// \brief
            /// Write dirty pages to log.
            /// \param[in] log \see{RandomSeekSerializer} to write dirty pages to.
            void Log (RandomSeekSerializer &log) {
                LockGuard<Lock> guard (lock);
                if (root != nullptr) {
                    root->Log (log);
                }
            }

            /// \brief
            /// Write dirty pages to their source and optionaly clear the page cache.
            /// \param[in] bitSink \see{RandomSeekSerializer} where \see{Page} bits go.
            /// \param[in] clearCache true == Delete the page cache after.
            void Flush (
                    RandomSeekSerializer &bitSink,
                    bool clearCache = false) {
                LockGuard<Lock> guard (lock);
                if (root != nullptr) {
                    root->Flush (bitSink, clearCache);
                    DeleteRoot (clearCache);
                }
            }

            /// \brief
            /// Shrink the address space (delete all pages whose offset > newSize).
            /// \param[in] newSize New size to clip the address space to.
            void Shrink (AddressType newSize) {
                LockGuard<Lock> guard (lock);
                if (root != nullptr) {
                    root->Shrink (newSize);
                    // If newSize is <= lastGetPageOffset, lastGetPagePage
                    // will have been deleted by root->Shrink.
                    DeleteRoot (lastGetPageOffset >= newSize);
                }
            }

            /// \brief
            /// Provides direct access to pages. See comment above \see{PageList}.
            /// NOTE: The callback is called while holding the lock.
            /// \param[in] callback Called for every page in the list.
            /// \param[in] reverse true == Walk the list tail to head.
            /// \return true == Iterated over all pages, false == callback returned false.
            bool EnumeratePages (
                    const typename PageList::Function &callback,
                    bool reverse = false) {
                LockGuard<Lock> guard (lock);
                return pageList.for_each (callback, reverse);
            }

        private:
            /// \brief
            /// Helper to recreate the root if it's gone. Depending on the address space
            /// parameterization, \see{Parent} can actually become pretty massive (>>1MB).
            /// In order to keep our footprint to a minimum if we're asked to clear the
            /// tree and the root is empty, we will dump it too, regardless.
            /// This method is used by GetPage above to rebuild it.
            /// \return root.
            Node *GetRoot () {
                if (root == nullptr) {
                    // levelMask == 0 is a corner case where one segment covers the
                    // entire address space...
                    root = levelMask == 0 ?
                        Segment::Alloc (*this, 0) :
                        // This is the most common case. One or more levels of internal
                        // nodes leading to segment nodes.
                        Internal::Alloc (*this, 0);
                }
                return root;
            }

            /// \brief
            /// Dump the root if it's empty. Used by various methods above after a tree
            /// prunning.
            /// \param[in] clearCache true == release the page cache.
            void DeleteRoot (bool clearCache = false) {
                if (root->IsEmpty ()) {
                    root->Release ();
                    root = nullptr;
                }
                if (clearCache) {
                    lastGetPageOffset = NOFFS;
                    lastGetPagePage.Reset ();
                }
            }

            /// \brief
            /// PageMap is neither copy or move constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (PageMap)
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
