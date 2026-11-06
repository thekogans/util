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
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/PageMap.h"
#include "thekogans/util/NullLock.h"
#include "thekogans/util/Mutex.h"
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

        /// \brief
        /// We do our own locking.
        using TransactedFileAddressSpaceType = PageMap<ui64, BitWidth<ui64>::value, NullLock>;

        /// \struct TransactedFileEvents TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        /// Subscribe to TransactedFileEvents to receive transaction notifications.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileEvents {
            /// \brief
            /// dtor.
            virtual ~TransactedFileEvents () {}

            /// \brief
            /// Transaction is committing. Depending on the phase do whatever
            /// is appropriate.
            /// If your object derives from \see{TransactedFile::Object}
            /// all this is done under the hood for you. All you will need
            /// to do is implement Size, Read and Write.
            /// \param[in] file TransactedFile comming the transaction.
            /// \param[in] phase Either COMMIT_PHASE_1 or COMMIT_PHASE_2.
            virtual void OnTransactedFileTransactionCommit (
                RefCounted::SharedPtr<TransactedFile> /*file*/,
                int phase) noexcept {}
            /// \brief
            /// Transaction is aborting.
            /// \param[in] file TransactedFile abortig the transaction.
            virtual void OnTransactedFileTransactionAbort (
                RefCounted::SharedPtr<TransactedFile> /*file*/) noexcept {}
        };

        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileStructureEvents {
            virtual ~TransactedFileStructureEvents () {}

            // Broadcast for terminal file destructions
            virtual void OnTransactedFileOffsetDestroyed (
                RefCounted::SharedPtr<TransactedFile> /*file*/,
                TransactedFileAddressSpaceType::AddressType offset) noexcept {}
        };

        /// \struct TransactedFile TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        /// TransactedFile is the heart of a structured data management system.
        /// Together with \see{TransactedFile::Allocator}, \see{TransactedFile::Registry},
        /// and \see{TransactedFile::Object} they form the core of an OODBMS engine.
        /// TransactedFile is designed to be used in one of two distinct modes;
        /// 1. Fully structured, taking advantage of the classes mentioned above
        /// and 2. As a very powerful and efficient replacement for \see{File} where
        /// you use the built in page caching machinery to speed up reads and writes
        /// but is otherwise unstructured.
        /// ************************** PLEASE READ ****************************
        /// VERY IMPORTANT: In general, TransactedFile is NOT thread safe. Specifically
        /// the legacy \see{File} API is not. I felt introducing a lock with every API
        /// access would be very costly performance wise. And even if I did, the API
        /// design is such that certain common operations (Seek/Read, Seek/Write) would
        /// still not be safe as they would not execute atomicaly. Instead, a higher
        /// level API (ReadEx, WriteEx, Grow, Shrink) is introduced. Together with
        /// \see{TransactedFile::Range} and it's derivatives and \see{TransactedFile::Allocator}
        /// and it's derivatives the following multi-threaded use case is prescribed;
        ///
        /// main thread;
        ///   open/create file.
        ///   release master threads.
        ///   close file.
        ///
        /// master thread;
        ///   if writting
        ///     start transaction. This will lock the file for exclusive access.
        ///   release worker threads that use \see{TransactedFile::Allocator} to
        ///     allocate blocs and \see{TransactedFile::Range} to read and write them.
        ///     It's highly recommended that you use \see{TransactedFile::TransactionParticipant}
        ///     and \see{TransactedFile::Object}. \see{TransactedFile::Regitry}
        ///     can also be used to store global heap objects based on \see{Serializable}.
        ///   wait for workers to enter a barrier (i.e. signal they're done).
        ///   if writting
        ///     optionally delete cache.
        ///     commit transaction.
        /// *******************************************************************
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFile :
                private File,
                public Producer<TransactedFileEvents>,
                public Producer<TransactedFileStructureEvents>,
                public Subscriber<TransactedFileEvents>,
                public TransactedFileAddressSpaceType::PageSource {
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (TransactedFile)

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
            static constexpr int COMMIT_PHASE_1 = 1;
            /// \brief
            /// Commit phase 2 (flush).
            static constexpr int COMMIT_PHASE_2 = 2;

            /// \struct TransactedFile::Transaction TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// A very simple transaction scope guard.
            /// Call Commit before the end of the
            /// scope to commit the transaction.
            /// NOTE: If you're only reading the file, this is unnecessary.
            struct _LIB_THEKOGANS_UTIL_DECL Transaction {
            private:
                /// \see{TransactedFile} we're guarding.
                TransactedFile &file;
                /// \brief
                /// This guard will serialize all transactions.
                LockGuard<Mutex> guard;

            public:
                /// \brief
                /// ctor.
                /// \param[in] file_ \see{TransactedFile} to transact.
                explicit Transaction (TransactedFile &file_) :
                    file (file_),
                    guard (file.mutex) {}
                /// \brief
                /// dtor. Abort an uncommitted transaction.
                ~Transaction ();

                /// \brief
                /// Commit transaction before dtor aborts it.
                /// NOTE: You can call Commit as many times as you like in the
                /// scope of a single transaction. Effectively turning Commit
                /// in to Checkpoint. That's only an illusion since there is
                /// no Rollback.
                /// \param[in] clearCache true == Clear \see{PageMap} cache after committing.
                void Commit (bool clearCache = false);

                /// \brief
                /// Transaction is neither copy or move constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (Transaction)
            };

        private:
            /// \struct TransactedFile::Header TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// Header contains the global heap values.
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
                AddressType dictionaryOffset;
                AddressType allocatorOffset;
                /// \brief
                /// Contains the offset of the \see{Registry}.
                AddressType registryOffset;
                AddressType userOffset;
                // NOTE: If you add new fields, adjust the SIZE and increment
                // the version of your derivative and add if statements to operator
                // << and >> to read and write them. And don't forget to rebuild
                // all dependent allocators as it's likely they will have based
                // their header extensions by deriving from this one.

                /// \brief
                /// The size of the header on disk.
                static const std::size_t SIZE =
                    UI32_SIZE +       // magic
                    UI32_SIZE +       // flags
                    AddressTypeSize + // dictionaryOffset
                    AddressTypeSize + // allocatorOffset
                    AddressTypeSize + // regisryOffset
                    AddressTypeSize;  // userOffset

                /// \brief
                /// ctor.
                /// \param[in] flags_ 0 or FLAGS_SECURE.
                Header (ui16 flags_ = 0) :
                        flags (flags_),
                        dictionaryOffset (0),
                        allocatorOffset (0),
                        registryOffset (0),
                        userOffset (0) {
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

                void Read (Serializer &serializer);
                void Write (Serializer &serializer);
            } header;
            /// \brief
            /// File size.
            SizeType size;
            /// \brief
            /// \see{PageMap} used to map file pages.
            AddressSpaceType::SharedPtr pageMap;
            /// \brief
            /// For use by \see{Transaction}.
            Mutex mutex;
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            #include "thekogans/util/TransactedFileRange.h"
            #include "thekogans/util/TransactedFileBlock.h"
            #include "thekogans/util/TransactedFileObject.h"
            #include "thekogans/util/TransactedFileDictionary.h"
            #include "thekogans/util/TransactedFileAllocator.h"
            #include "thekogans/util/TransactedFileRegistry.h"

        private:
            /// \brief
            /// \see{Allocator} associated with this file.
            Dictionary::SharedPtr dictionary;
            /// \brief
            /// \see{Allocator} associated with this file.
            Allocator::SharedPtr allocator;
            /// \brief
            /// \see{Registry} associated with this file.
            Registry::SharedPtr registry;

            /// \brief
            /// ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            TransactedFile (const std::string &path);
            TransactedFile (
                Endianness endianness,
                const std::string &path,
                bool secure,
                Dictionary::SharedPtr dictionary_,
                Allocator::SharedPtr allocator_,
                Registry::SharedPtr registry_,
                bool forceTruncate);

        public:
            /// \brief
            /// dtor. Close the file.
            virtual ~TransactedFile ();

            /// \brief
            /// Return the file path.
            /// \return File path.
            inline const std::string &GetPath () const {
                return path;
            }

            /// \brief
            /// Return true if secure.
            /// \return true == secure.
            inline bool IsSecure () const {
                return header.IsSecure ();
            }

            /// \brief
            /// Return the header.dictionaryOffset;
            /// \return header.dictionaryOffset;
            inline AddressType GetDictionaryOffset () const {
                return header.dictionaryOffset;
            }
            /// \brief
            /// Set the header.dictionaryOffset.
            /// \param[in] dictionaryOffset New dictionaryOffset.
            void SetDictionaryOffset (AddressType dictionaryOffset);

            /// \brief
            /// Return the header.allocatorOffset;
            /// \return header.allocatorOffset;
            inline AddressType GetAllocatorOffset () const {
                return header.allocatorOffset;
            }
            /// \brief
            /// Set the header.allocatorOffset.
            /// \param[in] allocatorOffset New allocatorOffset.
            void SetAllocatorOffset (AddressType allocatorOffset);

            /// \brief
            /// Return the header.registryOffset;
            /// \return header.registryOffset;
            inline AddressType GetRegistryOffset () const {
                return header.registryOffset;
            }
            /// \brief
            /// Set the header.registryOffset.
            /// \param[in] registryOffset New registryOffset to set.
            void SetRegistryOffset (AddressType registryOffset);


            /// \brief
            /// Return the header.userOffset;
            /// \return header.userOffset;
            inline AddressType GetUserOffset () const {
                return header.userOffset;
            }
            /// \brief
            /// Set the header.userOffset.
            /// \param[in] userOffset New userOffset to set.
            void SetUserOffset (AddressType userOffset);

            Dictionary::SharedPtr GetDictionary () const {
                return dictionary;
            }
            /// \brief
            /// Return the \see{Allocator} attached to thi file.
            /// Since allocator is constant after Init, this API
            /// is thread safe.
            /// \return allocator.
            Allocator::SharedPtr GetAllocator () const {
                return allocator;
            }
            /// \brief
            /// Return the \see{Registry} attached to thi file.
            /// Since registry is constant after Init, this API
            /// is thread safe.
            /// \return registry.
            Registry::SharedPtr GetRegistry () const {
                return registry;
            }

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
            /// Open the file.
            /// \param[in] path Path to file to open.
            /// \return Open file pointer.
            static SharedPtr Open (const std::string &path);
            /// \brief
            /// Create the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] allocator \see{Allocator} to attach to this file.
            /// \param[in] registry \see{Registry} to attach to this file.
            /// \param[in] overwrite true == Overwrite the existing file if it exists.
            /// \return Created and open file pointer.
            static SharedPtr Create (
                Endianness endianness,
                const std::string &path,
                bool secure,
                Dictionary::SharedPtr dictionary,
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry,
                bool overwrite = false);
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
                std::size_t count);
            /// \brief
            /// Thread safe version of Write.
            /// \param[in] offset Start of range.
            /// \param[in] buffer Where bytes come from.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            std::size_t WriteEx (
                AddressType offset,
                const void *buffer,
                std::size_t count);

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

        private:
            // TransactedFileEvents
            /// \brief
            /// Transaction is commiting. Flush the internal cache to file.
            /// \param[in] file \see{TransactedFile} commiting the transaction.
            virtual void OnTransactedFileTransactionCommit (
                TransactedFile::SharedPtr file,
                int phase) noexcept override;
            /// \brief
            /// Transaction is aborting. Reload the internal cache from file.
            /// \param[in] file \see{TransactedFile} aborting the transaction.
            virtual void OnTransactedFileTransactionAbort (
                TransactedFile::SharedPtr file) noexcept override;

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
            /// Used by Open to apply the log before opening the file.
            /// \path path File path.
            static void CommitLog (const std::string &path);
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

            /// \brief
            /// This is a wrapper around pageMap.GetPage.
            /// Thread safe.
            /// \param[in] offset Offset whose page to return.
            /// \return Page that covers the neighborhood around the given offset.
            AddressSpaceType::Page::SharedPtr GetPage (AddressType offset);

            /// \brief
            /// Given a file path, use the full file name to create
            /// a GUID to append to the path to act as the log path.
            /// Thread safe.
            /// \param[in] path File path to turn in to log path.
            /// \return Log path based on the passed in file path.
            static std::string GetLogPath (const std::string &path);

            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
                Serializer &serializer,
                const Header &header);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator >> (
                Serializer &serializer,
                Header &header);

            /// \brief
            /// TransactedFile is neither copy or move constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (TransactedFile)
        };

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX(_T)
        /// Declare the static base map initializer. This macro is private.
        #define THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX(_T)\
        public:\
            static void StaticInitEx ();

        /// \def THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX(_T)
        /// Define the static base map initializer. This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX(_T)\
            void _T::StaticInitEx () {\
                _T::StaticInit ();\
                thekogans::util::TransactedFile::Object::DeleterMapType &deleterMap =\
                    thekogans::util::TransactedFile::Object::GetDeleters ();\
                deleterMap[_T::TYPE] = _T::FreeObject;\
            }
    #else // defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX(_T)
        /// Declare the shared base map initializer. This macro is private.
        #define THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX(_T)\
        public:\
            static const thekogans::util::TransactedFile::Object::DeleterMapInitializer deleterMapInitializer;

        /// \def THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX(_T)
        /// Define the shared base map initializer. This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX(_T)\
            const thekogans::util::TransactedFile::Object::DeleterMapInitializer _T::deleterMapInitializer (\
                _T::TYPE, _T::FreeObject);
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        #define THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT(_T)\
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (_T)\
            THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX (_T)

        #define THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT(_T, version, classSize, ...)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (\
                _T, version, classSize, thekogans::util::TransactedFile::Object::TYPE, ##__VA_ARGS__)\
            THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX (_T)

        #define THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_HAVE_CREATE(_T, version, classSize, ...)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_HAVE_CREATE (\
                _T, version, classSize, thekogans::util::TransactedFile::Object::TYPE, ##__VA_ARGS__)\
            THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_STATIC_INIT_EX (_T)

        _LIB_THEKOGANS_UTIL_DECL TransactedFile::Range & _LIB_THEKOGANS_UTIL_API operator << (
            TransactedFile::Range &range,
            const SerializableHeader &header);
        _LIB_THEKOGANS_UTIL_DECL TransactedFile::Range & _LIB_THEKOGANS_UTIL_API operator << (
            TransactedFile::Range &range,
            TransactedFile::Object &object);
        _LIB_THEKOGANS_UTIL_DECL TransactedFile::Range & _LIB_THEKOGANS_UTIL_API operator >> (
            TransactedFile::Range &range,
            SerializableHeader &header);

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
        /// Implement \see{TransactedFile::Dictionary} extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_EXTRACTION_OPERATOR (TransactedFile::Dictionary)

        /// \brief
        /// Implement \see{TransactedFile::Allocator} extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_EXTRACTION_OPERATOR (TransactedFile::Allocator)

        /// \brief
        /// Implement \see{TransactedFile::Registry} extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_EXTRACTION_OPERATOR (TransactedFile::Registry)

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_TransactedFile_h
