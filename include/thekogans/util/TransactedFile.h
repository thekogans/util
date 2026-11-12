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
#include "thekogans/util/Flags.h"
#include "thekogans/util/PageMap.h"
#include "thekogans/util/File.h"
#include "thekogans/util/Serializable.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/Producer.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Forward declaration of \see{TransactedFile} needed by \see{TransactedFileTransactionEvents}.
        struct TransactedFile;

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
                PageMap64::AddressType offset) noexcept {}
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

        /// \struct TransactedFile TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFile :
                private File,
                public Producer<TransactedFileTransactionEvents>,
                public Producer<TransactedFileStructureEvents>,
                public PageMap64::PageSource {
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (TransactedFile)

            using AddressSpaceType = PageMap64;
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
            #include "thekogans/util/TransactedFileHeader.h"

        private:
            /// \brief
            /// File size.
            std::atomic<SizeType> size{0};
            /// \brief
            /// File \see{PageMap} used to map file pages.
            AddressSpaceType::SharedPtr pageMap;
            /// \brief
            /// File header.
            Header::SharedPtr header;
            /// \brief
            /// File \see{DynamicCreatable::TypeDictionary}.
            DynamicCreatable::TypeDictionary::SharedPtr dictionary;
            /// \brief
            /// File \see{Allocator}.
            Allocator::SharedPtr allocator;
            /// \brief
            /// File \see{Registry}.
            Registry::SharedPtr registry;
            /// \brief
            /// Current \see{Transaction} phase.
            std::atomic<int> phase{Transaction::PHASE_NONE};
            /// \brief
            /// \see{Transaction} mutex.
            Mutex mutex;

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
            /// Return file size in bytes.
            /// Thread safe.
            /// \return File size in bytes.
            inline SizeType GetSizeEx () const {
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
            /// Return true if secure.
            /// \return true == secure.
            inline bool IsSecure () const {
                return header->IsSecure ();
            }

            virtual DynamicCreatable::TypeDictionary::SharedPtr GetTypeDictionary () const override {
                return dictionary;
            }
            /// \brief
            /// Return the \see{Allocator} attached to this file.
            /// \return allocator.
            inline Allocator::SharedPtr GetAllocator () const {
                return allocator;
            }
            /// \brief
            /// Return the \see{Registry} attached to this file.
            /// \return registry.
            inline Registry::SharedPtr GetRegistry () const {
                return registry;
            }

            inline int GetTransactionPhase () const {
                return phase;
            }
            inline bool CheckCanMutate () const {
                return phase == Transaction::PHASE_IN_PROGRESS || phase == Transaction::PHASE_ALLOC;
            }

            /// \brief
            /// Grow the file by the given amount.
            /// \param[in] amount Amount to grow the file by.
            /// \return Old file size.
            SizeType Grow (SizeType amount);
            /// \brief
            /// Shrink the file by the given amount.
            /// \param[in] amount Amount to shrink the file by.
            /// \return New file size.
            SizeType Shrink (SizeType amount);

        protected:
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

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_EXTRACTION_OPERATOR (TransactedFile::Header)

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_TransactedFile_h
