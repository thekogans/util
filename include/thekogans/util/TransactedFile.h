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
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/File.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Serializable.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/Producer.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Forward declaration of \see{TransactedFile} needed by \see{TransactedFileTransactionEvents}.
        struct TransactedFile;

        /// \brief
        /// We do our own locking.
        using TransactedFileAddressSpaceType = PageMap<ui64, BitWidth<ui64>::value, NullLock>;

        /// \struct TransactedFileTransactionEvents TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        /// Subscribe to TransactedFileTransactionEvents to receive transaction notifications.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileTransactionEvents {
            /// \brief
            /// dtor.
            virtual ~TransactedFileTransactionEvents () {}

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
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFile :
                private File,
                public Producer<TransactedFileTransactionEvents>,
                public Producer<TransactedFileStructureEvents>,
                public Subscriber<TransactedFileTransactionEvents>,
                public TransactedFileAddressSpaceType::PageSource {
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (TransactedFile)

            using AddressSpaceType = TransactedFileAddressSpaceType;
            using AddressType = AddressSpaceType::AddressType;
            static constexpr std::size_t AddressTypeSize = AddressSpaceType::AddressTypeSize;
            using SizeType = AddressSpaceType::SizeType;
            static constexpr std::size_t SizeTypeSize = AddressSpaceType::SizeTypeSize;

            #include "thekogans/util/TransactedFileTransaction.h"
            #include "thekogans/util/TransactedFileRange.h"
            #include "thekogans/util/TransactedFileBlock.h"
            #include "thekogans/util/TransactedFileObject.h"
            #include "thekogans/util/TransactedFileAllocator.h"
            #include "thekogans/util/TransactedFileRegistry.h"

        private:
            /// \struct TransactedFile::Header TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// Header contains the global heap values.
            struct _LIB_THEKOGANS_UTIL_DECL Header {
                /// \brief
                /// If set, zero out freed blocks.
                static constexpr ui32 FLAGS_SECURE = 1;
                /// \brief
                /// Heap flags.
                Flags32 flags;
                AddressType dictionaryOffset{0};
                AddressType allocatorOffset{0};
                /// \brief
                /// Contains the offset of the \see{Registry}.
                AddressType registryOffset{0};
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
                    AddressTypeSize + // dictionaryOffset
                    AddressTypeSize + // allocatorOffset
                    AddressTypeSize;  // regisryOffset

                /// \brief
                /// ctor.
                /// \param[in] flags_ 0 or FLAGS_SECURE.
                Header (ui32 flags_ = 0) :
                    flags (flags_) {}

                /// \brief
                /// Return true if secure.
                /// \return true == secure.
                inline bool IsSecure () const {
                    return flags.Test (FLAGS_SECURE);
                }
            } header;
            /// \brief
            /// File size.
            std::atomic<SizeType> size{0};
            /// \brief
            /// \see{PageMap} used to map file pages.
            AddressSpaceType::SharedPtr pageMap{nullptr};
            /// \brief
            /// \see{Allocator} associated with this file.
            DynamicCreatable::TypeDictionary::SharedPtr dictionary{nullptr};
            /// \brief
            /// \see{Allocator} associated with this file.
            Allocator::SharedPtr allocator{nullptr};
            /// \brief
            /// \see{Registry} associated with this file.
            Registry::SharedPtr registry{nullptr};
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

            /// \brief
            /// ctor.
            /// \param[in] path File path.
            TransactedFile (const std::string &path);
            /// \brief
            /// ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] path File path.
            /// \param[in] secure
            /// \param[in] dictionary_
            /// \param[in] allocator_
            /// \param[in] registry_
            /// \param[in] forceTruncate
            TransactedFile (
                Endianness endianness,
                const std::string &path,
                bool secure,
                DynamicCreatable::TypeDictionary::SharedPtr dictionary_,
                Allocator::SharedPtr allocator_,
                Registry::SharedPtr registry_,
                bool forceTruncate);

        public:
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
                DynamicCreatable::TypeDictionary::SharedPtr dictionary,
                Allocator::SharedPtr allocator,
                Registry::SharedPtr registry,
                bool overwrite = false);

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
            /// Set the header.dictionaryOffset.
            /// \param[in] dictionaryOffset New dictionaryOffset.
            void SetTypeDictionaryOffset (AddressType dictionaryOffset);
            /// \brief
            /// Set the header.allocatorOffset.
            /// \param[in] allocatorOffset New allocatorOffset.
            void SetAllocatorOffset (AddressType allocatorOffset);
            /// \brief
            /// Set the header.registryOffset.
            /// \param[in] registryOffset New registryOffset to set.
            void SetRegistryOffset (AddressType registryOffset);

            virtual DynamicCreatable::TypeDictionary::SharedPtr GetTypeDictionary () const override {
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

        protected:
            // TransactedFileTransactionEvents
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

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_TransactedFile_h
