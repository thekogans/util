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

#if !defined (__thekogans_util_TransactedFileBTreeRegistry_h)
#define __thekogans_util_TransactedFileBTreeRegistry_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/TransactedFileBTree.h"
#include "thekogans/util/TransactedFileBTreeKeys.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        /// \struct TransactedFileBTreeRegistry TransactedFileBTreeRegistry.h
        /// thekogans/util/TransactedFileBTreeRegistry.h
        ///
        /// \brief
        /// TransactedFileBTreeRegistry is a \see{TransactedFile::Regitry}.
        /// It uses a \see{TransactedFileBree} to provide global ordered,
        /// associative storage for \see{TransactedFile} clients. Use it to
        /// store and retrieve practically any value derived from \see{Serializable}.
        /// The key type is std::string. TransactedFileBTreeRegistry is thread safe.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileBTreeRegistry :
                public TransactedFile::Registry,
                public Subscriber<TransactedFile::ObjectEvents> {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (TransactedFileBTreeRegistry)

        private:
            /// \brief
            /// true == \see{TransactedFileBTree::Node} stores values
            /// as \see{TransactedFile::Object}.
            bool valueAsObject;
            /// \brief
            /// Number of entries per \see{TransactedFileBTree::Node}.
            std::size_t entriesPerNode;
            /// \brief
            /// Number of \see{TransactedFileBTree::Node}s per allocator page.
            std::size_t nodesPerPage;
            /// \brief
            /// Where \see{TransactedFileBTree::Node} pages come from.
            Allocator::SharedPtr allocator;
            /// \struct TransactedFileBTreeRegistry::Header TransactedFileBTreeRegistry.h
            /// thekogans/util/TransactedFileBTreeRegistry.h
            ///
            /// \brief
            /// Header stores global registry info.
            struct _LIB_THEKOGANS_UTIL_DECL Header {
                /// \brief
                /// \see{TransactedFileBTree::Header} offset.
                TransactedFile::Allocator::PtrType btreeOffset;

                /// \brief
                /// The size of the header on disk.
                static const std::size_t SIZE =
                    UI32_SIZE + // magic
                    TransactedFile::Allocator::PTR_TYPE_SIZE; // btreeOffset

                /// \brief
                /// ctor.
                Header () :
                    btreeOffset (0) {}
            } header;
            /// \brief
            /// \see{TransactedFileBTree} used to store key/values.
            TransactedFileBTree::SharedPtr btree;
            /// \brief
            /// Synchronization lock.
            mutable SpinLock spinLock;

        public:
            /// \brief
            /// Default number of entries per \see{TransactedFileBTree::Node}.
            static const std::size_t DEFAULT_BTREE_ENTRIES_PER_NODE = 32;
            /// \brief
            /// Number of \see{TransactedFileBTree::Node}s that will fit in to a \see{BlockAllocator} page.
            static const std::size_t DEFAULT_BTREE_NODES_PER_PAGE = 5;

            /// \brief
            /// ctor.
            /// \param[in] valueAsObject_ true == \see{TransactedFileBTree::Node} stores values
            /// as \see{TransactedFile::Object}.
            /// \param[in] entriesPerNode_ Number of entries per \see{TransactedFileBTree::Node}.
            /// \param[in] nodesPerPage Number of \see{TransactedFileBTree::Node}s per allocator page.
            /// \param[in] allocator_ Where \see{TransactedFileBTree::Node} pages come from.
            TransactedFileBTreeRegistry (
                bool valueAsObject_ = true,
                std::size_t entriesPerNode_ = DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t nodesPerPage_ = DEFAULT_BTREE_NODES_PER_PAGE,
                util::Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ()) :
                valueAsObject (valueAsObject_),
                entriesPerNode (entriesPerNode_),
                nodesPerPage (nodesPerPage_),
                allocator (allocator_) {}

            /// \brief
            /// Given a key, retrieve the associated value. If key is not found,
            /// return nullptr.
            /// \param[in] key Key whose value to retrieve.
            /// \return Value @ key, nullptr if key is not found.
            virtual Serializable::SharedPtr GetValue (const std::string &key) override;
            /// \brief
            /// Given a key, do one of the following three;
            /// 1. If value != nullptr and key is not found, insert new key/value.
            /// 2. If value != nullptr and key is found, replace the old value with new.
            /// 3. if value == nullptr, and key if found, delete the key from the registry.
            /// \param[in] key Key to search/delete.
            /// \param[in] value Value to set/replace/delete.
            virtual void SetValue (
                const std::string &key,
                Serializable::SharedPtr value) override;

        protected:
            // Serializable
            /// \brief
            /// Read the \see{Header} from the file.
            /// \param[in] header \see{SerializableHeader} governig the underlying data.
            /// \param[in] serializer \see{Serializer} to read the \see{Header} from.
            virtual void Read (
                const SerializableHeader & /*header*/,
                Serializer &serializer) override;
            /// \brief
            /// Write the \see{Header} to the file.
            /// \param[in] serializer \see{Serializer} to write the \see{Header} to.
            virtual void Write (Serializer &serializer) const override;

            // TransactedFile::ObjectEvents
            /// \brief
            /// \see{TransactedFileBTree} allocated a block for it's \see{TransactedFileBTree::Header}.
            /// \param[in] object \see{TransactedFileBTree} whose offset has become valid.
            virtual void OnTransactedFileObjectAlloc (
                    TransactedFile::Object::SharedPtr object) noexcept override {
                LockGuard<SpinLock> guard (spinLock);
                header.btreeOffset = object->GetOffset ();
                SetDirty (true);
            }
            /// \brief
            /// \see{TransactedFileBTree} freed its \see{TransactedFileBTree::Header} block.
            /// \param[in] object \see{TransactedFileBTree} whose \see{TransactedFileBTree::Header}
            /// has become invalid.
            virtual void OnTransactedFileObjectFree (
                    TransactedFile::Object::SharedPtr /*object*/) noexcept override {
                LockGuard<SpinLock> guard (spinLock);
                header.btreeOffset = 0;
                SetDirty (true);
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
            /// TransactedFileBTreeRegistry is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TransactedFileBTreeRegistry)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileBTreeRegistry_h)
