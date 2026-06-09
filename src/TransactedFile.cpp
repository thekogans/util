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
    #if defined (TOOLCHAIN_OS_Linux)
        #define _GNU_SOURCE
    #endif // defined (TOOLCHAIN_OS_Linux)
    #include <fcntl.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include <string>
#include "thekogans/util/Heap.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/AlignedAllocator.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (
            thekogans::util::TransactedFile,
            Serializer::TYPE, RandomSeekSerializer::TYPE)

        const int TransactedFile::COMMIT_PHASE_1 = 1;
        const int TransactedFile::COMMIT_PHASE_2 = 2;

        TransactedFile::Transaction::Transaction (TransactedFile::SharedPtr file_) :
                file (file_),
                guard (file->mutex) {
            file->BeginTransaction ();
        }

        TransactedFile::Transaction::~Transaction () {
            file->AbortTransaction ();
            file->Unsubscribe ();
        }

        void TransactedFile::Transaction::Commit () {
            file->CommitTransaction ();
            file->Unsubscribe ();
        }

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (TransactedFile::Page)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (TransactedFile::Segment)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (TransactedFile::Internal)

        bool TransactedFile::Segment::Clear (bool all) {
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

        void TransactedFile::Segment::Flush (
                File &file,
                bool log) {
            pageList.for_each (
                [&file, log] (PageList::Callback::argument_type page) ->
                        PageList::Callback::result_type {
                    if (page->dirty) {
                        if (log) {
                            file << page->offset;
                            // NOTE: Here we write the full page size (Bufer::SIZE),
                            // trailing '0' and all. Since Page does not maintain
                            // internal size, all pages other than possibly the last
                            // are Page::SIZE in length, we have no choice but to
                            // write Page::SIZE and have the actual size be adjusted
                            // upstream (Flush).
                        }
                        else {
                            file.Seek (page->offset, SEEK_SET);
                        }
                        file.Write (page->data, Page::SIZE);
                        page->dirty = false;
                    }
                    return true;
                }
            );
        }

        bool TransactedFile::Segment::Shrink (ui64 newSize) {
            pageList.for_each (
                [this, newSize] (PageList::Callback::argument_type page) ->
                        PageList::Callback::result_type {
                    if (page->offset >= newSize) {
                        pageList.erase (page);
                        pages[page->index].Reset ();
                        return true;
                    }
                    else {
                        ui64 consumed = newSize - page->offset;
                        if (consumed < Page::SIZE) {
                            // Pages don't maintain internal lengths. All pages
                            // are Page::SIZE long (with potentially the last one
                            // being less). If this is the last page, we clear that
                            // part which falls outside the new file size.
                            SecureZeroMemory (page->data + consumed, Page::SIZE - consumed);
                        }
                        return false;
                    }
                },
                true
            );
            return pageList.empty ();
        }

        TransactedFile::Page::SharedPtr TransactedFile::Segment::GetPage (
                ui32 pageIndex,
                ui64 pageOffset,
                util::Allocator &pageAllocator,
                File &file) {
            if (pages[pageIndex] == nullptr) {
                // We don't align the page boundary as it's a fairly complex
                // structure with internal machinery that's hidden from view
                // (vptr tables...). Instead we pass a pageAllocator to the
                // page ctor and have it use that to align it's internal data
                // buffer.
                Page *page = new Page (pageIndex, pageOffset, pageAllocator);
                pages[pageIndex].Reset (page);
                file.Seek (pageOffset, SEEK_SET);
                ui64 countRead = file.Read (page->data, Page::SIZE);
                SecureZeroMemory (page->data + countRead, Page::SIZE - countRead);
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

        bool TransactedFile::Internal::Clear (bool all) {
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

        void TransactedFile::Internal::Flush (
                File &file,
                bool log) {
            nodeList.for_each (
                [&file, log] (NodeList::Callback::argument_type node) ->
                        NodeList::Callback::result_type {
                    node->Flush (file, log);
                    return true;
                }
            );
        }

        bool TransactedFile::Internal::Shrink (ui64 newSize) {
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

        TransactedFile::Internal::Node *TransactedFile::Internal::GetNode (
                ui8 index,
                bool segment) {
            if (nodes[index] == nullptr) {
                Node *node = segment ? (Node *)new Segment (index) : (Node *)new Internal (index);
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

        TransactedFile::Page::SharedPtr TransactedFile::Internal::GetPage (
                File &file,
                ui64 offset,
                util::Allocator &pageAllocator) {
            // --
            ui32 nodeIndex = THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (offset, 0);
            Internal *internal = (Internal *)GetNode (
                THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (nodeIndex, 0));
            internal = (Internal *)internal->GetNode (
                THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (nodeIndex, 1));
            internal = (Internal *)internal->GetNode (
                THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (nodeIndex, 2));
            // Leafs are segments.
            Segment *segment = (Segment *)internal->GetNode (
                THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (nodeIndex, 3), true);
            // -- We've just sparsely traversed the first 4
            // layers of the 5 layer 64 bit index.
            // --
            static const std::size_t SHIFT_COUNT = TrailingZeroBitCount (Page::SIZE);
            return segment->GetPage (
                THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (offset, 1) >> SHIFT_COUNT,
                offset & ~(Page::SIZE - 1),
                pageAllocator,
                file);
            // -- After potentially creating the page in Segment::GetPage,
            // we've arrived at the end of the 5 layer deep sparse index.
            // This mapping is constant (with the create code being amortized
            // accross multiple page accesses). It's O(c) where c = 5 shifts.
        }

        TransactedFile::TransactedFile (
                Endianness endianness,
                THEKOGANS_UTIL_HANDLE handle,
                const std::string &path,
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry) :
                File (endianness, handle, path),
                position (0),
                size (0),
                flags (0),
                pageAllocator (4096),
                root (0),
                currPageOffset (NOFFS) {
            if (IsOpen ()) {
                position = File::Tell ();
                size = File::GetSize ();
                Init (allocator, registry);
            }
        }

        TransactedFile::TransactedFile (
                Endianness endianness,
                const std::string &path,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess | GENERIC_WRITE,
                DWORD dwShareMode,
                DWORD dwCreationDisposition,
                DWORD dwFlagsAndAttributes,
            #else // defined (TOOLCHAIN_OS_Windows)
                i32 flags,
                i32 mode,
            #endif // defined (TOOLCHAIN_OS_Windows)
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry) :
                File (endianness),
                position (0),
                size (0),
                flags (0),
                pageAllocator (4096),
                root (0),
                currPageOffset (NOFFS) {
            OpenEx (
                path,
            #if defined (TOOLCHAIN_OS_Windows)
                dwDesiredAccess,
                dwShareMode,
                dwCreationDisposition,
                dwFlagsAndAttributes,
            #else // defined (TOOLCHAIN_OS_Windows)
                flags,
                mode,
            #endif // defined (TOOLCHAIN_OS_Windows)
                allocator,
                registry);
        }

        TransactedFile::~TransactedFile () {
            THEKOGANS_UTIL_TRY {
                Close ();
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        std::size_t TransactedFile::ReadEx (
                ui64 offset,
                void *buffer,
                std::size_t count) {
            if (buffer != nullptr && count > 0) {
                if (IsOpen ()) {
                    LockGuard<SpinLock> guard (spinLock);
                    std::size_t countRead = 0;
                    ui8 *ptr = (ui8 *)buffer;
                    while (count > 0 && offset < size) {
                        Page::SharedPtr page_ = GetPageHelper (offset);
                        std::size_t pageOffset = offset - page_->offset;
                        std::size_t countToRead = MIN (
                            // Calculate the amount we can read from this page...
                            MIN (Page::SIZE - pageOffset, count),
                            // ...and clamp it to the amount left to read in the file.
                            size - page_->offset);
                        std::memcpy (ptr, page_->data + pageOffset, countToRead);
                        ptr += countToRead;
                        countRead += countToRead;
                        offset += countToRead;
                        count -= countToRead;
                    }
                    return countRead;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        std::size_t TransactedFile::WriteEx (
                ui64 offset,
                const void *buffer,
                std::size_t count) {
            if (buffer != nullptr && count > 0) {
                if (IsOpen ()) {
                    LockGuard<SpinLock> guard (spinLock);
                    std::size_t countWritten = 0;
                    ui8 *ptr = (ui8 *)buffer;
                    while (count > 0) {
                        Page::SharedPtr page_ = GetPageHelper (offset);
                        std::size_t pageOffset = offset - page_->offset;
                        std::size_t countToWrite = MIN (Page::SIZE - pageOffset, count);
                        std::memcpy (page_->data + pageOffset, ptr, countToWrite);
                        page_->dirty = true;
                        ptr += countToWrite;
                        countWritten += countToWrite;
                        offset += countToWrite;
                        count -= countToWrite;
                    }
                    if (size < offset) {
                        size = offset;
                    }
                    SetDirty (true);
                    return countWritten;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void TransactedFile::OpenEx (
                const std::string &path,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess,
                DWORD dwShareMode,
                DWORD dwCreationDisposition,
                DWORD dwFlagsAndAttributes,
            #else // defined (TOOLCHAIN_OS_Windows)
                i32 flags,
                i32 mode,
            #endif // defined (TOOLCHAIN_OS_Windows)
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry) {
        #if defined (TOOLCHAIN_OS_Windows)
            Open (
                path,
                dwDesiredAccess,
                dwShareMode,
                dwCreationDisposition,
                dwFlagsAndAttributes);
        #else // defined (TOOLCHAIN_OS_Windows)
            Open (path, flags, mode);
        #endif // defined (TOOLCHAIN_OS_Windows)
            Init (allocator, registry);
        }

        ui64 TransactedFile::Grow (ui64 amount) {
            if (IsOpen ()) {
                LockGuard<SpinLock> guard (spinLock);
                ui32 oldSize = size;
                size += amount;
                SetDirty (true);
                return oldSize;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        ui64 TransactedFile::Shrink (ui64 amount) {
            if (IsOpen ()) {
                LockGuard<SpinLock> guard (spinLock);
                if (size >= amount) {
                    size -= amount;
                    root.Shrink (size);
                    // If size is <= current page offset, currPage
                    // will have been deleted by root.Shrink.
                    if (currPageOffset >= size) {
                        currPageOffset = NOFFS;
                        currPage.Reset ();
                    }
                    SetDirty (true);
                    return size;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        void TransactedFile::DeleteCache () {
            if (IsOpen ()) {
                Flush ();
                root.Clear (true);
                currPageOffset = NOFFS;
                currPage.Reset ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        std::size_t TransactedFile::Read (
                void *buffer,
                std::size_t count) {
            std::size_t countRead = ReadEx (position, buffer, count);
            position += countRead;
            return countRead;
        }

        std::size_t TransactedFile::Write (
                const void *buffer,
                std::size_t count) {
            std::size_t countWritten = WriteEx (position, buffer, count);
            position += countWritten;
            return countWritten;
        }

        i64 TransactedFile::Seek (
                i64 offset,
                i32 fromWhere) {
            if (IsOpen ()) {
                switch (fromWhere) {
                    case SEEK_SET:
                        if (offset < 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW);
                        }
                        position = offset;
                        break;
                    case SEEK_CUR:
                        if (position + offset < 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW);
                        }
                        position += offset;
                        break;
                    case SEEK_END:
                        if ((i64)size + offset < 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE_EOVERFLOW);
                        }
                        position = (i64)size + offset;
                        break;
                    default:
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
                return position;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        void TransactedFile::Open (
                const std::string &path,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess,
                DWORD dwShareMode,
                DWORD dwCreationDisposition,
                DWORD dwFlagsAndAttributes) {
            #else // defined (TOOLCHAIN_OS_Windows)
                i32 flags_,
                i32 mode) {
            #endif // defined (TOOLCHAIN_OS_Windows)
            CommitLog (path);
        #if defined (TOOLCHAIN_OS_Windows)
            File::Open (
                path,
                dwDesiredAccess,
                dwShareMode,
                dwCreationDisposition,
                dwFlagsAndAttributes | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
        #else // defined (TOOLCHAIN_OS_Windows)
        #if defined (TOOLCHAIN_OS_Linux)
            flags_ |= O_DIRECT;
        #endif // defined (TOOLCHAIN_OS_Linux)
            File::Open (path, flags_, mode);
        #if defined (TOOLCHAIN_OS_OSX)
            fcntl (handle, F_NOCACHE, 1);
        #endif // defined (TOOLCHAIN_OS_OSX)
        #endif // defined (TOOLCHAIN_OS_Windows)
            position = File::Tell ();
            size = File::GetSize ();
            flags = 0;
            currPageOffset = NOFFS;
            currPage.Reset ();
            allocator.Reset ();
            registry.Reset ();
        }

        void TransactedFile::Close () {
            if (IsOpen ()) {
                // All transactions must be commited before file close.
                // On the other hand dirty pages get flushed out to disk
                // to mimic what File would do.
                AbortTransaction ();
                DeleteCache ();
                position = 0;
                size = 0;
                flags = 0;
                currPageOffset = NOFFS;
                currPage.Reset ();
                allocator.Reset ();
                registry.Reset ();
                File::Close ();
            }
        }

        void TransactedFile::Flush () {
            if (IsOpen ()) {
                if (registry != nullptr && registry->IsDirty ()) {
                    // Can't have a registry without an allocator.
                    assert (allocator != nullptr);
                    allocator->SetRegistryOffset (
                        allocator->Realloc (
                            allocator->GetRegistryOffset (),
                            UI32_SIZE + registry->GetSize ()));
                    BlockRange range (*this, allocator->GetRegistryOffset (), false);
                    range << MAGIC32 << *registry;
                    registry->SetDirty (false);
                }
                if (allocator != nullptr && allocator->IsDirty ()) {
                    // Since allocator block is special (it's first and unresizable)...
                    SerializableHeader allocatorHeader;
                    {
                        // ...extract the original SerializableHeader...
                        BlockRange range (*this, Allocator::Block::HEADER_SIZE);
                        // skip over magic.
                        range.Seek (UI32_SIZE, SEEK_CUR);
                        range >> allocatorHeader;
                    }
                    {
                        BlockRange range (*this, Allocator::Block::HEADER_SIZE, false);
                        // skip over magic and serializable header.
                        range.Seek (UI32_SIZE + allocatorHeader.Size (), SEEK_CUR);
                        // ...and force the allocator to write that particular
                        // version of itself so as not to overflow the first block.
                        ContextGuard guard (range, allocatorHeader);
                        range << *allocator;
                    }
                    allocator->SetDirty (false);
                }
                if (IsDirty ()) {
                    if (IsTransactionPending ()) {
                        std::string logPath = GetLogPath (path);
                        SimpleFile log (
                            endianness,
                            logPath,
                            SimpleFile::ReadWrite | SimpleFile::Create);
                        if (log.GetSize () > 0) {
                            ui32 magic;
                            log >> magic;
                            if (magic == MAGIC32) {
                                ui32 isClean;
                                log >> isClean;
                                log << size;
                                log.Seek (0, SEEK_END);
                            }
                            else {
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                    "Corrupt log %s",
                                    logPath.c_str ());
                            }
                        }
                        else {
                            log << MAGIC32 << (ui32)0 << size;
                        }
                        root.Flush (log, true);
                    }
                    else {
                        // Give Flush a \see{TenantFile} as it's interface is that of \see{File}.
                        // If we were to pass *this, Flush would call in to our Seek and Write.
                        // And thats not what we want!
                        TenantFile file (endianness, handle, path);
                        root.Flush (file, false);
                        file.SetSize (size);
                        file.Flush ();
                    }
                    SetDirty (false);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        void TransactedFile::SetSize (ui64 newSize) {
            if (IsOpen ()) {
                if (size != newSize) {
                    if (size > newSize) {
                        // shrinking
                        root.Shrink (newSize);
                        // If new size is <= current page offset,
                        // currPage will be deleted by root.Shrink.
                        if (currPageOffset >= newSize) {
                            currPageOffset = NOFFS;
                            currPage.Reset ();
                        }
                    }
                    size = newSize;
                    SetDirty (true);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        void TransactedFile::Init (
                Allocator::SharedPtr allocator_,
                Registry::SharedPtr registry_) {
            if (allocator_ != nullptr) {
                if (GetSize () == 0) {
                    // Initialize the first block.
                    Allocator::Block block (
                        *this, Allocator::Block::HEADER_SIZE, 0, UI32_SIZE + allocator_->GetSize ());
                    // For performance reasons Range assumes that all
                    // reads and writes are within file bounds. We set
                    // the file size here so that block.Write and range
                    // insert below honor that assumption.
                    SetSize (Allocator::Block::SIZE + block.GetSize ());
                    block.Write ();
                    BlockRange range (*this, block.GetOffset (), false);
                    range << MAGIC32 << *allocator_;
                }
                BlockRange range (*this, Allocator::Block::HEADER_SIZE);
                ui32 magic;
                range >> magic;
                if (magic == MAGIC32) {
                    // File is host endian.
                }
                else if (ByteSwap<GuestEndian, HostEndian> (magic) == MAGIC32) {
                    // File is guest endian.
                    endianness = GuestEndian;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt TransactedFile file (%s).",
                        GetPath ().c_str ());
                }
                {
                    ContextGuard guard (range, SerializableHeader (), nullptr,
                        [this] (DynamicCreatable::SharedPtr dynamicCreatable) {
                            Allocator::SharedPtr allocator = dynamicCreatable;
                            if (allocator != nullptr) {
                                allocator->file = this;
                            }
                        }
                    );
                    range >> allocator;
                }
                if (allocator->GetRegistryOffset () == 0 && registry_ != nullptr) {
                    allocator->SetRegistryOffset (
                        allocator->Alloc (UI32_SIZE + registry_->GetSize ()));
                    BlockRange range (*this, allocator->GetRegistryOffset (), false);
                    range << MAGIC32 << *registry_;
                }
                if (allocator->GetRegistryOffset () != 0) {
                    BlockRange range (*this, allocator->GetRegistryOffset ());
                    range >> magic;
                    if (magic == MAGIC32) {
                        ContextGuard guard (range, SerializableHeader (), nullptr,
                            [this] (DynamicCreatable::SharedPtr dynamicCreatable) {
                                Registry::SharedPtr registry = dynamicCreatable;
                                if (registry != nullptr) {
                                    registry->file = this;
                                }
                            }
                        );
                        range >> registry;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Corrupt TransactedFile file (%s).",
                            GetPath ().c_str ());
                    }
                }
            }
        }

        void TransactedFile::CommitLog (const std::string &path) {
            std::string logPath = GetLogPath (path);
            if (Path (path).Exists () && Path (logPath).Exists ()) {
                {
                    SimpleFile file (HostEndian, path, SimpleFile::ReadWrite);
                    ReadOnlyFile log (HostEndian, logPath);
                    // Magic serves two purposes. Firstly it gives us a quick
                    // check to make sure we're dealing with a log file and second,
                    // it allows us to move logs from little to big endian (and
                    // vise versa) machines for analysis and resolution.
                    ui32 magic;
                    log >> magic;
                    if (magic == MAGIC32) {
                        // Log is host endian.
                    }
                    else if (ByteSwap<GuestEndian, HostEndian> (magic) == MAGIC32) {
                        // Log is guest endian. File endianness doesn't mater as it is
                        // just being patched up with dirty tiles. Although it is assumed
                        // to be the same as the log endianness.
                        log.endianness = GuestEndian;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Corrupt log %s",
                            logPath.c_str ());
                    }
                    ui32 isClean;
                    log >> isClean;
                    if (isClean == 1) {
                        ui64 size;
                        log >> size;
                        ui64 offset;
                        HostBuffer page (Page::SIZE);
                        for (ui64 logPosition = log.Tell (), logSize = log.GetSize (); logPosition < logSize;) {
                            log >> offset;
                            logPosition += UI64_SIZE + log.Read (page.GetDataPtr (), Page::SIZE);
                            if (offset < size) {
                                ui64 available = MIN (size - offset, Page::SIZE);
                                if (available != 0) {
                                    file.Seek (offset, SEEK_SET);
                                    file.Write (page.GetDataPtr (), available);
                                }
                            }
                        }
                        file.SetSize (size);
                        file.Flush ();
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Log is not clean %s",
                            logPath.c_str ());
                    }
                }
                File::Delete (logPath);
            }
        }

        void TransactedFile::BeginTransaction () {
            if (IsOpen ()) {
                if (!IsTransactionPending ()) {
                    Flush ();
                    SetTransactionPending (true);
                    Produce (
                        std::bind (
                            &TransactedFileEvents::OnTransactedFileTransactionBegin,
                            std::placeholders::_1,
                            this));
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        void TransactedFile::CommitTransaction () {
            if (IsOpen ()) {
                if (IsTransactionPending ()) {
                    Array<SharedSubscriberInfo> subscribers;
                    std::size_t count = GetSubscribers (subscribers);
                    for (std::size_t i = 0; i < count; ++i) {
                        subscribers[i].second->DeliverEvent (
                            std::bind (
                                &TransactedFileEvents::OnTransactedFileTransactionCommit,
                                std::placeholders::_1,
                                this,
                                COMMIT_PHASE_1),
                            subscribers[i].first);
                    }
                    for (std::size_t i = 0; i < count; ++i) {
                        subscribers[i].second->DeliverEvent (
                            std::bind (
                                &TransactedFileEvents::OnTransactedFileTransactionCommit,
                                std::placeholders::_1,
                                this,
                                COMMIT_PHASE_2),
                            subscribers[i].first);
                    }
                    Flush ();
                    std::string logPath = GetLogPath (path);
                    if (Path (logPath).Exists ()) {
                        {
                            TenantFile file (endianness, handle, path);
                            SimpleFile log (endianness, logPath, SimpleFile::ReadWrite);
                            ui32 magic;
                            log >> magic;
                            if (magic == MAGIC32) {
                                ui32 isClean = 1;
                                log << isClean;
                                log >> size;
                                ui64 offset;
                                HostBuffer page (Page::SIZE);
                                for (ui64 logPosition = log.Tell (), logSize = log.GetSize (); logPosition < logSize;) {
                                    log >> offset;
                                    logPosition += UI64_SIZE + log.Read (page.GetDataPtr (), Page::SIZE);
                                    if (offset < size) {
                                        ui64 available = MIN (size - offset, Page::SIZE);
                                        if (available != 0) {
                                            file.Seek (offset, SEEK_SET);
                                            file.Write (page.GetDataPtr (), available);
                                        }
                                    }
                                }
                                file.SetSize (size);
                                file.Flush ();
                            }
                            else {
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                    "Corrupt log %s",
                                    logPath.c_str ());
                            }
                        }
                        File::Delete (logPath);
                    }
                    SetTransactionPending (false);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        void TransactedFile::AbortTransaction () {
            if (IsOpen ()) {
                if (IsTransactionPending ()) {
                    if (IsDirty ()) {
                        SetSize (File::GetSize ());
                        // If SetSize did not delete the current page and
                        // if it is dirty, it will be deleted by root.Clear.
                        if (currPage != nullptr && currPage->dirty) {
                            currPageOffset = NOFFS;
                            currPage.Reset ();
                        }
                        root.Clear ();
                        SetDirty (false);
                    }
                    std::string logPath = GetLogPath (path);
                    if (Path (logPath).Exists ()) {
                        File::Delete (logPath);
                    }
                    SetTransactionPending (false);
                    if ((allocator != nullptr && allocator->IsDirty ()) ||
                            (registry != nullptr && registry->IsDirty ())) {
                        Init (allocator, registry);
                    }
                    Produce (
                        std::bind (
                            &TransactedFileEvents::OnTransactedFileTransactionAbort,
                            std::placeholders::_1,
                            this));
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        TransactedFile::Page::SharedPtr TransactedFile::GetPage (ui64 offset) {
            LockGuard<SpinLock> guard (spinLock);
            return GetPageHelper (offset);
        }

        TransactedFile::Page::SharedPtr TransactedFile::GetPageHelper (ui64 offset) {
            ui64 pageOffset = offset & ~(Page::SIZE - 1);
            if (currPageOffset != pageOffset) {
                currPageOffset = pageOffset;
                // Give GetPage a \see{TenantFile} as it's interface is that of \see{File}.
                // If we were to pass *this, GetPage would call in to our Seek and Read.
                // And thats not what we want!
                TenantFile file (endianness, handle, path);
                // We take advantage of locality of reference and cache the last
                // page accessed in the hopes that it will be accessed next and
                // we can skip the tree walk.
                currPage = root.GetPage (file, offset, pageAllocator);
            }
            return currPage;
        }

        std::string TransactedFile::GetLogPath (const std::string &path) {
            std::string name = Path (path).GetFullFileName ();
            return path + "-" +
                GUID::FromBuffer (name.data (), name.size ()).ToHexString () + ".log";
        }

        SimpleTransactedFile::SimpleTransactedFile (
                Endianness endianness,
                const std::string &path,
                Flags32 flags,
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry) :
                TransactedFile (endianness) {
            SimpleOpen (path, flags, allocator, registry);
        }

        void SimpleTransactedFile::SimpleOpen (
                const std::string &path,
                Flags32 flags,
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry) {
        #if defined (TOOLCHAIN_OS_Windows)
            DWORD dwDesiredAccess = 0;
            DWORD dwShareMode = 0;
            if (flags.Test (SimpleFile::ReadOnly)) {
                dwDesiredAccess |= GENERIC_READ;
                dwShareMode |= FILE_SHARE_READ;
            }
            if (flags.Test (SimpleFile::WriteOnly)) {
                dwDesiredAccess |= GENERIC_WRITE;
                dwShareMode |= FILE_SHARE_WRITE | FILE_SHARE_DELETE;
            }
            if (flags.Test (SimpleFile::Append)) {
                dwDesiredAccess |= FILE_APPEND_DATA;
            }
            DWORD dwCreationDisposition = 0;
            if (flags.Test (SimpleFile::Create)) {
                if (flags.Test (SimpleFile::Truncate)) {
                    dwCreationDisposition |= CREATE_ALWAYS;
                }
                else {
                    dwCreationDisposition |= OPEN_ALWAYS;
                }
            }
            else if (flags.Test (SimpleFile::Truncate)) {
                dwCreationDisposition |= TRUNCATE_EXISTING;
            }
            else {
                dwCreationDisposition |= OPEN_EXISTING;
            }
            DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
            TransactedFile::OpenEx (
                path,
                dwDesiredAccess,
                dwShareMode,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                allocator,
                registry);
        #else // defined (TOOLCHAIN_OS_Windows)
            i32 flags_ = 0;
            if (flags.Test (SimpleFile::ReadOnly)) {
                if (flags.Test (SimpleFile::WriteOnly)) {
                    flags_ |= O_RDWR;
                }
                else {
                    flags_ |= O_RDONLY;
                }
            }
            else if (flags.Test (SimpleFile::WriteOnly)) {
                flags_ |= O_WRONLY;
            }
            if (flags.Test (SimpleFile::Create)) {
                flags_ |= O_CREAT;
            }
            if (flags.Test (SimpleFile::Truncate)) {
                flags_ |= O_TRUNC;
            }
            if (flags.Test (SimpleFile::Append)) {
                flags_ |= O_APPEND;
            }
            i32 mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            TransactedFile::OpenEx (path, flags_, mode, allocator, registry);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
