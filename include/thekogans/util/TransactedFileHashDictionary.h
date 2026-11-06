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

#if !defined (__thekogans_util_TransactedFileHashDictionary_h)
#define __thekogans_util_TransactedFileHashDictionary_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/TransactedFileBTree.h"
#include "thekogans/util/TransactedFileBTreeKeys.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        /// \struct TransactedFileHashDictionary TransactedFileHashDictionary.h
        /// thekogans/util/TransactedFileHashDictionary.h
        ///
        /// \brief
        /// TransactedFileHashDictionary is a \see{TransactedFile::Regitry}.
        /// It uses a \see{TransactedFileBree} to provide global ordered,
        /// associative storage for \see{TransactedFile} clients. Use it to
        /// store and retrieve practically any value derived from \see{Serializable}.
        /// The key type is std::string. TransactedFileHashDictionary is thread safe.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileHashDictionary : public TransactedFile::Dictionary {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT (TransactedFileHashDictionary)

        private:
            /// \struct TransactedFileHashDictionary::Header TransactedFileHashDictionary.h
            /// thekogans/util/TransactedFileHashDictionary.h
            ///
            /// \brief
            /// Header stores global registry info.
            struct _LIB_THEKOGANS_UTIL_DECL Header {
                /// \brief
                /// \see{TransactedFileBTree::Header} offset.
                TransactedFile::AddressType typesOffset;

                /// \brief
                /// The size of the header on disk.
                static const std::size_t SIZE =
                    UI32_SIZE + // magic
                    TransactedFile::AddressTypeSize; // typesOffset

                /// \brief
                /// ctor.
                Header (TransactedFile::AddressType typesOffset_ = 0) :
                    typesOffset (typesOffset_) {}
            } header;
            /// \brief
            std::vector<std::string> types;
            using HashMapType = std::unordered_map<std::string, TypeIdType>;
            HashMapType hashMap;
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            /// \brief
            /// ctor.
            TransactedFileHashDictionary (
                TransactedFile::SharedPtr file = nullptr,
                TransactedFile::AddressType offset = 0);

            virtual TypeIdType GetOrCreateTypeId (const std::string &type) override;
            virtual std::string ResolveTypeId (TypeIdType typeId) override;

        protected:
            // Serializable
            /// \brief
            /// Read the \see{Header} from the file.
            /// \param[in] serializer \see{Serializer} to read the \see{Header} from.
            virtual void Read (Serializer &serializer) override;
            /// \brief
            /// Write the \see{Header} to the file.
            /// \param[in] serializer \see{Serializer} to write the \see{Header} to.
            virtual void Write (Serializer &serializer) override;

            // TransactedFile::Object
            virtual void AllocBlock () override {
                TransactedFile::Object::AllocBlock ();
                file->SetDictionaryOffset (offset);
            }
            virtual void FreeBlock () override {
                file->GetAllocator ()->Free (header.typesOffset);
                header.typesOffset = 0;
                Object::FreeBlock ();
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
            /// TransactedFileHashDictionary is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (TransactedFileHashDictionary)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileHashDictionary_h)
