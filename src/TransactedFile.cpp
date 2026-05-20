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
                guard (file->GetLock ()) {
            file->BeginTransaction ();
        }

        TransactedFile::Transaction::~Transaction () {
            file->AbortTransaction ();
        }

        void TransactedFile::Transaction::Commit () {
            file->CommitTransaction ();
        }

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (TransactedFile::Buffer)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (TransactedFile::Segment)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (TransactedFile::Internal)

        TransactedFile::Segment::~Segment () {
            Delete ();
        }

        void TransactedFile::Segment::Delete () {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (buffers[i] != nullptr) {
                    DeleteBuffer (i);
                }
            }
        }

        void TransactedFile::Segment::Clear () {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (buffers[i] != nullptr) {
                    if (buffers[i]->dirty) {
                        DeleteBuffer (i);
                    }
                }
            }
        }

        void TransactedFile::Segment::Save (
                File &log,
                ui64 &count) {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (buffers[i] != nullptr) {
                    if (buffers[i]->dirty) {
                        log << buffers[i]->offset << buffers[i]->length;
                        log.Write (buffers[i]->data, buffers[i]->length);
                        buffers[i]->dirty = false;
                        ++count;
                    }
                }
            }
        }

        void TransactedFile::Segment::Flush (File &file) {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (buffers[i] != nullptr) {
                    if (buffers[i]->dirty) {
                        file.Seek (buffers[i]->offset, SEEK_SET);
                        file.Write (buffers[i]->data, buffers[i]->length);
                        buffers[i]->dirty = false;
                    }
                }
            }
        }

        bool TransactedFile::Segment::SetSize (ui64 newSize) {
            for (std::size_t i = BRANCHING_LEVEL; i-- != 0;) {
                if (buffers[i] != nullptr) {
                    if (buffers[i]->offset > newSize) {
                        DeleteBuffer (i);
                    }
                    else {
                        if (buffers[i]->offset + buffers[i]->length > newSize) {
                            buffers[i]->length = newSize - buffers[i]->offset;
                            if (buffers[i]->length == 0) {
                                DeleteBuffer (i);
                            }
                        }
                        return false;
                    }
                }
            }
            return true;
        }

        TransactedFile::Internal::~Internal () {
            Delete ();
        }

        void TransactedFile::Internal::Delete () {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (nodes[i] != nullptr) {
                    DeleteNode (i);
                }
            }
        }

        void TransactedFile::Internal::Clear () {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (nodes[i] != nullptr) {
                    nodes[i]->Clear ();
                }
            }
        }

        void TransactedFile::Internal::Save (
                File &log,
                ui64 &count) {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (nodes[i] != nullptr) {
                    nodes[i]->Save (log, count);
                }
            }
        }

        void TransactedFile::Internal::Flush (File &file) {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (nodes[i] != nullptr) {
                    nodes[i]->Flush (file);
                }
            }
        }

        bool TransactedFile::Internal::SetSize (ui64 newSize) {
            for (std::size_t i = BRANCHING_LEVEL; i-- != 0;) {
                if (nodes[i] != nullptr && !nodes[i]->SetSize (newSize)) {
                    return false;
                }
                DeleteNode (i);
            }
            return true;
        }

        TransactedFile::Internal::Node *TransactedFile::Internal::GetNode (
                ui8 index,
                bool segment) {
            if (nodes[index] == nullptr) {
                if (segment) {
                    nodes[index] = new Segment;
                }
                else {
                    nodes[index] = new Internal;
                }
            }
            return nodes[index];
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
                    std::size_t countRead = 0;
                    ui8 *ptr = (ui8 *)buffer;
                    while (count > 0 && offset < size) {
                        Buffer *buffer_ = GetBuffer (offset);
                        std::size_t bufferOffset = offset - buffer_->offset;
                        std::size_t countToRead = MIN (buffer_->length - bufferOffset, count);
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

        std::size_t TransactedFile::Read (
                void *buffer,
                std::size_t count) {
            std::size_t countRead = ReadEx (position, buffer, count);
            position += countRead;
            return countRead;
        }

        std::size_t TransactedFile::WriteEx (
                ui64 offset,
                const void *buffer,
                std::size_t count) {
            if (buffer != nullptr && count > 0) {
                if (IsOpen ()) {
                    std::size_t countWritten = 0;
                    ui8 *ptr = (ui8 *)buffer;
                    while (count > 0) {
                        Buffer *buffer_ = GetBuffer (offset);
                        std::size_t bufferOffset = offset - buffer_->offset;
                        if (buffer_->length < bufferOffset + count) {
                            buffer_->length = MIN (bufferOffset + count, Buffer::SIZE);
                        }
                        std::size_t countToWrite = MIN (buffer_->length - bufferOffset, count);
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
                i32 flags,
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
            File::Open (path, flags, mode);
        #endif // defined (TOOLCHAIN_OS_Windows)
            position = File::Tell ();
            sizeOnDisk = File::GetSize ();
            size = sizeOnDisk;
            flags = 0;
            currBufferOffset = NOFFS;
            currBuffer = nullptr;
        }

        void TransactedFile::Close () {
            if (IsOpen ()) {
                // All transactions must be commited before file close.
                // On the other hand dirty pages get flushed out to disk
                // to mimic what File would do.
                AbortTransaction ();
                DeleteCache ();
                position = 0;
                sizeOnDisk = 0;
                size = 0;
                flags = 0;
                currBufferOffset = NOFFS;
                currBuffer = nullptr;
                allocator.Reset ();
                registry.Reset ();
                File::Close ();
            }
        }

        void TransactedFile::Flush () {
            if (IsOpen ()) {
                if (IsDirty ()) {
                    if (IsTransactionPending ()) {
                        std::string logPath = GetLogPath (path);
                        SimpleFile log (
                            endianness,
                            logPath,
                            SimpleFile::ReadWrite | SimpleFile::Create);
                        ui32 isClean = 0;
                        ui64 count = 0;
                        if (log.GetSize () > 0) {
                            ui32 magic;
                            log >> magic;
                            if (magic == MAGIC32) {
                                log >> isClean >> count;
                                log.Seek (0, SEEK_END);
                            }
                            else {
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                    "Corrupt log %s",
                                    logPath.c_str ());
                            }
                        }
                        else {
                            log << MAGIC32 << isClean << count << sizeOnDisk << size;
                        }
                        root.Save (log, count);
                        log.Seek (0, SEEK_SET);
                        log << MAGIC32 << isClean << count << sizeOnDisk << size;
                    }
                    else {
                        {
                            // Give Flush a \see{TenantFile} as it's interface is that of \see{File}.
                            // If we were to pass *this, Flush would call in to our Seek and Write.
                            // And thats not what we want!
                            TenantFile file (endianness, handle, path);
                            root.Flush (file);
                        }
                        if (sizeOnDisk != size) {
                            File::SetSize (size);
                            sizeOnDisk = size;
                        }
                        File::Flush ();
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
                        root.SetSize (newSize);
                        // If new size is <= current position,
                        // currBuffer will be deleted by root.SetSize.
                        if ((ui64)position >= newSize) {
                            currBufferOffset = NOFFS;
                            currBuffer = nullptr;
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

        void TransactedFile::DeleteCache () {
            if (IsOpen ()) {
                Flush ();
                root.Delete ();
                currBufferOffset = NOFFS;
                currBuffer = nullptr;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        void TransactedFile::Init () {
            if (allocator != nullptr) {
                if (GetSize () == 0) {
                    allocator->file = this;
                    UnsafeWriteOnlyRange buffer (*this, 0, UI32_SIZE + allocator->GetSize ());
                    buffer << MAGIC32 << *allocator;
                    if (registry != nullptr) {
                        registry->Init (this);
                    }
                }
                else {
                    ui32 magic;
                    *this >> magic;
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
                    SerializableHeader header;
                    *this >> header;
                    allocator = Allocator::CreateType (
                        header.type.c_str (),
                        [this] (DynamicCreatable::SharedPtr dynamicCreatble) {
                            Allocator::SharedPtr allocator = dynamicCreatble;
                            if (allocator != nullptr) {
                                allocator->file = this;
                            }
                        }
                    );
                    if (allocator != nullptr) {
                        UnsafeReadOnlyRange buffer (*this, Tell (), header.size);
                        allocator->Read (header, buffer);
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
                        ui64 count;
                        ui64 sizeOnDisk;
                        ui64 size;
                        log >> count >> sizeOnDisk >> size;
                        std::unique_ptr<Buffer> buffer (new Buffer);
                        while (count-- != 0) {
                            log >> buffer->offset >> buffer->length;
                            log.Read (buffer->data, buffer->length);
                            if (buffer->offset < size) {
                                ui64 length = size - buffer->offset;
                                if (length > buffer->length) {
                                    length = buffer->length;
                                }
                                if (length != 0) {
                                    file.Seek (buffer->offset, SEEK_SET);
                                    file.Write (buffer->data, length);
                                }
                            }
                        }
                        if (sizeOnDisk != size) {
                            file.SetSize (size);
                        }
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
                    SetTransactionPending (true);
                    Produce (
                        std::bind (
                            &TransactedFileEvents::OnTransactedFileTransactionBegin,
                            std::placeholders::_1,
                            this));
                    Flush ();
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
                                ui64 count;
                                log >> count >> sizeOnDisk >> size;
                                std::unique_ptr<Buffer> buffer (new Buffer);
                                while (count-- != 0) {
                                    log >> buffer->offset >> buffer->length;
                                    log.Read (buffer->data, buffer->length);
                                    if (buffer->offset < size) {
                                        ui64 length = size - buffer->offset;
                                        if (length > buffer->length) {
                                            length = buffer->length;
                                        }
                                        if (length != 0) {
                                            File::Seek (buffer->offset, SEEK_SET);
                                            File::Write (buffer->data, length);
                                        }
                                    }
                                }
                                if (sizeOnDisk != size) {
                                    File::SetSize (size);
                                    sizeOnDisk = size;
                                }
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
                        SetSize (sizeOnDisk);
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

        TransactedFile::Buffer *TransactedFile::GetBuffer (ui64 offset) {
            ui64 bufferOffset = offset & ~(Buffer::SIZE - 1);
            if (currBufferOffset != bufferOffset) {
                // --
                ui32 segmentIndex = THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (offset, 0);
                Node *internal = root.GetNode (
                    THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 0));
                internal = internal->GetNode (
                    THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 1));
                internal = internal->GetNode (
                    THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 2));
                // Leafs are segments.
                Segment *segment = (Segment *)internal->GetNode (
                    THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 3), true);
                // -- We've just sparsely traversed the first 4
                // layers of the 5 layer 64 bit index.
                // --
                ui32 bufferIndex =
                    THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (offset, 1) >> Buffer::SHIFT_COUNT;
                if (segment->buffers[bufferIndex] == nullptr) {
                    ui64 bufferLength = MIN (
                        bufferOffset < size ? size - bufferOffset : 0,
                        Buffer::SIZE);
                    segment->buffers[bufferIndex] = new Buffer (bufferOffset, bufferLength);
                    if (bufferOffset < sizeOnDisk) {
                        File::Seek (bufferOffset, SEEK_SET);
                        File::Read (
                            segment->buffers[bufferIndex]->data,
                            MIN (sizeOnDisk - bufferOffset, bufferLength));
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
                Flags32 flags) :
                TransactedFile (endianness) {
            SimpleOpen (path, flags);
        }

        void SimpleTransactedFile::SimpleOpen (
                const std::string &path,
                Flags32 flags) {
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
            TransactedFile::Open (
                path,
                dwDesiredAccess,
                dwShareMode,
                dwCreationDisposition,
                dwFlagsAndAttributes);
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
            TransactedFile::Open (path, flags_, mode);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
