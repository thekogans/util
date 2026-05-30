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
#include <string>
#include "thekogans/util/Heap.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Constants.h"
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

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (TransactedFile::Buffer)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (TransactedFile::Segment)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (TransactedFile::Internal)

        bool TransactedFile::Segment::Clear () {
            bool clean = false;
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (buffers[i] != nullptr) {
                    if (buffers[i]->dirty) {
                        buffers[i].Reset ();
                    }
                    else {
                        clean = true;
                    }
                }
            }
            return clean;
        }

        void TransactedFile::Segment::Save (File &log) {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (buffers[i] != nullptr && buffers[i]->dirty) {
                    log << buffers[i]->offset;
                    log.Write (buffers[i]->data, Buffer::SIZE);
                    buffers[i]->dirty = false;
                }
            }
        }

        void TransactedFile::Segment::Flush (
                File &file,
                ui64 size) {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (buffers[i] != nullptr && buffers[i]->dirty) {
                    file.Seek (buffers[i]->offset, SEEK_SET);
                    file.Write (buffers[i]->data, MIN (size - buffers[i]->offset, Buffer::SIZE));
                    buffers[i]->dirty = false;
                }
            }
        }

        bool TransactedFile::Segment::SetSize (ui64 newSize) {
            for (std::size_t i = BRANCHING_LEVEL; i-- != 0;) {
                if (buffers[i] != nullptr) {
                    if (buffers[i]->offset >= newSize) {
                        buffers[i].Reset ();
                    }
                    else {
                        ui64 consumed = newSize - buffers[i]->offset;
                        if (consumed < Buffer::SIZE) {
                            std::memset (
                                buffers[i]->data + consumed, 0, Buffer::SIZE - consumed);
                        }
                        return false;
                    }
                }
            }
            return true;
        }

        void TransactedFile::Internal::Delete () {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                nodes[i].Reset ();
            }
        }

        bool TransactedFile::Internal::Clear () {
            bool clean = false;
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (nodes[i] != nullptr) {
                    if (nodes[i]->Clear ()) {
                        nodes[i].Reset ();
                    }
                    else {
                        clean = true;
                    }
                }
            }
            return clean;
        }

        void TransactedFile::Internal::Save (File &log) {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (nodes[i] != nullptr) {
                    nodes[i]->Save (log);
                }
            }
        }

        void TransactedFile::Internal::Flush (
                File &file,
                ui64 size) {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (nodes[i] != nullptr) {
                    nodes[i]->Flush (file, size);
                }
            }
        }

        bool TransactedFile::Internal::SetSize (ui64 newSize) {
            for (std::size_t i = BRANCHING_LEVEL; i-- != 0;) {
                if (nodes[i] != nullptr && !nodes[i]->SetSize (newSize)) {
                    return false;
                }
                nodes[i].Reset ();
            }
            return true;
        }

        TransactedFile::Internal::Node::SharedPtr TransactedFile::Internal::GetNode (
                ui8 index,
                bool segment) {
            if (nodes[index] == nullptr) {
                if (segment) {
                    nodes[index].Reset (new Segment);
                }
                else {
                    nodes[index].Reset (new Internal);
                }
            }
            return nodes[index];
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
                currBufferOffset (NOFFS) {
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
                currBufferOffset (NOFFS) {
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
                        Buffer::SharedPtr buffer_ = GetBufferHelper (offset);
                        std::size_t bufferOffset = offset - buffer_->offset;
                        std::size_t countToRead = MIN (
                            // Calculate the amount we can read from this buffer...
                            MIN (Buffer::SIZE - bufferOffset, count),
                            // ...and clamp that to the amount left to read in the file.
                            size - buffer_->offset);
                        std::memcpy (ptr, buffer_->data + bufferOffset, countToRead);
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
                        Buffer::SharedPtr buffer_ = GetBufferHelper (offset);
                        std::size_t bufferOffset = offset - buffer_->offset;
                        std::size_t countToWrite = MIN (Buffer::SIZE - bufferOffset, count);
                        std::memcpy (buffer_->data + bufferOffset, ptr, countToWrite);
                        buffer_->dirty = true;
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
                Allocator::SharedPtr allocator_,
                Registry::SharedPtr registry_) {
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
            Init (allocator_, registry_);
        }

        ui32 TransactedFile::Grow (ui64 amount) {
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

        void TransactedFile::DeleteCache () {
            if (IsOpen ()) {
                Flush ();
                root.Delete ();
                currBufferOffset = NOFFS;
                currBuffer.Reset ();
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
                dwFlagsAndAttributes);
        #else // defined (TOOLCHAIN_OS_Windows)
            File::Open (path, flags_, mode);
        #endif // defined (TOOLCHAIN_OS_Windows)
            position = File::Tell ();
            size = File::GetSize ();
            flags = 0;
            currBufferOffset = NOFFS;
            currBuffer.Reset ();
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
                currBufferOffset = NOFFS;
                currBuffer.Reset ();
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
                    std::size_t registrySize = UI32_SIZE + registry->GetSize ();
                    allocator->SetRegistryOffset (
                        allocator->Realloc (allocator->GetRegistryOffset (), registrySize));
                    Range range (*this, allocator->GetRegistryOffset (), registrySize, false);
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
                        root.Save (log);
                    }
                    else {
                        // Give Flush a \see{TenantFile} as it's interface is that of \see{File}.
                        // If we were to pass *this, Flush would call in to our Seek and Write.
                        // And thats not what we want!
                        TenantFile file (endianness, handle, path);
                        root.Flush (file, size);
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
                LockGuard<SpinLock> guard (spinLock);
                if (size != newSize) {
                    if (size > newSize) {
                        // shrinking
                        root.SetSize (newSize);
                        // If new size is <= current buffer offset,
                        // currBuffer will be deleted by root.SetSize.
                        if (currBufferOffset >= newSize) {
                            currBufferOffset = NOFFS;
                            currBuffer.Reset ();
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
                    std::size_t allocatorSize = UI32_SIZE + allocator_->GetSize ();
                    SetSize (Allocator::Block::SIZE + allocatorSize);
                    Allocator::Block block (Allocator::Block::HEADER_SIZE, 0, allocatorSize);
                    block.Write (*this);
                    Range range (*this, Allocator::Block::HEADER_SIZE, allocatorSize, false);
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
                    std::size_t registrySize = UI32_SIZE + registry_->GetSize ();
                    allocator->SetRegistryOffset (allocator->Alloc (registrySize));
                    Range range (*this, allocator->GetRegistryOffset (), registrySize, false);
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
                        HostBuffer buffer (Buffer::SIZE);
                        for (ui64 logPosition = log.Tell (), logSize = log.GetSize (); logPosition < logSize;) {
                            log >> offset;
                            logPosition += UI64_SIZE + log.Read (buffer.GetDataPtr (), Buffer::SIZE);
                            if (offset < size) {
                                ui64 available = MIN (size - offset, Buffer::SIZE);
                                if (available != 0) {
                                    file.Seek (offset, SEEK_SET);
                                    file.Write (buffer.GetDataPtr (), available);
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
                    Produce (
                        std::bind (
                            &TransactedFileEvents::OnTransactedFileTransactionCommit,
                            std::placeholders::_1,
                            this,
                            COMMIT_PHASE_1));
                    Produce (
                        std::bind (
                            &TransactedFileEvents::OnTransactedFileTransactionCommit,
                            std::placeholders::_1,
                            this,
                            COMMIT_PHASE_2));
                    Flush ();
                    std::string logPath = GetLogPath (path);
                    if (Path (logPath).Exists ()) {
                        {
                            SimpleFile log (endianness, logPath, SimpleFile::ReadWrite);
                            ui32 magic;
                            log >> magic;
                            if (magic == MAGIC32) {
                                ui32 isClean = 1;
                                log << isClean;
                                log >> size;
                                ui64 offset;
                                HostBuffer buffer (Buffer::SIZE);
                                for (ui64 logPosition = log.Tell (), logSize = log.GetSize (); logPosition < logSize;) {
                                    log >> offset;
                                    logPosition += UI64_SIZE + log.Read (buffer.GetDataPtr (), Buffer::SIZE);
                                    if (offset < size) {
                                        ui64 available = MIN (size - offset, Buffer::SIZE);
                                        if (available != 0) {
                                            File::Seek (offset, SEEK_SET);
                                            File::Write (buffer.GetDataPtr (), available);
                                        }
                                    }
                                }
                                File::SetSize (size);
                                File::Flush ();
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
                        // If SetSize did not delete the current buffer and
                        // if it is dirty, it will be deleted bu root.Clear.
                        if (currBuffer != nullptr && currBuffer->dirty) {
                            currBufferOffset = NOFFS;
                            currBuffer = nullptr;
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

        TransactedFile::Buffer::SharedPtr TransactedFile::GetBuffer (ui64 offset) {
            LockGuard<SpinLock> guard (spinLock);
            return GetBufferHelper (offset);
        }

        TransactedFile::Buffer::SharedPtr TransactedFile::GetBufferHelper (ui64 offset) {
            ui64 bufferOffset = offset & ~(Buffer::SIZE - 1);
            if (currBufferOffset != bufferOffset) {
                // --
                ui32 segmentIndex = THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (offset, 0);
                Node::SharedPtr internal = root.GetNode (
                    THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 0));
                internal = internal->GetNode (
                    THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 1));
                internal = internal->GetNode (
                    THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 2));
                // Leafs are segments.
                Segment::SharedPtr segment = internal->GetNode (
                    THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 3), true);
                // -- We've just sparsely traversed the first 4
                // layers of the 5 layer 64 bit index.
                // --
                ui32 bufferIndex =
                    THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (offset, 1) >> Buffer::SHIFT_COUNT;
                if (segment->buffers[bufferIndex] == nullptr) {
                    segment->buffers[bufferIndex].Reset (new Buffer (bufferOffset));
                    ui64 sizeOnDisk = File::GetSize ();
                    if (bufferOffset < sizeOnDisk) {
                        File::Seek (bufferOffset, SEEK_SET);
                        ui64 countRead = File::Read (
                            segment->buffers[bufferIndex]->data,
                            MIN (sizeOnDisk - bufferOffset, Buffer::SIZE));
                        std::memset (
                            segment->buffers[bufferIndex]->data + countRead, 0, Buffer::SIZE - countRead);
                    }
                }
                // -- After potentially creating the buffer above,
                // we've arrived at the end of the 5 layer deep sparse index.
                // This mapping is constant (with the create code being amortized
                // accross multiple buffer accesses). It's O(c) where c = 5 shifts.
                // We take advantage of locality of reference and cache the last
                // buffer accessed in the hopes that it will be accessed next and
                // we can skip the tree walk.
                currBufferOffset = bufferOffset;
                currBuffer = segment->buffers[bufferIndex];
            }
            return currBuffer;
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
                Registry::SharedPtr regitry) :
                TransactedFile (endianness) {
            SimpleOpen (path, flags, allocator, regitry);
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
