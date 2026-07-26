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
#include "thekogans/util/TransactedFileBTreeAllocator.h"
#include "thekogans/util/TransactedFileBTreeRegistry.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (
            thekogans::util::TransactedFile,
            Serializer::TYPE, RandomSeekSerializer::TYPE)

        const int TransactedFile::COMMIT_PHASE_1 = 1;
        const int TransactedFile::COMMIT_PHASE_2 = 2;

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_T (TransactedFileAddressSpaceType::Page)

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
            // We must account for objects that dirty other objects during commit.
            // We keep looping, collecting the newly dirtied ones and make them
            // clean again. Eventually the noise will settle down. Even though
            // this might look cheotic and unpredictable, in reallity I find this
            // much more reasuring then devising ellaborate synchonizaton schemes.
            while (file.GetSubscriberCount () != 0) {
                std::vector<SharedSubscriberInfo> subscribers;
                file.GetSubscribers (subscribers, true);
                for (std::size_t i = 0, count = subscribers.size (); i < count; ++i) {
                    subscribers[i].second->DeliverEvent (
                        std::bind (
                            &TransactedFileEvents::OnTransactedFileTransactionCommit,
                            std::placeholders::_1,
                            &file,
                            COMMIT_PHASE_1),
                        subscribers[i].first);
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
            }
            file.Commit (clearCache);
        }

        void TransactedFile::TransactionParticipant::SetDirty (bool dirty) {
            if (dirty) {
                Subscribe (*file);
            }
            else {
                Unsubscribe (*file);
            }
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
                pageMap.Reset (
                    new TransactedFileAddressSpaceType (
                        32, 8, 20, GetPhysicalSectorSize (handle)));
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
            CommitLog (path);
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
            pageMap.Reset (
                new TransactedFileAddressSpaceType (
                    32, 8, 20, GetPhysicalSectorSize (handle)));
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
                TransactedFileAddressSpaceType::AddressType offset,
                void *buffer,
                std::size_t count) {
            if (offset < size) {
                LockGuard<SpinLock> guard (spinLock);
                if (IsOpen ()) {
                    TransactedFileAddressSpaceType::AddressType available = size - offset;
                    if (count > available) {
                        count = available;
                    }
                    return pageMap->Read (offset, this, buffer, count);
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
                }
            }
            return 0;
        }

        std::size_t TransactedFile::WriteEx (
                TransactedFileAddressSpaceType::AddressType offset,
                const void *buffer,
                std::size_t count) {
            LockGuard<SpinLock> guard (spinLock);
            if (IsOpen ()) {
                count = pageMap->Write (offset, this, buffer, count);
                offset += count;
                if (size < offset) {
                    size = offset;
                }
                return count;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        TransactedFileAddressSpaceType::AddressType TransactedFile::Grow (
                TransactedFileAddressSpaceType::AddressType amount) {
            LockGuard<SpinLock> guard (spinLock);
            if (IsOpen ()) {
                TransactedFileAddressSpaceType::AddressType oldSize = size;
                size += MIN (pageMap->GetMaxOffset () - size, amount);
                return oldSize;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        TransactedFileAddressSpaceType::AddressType TransactedFile::Shrink (
                TransactedFileAddressSpaceType::AddressType amount) {
            LockGuard<SpinLock> guard (spinLock);
            if (IsOpen ()) {
                size -= MIN (amount, size);
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
            Transaction transaction (*this);
            if (GetSize () == 0) {
                if (allocator_ != nullptr) {
                    allocator_.Reset (new TransactedFileBTreeAllocator);
                }
                // Initialize the first block.
                Allocator::Block block (
                    *this,
                    Allocator::Block::HEADER_SIZE,
                    0,
                    allocator_->GetSize ());
                // For performance reasons Range assumes that all
                // reads and writes are within file bounds. We set
                // the file size here so that block.Write and range
                // insert below honor that assumption.
                Grow (Allocator::Block::SIZE + block.GetSize ());
                block.Write ();
                Range range (*this, block.GetOffset (), block.GetSize (), false);
                range << *allocator_;
            }
            {
                BlockRange range (*this, Allocator::Block::HEADER_SIZE);
                ContextGuard guard (range, SerializableHeader (), nullptr,
                    [this] (DynamicCreatable::SharedPtr dynamicCreatable) {
                        Allocator::SharedPtr allocator = dynamicCreatable;
                        if (allocator != nullptr) {
                            allocator->file = this;
                        }
                    }
                );
                range >> allocator;
                assert (allocator->file == this);
            }
            if (allocator->GetRegistryOffset () == 0) {
                if (registry_ != nullptr) {
                    registry_.Reset (new TransactedFileBTreeRegistry);
                }
                allocator->SetRegistryOffset (
                    allocator->Alloc (registry_->GetSize ()));
                BlockRange range (*this, allocator->GetRegistryOffset (), false);
                range << *registry_;
            }
            {
                BlockRange range (*this, allocator->GetRegistryOffset ());
                ContextGuard guard (range, SerializableHeader (), nullptr,
                    [this] (DynamicCreatable::SharedPtr dynamicCreatable) {
                        Registry::SharedPtr registry = dynamicCreatable;
                        if (registry != nullptr) {
                            registry->file = this;
                        }
                    }
                );
                range >> registry;
                assert (registry->file == this);
            }
            transaction.Commit ();
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
                        // just being patched up with dirty tiles. Obviously it is assumed
                        // to be the same as the log endianness.
                        log.endianness = GuestEndian;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Corrupt log %s",
                            logPath.c_str ());
                    }
                    ui32 count;
                    TransactedFileAddressSpaceType::AddressType size;
                    TransactedFileAddressSpaceType::AddressType pageSize;
                    log >> count >> size >> pageSize;
                    TransactedFileAddressSpaceType::AddressType offset;
                    HostBuffer page (pageSize);
                    while (count-- != 0) {
                        log >> offset;
                        log.Read (page.GetDataPtr (), pageSize);
                        file.Seek (offset, SEEK_SET);
                        file.Write (page.GetDataPtr (), pageSize);
                    }
                    file.SetSize (size);
                    file.Flush ();
                }
                Delete (logPath);
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
                    log << (ui32)0 << (ui32)0 << size <<
                        (TransactedFileAddressSpaceType::AddressType)pageMap->GetPageSize ();
                    std::size_t count = pageMap->Log (log);
                    log.Seek (0, SEEK_SET);
                    log << MAGIC32 << (ui32)count;
                }
                pageMap->Flush (*this, clearCache);
                SetSize (size);
                Flush ();
                Delete (logPath);
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
                pageMap->Clear (true);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        TransactedFileAddressSpaceType::Page::SharedPtr TransactedFile::GetPage (
                TransactedFileAddressSpaceType::AddressType offset) {
            LockGuard<SpinLock> guard (spinLock);
            return pageMap->GetPage (offset, this);
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
