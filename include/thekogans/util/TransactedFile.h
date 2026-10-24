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

#if !defined (__thekogans_util_TransactedFile_h)
#define __thekogans_util_TransactedFile_h

#include <string>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/PageMap.h"
#include "thekogans/util/NullLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/File.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Producer.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Forward declaration of \see{TransactedFile} needed by \see{TransactedFileEvents}.
        struct TransactedFile;
        /// \brief
        /// Forward declaration of \see{SimpleTransactedFile} needed by \see{TransactedFile}.
        struct SimpleTransactedFile;

        /// \struct TransactedFileEvents TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        /// Subscribe to TransactedFileEvents to receive transaction notifications.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileEvents {
            /// \brief
            /// Transaction is committing. Depending on the phase do whatever
            /// is appropriate.
            /// If your object derives from \see{TransactedFile::Object}
            /// all this is done under the hood for you. All you will need
            /// to do is implement Size, Read and Write.
            /// \param[in] file TransactedFile comming the transaction.
            /// \param[in] phase Either COMMIT_PHASE_1 or COMMIT_PHASE_2.
            virtual void OnTransactedFileCommit (
                RefCounted::SharedPtr<TransactedFile> /*file*/,
                int phase) noexcept {}
            /// \brief
            /// Transaction is aborting.
            /// \param[in] file TransactedFile abortig the transaction.
            virtual void OnTransactedFileAbort (
                RefCounted::SharedPtr<TransactedFile> /*file*/) noexcept {}
        };

        /// \brief
        /// We do our own locking.
        using TransactedFileAddressSpaceType = PageMap<ui64, BitWidth<ui64>::value, NullLock>;

        /// \struct TransactedFile TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFile :
                private File,
                public Producer<TransactedFileEvents>,
                public Subscriber<TransactedFileEvents>,
                public TransactedFileAddressSpaceType::PageSource {
            /// \brief
            /// TransactedFile participates in the \see{DynamicCreatable}
            /// dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (TransactedFile)

            using AddressSpaceType = TransactedFileAddressSpaceType;
            using AddressType = AddressSpaceType::AddressType;
            static constexpr std::size_t AddressTypeSize = AddressSpaceType::AddressTypeSize;
            using SizeType = AddressSpaceType::SizeType;
            static constexpr std::size_t SizeTypeSize = AddressSpaceType::SizeTypeSize;

            /// \brief
            /// TransactedFile implements a two phase commit. The
            /// first phase (usually called the allocation phase)
            /// will have all \see{Object}s allocate the space they
            /// need to commit themselves to disk. The space is
            /// allocated using an \see{Allocator}. This is also
            /// the time when all offset pointers are updated (Again
            /// see \see{Object} and \see{ObjectEvents}). The second
            /// phase (usually called the commit phase) will have
            /// all objects flush themselves to disk. This logic is
            /// enshrined in \see{Object}.

            /// \brief
            /// Commit phase 1 (allocation).
            static const int COMMIT_PHASE_1;
            /// \brief
            /// Commit phase 2 (flush).
            static const int COMMIT_PHASE_2;

        private:
            /// \brief
            /// File size.
            SizeType size;
            /// \brief
            /// \see{PageMap} used to map file pages.
            AddressSpaceType::SharedPtr pageMap;
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            #include "thekogans/util/TransactedFileRange.h"
            #include "thekogans/util/TransactedFileBlock.h"
            #include "thekogans/util/TransactedFileObject.h"
            #include "thekogans/util/TransactedFileAllocator.h"

        private:
            /// \struct TransactedFile::Header TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            struct _LIB_THEKOGANS_UTIL_DECL Header {
                /// \brief
                /// When creating a new heap, this flag will stamp the structure
                /// of \see{Block} so that if you ever try to open it with a
                /// wrongly compiled version of thekogans_util it will complain
                /// instead of corrupting your data.
                static const ui32 FLAGS_BLOCK_USES_MAGIC = 1;
                /// \brief
                /// If set, zero out freed blocks.
                static const ui32 FLAGS_SECURE = 2;
                /// \brief
                /// Heap flags.
                Flags32 flags;
                AddressType allocatorOffset;
                /// \brief
                /// Contains the offset of the \see{Registry}.
                AddressType user1Offset;
                AddressType user2Offset;
                // NOTE: If you add new fields, adjust the SIZE and increment
                // the version of your derivative and add if statements to operator
                // << and >> to read and write them. And don't forget to rebuild
                // all dependent allocators as it's likely they will have based
                // their header extensions by deriving from this one.

                /// \brief
                /// The size of the header on disk.
                static constexpr std::size_t SIZE =
                    UI32_SIZE +       // magic
                    UI32_SIZE +       // flags
                    AddressTypeSize + // allocatorOffset
                    AddressTypeSize + // user1Offset
                    AddressTypeSize;  // user2Offset

                /// \brief
                /// ctor.
                /// \param[in] flags_ 0 or FLAGS_SECURE.
                Header (ui16 flags_ = 0) :
                        flags (flags_),
                        allocatorOffset (0),
                        user1Offset (0),
                        user2Offset (0) {
                #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                    flags.Set (FLAGS_BLOCK_USES_MAGIC, true);
                #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                }

                /// \brief
                /// Return true if this heap was created with a version of thekogans_util
                /// that was built with THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC.
                /// \return true == FLAGS_BLOCK_USES_MAGIC is set.
                inline bool IsBlockUsesMagic () const {
                    return flags.Test (FLAGS_BLOCK_USES_MAGIC);
                }
                /// \brief
                /// Return true if secure.
                /// \return true == secure.
                inline bool IsSecure () const {
                    return flags.Test (FLAGS_SECURE);
                }
            } header;

            /// \brief
            /// \see{Allocator} associated with this file.
            Allocator::SharedPtr allocator;

        public:
            /// \brief
            /// ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            TransactedFile (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path = std::string ());
            /// \brief
            /// ctor. Open or create the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
        #if defined (TOOLCHAIN_OS_Windows)
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
        #endif // defined (TOOLCHAIN_OS_Windows)
            TransactedFile (
                Endianness endianness,
                const std::string &path,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                DWORD dwCreationDisposition = OPEN_ALWAYS,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL);
            #else // defined (TOOLCHAIN_OS_Windows)
                i32 flags = O_RDWR | O_CREAT,
                i32 mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// dtor. Close the file.
            virtual ~TransactedFile ();

            inline bool IsSecure () const noexcept {
                return header.IsSecure ();
            }

            /// \brief
            /// Return the file path.
            /// \return File path.
            inline const std::string &GetPath () const {
                return path;
            }

            /// \brief
            /// Return the \see{Allocator} attached to thi file.
            /// Since allocator is constant after Init, this API
            /// is thread safe.
            /// \return allocator.
            inline Allocator::SharedPtr GetAllocator () const {
                return allocator;
            }
            /// \brief
            /// Return the \see{Registry} attached to thi file.
            /// Since registry is constant after Init, this API
            /// is thread safe.
            /// \return registry.
            void SetAllocator (Allocator::SharedPtr allocator);

            /// \brief
            /// Return file size in bytes.
            /// Thread safe.
            /// \return File size in bytes.
            inline SizeType GetSizeEx () {
                LockGuard<SpinLock> guard (spinLock);
                return size;
            }

            /// \brief
            /// Return page size.
            /// \return pageMap->GetPageSize ().
            inline std::size_t GetPageSize () const {
                return pageMap->GetPageSize ();
            }
            /// \brief
            /// Return page mask.
            /// \return pageMap->GetPageMask ().
            inline std::size_t GetPageMask () const {
                return pageMap->GetPageMask ();
            }

            /// \brief
            /// Open or create the file.
            /// NOT thread safe.
            /// \param[in] path Path to file to open.
        #if defined (TOOLCHAIN_OS_Windows)
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
        #endif // defined (TOOLCHAIN_OS_Windows)
            virtual void Open (
                const std::string &path,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE,
                DWORD dwCreationDisposition = OPEN_EXISTING,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL) override;
            #else // defined (TOOLCHAIN_OS_Windows)
                i32 flags = O_RDWR,
                i32 mode = S_IRUSR | S_IWUSR) override;
            #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Close the file.
            /// Thread safe.
            virtual void Close () override;

            /// \brief
            /// Thread safe version of Read.
            /// \param[in] offset Start of range.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            std::size_t ReadEx (
                AddressType offset,
                void *buffer,
                SizeType count);
            /// \brief
            /// Thread safe version of Write.
            /// \param[in] offset Start of range.
            /// \param[in] buffer Where bytes come from.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            std::size_t WriteEx (
                AddressType offset,
                const void *buffer,
                SizeType count);

            /// \brief
            /// Grow the file by the given amount.
            /// Thread safe.
            /// \param[in] amount Amount to grow the file by.
            /// \return Old file size.
            SizeType Grow (SizeType amount);
            /// \brief
            /// Shrink the file by the given amount.
            /// Thread safe.
            /// \param[in] amount Amount to shrink the file by.
            /// \return New file size.
            SizeType Shrink (SizeType amount);

            /// \brief
            /// Commit the current transaction.
            /// Used by \see{Transaction} to commit the current changes.
            /// Thread safe.
            void Commit (bool clearCache = false);
            /// \brief
            /// Abort the current transaction.
            /// Used by \see{Transaction} dtor to abort uncommitted changes.
            /// Thread safe.
            void Abort ();

        private:
            // PageSource
            /// \brief
            /// Read raw bytes.
            /// \param[in] offset Offset to seek to.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual SizeType ReadPage (
                    AddressType offset,
                    void *buffer,
                    SizeType count) override {
                Seek (offset, SEEK_SET);
                return Read (buffer, count);
            }
            /// \brief
            /// Write raw bytes.
            /// \param[in] offset Offset to seek to.
            /// \param[in] buffer Bytes to write.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual SizeType WritePage (
                    AddressType offset,
                    const void *buffer,
                    SizeType count) override {
                Seek (offset, SEEK_SET);
                return Write (buffer, count);
            }

            /// \brief
            /// This is a wrapper around pageMap.GetPage.
            /// Thread safe.
            /// \param[in] offset Offset whose page to return.
            /// \return Page that covers the neighborhood around the given offset.
            AddressSpaceType::Page::SharedPtr GetPage (AddressType offset);

            virtual void OnTransactedFileCommit (
                RefCounted::SharedPtr<TransactedFile> /*file*/,
                int phase) noexcept override;
            /// \brief
            /// Transaction is aborting.
            /// \param[in] file TransactedFile abortig the transaction.
            virtual void OnTransactedFileAbort (
                RefCounted::SharedPtr<TransactedFile> /*file*/) noexcept override;

            /// \brief
            /// Used by Open to apply the log before opening the file.
            /// \path path File path.
            static void CommitLog (const std::string &path);

            /// \brief
            /// Given a file path, use the full file name to create
            /// a GUID to append to the path to act as the log path.
            /// Thread safe.
            /// \param[in] path File path to turn in to log path.
            /// \return Log path based on the passed in file path.
            static std::string GetLogPath (const std::string &path);

            /// \brief
            /// TransactedFile is neither copy or move constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (TransactedFile)
        };

        _LIB_THEKOGANS_UTIL_DECL TransactedFile::Range & _LIB_THEKOGANS_UTIL_API operator >> (
            TransactedFile::Range &range,
            TransactedFile::Object::SharedPtr &object);

        /// \def THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_EXTRACTION_OPERATOR(_T)
        /// Implement extraction operator for _T::SharedPtr.
        #define THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_EXTRACTION_OPERATOR(_T)\
            inline thekogans::util::TransactedFile::Range & _LIB_THEKOGANS_UTIL_API operator >> (\
                    thekogans::util::TransactedFile::Range &range,\
                    _T::SharedPtr &object) {\
                thekogans::util::TransactedFile::Object::SharedPtr base;\
                range >> base;\
                object = base;\
                return range;\
            }

        /// \brief
        /// Implement \see{TransactedFile::Allocator} extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_EXTRACTION_OPERATOR (TransactedFile::Allocator)

        /// \struct SimpleTransactedFile TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        /// SimpleTransactedFile exposes the basic flags supported by the standard
        /// library open that are portable across Windows, Linux and OS X.
        /// NOTE: On Linux and OS X if a file needs to be created, it's
        /// mode will be 0644. This is fine for most cases but might not
        /// be appropriate for some. If you need to control the mode of
        /// the created file use TransactedFile instead.
        struct _LIB_THEKOGANS_UTIL_DECL SimpleTransactedFile : public TransactedFile {
            /// \brief
            /// Default ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            SimpleTransactedFile (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path = std::string ()) :
                TransactedFile (endianness, handle, path) {}
            /// \brief
            /// ctor. Abstracts most useful functionality from POSIX open.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] flags Most useful POSIX open flags.
            SimpleTransactedFile (
                Endianness endianness,
                const std::string &path,
                Flags32 flags = SimpleFile::ReadWrite | SimpleFile::Create);

            /// \brief
            /// Open the file.
            /// \param[in] path File path.
            /// \param[in] flags File open flags.
            void SimpleOpen (
                const std::string &path,
                Flags32 flags = SimpleFile::ReadWrite | SimpleFile::Create);

            /// \brief
            /// SimpleTransactedFile is neither copy or move constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (SimpleTransactedFile)
        };

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_TransactedFile_h
