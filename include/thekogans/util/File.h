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

#if !defined (__thekogans_util_File_h)
#define __thekogans_util_File_h

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#else // defined (TOOLCHAIN_OS_Windows)
    #if defined (TOOLCHAIN_OS_Linux)
        #if !defined (_LARGEFILE64_SOURCE)
            #define _LARGEFILE64_SOURCE 1
        #endif // !defined (_LARGEFILE64_SOURCE)
        #define ftruncate ftruncate64
        #define lseek lseek64
    #endif // defined (TOOLCHAIN_OS_Linux)
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/time.h>
    #include <unistd.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cstdio>
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/Buffer.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        /// \struct File File.h thekogans/util/File.h
        ///
        /// \brief
        /// File is a platform independent file system file.

        struct _LIB_THEKOGANS_UTIL_DECL File : public Serializer {
        protected:
            /// \brief
            /// OS specific file handle.
            THEKOGANS_UTIL_HANDLE handle;
            /// \brief
            /// Path that was used to open the file.
            std::string path;

        public:
            /// \brief
            /// Default ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle_ OS file handle.
            /// \param[in] path_ File path.
            File (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle_ = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path_ = std::string ()) :
                Serializer (endianness),
                handle (handle_),
                path (path_) {}
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief ctor
            /// Open the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
            File (
                Endianness endianness,
                const std::string &path,
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                DWORD dwCreationDisposition = OPEN_ALWAYS,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL);
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief ctor
            /// Open the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
            File (
                Endianness endianness,
                const std::string &path,
                i32 flags = O_RDWR | O_CREAT,
                i32 mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief dtor
            /// Close the file.
            virtual ~File ();

            /// \brief
            /// Return true if file is open.
            /// \return true == file is open.
            inline bool IsOpen () const {
                return handle != THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
            }

        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Open the file.
            /// \param[in] path_ Windows CreateFile parameter.
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
            virtual void Open (
                const std::string &path_,
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                DWORD dwCreationDisposition = OPEN_ALWAYS,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL);
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Open the file.
            /// \param[in] path_ POSIX open parameter.
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
            virtual void Open (
                const std::string &path_,
                i32 flags = O_RDWR | O_CREAT,
                i32 mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Close file.
            virtual void Close ();
            /// \brief
            /// Flush pending writes to a file.
            virtual void Flush ();

            /// \brief
            /// Return number of bytes available for reading.
            /// \return Number of bytes available for reading.
            virtual ui64 GetDataAvailableForReading () const;

            // Serializer
            /// \brief
            /// Read bytes from a file.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual std::size_t Read (
                void *buffer,
                std::size_t count);
            /// \brief
            /// Write bytes to a file.
            /// \param[in] buffer Where the bytes come from.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual std::size_t Write (
                const void *buffer,
                std::size_t count);

            /// \brief
            /// Return the file pointer position.
            /// \return The file pointer position.
            virtual i64 Tell () const;
            /// \brief
            /// Reposition the file pointer.
            /// \param[in] offset Offset to move relative to fromWhere.
            /// \param[in] fromWhere SEEK_SET, SEEK_CUR or SEEK_END.
            /// \return The new file pointer position.
            virtual i64 Seek (
                i64 offset,
                i32 fromWhere);
            /// \brief
            /// Return file size in bytes.
            /// \return File size in bytes.
            virtual ui64 GetSize () const;
            /// \brief
            /// Truncates or expands the file.
            /// \param[in] newSize New size to set the file to.
            virtual void SetSize (ui64 newSize);

            /// \struct File::Region File.h thekogans/util/File.h
            ///
            /// \brief
            /// Encapsulates file region lock/unlock parameters.
            struct Region {
                /// \brief
                /// File offset where the region starts.
                ui64 offset;
                /// \brief
                /// Region length.
                ui64 length;
                /// \brief
                /// ctor.
                /// \param[in] offset_ File offset where the region starts.
                /// \param[in] length_ Region length.
                Region (
                    ui64 offset_,
                    ui64 length_) :
                    offset (offset_),
                    length (length_) {}
            };
            /// \brief
            /// Lock a range of bytes in the file.
            /// \region[in] region region to lock.
            /// \region[in] exclusive lock for exclusive access.
            virtual void LockRegion (
                const Region &region,
                bool exclusive);
            /// \brief
            /// Unlock a range of bytes in the file.
            /// \region[in] region region to unlock.
            virtual void UnlockRegion (const Region &region);

            /// \brief
            /// Return the file path.
            /// \return File path.
            inline std::string GetPath () const {
                return path;
            }

            /// \brief
            /// Return a unique identifier for this file.
            /// \return Unique identifier for this file.
            GUID GetId () const;

            /// \brief
            /// Delete the file at the given path.
            /// \param[in] path File to delete.
            static void Delete (const std::string &path);
            enum TouchType {
                /// \brief
                /// Update last accessed time.
                TOUCH_ACCESS_TIME = 1,
                /// \brief
                /// Update last modified time.
                TOUCH_WRITE_TIME = 2,
                /// \brief
                /// Update both las accessed and last modified times.
                TOUCH_BOTH = 3
            };
            /// \brief
            /// Update the last accessed and modified times. If the given
            /// file does not exist, create it.
            /// \param[in] path File to touch.
            /// \param[in] touchType What to touch.
            /// \param[in] lastAccessTime Files new last accessed time.
            /// \param[in] lastWriteTime Files new last modified time.
            static void Touch (
                const std::string &path,
                TouchType touchType = TOUCH_BOTH,
                const TimeSpec &lastAccessTime = GetCurrentTime (),
                const TimeSpec &lastWriteTime = GetCurrentTime ());

        private:
            /// \brief
            /// Platform specific seek.
            /// \param[in] offset Offset to move relative to fromWhere.
            /// \param[in] fromWhere SEEK_SET, SEEK_CUR or SEEK_END.
            /// \return The new file pointer position.
            i64 PlatformSeek (
                i64 offset,
                i32 fromWhere) const;

            /// \brief
            /// File is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (File)
        };

        /// \struct SimpleFile File.h thekogans/util/File.h
        ///
        /// \brief
        /// SimpleFile exposes the basic flags supported by the standard
        /// library open that are portable across Windows, Linux and OS X.
        /// NOTE: On Linux and OS X if a file needs to be created, it's
        /// mode will be 0644. This is fine for most cases but might not
        /// be appropriate for some. If you need to control the mode of
        /// the created file use File instead.

        struct _LIB_THEKOGANS_UTIL_DECL SimpleFile : public File {
            /// \brief
            /// Default ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            SimpleFile (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path = std::string ()) :
                File (endianness, handle, path) {}
            enum {
                /// \brief
                /// Open for reading.
                ReadOnly = 1,
                /// \brief
                /// Open for writing.
                WriteOnly = 2,
                /// \brief
                /// Open for both reading and writing.
                ReadWrite = ReadOnly | WriteOnly,
                /// \brief
                /// Create if doesn't exist.
                Create = 4,
                /// \brief
                /// If the file exists, truncate it to 0 length.
                Truncate = 8,
                /// \brief
                /// After opening, position the cursor at the end of the file.
                Append = 16
            };
            /// \brief
            /// ctor. Abstracts most useful functionality from POSIX open.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] flags Most useful POSIX open flags.
            SimpleFile (
                    Endianness endianness,
                    const std::string &path,
                    i32 flags = ReadWrite | Create) :
                    File (endianness) {
                Open (path, flags);
            }

            /// \brief
            /// Open the file.
            /// NOTE: This function overrides the one in \see{File}
            /// for POSIX file systems (Linux/ OS X).
            /// \param[in] path Path to file to open.
            /// \param[in] flags Most useful POSIX open flags.
            /// \param[in] mode Not used. Here to match the signature of \see{File::Open}.
            virtual void Open (
                const std::string &path,
                i32 flags = ReadWrite | Create,
                i32 /*mode*/ = 0);

            /// \brief
            /// SimpleFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SimpleFile)
        };

        /// \struct ReadOnlyFile File.h thekogans/util/File.h
        ///
        /// \brief
        /// As its name implies, ReadOnlyFile is a convenient
        /// platform independent read only file.

        struct _LIB_THEKOGANS_UTIL_DECL ReadOnlyFile : public SimpleFile {
            /// \brief
            /// ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            ReadOnlyFile (
                Endianness endianness,
                const std::string &path) :
                SimpleFile (endianness, path, ReadOnly) {}

            /// \brief
            /// ReadOnlyFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (ReadOnlyFile)
        };

        /// \struct TenantFile File.h thekogans/util/File.h
        ///
        /// \brief
        /// TenantFile wraps a raw file handle to provide the features
        /// of File without taking ownership.

        struct _LIB_THEKOGANS_UTIL_DECL TenantFile : public File {
            /// \brief
            /// ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle File handle.
            /// \param[in] path File path.
            TenantFile (
                Endianness endianness,
                THEKOGANS_UTIL_HANDLE handle,
                const std::string &path) :
                File (endianness, handle, path) {}
            /// \brief
            /// dtor.
            /// Step on the handle so that ~File () don't close it.
            virtual ~TenantFile () {
                // Prevent File dtor from closing a descriptor
                // that does not belong to us.
                handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
            }

        private:
            /// \brief
            /// Close file.
            virtual void Close () {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s", "TenantFile is not allowed to close the file.");
            }

            /// \brief
            /// TenantFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TenantFile)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_File_h)
