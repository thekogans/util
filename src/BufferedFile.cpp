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

#include <errno.h>
#include "thekogans/util/Path.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/BufferedFile.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (
            thekogans::util::BufferedFile,
            Serializer::TYPE, RandomSeekSerializer::TYPE)

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (BufferedFile::Buffer)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (BufferedFile::Segment)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (BufferedFile::Internal)

        BufferedFile::Segment::~Segment () {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (buffers[i] != nullptr) {
                    delete buffers[i];
                }
            }
        }

        void BufferedFile::Segment::Flush (File &file) {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (buffers[i] != nullptr) {
                    if (buffers[i]->dirty) {
                        file.Seek (buffers[i]->offset, SEEK_SET);
                        file.Write (buffers[i]->data, buffers[i]->length);
                    }
                    delete buffers[i];
                    buffers[i] = nullptr;
                }
            }
        }

        bool BufferedFile::Segment::SetSize (ui64 newSize) {
            for (std::size_t i = BRANCHING_LEVEL; i-- != 0;) {
                if (buffers[i] != nullptr) {
                    if (buffers[i]->offset > newSize) {
                        delete buffers[i];
                        buffers[i] = nullptr;
                    }
                    else {
                        if (buffers[i]->offset + buffers[i]->length > newSize) {
                            buffers[i]->length = newSize - buffers[i]->offset;
                            if (buffers[i]->length == 0) {
                                delete buffers[i];
                                buffers[i] = nullptr;
                            }
                        }
                        return false;
                    }
                }
            }
            return true;
        }

        BufferedFile::Internal::~Internal () {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (nodes[i] != nullptr) {
                    delete nodes[i];
                }
            }
        }

        void BufferedFile::Internal::Flush (File &file) {
            for (std::size_t i = 0; i < BRANCHING_LEVEL; ++i) {
                if (nodes[i] != nullptr) {
                    nodes[i]->Flush (file);
                    delete nodes[i];
                    nodes[i] = nullptr;
                }
            }
        }

        bool BufferedFile::Internal::SetSize (ui64 newSize) {
            for (std::size_t i = BRANCHING_LEVEL; i-- != 0;) {
                if (nodes[i] != nullptr && !nodes[i]->SetSize (newSize)) {
                    return false;
                }
                delete nodes[i];
                nodes[i] = nullptr;
            }
            return true;
        }

        BufferedFile::Internal::Node *BufferedFile::Internal::GetNode (
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

        BufferedFile::~BufferedFile () {
            THEKOGANS_UTIL_TRY {
                Close ();
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        std::size_t BufferedFile::Read (
                void *buffer,
                std::size_t count) {
            std::size_t countRead = 0;
            ui8 *ptr = (ui8 *)buffer;
            while (count > 0 && (ui64)position < size) {
                Buffer *buffer_ = GetBuffer ();
                std::size_t index = position - buffer_->offset;
                std::size_t countToRead = MIN (buffer_->length - index, count);
                memcpy (ptr, &buffer_->data[index], countToRead);
                ptr += countToRead;
                countRead += countToRead;
                position += countToRead;
                count -= countToRead;
            }
            return countRead;
        }

        std::size_t BufferedFile::Write (
                const void *buffer,
                std::size_t count) {
            std::size_t countWritten = 0;
            ui8 *ptr = (ui8 *)buffer;
            while (count > 0) {
                Buffer *buffer_ = GetBuffer ();
                std::size_t index = position - buffer_->offset;
                if (index + count > buffer_->length) {
                    buffer_->length = MIN (index + count, Buffer::PAGE_SIZE);
                }
                std::size_t countToWrite = MIN (buffer_->length - index, count);
                memcpy (&buffer_->data[index], ptr, countToWrite);
                buffer_->dirty = true;
                ptr += countToWrite;
                countWritten += countToWrite;
                position += countToWrite;
                if (size < (ui64)position) {
                    size = position;
                }
                count -= countToWrite;
            }
            return countWritten;
        }

        i64 BufferedFile::Seek (
                i64 offset,
                i32 fromWhere) {
            if (!IsOpen ()) {
                errno = EBADF;
                return -1;
            }
            switch (fromWhere) {
                case SEEK_SET:
                    if (offset < 0) {
                        errno = EOVERFLOW;
                        return -1;
                    }
                    position = offset;
                    break;
                case SEEK_CUR:
                    if (position + offset < 0) {
                        errno = EOVERFLOW;
                        return -1;
                    }
                    position += offset;
                    break;
                case SEEK_END:
                    if ((i64)size + offset < 0) {
                        errno = EOVERFLOW;
                        return -1;
                    }
                    position = (i64)size + offset;
                    break;
                default:
                    errno = EINVAL;
                    return -1;
            }
            errno = 0;
            return position;
        }

    #if defined (TOOLCHAIN_OS_Windows)
        void BufferedFile::Open (
                const std::string &path,
                DWORD dwDesiredAccess,
                DWORD dwShareMode,
                DWORD dwCreationDisposition,
                DWORD dwFlagsAndAttributes) {
    #else // defined (TOOLCHAIN_OS_Windows)
        void BufferedFile::Open (
                const std::string &path,
                i32 flags,
                i32 mode) {
    #endif // defined (TOOLCHAIN_OS_Windows)
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
            originalSize = File::GetSize ();
            size = originalSize;
        }

        void BufferedFile::Close () {
            if (IsOpen ()) {
                Flush ();
                File::Close ();
            }
        }

        void BufferedFile::Flush () {
            TenantFile file (endianness, handle, path);
            for (std::size_t i = 0; i < Internal::BRANCHING_LEVEL; ++i) {
                if (root.nodes[i] != nullptr) {
                    root.nodes[i]->Flush (file);
                }
            }
            if (originalSize != size) {
                File::SetSize (size);
                originalSize = size;
            }
            File::Flush ();
        }

        void BufferedFile::SetSize (ui64 newSize) {
            if (size > newSize) {
                // shrinking
                for (std::size_t i = Internal::BRANCHING_LEVEL; i-- != 0;) {
                    if (root.nodes[i] != nullptr && !root.nodes[i]->SetSize (newSize)) {
                        break;
                    }
                }
            }
            size = newSize;
        }

        BufferedFile::Buffer *BufferedFile::GetBuffer () {
            // --
            ui32 segmentIndex = THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (position, 0);
            Node *internal = root.GetNode (
                THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 0));
            internal = internal->GetNode (
                THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 1));
            internal = internal->GetNode (
                THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 2));
            Segment *segment = (Segment *)internal->GetNode (
                THEKOGANS_UTIL_UI32_GET_UI8_AT_INDEX (segmentIndex, 3), true);
            // -- We've just sparsely traversed the first 4
            // layers of the 5 layer 64 bit index.
            // --
            ui32 bufferIndex =
                THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (position, 1) >> Buffer::SHIFT_COUNT;
            if (segment->buffers[bufferIndex] == nullptr) {
                static const ui64 OFFSET_MASK = Buffer::PAGE_SIZE - 1;
                ui64 bufferOffset = position & ~OFFSET_MASK;
                ui64 bufferLength = bufferOffset < size ? size - bufferOffset : 0;
                if (bufferLength > Buffer::PAGE_SIZE) {
                    bufferLength = Buffer::PAGE_SIZE;
                }
                segment->buffers[bufferIndex] = new Buffer (bufferOffset, bufferLength);
                if (bufferOffset < originalSize) {
                    File::Seek (bufferOffset, SEEK_SET);
                    ui64 readLength = originalSize - bufferOffset;
                    if (readLength > bufferLength) {
                        readLength = bufferLength;
                    }
                    File::Read (segment->buffers[bufferIndex]->data, readLength);
                }
            }
            // -- After potentially creating the buffer above,
            // we've arrived at the end of the 5 later deep sparse index.
            // This mapping is constant (with the create code being amortized
            // accross multiple buffer accesses). It's O(c) where c = a few shifts.
            return segment->buffers[bufferIndex];
        }

        SimpleBufferedFile::SimpleBufferedFile (
                Endianness endianness,
                const std::string &path,
                Flags32 flags) :
                BufferedFile (endianness) {
            Open (path, flags);
        }

        void SimpleBufferedFile::Open (
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
            BufferedFile::Open (
                path,
                dwDesiredAccess,
                dwShareMode,
                dwCreationDisposition,
                dwFlagsAndAttributes);
        #else // defined (TOOLCHAIN_OS_Windows)
            i32 flags_ = 0;
            if (flags.Test (SimpleFile::ReadOnly)) {
                if (flags.Test (WriteOnly)) {
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
            BufferedFile::Open (path, flags_, mode);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
