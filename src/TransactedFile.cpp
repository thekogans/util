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
        #include "thekogans/util/os/linux/LinuxUtils.h"
    #elif defined (TOOLCHAIN_OS_OSX)
        #include "thekogans/util/os/osx/OSXUtils.h"
    #endif // defined (TOOLCHAIN_OS_Linux)
    #include <fcntl.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include <string>
#include "thekogans/util/Path.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/TransactedFileTypeDictionary.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

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
                const std::string &path,
                std::size_t bitsPerSegment,
                std::size_t bitsPerLevel,
                std::size_t bitsPerPage,
                std::size_t bitsPerAddress) :
            #if defined (TOOLCHAIN_OS_Windows)
                File (
                    HostEndian,
                    path,
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
            #elif defined (TOOLCHAIN_OS_Linux)
                File (HostEndian, path, O_RDWR | O_DIRECT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
            #elif defined (TOOLCHAIN_OS_OSX)
                File (HostEndian, path, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
            #endif // defined (TOOLCHAIN_OS_Windows)
                PageMap64 (
                    bitsPerSegment,
                    bitsPerLevel,
                    bitsPerPage,
                    bitsPerAddress,
                    GetPhysicalSectorSize (handle)),
                size (GetSize ()),
                // TransactedFileTypeDictionary implements an internal system
                // dictionary to allow us to bootstrap the header.
                dictionary (new TransactedFileTypeDictionary) {
        #if defined (TOOLCHAIN_OS_OSX)
            // Turn off OS page buffering. With PageMap's page alignment,
            // we don't need the cost of extra copying.
            fcntl (handle, F_NOCACHE, 1);
        #endif // defined (TOOLCHAIN_OS_OSX)
            Range magicRange (this, 0, UI32_SIZE);
            ui32 magic;
            magicRange >> magic;
            if (THEKOGANS_UTIL_LIKELY (magic == MAGIC32)) {
                // File is host endian.
            }
            else if (ByteSwap<GuestEndian, HostEndian> (magic) == MAGIC32) {
                // File is guest endian.
                endianness = GuestEndian;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("Corrupt TransactedFile %s", path.c_str ());
            }
            ObjectRange headerRange (this, Header::BLOCK_OFFSET);
            if (THEKOGANS_UTIL_LIKELY (
                    headerRange.context.WeakCompare (SerializableHeader{Header::TYPE, Header::VERSION}))) {
                headerRange >> header;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Expecting Header as the first block in %s", path.c_str ());
            }
        }

        TransactedFile::TransactedFile (
                Endianness endianness,
                const std::string &path,
                bool secure,
                DynamicCreatable::TypeDictionary::SharedPtr dictionary_,
                Allocator::SharedPtr allocator_,
                Registry::SharedPtr registry_,
                bool forceTruncate,
                std::size_t bitsPerSegment,
                std::size_t bitsPerLevel,
                std::size_t bitsPerPage,
                std::size_t bitsPerAddress) :
            #if defined (TOOLCHAIN_OS_Windows)
                File (
                    endianness,
                    path,
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    forceTruncate ? TRUNCATE_EXISTING : CREATE_NEW,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH),
            #elif defined (TOOLCHAIN_OS_Linux)
                File (
                    endianness,
                    path,
                    O_RDWR | O_CREAT | (forceTruncate ? O_TRUNC : O_EXCL) | O_DIRECT,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
            #elif defined (TOOLCHAIN_OS_OSX)
                File (
                    endianness,
                    path,
                    O_RDWR | O_CREAT | (forceTruncate ? O_TRUNC : O_EXCL),
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
            #endif // defined (TOOLCHAIN_OS_Windows)
                PageMap64 (
                    bitsPerSegment,
                    bitsPerLevel,
                    bitsPerPage,
                    bitsPerAddress,
                    GetPhysicalSectorSize (handle)),
                dictionary (dictionary_),
                allocator (allocator_),
                registry (registry_) {
        #if defined (TOOLCHAIN_OS_OSX)
            fcntl (handle, F_NOCACHE, 1);
        #endif // defined (TOOLCHAIN_OS_OSX)
            Transaction transaction (this);
            header = Object::Create<Header> (this, 0, secure);
            transaction.Commit ();
        }

        TransactedFile::SharedPtr TransactedFile::Open (
                const std::string &path,
                std::size_t bitsPerSegment,
                std::size_t bitsPerLevel,
                std::size_t bitsPerPage,
                std::size_t bitsPerAddress) {
            if (Path (path).Exists ()) {
                Transaction::CommitLog (path);
                return SharedPtr (
                    new TransactedFile (
                        path,
                        bitsPerSegment,
                        bitsPerLevel,
                        bitsPerPage,
                        bitsPerAddress));
            }
            return nullptr;
        }

        TransactedFile::SharedPtr TransactedFile::Create (
                Endianness endianness,
                const std::string &path,
                bool secure,
                DynamicCreatable::TypeDictionary::SharedPtr dictionary,
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry,
                bool overwrite,
                std::size_t bitsPerSegment,
                std::size_t bitsPerLevel,
                std::size_t bitsPerPage,
                std::size_t bitsPerAddress) {
            bool exists = Path (path).Exists ();
            if (exists && !overwrite) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("%s exists.", path.c_str ());
            }
            std::string logPath = Transaction::GetLogPath (path);
            if (Path (logPath).Exists ()) {
                File::Delete (logPath);
            }
            return SharedPtr (
                new TransactedFile (
                    endianness,
                    path,
                    secure,
                    dictionary,
                    allocator,
                    registry,
                    exists && overwrite,
                    bitsPerSegment,
                    bitsPerLevel,
                    bitsPerPage,
                    bitsPerAddress));
        }

    } // namespace util
} // namespace thekogans
