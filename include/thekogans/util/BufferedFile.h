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

#if !defined (__thekogans_util_BufferedFile_h)
#define __thekogans_util_BufferedFile_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/OwnerMap.h"
#include "thekogans/util/File.h"

namespace thekogans {
    namespace util {

        struct _LIB_THEKOGANS_UTIL_DECL BufferedFile : public File {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (BufferedFile)

            struct Buffer {
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                ui64 offset;
                ui64 length;
                static const std::size_t PAGE_SIZE = 0x00001000;
                ui8 data[PAGE_SIZE];
                bool dirty;

                Buffer (
                    ui64 offset_ = 0,
                    ui64 length_ = 0) :
                    offset (offset_),
                    length (length_),
                    dirty (false) {}
            };

        private:
            i64 position;
            ui64 size;
            using BufferMap = OwnerMap<ui64, Buffer>;
            BufferMap buffers;

        public:
            /// \brief
            /// Default ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            BufferedFile (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path = std::string ()) :
                File (endianness, handle, path),
                position (IsOpen () ? File::Tell () : 0),
                size (IsOpen () ? File::GetSize () : 0) {}
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief ctor
            /// Open the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
            BufferedFile (
                Endianness endianness,
                const std::string &path,
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                DWORD dwCreationDisposition = OPEN_ALWAYS,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL) :
                File (
                    endianness,
                    path,
                    dwDesiredAccess,
                    dwShareMode,
                    dwCreationDisposition,
                    dwFlagsAndAttributes),
                position (IsOpen () ? File::Tell () : 0),
                size (IsOpen () ? File::GetSize () : 0) {}
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief ctor
            /// Open the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
            BufferedFile (
                Endianness endianness,
                const std::string &path,
                i32 flags = O_RDWR | O_CREAT,
                i32 mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) :
                File (endianness, path, flags, mode),
                position (IsOpen () ? File::Tell () : 0),
                size (IsOpen () ? File::GetSize () : 0) {}
        #endif // defined (TOOLCHAIN_OS_Windows)
            virtual ~BufferedFile ();

            static void CommitLog (const std::string &path);

            // Serializer
            /// \brief
            /// Read bytes from a file.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual std::size_t Read (
                void *buffer,
                std::size_t count) override;
            /// \brief
            /// Write bytes to a file.
            /// \param[in] buffer Where the bytes come from.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual std::size_t Write (
                const void *buffer,
                std::size_t count) override;

            // RandomSeekSerializer
            /// \brief
            /// Return the file pointer position.
            /// \return The file pointer position.
            virtual i64 Tell () const override {
                return position;
            }
            /// \brief
            /// Reposition the file pointer.
            /// \param[in] offset Offset to move relative to fromWhere.
            /// \param[in] fromWhere SEEK_SET, SEEK_CUR or SEEK_END.
            /// \return The new file pointer position.
            virtual i64 Seek (
                i64 offset,
                i32 fromWhere) override;

            // File
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Open the file.
            /// \param[in] path Windows CreateFile parameter.
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
            virtual void Open (
                const std::string &path,
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE,
                DWORD dwCreationDisposition = OPEN_EXISTING,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL) override;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Open the file.
            /// \param[in] path POSIX open parameter.
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
            virtual void Open (
                const std::string &path,
                i32 flags = O_RDWR,
                i32 mode = S_IRUSR | S_IWUSR) override;
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Close file.
            virtual void Close () override;
            /// \brief
            /// Flush pending writes to disk.
            virtual void Flush () override;

            /// \brief
            /// Return number of bytes available for reading.
            /// \return Number of bytes available for reading.
            virtual ui64 GetDataAvailableForReading () const override {
                return size - position;
            }

            /// \brief
            /// Return file size in bytes.
            /// \return File size in bytes.
            virtual ui64 GetSize () const override {
                return size;
            }
            /// \brief
            /// Truncates or expands the file.
            /// \param[in] newSize New size to set the file to.
            virtual void SetSize (ui64 newSize) override;

            /// \brief
            /// Lock a range of bytes in the file.
            /// \region[in] region region to lock.
            /// \region[in] exclusive lock for exclusive access.
            virtual void LockRegion (
                const Region & /*region*/,
                bool /*exclusive*/) {}
            /// \brief
            /// Unlock a range of bytes in the file.
            /// \region[in] region region to unlock.
            virtual void UnlockRegion (const Region & /*region*/) {}

        private:
            Buffer *GetBuffer (bool create);

            /// \brief
            /// BufferedFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BufferedFile)
        };

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_BufferedFile_h
