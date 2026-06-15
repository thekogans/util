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

#include <memory>
#include "thekogans/util/Heap.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/PageMap.h"

namespace thekogans {
    namespace util {

#if 0
        DWORD GetPhysicalSectorSize (HANDLE hFile) {
            STORAGE_PROPERTY_QUERY query = {
                StorageAccessAlignmentProperty,
                PropertyStandardQuery
            };
            STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR alignment = {0};
            DWORD bytesReturned = 0;
            return DeviceIoControl (
                hFile,
                IOCTL_STORAGE_QUERY_PROPERTY,
                &query, sizeof (query),
                &alignment, sizeof (alignment),
                &bytesReturned,
                NULL) ? alignment.BytesPerPhysicalSector : 0;
        }

        unsigned int GetPhysicalSectorSize (int fd) {
            unsigned int physical_sector_size = 0;
            // BLKPBSZGET returns the physical sector size in bytes
            return ioctl (fd, BLKPBSZGET, &physical_sector_size) == 0 ? physical_sector_size : 0;
        }
#endif

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (PageMap::Page)

        PageMap::Page::Page (
                PageMap &pageMap_,
                std::size_t index_,
                BaseType offset_) :
                pageMap (pageMap_),
                index (index_),
                offset (offset_),
                data ((ui8 *)pageMap.pageAllocator.Alloc (pageMap.pageSize)),
                dirty (false) {
            pageMap.bitSource.Seek (offset, SEEK_SET);
            ui64 countRead = pageMap.bitSource.Read (data, pageMap.pageSize);
            SecureZeroMemory (data + countRead, pageMap.pageSize - countRead);
        }

        PageMap::Page::~Page () {
            pageMap.pageAllocator.Free (data, pageMap.pageSize);
        }

        void PageMap::Page::Log (RandomSeekSerializer &log) {
            if (dirty) {
                log << offset;
                log.Write (data, pageMap.pageSize);
                dirty = false;
            }
        }

        void PageMap::Page::Flush () {
            if (dirty) {
                pageMap.bitSource.Seek (offset, SEEK_SET);
                pageMap.bitSource.Write (data, pageMap.pageSize);
                dirty = false;
            }
        }

