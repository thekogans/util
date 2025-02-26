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
    #include <direct.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>
#include "thekogans/util/Buffer.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsUtils.h"
#elif defined (TOOLCHAIN_OS_Linux)
    #include "thekogans/util/os/linux/LinuxUtils.h"
#elif defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/os/osx/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/File.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (
            thekogans::util::File,
            Serializer::TYPE, RandomSeekSerializer::TYPE)

        File::File (
                Endianness endianness,
                const std::string &path,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess,
                DWORD dwShareMode,
                DWORD dwCreationDisposition,
                DWORD dwFlagsAndAttributes) :
            #else // defined (TOOLCHAIN_OS_Windows)
                int flags,
                int mode) :
            #endif // defined (TOOLCHAIN_OS_Windows)
                RandomSeekSerializer (endianness),
                handle (THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
            Open (
                path,
            #if defined (TOOLCHAIN_OS_Windows)
                dwDesiredAccess,
                dwShareMode,
                dwCreationDisposition,
                dwFlagsAndAttributes);
            #else // defined (TOOLCHAIN_OS_Windows)
                flags,
                mode);
            #endif // defined (TOOLCHAIN_OS_Windows)
        }

        File::~File () {
            THEKOGANS_UTIL_TRY {
                Close ();
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void File::Open (
                const std::string &path_,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess,
                DWORD dwShareMode,
                DWORD dwCreationDisposition,
                DWORD dwFlagsAndAttributes) {
            #else // defined (TOOLCHAIN_OS_Windows)
                i32 flags,
                i32 mode) {
            #endif // defined (TOOLCHAIN_OS_Windows)
            Close ();
        #if defined (TOOLCHAIN_OS_Windows)
            handle = CreateFileW (os::windows::UTF8ToUTF16 (path_).c_str (),
                dwDesiredAccess, dwShareMode, 0, dwCreationDisposition, dwFlagsAndAttributes, 0);
        #else // defined (TOOLCHAIN_OS_Windows)
            handle =  open (path_.c_str (), flags, mode);
        #endif // defined (TOOLCHAIN_OS_Windows)
            if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path_.c_str ());
            }
            path = path_;
        }

        void File::Close () {
            if (handle != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
            #if defined (TOOLCHAIN_OS_Windows)
                if (!CloseHandle (handle)) {
            #else // defined (TOOLCHAIN_OS_Windows)
                if (close (handle) < 0) {
            #endif // defined (TOOLCHAIN_OS_Windows)
                    THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
                }
                handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                path.clear ();
            }
        }

        void File::Flush () {
        #if defined (TOOLCHAIN_OS_Windows)
            if (!FlushFileBuffers (handle)) {
        #else // defined (TOOLCHAIN_OS_Windows)
            if (fsync (handle) < 0) {
        #endif // defined (TOOLCHAIN_OS_Windows)
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        }

        ui64 File::GetDataAvailableForReading () const {
            return GetSize () - Tell ();
        }

    #if defined (TOOLCHAIN_OS_Windows)
        std::size_t File::Read (
                void *buffer,
                std::size_t count) {
            DWORD countRead = 0;
            if (!ReadFile (handle, buffer, (DWORD)count, &countRead, 0)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
            return countRead;
        }

        std::size_t File::Write (
                const void *buffer,
                std::size_t count) {
            DWORD countWritten = 0;
            if (!WriteFile (handle, buffer, (DWORD)count, &countWritten, 0)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
            return countWritten;
        }
    #else // defined (TOOLCHAIN_OS_Windows)
        std::size_t File::Read (
                void *buffer,
                std::size_t count) {
            ssize_t countRead;
            do {
                countRead = read (handle, buffer, count);
                if (countRead < 0 && THEKOGANS_UTIL_OS_ERROR_CODE != EINTR) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
                }
            } while (countRead < 0);
            return (std::size_t)countRead;
        }

        std::size_t File::Write (
                const void *buffer,
                std::size_t count) {
            ssize_t countWritten;
            do {
                countWritten = write (handle, buffer, count);
                if (countWritten < 0 && THEKOGANS_UTIL_OS_ERROR_CODE != EINTR) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
                }
            } while (countWritten < 0);
            return (std::size_t)countWritten;
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        i64 File::Tell () const {
            return PlatformSeek (0, SEEK_CUR);
        }

        i64 File::Seek (
                i64 offset,
                i32 fromWhere) {
            return PlatformSeek (offset, fromWhere);
        }

        ui64 File::GetSize () const {
            ui64 size = 0;
        #if defined (TOOLCHAIN_OS_Windows)
            ULARGE_INTEGER uli;
            uli.LowPart = GetFileSize (handle, &uli.HighPart);
            if (uli.LowPart != 0xffffffff ||
                    THEKOGANS_UTIL_OS_ERROR_CODE == NO_ERROR) {
                size = uli.QuadPart;
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            ui64 position = PlatformSeek (0, SEEK_CUR);
            size = PlatformSeek (0, SEEK_END);
            PlatformSeek (position, SEEK_SET);
        #endif // defined (TOOLCHAIN_OS_Windows)
            return size;
        }

        void File::SetSize (ui64 newSize) {
        #if defined (TOOLCHAIN_OS_Windows)
            ui64 position = PlatformSeek (0, SEEK_CUR);
            PlatformSeek (newSize, SEEK_SET);
            if (!SetEndOfFile (handle)) {
        #else // defined (TOOLCHAIN_OS_Windows)
            if (ftruncate (handle, newSize) < 0) {
        #endif // defined (TOOLCHAIN_OS_Windows)
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        #if defined (TOOLCHAIN_OS_Windows)
            PlatformSeek (position, SEEK_SET);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    #if defined (TOOLCHAIN_OS_Windows)
        void File::LockRegion (
                const Region &region,
                bool exclusive) {
            DWORD dwFlags = LOCKFILE_FAIL_IMMEDIATELY;
            if (exclusive) {
                dwFlags |= LOCKFILE_EXCLUSIVE_LOCK;
            }
            ULARGE_INTEGER length;
            length.QuadPart = region.length;
            OVERLAPPED overlapped;
            memset (&overlapped, 0, sizeof (OVERLAPPED));
            ULARGE_INTEGER offset;
            offset.QuadPart = region.offset;
            overlapped.Offset = offset.LowPart;
            overlapped.OffsetHigh = offset.HighPart;
            if (!LockFileEx (handle, dwFlags, 0, length.LowPart,
                    length.HighPart, &overlapped)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        }

        void File::UnlockRegion (const Region &region) {
            ULARGE_INTEGER length;
            length.QuadPart = region.length;
            OVERLAPPED overlapped;
            memset (&overlapped, 0, sizeof (OVERLAPPED));
            ULARGE_INTEGER offset;
            offset.QuadPart = region.offset;
            overlapped.Offset = offset.LowPart;
            overlapped.OffsetHigh = offset.HighPart;
            if (!UnlockFileEx (handle, LOCKFILE_FAIL_IMMEDIATELY,
                    length.LowPart, length.HighPart, &overlapped)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        }
    #else // defined (TOOLCHAIN_OS_Windows)
        void File::LockRegion (
                const Region &region,
                bool exclusive) {
            struct flock fl;
            fl.l_type = exclusive ? F_WRLCK : F_RDLCK;
            fl.l_whence = SEEK_SET;
            fl.l_start = region.offset;
            fl.l_len = region.length;
            if (fcntl (handle, F_SETLK, &fl) < 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        }

        void File::UnlockRegion (const Region &region) {
            struct flock fl;
            fl.l_type = F_UNLCK;
            fl.l_whence = SEEK_SET;
            fl.l_start = region.offset;
            fl.l_len = region.length;
            if (fcntl (handle, F_SETLK, &fl) < 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        GUID File::GetId () const {
            HostBuffer buffer (GUID_SIZE);
        #if defined (TOOLCHAIN_OS_Windows)
            BY_HANDLE_FILE_INFORMATION fileInformation;
            if (!GetFileInformationByHandle (handle, &fileInformation)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
            buffer <<
                (ui32)0 <<
                (ui32)fileInformation.dwVolumeSerialNumber <<
                (ui32)fileInformation.nFileIndexHigh <<
                (ui32)fileInformation.nFileIndexLow;
        #else // defined (TOOLCHAIN_OS_Windows)
            STAT_STRUCT buf;
            if (FSTAT_FUNC (handle, &buf) < 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
            buffer << (ui64)0 << (ui64)buf.st_ino;
        #endif // defined (TOOLCHAIN_OS_Windows)
            return GUID::FromBuffer (
                buffer.GetReadPtr (),
                buffer.GetDataAvailableForReading ());
        }

        void File::Delete (const std::string &path) {
        #if defined (TOOLCHAIN_OS_Windows)
            if (!DeleteFileW (os::windows::UTF8ToUTF16 (path).c_str ())) {
        #else // defined (TOOLCHAIN_OS_Windows)
            if (unlink (path.c_str ()) < 0) {
        #endif // defined (TOOLCHAIN_OS_Windows)
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        }

        void File::Touch (
                const std::string &path,
                TouchType touchType,
                const TimeSpec &lastAccessTime,
                const TimeSpec &lastWriteTime) {
            if (Path (path).Exists ()) {
            #if defined (TOOLCHAIN_OS_Windows)
                File file (HostEndian, path, FILE_WRITE_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING);
                FILETIME fileLastAccessTime = lastAccessTime.ToFILETIME ();
                FILETIME fileLastWriteTime = lastWriteTime.ToFILETIME ();
                if (!SetFileTime (file.handle, 0,
                        (touchType & TOUCH_ACCESS_TIME) == TOUCH_ACCESS_TIME ? &fileLastAccessTime : 0,
                        (touchType & TOUCH_WRITE_TIME) == TOUCH_WRITE_TIME ? &fileLastWriteTime : 0)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                STAT_STRUCT buf;
                if (LSTAT_FUNC (path.c_str (), &buf) < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
                }
                const timeval times[2] = {
                    (touchType & TOUCH_ACCESS_TIME) == TOUCH_ACCESS_TIME ?
                        lastAccessTime.Totimeval () : TimeSpec (buf.st_atime).Totimeval (),
                    (touchType & TOUCH_WRITE_TIME) == TOUCH_WRITE_TIME ?
                        lastWriteTime.Totimeval () : TimeSpec (buf.st_mtime).Totimeval (),
                };
                if (utimes (path.c_str (), times) < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            else {
                // The file does not exist, just create it.
                File file (HostEndian, path);
            }
        }

    #if defined (TOOLCHAIN_OS_Windows)
        namespace {
            inline DWORD GetMoveMethod (i32 fromWhere) {
                return
                    fromWhere == SEEK_SET ? FILE_BEGIN :
                    fromWhere == SEEK_CUR ? FILE_CURRENT : FILE_END;
            }
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        i64 File::PlatformSeek (
                i64 offset,
                i32 fromWhere) const {
        #if defined (TOOLCHAIN_OS_Windows)
            LARGE_INTEGER li;
            li.QuadPart = offset;
            li.LowPart = SetFilePointer (handle, li.LowPart,
                &li.HighPart, GetMoveMethod (fromWhere));
            if (li.LowPart == 0xffffffff &&
                    THEKOGANS_UTIL_OS_ERROR_CODE != NO_ERROR) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
            ULARGE_INTEGER uli;
            uli.LowPart = li.LowPart;
            uli.HighPart = (DWORD)li.HighPart;
            return uli.QuadPart;
        #else // defined (TOOLCHAIN_OS_Windows)
            i64 position = LSEEK_FUNC (handle, offset, fromWhere);
            if (position < 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
            return position;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        SimpleFile::SimpleFile (
                Endianness endianness,
                const std::string &path,
                Flags32 flags) :
                File (endianness) {
            Open (path, flags);
        }

        void SimpleFile::Open (
                const std::string &path,
                Flags32 flags) {
        #if defined (TOOLCHAIN_OS_Windows)
            DWORD dwDesiredAccess = 0;
            DWORD dwShareMode = 0;
            if (flags.Test (ReadOnly)) {
                dwDesiredAccess |= GENERIC_READ;
                dwShareMode |= FILE_SHARE_READ;
            }
            if (flags.Test (WriteOnly)) {
                dwDesiredAccess |= GENERIC_WRITE;
                dwShareMode |= FILE_SHARE_WRITE | FILE_SHARE_DELETE;
            }
            if (flags.Test (Append)) {
                dwDesiredAccess |= FILE_APPEND_DATA;
            }
            DWORD dwCreationDisposition = 0;
            if (flags.Test (Create)) {
                if (flags.Test (Truncate)) {
                    dwCreationDisposition |= CREATE_ALWAYS;
                }
                else {
                    dwCreationDisposition |= OPEN_ALWAYS;
                }
            }
            else if (flags.Test (Truncate)) {
                dwCreationDisposition |= TRUNCATE_EXISTING;
            }
            else {
                dwCreationDisposition |= OPEN_EXISTING;
            }
            DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
            File::Open (
                path,
                dwDesiredAccess,
                dwShareMode,
                dwCreationDisposition,
                dwFlagsAndAttributes);
        #else // defined (TOOLCHAIN_OS_Windows)
            i32 flags_ = 0;
            if (flags.Test (ReadOnly)) {
                if (flags.Test (WriteOnly)) {
                    flags_ |= O_RDWR;
                }
                else {
                    flags_ |= O_RDONLY;
                }
            }
            else if (flags.Test (WriteOnly)) {
                flags_ |= O_WRONLY;
            }
            if (flags.Test (Create)) {
                flags_ |= O_CREAT;
            }
            if (flags.Test (Truncate)) {
                flags_ |= O_TRUNC;
            }
            if (flags.Test (Append)) {
                flags_ |= O_APPEND;
            }
            i32 mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            File::Open (path, flags_, mode);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
