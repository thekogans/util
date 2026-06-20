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

        TransactedFile::Transaction::Transaction (TransactedFile &file_) :
                file (file_),
                guard (file.mutex) {
            file.Produce (
                std::bind (
                    &TransactedFileEvents::OnTransactedFileTransactionBegin,
                    std::placeholders::_1,
                    &file));
        }

        TransactedFile::Transaction::~Transaction () {
            file.Abort ();
            file.Produce (
                std::bind (
                    &TransactedFileEvents::OnTransactedFileTransactionAbort,
                    std::placeholders::_1,
                    &file),
                true);
        }

        void TransactedFile::Transaction::Commit (bool clearCache) {
            // We must account for objects that allocate other objects during phase 1.
            // We loop collecting new ones after every call to commit.
            std::vector<SharedSubscriberInfo> subscribers;
            subscribers.reserve (file.GetSubscriberCount ());
            while (file.GetSubscriberCount () != 0) {
                Array<SharedSubscriberInfo> subscribers_;
                std::size_t count = file.GetSubscribers (subscribers_, true);
                for (std::size_t i = 0; i < count; ++i) {
                    subscribers_[i].second->DeliverEvent (
                        std::bind (
                            &TransactedFileEvents::OnTransactedFileTransactionCommit,
                            std::placeholders::_1,
                            &file,
                            COMMIT_PHASE_1),
                        subscribers_[i].first);
                    subscribers.push_back (subscribers_[i]);
                }
            }
            for (std::size_t i = 0, count = subscribers.size (); i < count; ++i) {
                subscribers[i].second->DeliverEvent (
                    std::bind (
                        &TransactedFileEvents::OnTransactedFileTransactionCommit,
                        std::placeholders::_1,
                        &file,
                        COMMIT_PHASE_2),
                    subscribers[i].first);
            }
            file.Commit (clearCache);
        }

        bool TransactedFile::TransactionParticipant::SetDirty (bool dirty) {
            // Only subscribe @the transition from clean to dirty.
            if (!flags.Set (FLAGS_DIRTY, dirty) && dirty) {
                Subscriber<TransactedFileEvents>::Subscribe (*file);
                return true;
            }
            return false;
        }

        namespace {
            std::size_t GetPhysicalSectorSize (THEKOGANS_UTIL_HANDLE handle) {
            #if defined (TOOLCHAIN_OS_Windows)
                STORAGE_PROPERTY_QUERY query = {
                    StorageAccessAlignmentProperty,
                    PropertyStandardQuery
                };
                STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR alignment = {0};
                DWORD bytesReturned = 0;
                return DeviceIoControl (
                    handle,
                    IOCTL_STORAGE_QUERY_PROPERTY,
                    &query, sizeof (query),
                    &alignment, sizeof (alignment),
                    &bytesReturned,
                    NULL) ? alignment.BytesPerPhysicalSector : 0;
            #else // defined (TOOLCHAIN_OS_Windows)
                STAT_STRUCT buf;
                return FSTAT_FUNC (handle, &buf) == 0 ? buf.st_blksize : 0;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        }

        TransactedFile::TransactedFile (
                Endianness endianness,
                THEKOGANS_UTIL_HANDLE handle,
                const std::string &path,
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry) :
                File (endianness, handle, path),
                size (0) {
            if (IsOpen ()) {
                size = GetSize ();
                pageMap.Reset (new PageMap64 (*this, 32, 8, 20, GetPhysicalSectorSize (handle)));
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
                size (0) {
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
                CloseEx ();
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
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
            CloseEx ();
        #if defined (TOOLCHAIN_OS_Windows)
            Open (
                path,
                dwDesiredAccess,
                dwShareMode,
                dwCreationDisposition,
                dwFlagsAndAttributes | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
        #else // defined (TOOLCHAIN_OS_Windows)
        #if defined (TOOLCHAIN_OS_Linux)
            flags |= O_DIRECT;
        #endif // defined (TOOLCHAIN_OS_Linux)
            Open (path, flags, mode);
        #if defined (TOOLCHAIN_OS_OSX)
            fcntl (handle, F_NOCACHE, 1);
        #endif // defined (TOOLCHAIN_OS_OSX)
        #endif // defined (TOOLCHAIN_OS_Windows)
            size = GetSize ();
            pageMap.Reset (new PageMap64 (*this, 32, 8, 20, GetPhysicalSectorSize (handle)));
            Init (allocator, registry);
        }

        void TransactedFile::CloseEx () {
            LockGuard<SpinLock> guard (spinLock);
            if (IsOpen ()) {
                Close ();
                size = 0;
                pageMap.Reset ();
                allocator.Reset ();
                registry.Reset ();
            }
        }

        std::size_t TransactedFile::ReadEx (
                ui64 offset,
                void *buffer,
                std::size_t count) {
            if (buffer != nullptr && count > 0) {
                LockGuard<SpinLock> guard (spinLock);
                if (IsOpen ()) {
                    std::size_t countRead = 0;
                    ui8 *ptr = (ui8 *)buffer;
                    while (count > 0 && offset < size) {
                        PageMap64::Page::SharedPtr page = pageMap->GetPage (offset);
                        std::size_t pageOffset = offset - page->offset;
                        std::size_t countToRead = MIN (
                            // Calculate the amount we can read from this page...
                            MIN (pageMap->GetPageSize () - pageOffset, count),
                            // ...and clamp it to the amount left to read in the file.
                            size - page->offset);
                        std::memcpy (ptr, page->data + pageOffset, countToRead);
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
                LockGuard<SpinLock> guard (spinLock);
                if (IsOpen ()) {
                    std::size_t countWritten = 0;
                    ui8 *ptr = (ui8 *)buffer;
                    while (count > 0) {
                        PageMap64::Page::SharedPtr page = pageMap->GetPage (offset);
                        std::size_t pageOffset = offset - page->offset;
                        std::size_t countToWrite = MIN (pageMap->GetPageSize () - pageOffset, count);
                        std::memcpy (page->data + pageOffset, ptr, countToWrite);
                        page->dirty = true;
                        ptr += countToWrite;
                        countWritten += countToWrite;
                        offset += countToWrite;
                        count -= countToWrite;
                    }
                    if (size < offset) {
                        size = offset;
                    }
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

        ui64 TransactedFile::Grow (ui64 amount) {
            LockGuard<SpinLock> guard (spinLock);
            if (IsOpen ()) {
                ui32 oldSize = size;
                size += amount;
                return oldSize;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        ui64 TransactedFile::Shrink (ui64 amount) {
            LockGuard<SpinLock> guard (spinLock);
            if (IsOpen ()) {
                if (amount > size) {
                    amount = size;
                }
                size -= amount;
                pageMap->Shrink (size);
                return size;
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
                Transaction transaction (*this);
                if (GetSize () == 0) {
                    // Initialize the first block.
                    Allocator::Block block (
                        *this,
                        Allocator::Block::HEADER_SIZE,
                        0,
                        UI32_SIZE + allocator_->GetSize ());
                    // For performance reasons Range assumes that all
                    // reads and writes are within file bounds. We set
                    // the file size here so that block.Write and range
                    // insert below honor that assumption.
                    Grow (Allocator::Block::SIZE + block.GetSize ());
                    block.Write ();
                    Range range (*this, block.GetOffset (), block.GetSize (), false);
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
                transaction.Commit ();
            }
        }

        void TransactedFile::Commit (bool clearCache) {
            LockGuard<SpinLock> guard (spinLock);
            if (IsOpen ()) {
                std::string logPath = GetLogPath (path);
                {
                    SimpleFile log (
                        endianness,
                        logPath,
                        SimpleFile::ReadWrite | SimpleFile::Create | SimpleFile::Truncate);
                    log << (ui32)0 << size << (ui64)pageMap->GetPageSize ();
                    pageMap->Log (log, clearCache);
                    log.Seek (0, SEEK_SET);
                    log << MAGIC32;
                    log.Flush ();
                    ui64 size;
                    ui64 pageSize;
                    log >> size >> pageSize;
                    ui64 offset;
                    HostBuffer page (pageSize);
                    for (ui64 logPosition = log.Tell (), logSize = log.GetSize (); logPosition < logSize;) {
                        log >> offset;
                        logPosition += UI64_SIZE + log.Read (page.GetDataPtr (), pageSize);
                        Seek (offset, SEEK_SET);
                        Write (page.GetDataPtr (), pageSize);
                    }
                    SetSize (size);
                    Flush ();
                }
                File::Delete (logPath);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        void TransactedFile::Abort () {
            LockGuard<SpinLock> guard (spinLock);
            if (IsOpen ()) {
                size = GetSize ();
                pageMap->Clear (PageMap64::FLAGS_CLEAR_DIRTY);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        PageMap64::Page::SharedPtr TransactedFile::GetPage (ui64 offset) {
            LockGuard<SpinLock> guard (spinLock);
            return pageMap->GetPage (offset);
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