        bool PageMap::Page::Shrink (ui64 newSize) {
            if (offset < newSize) {
                BaseType consumed = newSize - offset;
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

        PageMap::Segment::Segment (
                PageMap &pageMap,
                std::size_t index) :
                Node (pageMap, index),
                pages ((Page::SharedPtr *)(this + 1)) {
            for (std::size_t i = 0; i < pageMap.pagesPerSegment; ++i) {
                new (&pages[i]) Page::SharedPtr ();
            }
        }

        PageMap::Segment::~Segment () {
            for (std::size_t i = 0; i < pageMap.pagesPerSegment; ++i) {
                pages[i].~SharedPtr ();
            }
        }

        bool PageMap::Segment::Clear (bool all) {
            pageList.for_each (
                [this, all] (PageList::Callback::argument_type page) ->
                        PageList::Callback::result_type {
                    if (all || page->dirty) {
                        pageList.erase (page);
                        pages[page->index].Reset ();
                    }
                    return true;
                }
            );
            return pageList.empty ();
        }

        void PageMap::Segment::Log (RandomSeekSerializer &log) {
            pageList.for_each (
                [&log] (PageList::Callback::argument_type page) ->
                        PageList::Callback::result_type {
                    page->Log (log);
                    return true;
                }
            );
        }

        void PageMap::Segment::Flush () {
            pageList.for_each (
                [] (PageList::Callback::argument_type page) ->
                        PageList::Callback::result_type {
                    page->Flush ();
                    return true;
                }
            );
        }

        bool PageMap::Segment::Shrink (BaseType newSize) {
            pageList.for_each (
                [this, newSize] (PageList::Callback::argument_type page) ->
                        PageList::Callback::result_type {
                    if (page->Shrink (newSize)) {
                        pageList.erase (page);
                        pages[page->index].Reset ();
                        return true;
                    }
                    return false;
                },
                true
            );
            return pageList.empty ();
        }

        PageMap::Page::SharedPtr PageMap::Segment::GetPage (
                ui32 pageIndex,
                BaseType pageOffset) {
            if (pages[pageIndex] == nullptr) {
                // We don't align the page boundary as it's a fairly complex
                // structure with internal machinery that's hidden from view
                // (vptr tables...).
                Page *page = new Page (pageMap, pageIndex, pageOffset);
                pages[pageIndex].Reset (page);
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
                        [this, page] (PageList::Callback::argument_type page_) ->
                                PageList::Callback::result_type {
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

        void PageMap::Segment::Harakiri () {
            this->~Segment ();
            pageMap.segmentAllocator.Free (this, pageMap.segmentSize);
        }

        std::size_t PageMap::Segment::Size (std::size_t pagesPerSegment) {
            return sizeof (Segment) + pagesPerSegment * sizeof (Page::SharedPtr);
        }

        PageMap::Node *PageMap::Segment::Alloc (
                PageMap &pageMap,
                std::size_t index) {
            return new (
                pageMap.segmentAllocator.Alloc (
                    pageMap.segmentSize)) Segment (pageMap, index);
        }

        PageMap::Internal::Internal (
                PageMap &pageMap,
                std::size_t index) :
                Node (pageMap, index),
                nodes ((Node::SharedPtr *)(this + 1)) {
            for (std::size_t i = 0; i < pageMap.nodesPerInternal; ++i) {
                new (&nodes[i]) Node::SharedPtr ();
            }
        }

        PageMap::Internal::~Internal () {
            for (std::size_t i = 0; i < pageMap.nodesPerInternal; ++i) {
                nodes[i].~SharedPtr ();
            }
        }

        bool PageMap::Internal::Clear (bool all) {
            nodeList.for_each (
                [this, all] (NodeList::Callback::argument_type node) ->
                        NodeList::Callback::result_type {
                    if (all || node->Clear (all)) {
                        nodeList.erase (node);
                        nodes[node->index].Reset ();
                    }
                    return true;
                }
            );
            return nodeList.empty ();
        }

        void PageMap::Internal::Log (RandomSeekSerializer &log) {
            nodeList.for_each (
                [&log] (NodeList::Callback::argument_type node) ->
                        NodeList::Callback::result_type {
                    node->Log (log);
                    return true;
                }
            );
        }

        void PageMap::Internal::Flush () {
            nodeList.for_each (
                [] (NodeList::Callback::argument_type node) ->
                        NodeList::Callback::result_type {
                    node->Flush ();
                    return true;
                }
            );
        }

        bool PageMap::Internal::Shrink (BaseType newSize) {
            nodeList.for_each (
                [this, newSize] (NodeList::Callback::argument_type node) ->
                        NodeList::Callback::result_type {
                    if (!node->Shrink (newSize)) {
                        return false;
                    }
                    nodes[node->index].Reset ();
                    return true;
                },
                true
            );
            return nodeList.empty ();
        }

        PageMap::Page::SharedPtr PageMap::Internal::GetPage (BaseType offset) {
            Internal *internal = this;
            std::size_t levelShift = pageMap.levelShift;
            BaseType levelMask = pageMap.levelMask;
            for (std::size_t i = 0; i < pageMap.levelCount - 1; ++i) {
                internal = (Internal *)internal->GetNode ((offset & levelMask) >> levelShift);
                levelShift -= pageMap.bitsPerLevel;
                levelMask >>= pageMap.bitsPerLevel;
            }
            // Leafs are segments.
            Segment *segment = (Segment *)internal->GetNode ((offset & levelMask) >> levelShift, true);
            return segment->GetPage (
                (offset & pageMap.segmentMask) >> pageMap.bitsPerPage,
                offset & ~(pageMap.pageSize - 1));
        }

        PageMap::Internal::Node *PageMap::Internal::GetNode (
                std::size_t index,
                bool segment) {
            if (nodes[index] == nullptr) {
                Node *node = segment ?
                    Segment::Alloc (pageMap, index) :
                    Internal::Alloc (pageMap, index);
                nodes[index].Reset (node);
                if (nodeList.empty () || nodeList.tail->index < node->index) {
                    nodeList.push_back (node);
                }
                else {
                    nodeList.for_each (
                        [this, node] (NodeList::Callback::argument_type node_) ->
                                NodeList::Callback::result_type {
                            if (node_->index > node->index) {
                                nodeList.insert (node, node_);
                                return false;
                            }
                            return true;
                        }
                    );
                }
            }
            return nodes[index].Get ();
        }

        void PageMap::Internal::Harakiri () {
            this->~Internal ();
            pageMap.internalAllocator.Free (this, pageMap.internalSize);
        }

        std::size_t PageMap::Internal::Size (std::size_t nodesPerInternal) {
            return sizeof (Internal) + nodesPerInternal * sizeof (Node::SharedPtr);
        }

        PageMap::Node *PageMap::Internal::Alloc (
                PageMap &pageMap,
                std::size_t index) {
            return new (
                pageMap.segmentAllocator.Alloc (
                    pageMap.internalSize)) Internal (pageMap, index);
        }

        PageMap::Page::SharedPtr PageMap::GetPage (BaseType offset) {
            BaseType pageOffset = offset & ~(pageSize - 1);
            if (lastGetPageOffset != pageOffset) {
                lastGetPageOffset = pageOffset;
                lastGetPage = root.GetPage (offset);
            }
            return lastGetPage;
        }

        void PageMap::Clear (bool all) {
            root.Clear (all);
            if (all || (lastGetPage != nullptr && lastGetPage->dirty)) {
                lastGetPageOffset = NOFFS;
                lastGetPage.Reset ();
            }
        }

        void PageMap::Log (RandomSeekSerializer &log) {
            root.Log (log);
        }

        void PageMap::Flush () {
            root.Flush ();
        }

        void PageMap::Shrink (BaseType newSize) {
            root.Shrink (newSize);
            // If newSize is <= lastGetPageOffset, lastGetPage
            // will have been deleted by root.Shrink.
            if (lastGetPageOffset >= newSize) {
                lastGetPageOffset = NOFFS;
                lastGetPage.Reset ();
            }
        }

        PageMap::BaseType PageMap::BitMask (std::size_t count) {
            BaseType mask = 0;
            while (count--) {
                mask <<= 1;
                ++mask;
            }
            return mask;
        }

    } // namespace util
} // namespace thekogans
