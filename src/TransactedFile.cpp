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
    #elif defined (TOOLCHAIN_OS_OSH)
        #include "thekogans/util/os/windows/OSXUtils.h"
    #endif // defined (TOOLCHAIN_OS_Linux)
    #include <fcntl.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include <string>
#include "thekogans/util/Path.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Constants.h"
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

            struct HandleGuard {
                THEKOGANS_UTIL_HANDLE handle;
                bool dismissed = false;
                ~HandleGuard () {
                    if (!dismissed && handle != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                        // If we exit via exception, close the handle immediately
                    #if defined (TOOLCHAIN_OS_Windows)
                        ::CloseHandle (handle);
                    #else
                        ::close (handle);
                    #endif
                    }
                }
            };
        }

        Serializer &operator << (
                Serializer &serializer,
                const TransactedFile::Header &header) {
            serializer << MAGIC32 << header.flags << header.dictionaryOffset <<
                header.allocatorOffset << header.registryOffset << header.userOffset;
            return serializer;
        }

        Serializer &operator >> (
                Serializer &serializer,
                TransactedFile::Header &header) {
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> header.flags >> header.dictionaryOffset >>
                    header.allocatorOffset >> header.registryOffset >> header.userOffset;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFile file.");
            }
            return serializer;
        }

        TransactedFile::TransactedFile (const std::string &path) {
            AddRef ();
            // Turn off OS page buffering. With PageMap's page alignment,
            // we don't need the cost of extra copying.
        #if defined (TOOLCHAIN_OS_Windows)
            File::Open (
                path,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
        #else // defined (TOOLCHAIN_OS_Windows)
            i32 flags = O_RDWR;
        #if defined (TOOLCHAIN_OS_Linux)
            flags |= O_DIRECT;
        #endif // defined (TOOLCHAIN_OS_Linux)
            File::Open (path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        #if defined (TOOLCHAIN_OS_OSX)
            fcntl (handle, F_NOCACHE, 1);
        #endif // defined (TOOLCHAIN_OS_OSX)
        #endif // defined (TOOLCHAIN_OS_Windows)
            HandleGuard guard{handle};
            size = GetSize ();
            pageMap.Reset (new AddressSpaceType (32, 8, 20, GetPhysicalSectorSize (handle)));
            Range magicRange (this, 0, UI32_SIZE);
            ui32 magic;
            magicRange >> magic;
            if (magic == MAGIC32) {
                // Log is host endian.
            }
            else if (ByteSwap<GuestEndian, HostEndian> (magic) == MAGIC32) {
                // File is guest endian.
                endianness = GuestEndian;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFile %s",
                    path.c_str ());
            }
            BlockRange headerRange (this, Block::BLOCK_0_OFFSET);
            headerRange >> header;
            {
                Object::SharedPtr object;
                BlockRange dictionaryRange (this, header.dictionaryOffset);
                dictionaryRange >> object;
                dictionary = object;
                assert (dictionary != nullptr);
            }
            {
                Object::SharedPtr object;
                BlockRange allocatorRange (this, header.allocatorOffset);
                allocatorRange >> object;
                allocator = object;
                assert (allocator != nullptr);
            }
            {
                Object::SharedPtr object;
                BlockRange registryRange (this, header.registryOffset);
                registryRange >> object;
                registry = object;
                assert (registry != nullptr);
            }
            guard.dismissed = true;
        }

        TransactedFile::TransactedFile (
                Endianness endianness,
                const std::string &path,
                bool secure,
                DynamicCreatable::TypeDictionary::SharedPtr dictionary_,
                Allocator::SharedPtr allocator_,
                Registry::SharedPtr registry_,
                bool forceTruncate) :
                File (endianness),
                header (secure ? Header::FLAGS_SECURE : 0),
                dictionary (dictionary_),
                allocator (allocator_),
                registry (registry_) {
            AddRef ();
            // Turn off OS page buffering. With PageMap's page alignment,
            // we don't need the cost of extra copying.
        #if defined (TOOLCHAIN_OS_Windows)
            DWORD disposition = forceTruncate ? TRUNCATE_EXISTING : CREATE_NEW;
            File::Open (
                path,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                disposition,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
        #else // defined (TOOLCHAIN_OS_Windows)
            i32 flags = O_RDWR | O_CREAT;
            flags |= forceTruncate ? O_TRUNC : O_EXCL;
        #if defined (TOOLCHAIN_OS_Linux)
            flags |= O_DIRECT;
        #endif // defined (TOOLCHAIN_OS_Linux)
            File::Open (path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        #if defined (TOOLCHAIN_OS_OSX)
            fcntl (handle, F_NOCACHE, 1);
        #endif // defined (TOOLCHAIN_OS_OSX)
        #endif // defined (TOOLCHAIN_OS_Windows)
            HandleGuard guard{handle};
            pageMap.Reset (new AddressSpaceType (32, 8, 20, GetPhysicalSectorSize (handle)));
            Transaction transaction (*this);
            Grow (Block::SIZE + Header::SIZE);
            Block block (this, Block::BLOCK_0_OFFSET, 0, Header::SIZE);
            block.Write ();
            Subscriber<TransactedFileTransactionEvents>::Subscribe (*this);
            {
                Object::SharedPtr object = dictionary;
                if (object != nullptr) {
                    object->Init (this, 0);
                }
            }
            {
                Object::SharedPtr object = allocator;
                if (object != nullptr) {
                    object->Init (this, 0);
                }
            }
            {
                Object::SharedPtr object = registry;
                if (object != nullptr) {
                    object->Init (this, 0);
                }
            }
            transaction.Commit ();
            guard.dismissed = true;
        }

        void TransactedFile::SetTypeDictionaryOffset (AddressType dictionaryOffset) {
            LockGuard<SpinLock> guard (spinLock);
            if (header.dictionaryOffset != dictionaryOffset) {
                header.dictionaryOffset = dictionaryOffset;
                Subscriber<TransactedFileTransactionEvents>::Subscribe (*this);
            }
        }

        void TransactedFile::SetAllocatorOffset (AddressType allocatorOffset) {
            LockGuard<SpinLock> guard (spinLock);
            if (header.allocatorOffset != allocatorOffset) {
                header.allocatorOffset = allocatorOffset;
                Subscriber<TransactedFileTransactionEvents>::Subscribe (*this);
            }
        }

        void TransactedFile::SetRegistryOffset (AddressType registryOffset) {
            LockGuard<SpinLock> guard (spinLock);
            if (header.registryOffset != registryOffset) {
                header.registryOffset = registryOffset;
                Subscriber<TransactedFileTransactionEvents>::Subscribe (*this);
            }
        }

        void TransactedFile::SetUserOffset (AddressType userOffset) {
            LockGuard<SpinLock> guard (spinLock);
            if (header.userOffset != userOffset) {
                header.userOffset = userOffset;
                Subscriber<TransactedFileTransactionEvents>::Subscribe (*this);
            }
        }

        TransactedFile::SharedPtr TransactedFile::Open (const std::string &path) {
            if (Path (path).Exists ()) {
                Transaction::CommitLog (path);
                return SharedPtr (new TransactedFile (path), false);
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
                bool overwrite) {
            bool exists = Path (path).Exists ();
            if (exists && !overwrite) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("TransactedFile %s exists.", path.c_str ());
            }
            return SharedPtr (
                new TransactedFile (
                    endianness, path, secure, dictionary, allocator, registry, exists && overwrite), false);
        }

        TransactedFile::SizeType TransactedFile::Grow (
                SizeType amount) {
            LockGuard<SpinLock> guard (spinLock);
            if (IsOpen ()) {
                SizeType oldSize = size;
                size += MIN (pageMap->GetMaxOffset () - size, amount);
                return oldSize;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        TransactedFile::SizeType TransactedFile::Shrink (
                SizeType amount) {
            LockGuard<SpinLock> guard (spinLock);
            if (IsOpen ()) {
                size -= MIN (amount, size.load (std::memory_order_relaxed));
                pageMap->Shrink (size);
                return size;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EBADF);
            }
        }

        void TransactedFile::OnTransactedFileTransactionCommit (
                TransactedFile::SharedPtr file,
                int phase) noexcept {
            if (phase == COMMIT_PHASE_2) {
                Range range (this, Block::BLOCK_0_OFFSET, Header::SIZE, false);
                range << header;
            }
        }

        void TransactedFile::OnTransactedFileTransactionAbort (
                TransactedFile::SharedPtr file) noexcept {
            Range range (this, Block::BLOCK_0_OFFSET, Header::SIZE);
            range >> header;
        }

        _LIB_THEKOGANS_UTIL_DECL TransactedFile::Range & _LIB_THEKOGANS_UTIL_API operator >> (
                TransactedFile::Range &range,
                TransactedFile::Object::SharedPtr &object) {
            ui32 magic;
            range >> magic;
            if (magic == TransactedFile::Object::MAGIC) {
                SerializableHeader header;
                range >> header;
                if (range.factory != nullptr) {
                    object = range.factory (range.parameters);
                }
                else {
                    object = TransactedFile::Object::CreateObject (
                        header.type.c_str (), range.GetFile (), range.GetOffset ());
                }
                if (object != nullptr) {
                    if (header.version <= object->Version ()) {
                        Serializer::ContextGuard guard (range, header);
                        object->Read (range);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Header version (%u) is greater then the TransactedFile::Object version (%u).",
                            header.version,
                            object->Version ());
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unable to create a TransactedFile::Object of type %s.",
                        header.type.c_str ());
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("Corrupt Object header (%x).", magic);
            }
            return range;
        }

    } // namespace util
} // namespace thekogans
