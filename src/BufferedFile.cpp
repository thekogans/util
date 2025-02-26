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

        namespace {
            Serializer &operator >> (
                    Serializer &serializer,
                    BufferedFile::Buffer &buffer) {
                serializer >> buffer.offset >> buffer.length;
                serializer.Read (buffer.data, BufferedFile::Buffer::PAGE_SIZE);
                return serializer;
            }

            struct __Log__ : public SimpleFile {
                File &file;
                ui32 isClean;
                ui64 count;
                ui64 size;

                __Log__ (File &file_) :
                        SimpleFile (
                            HostEndian,
                            file_.GetPath () + ".log",
                            SimpleFile::ReadWrite |
                            SimpleFile::Create |
                            SimpleFile::Truncate),
                        file (file_),
                        isClean (0),
                        count (0),
                        size (file.GetSize ()) {
                    *this << MAGIC32 << isClean << count << size;
                }
                ~__Log__ () {
                    Commit ();
                }

                void Commit () {
                    isClean = 1;
                    Seek (0, SEEK_SET);
                    *this << MAGIC32 << isClean << count << size;
                    BufferedFile::Buffer buffer;
                    while (count-- != 0) {
                        *this >> buffer;
                        file.Seek (buffer.offset, SEEK_SET);
                        file.Write (buffer.data, buffer.length);
                    }
                    file.SetSize (size);
                    Close ();
                    File::Delete (path);
                }

                static void Commit (const std::string &path) {
                    std::string logPath = path + ".log";
                    if (Path (path).Exists () && Path (logPath).Exists ()) {
                        SimpleFile file (HostEndian, path, SimpleFile::ReadWrite);
                        ReadOnlyFile log (HostEndian, logPath);
                        ui32 magic;
                        log >> magic;
                        if (magic == MAGIC32) {
                            ui32 isClean;
                            log >> isClean;
                            if (isClean == 1) {
                                ui64 count;
                                ui64 size;
                                log >> count >> size;
                                BufferedFile::Buffer buffer;
                                while (count-- != 0) {
                                    log >> buffer;
                                    file.Seek (buffer.offset, SEEK_SET);
                                    file.Write (buffer.data, buffer.length);
                                }
                                file.SetSize (size);
                            }
                            else {
                                THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING (
                                    THEKOGANS_UTIL,
                                    "The log (%s) for %s is dirty, skipping.",
                                    logPath.c_str (),
                                        path.c_str ());
                            }
                            log.Close ();
                            File::Delete (logPath);
                        }
                        else {
                            THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING (
                                THEKOGANS_UTIL,
                                "The log (%s) for %s is corrupt, skipping.",
                                logPath.c_str (),
                                path.c_str ());
                        }
                    }
                }
            };

            __Log__ &operator << (
                    __Log__ &log,
                    BufferedFile::Buffer &buffer) {
                log << buffer.offset << buffer.length;
                log.Write (buffer.data, BufferedFile::Buffer::PAGE_SIZE);
                ++log.count;
                return log;
            }
        }

        BufferedFile::~BufferedFile () {
            THEKOGANS_UTIL_TRY {
                Close ();
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void BufferedFile::CommitLog (const std::string &path) {
            __Log__::Commit (path);
        }

        std::size_t BufferedFile::Read (
                void *buffer,
                std::size_t count) {
            std::size_t countRead = 0;
            ui8 *ptr = (ui8 *)buffer;
            while (count > 0) {
                Buffer *buffer_ = GetBuffer (false);
                if (buffer_ == nullptr) {
                    break;
                }
                std::size_t index = position - buffer_->offset;
                std::size_t countToRead = MIN (buffer_->length - index, count);
                if (countToRead == 0) {
                    break;
                }
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
                Buffer *buffer_ = GetBuffer (true);
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
            __Log__::Commit (path);
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
            size = File::GetSize ();
        }

        void BufferedFile::Close () {
            if (IsOpen ()) {
                Flush ();
                File::Close ();
            }
        }

        void BufferedFile::Flush () {
            if (!buffers.empty ()) {
                __Log__ log (*this);
                for (BufferMap::const_iterator
                        it = buffers.begin (),
                        end = buffers.end (); it != end; ++it) {
                    if (it->second->dirty) {
                        log << *it->second;
                    }
                }
                buffers.deleteAndClear ();
            }
            // We need the log to commit (dtor) before flushing the file.
            File::Flush ();
        }

        void BufferedFile::SetSize (ui64 newSize) {
            if (size > newSize) {
                // shrinking
                if (!buffers.empty ()) {
                    BufferMap::iterator first = buffers.begin ();
                    BufferMap::iterator last = buffers.end ();
                    while (last-- != first) {
                        if (last->second->offset > newSize) {
                            delete last->second;
                        }
                        else {
                            if (last->second->offset + last->second->length > newSize) {
                                last->second->length = newSize - last->second->offset;
                                if (last->second->length == 0) {
                                    delete last->second;
                                    --last;
                                }
                            }
                            break;
                        }
                    }
                    buffers.erase (++last, buffers.end ());
                }
            }
            size = newSize;
        }

        BufferedFile::Buffer *BufferedFile::GetBuffer (bool create) {
            static const ui64 OFFSET_MASK = Buffer::PAGE_SIZE - 1;
            ui64 offset = position & ~OFFSET_MASK;
            BufferMap::iterator it = buffers.find (offset);
            if (it != buffers.end ()) {
                return it->second;
            }
            if (create || offset < size) {
                ui64 length = offset < size ? size - offset : 0;
                if (length > Buffer::PAGE_SIZE) {
                    length = Buffer::PAGE_SIZE;
                }
                std::unique_ptr<Buffer> buffer (new Buffer (offset, length));
                if (offset < size) {
                    File::Seek (offset, SEEK_SET);
                    File::Read (buffer->data, length);
                }
                std::pair<BufferMap::iterator, bool> result = buffers.insert (
                    BufferMap::value_type (offset, buffer.get ()));
                if (result.second) {
                    return buffer.release ();
                }
                else {
                    // FIXME: throw
                    assert (0);
                }
            }
            return nullptr;
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
