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

        TransactedFile::TransactedFile (
                Endianness endianness,
                THEKOGANS_UTIL_HANDLE handle,
                const std::string &path,
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry) :
                File (endianness, handle, path),
                size (0),
                flags (0),
                pageMap (*this) {
            if (IsOpen ()) {
                size = GetSize ();
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
                i32 flags_,
                i32 mode,
            #endif // defined (TOOLCHAIN_OS_Windows)
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry) :
                File (endianness),
                size (0),
                flags (0),
                pageMap (*this) {
            OpenEx (
                path,
            #if defined (TOOLCHAIN_OS_Windows)
                dwDesiredAccess,
                dwShareMode,
                dwCreationDisposition,
                dwFlagsAndAttributes,
            #else // defined (TOOLCHAIN_OS_Windows)
                flags_,
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
                i32 flags_,
                i32 mode,
            #endif // defined (TOOLCHAIN_OS_Windows)
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry) {
            CommitLog (path);
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
            flags_ |= O_DIRECT;
        #endif // defined (TOOLCHAIN_OS_Linux)
            Open (path, flags_, mode);
        #if defined (TOOLCHAIN_OS_OSX)
            fcntl (handle, F_NOCACHE, 1);
        #endif // defined (TOOLCHAIN_OS_OSX)
        #endif // defined (TOOLCHAIN_OS_Windows)
            size = GetSize ();
            flags = 0;
            Init (allocator, registry);
        }

        void TransactedFile::CloseEx () {
            if (IsOpen ()) {
                // All transactions must be commited before file close.
                // On the other hand dirty pages get flushed out to disk
                // to mimic what File would do.
                AbortTransaction ();
                DeleteCache ();
                size = 0;
                flags = 0;
                allocator.Reset ();
                registry.Reset ();
                Close ();
            }
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
                        PageMap::Page::SharedPtr page = pageMap.GetPage (offset);
                        std::size_t pageOffset = offset - page->offset;
                        std::size_t countToRead = MIN (
                            // Calculate the amount we can read from this page...
                            MIN (pageMap.pageSize - pageOffset, count),
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
                if (IsOpen ()) {
                    LockGuard<SpinLock> guard (spinLock);
                    if (IsTransactionPending ()) {
                        std::size_t countWritten = 0;
                        ui8 *ptr = (ui8 *)buffer;
                        while (count > 0) {
                            PageMap::Page::SharedPtr page = pageMap.GetPage (offset);
                            std::size_t pageOffset = offset - page->offset;
                            std::size_t countToWrite = MIN (pageMap.pageSize - pageOffset, count);
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
                        THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void TransactedFile::FlushEx () {
            if (IsOpen ()) {
                if (IsDirty ()) {
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
                        log << MAGIC32 << (ui32)0 << size << (ui64)pageMap.pageSize;
                    }
                    pageMap.Log (log);
                    SetDirty (false);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        ui64 TransactedFile::Grow (ui64 amount) {
            if (IsOpen ()) {
                LockGuard<SpinLock> guard (spinLock);
                if (IsTransactionPending ()) {
                    ui32 oldSize = size;
                    size += amount;
                    SetDirty (true);
                    return oldSize;
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        ui64 TransactedFile::Shrink (ui64 amount) {
            if (IsOpen ()) {
                if (IsTransactionPending ()) {
                    LockGuard<SpinLock> guard (spinLock);
                    if (size >= amount) {
                        size -= amount;
                        pageMap.Shrink (size);
                        SetDirty (true);
                        return size;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        void TransactedFile::DeleteCache () {
            if (IsOpen ()) {
                LockGuard<SpinLock> guard (spinLock);
                FlushEx ();
                pageMap.Clear (true);
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
                        *this,
                        Allocator::Block::HEADER_SIZE,
                        0,
                        UI32_SIZE + allocator_->GetSize ());
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
                        ui64 pageSize;
                        log >> size >> pageSize;
                        ui64 offset;
                        HostBuffer page (pageSize);
                        for (ui64 logPosition = log.Tell (), logSize = log.GetSize (); logPosition < logSize;) {
                            log >> offset;
                            logPosition += UI64_SIZE + log.Read (page.GetDataPtr (), pageSize);
                            if (offset < size) {
                                ui64 available = MIN (size - offset, pageSize);
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
                    FlushEx ();
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
                    FlushEx ();
                    std::string logPath = GetLogPath (path);
                    if (Path (logPath).Exists ()) {
                        {
                            SimpleFile log (endianness, logPath, SimpleFile::ReadWrite);
                            ui32 magic;
                            log >> magic;
                            if (magic == MAGIC32) {
                                ui32 isClean = 1;
                                log << isClean;
                                ui64 size;
                                ui64 pageSize;
                                log >> size >> pageSize;
                                ui64 offset;
                                HostBuffer page (pageSize);
                                for (ui64 logPosition = log.Tell (), logSize = log.GetSize (); logPosition < logSize;) {
                                    log >> offset;
                                    logPosition += UI64_SIZE + log.Read (page.GetDataPtr (), pageSize);
                                    if (offset < size) {
                                        ui64 available = MIN (size - offset, pageMap.pageSize);
                                        if (available != 0) {
                                            Seek (offset, SEEK_SET);
                                            Write (page.GetDataPtr (), available);
                                        }
                                    }
                                }
                                SetSize (size);
                                Flush ();
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
                        size = GetSize ();
                        pageMap.Clear ();
                        SetDirty (false);
                    }
                    std::string logPath = GetLogPath (path);
                    if (Path (logPath).Exists ()) {
                        File::Delete (logPath);
                    }
                    SetTransactionPending (false);
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

        PageMap::Page::SharedPtr TransactedFile::GetPage (ui64 offset) {
            LockGuard<SpinLock> guard (spinLock);
            return pageMap.GetPage (offset);
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
