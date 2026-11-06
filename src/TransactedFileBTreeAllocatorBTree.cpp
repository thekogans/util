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

#include <cstring>
#include "thekogans/util/Exception.h"
#include "thekogans/util/SlabAllocatorHeap.h"
#include "thekogans/util/TransactedFileBTreeAllocator.h"

namespace thekogans {
    namespace util {

        inline Serializer &operator << (
               Serializer &serializer,
               const TransactedFileBTreeAllocator::BTree::Key &key) {
            serializer << key.size << key.offset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocator::BTree::Key &key) {
            serializer >> key.size >> key.offset;
            return serializer;
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFileBTreeAllocator::BTree::Node::Entry &entry) {
            serializer << entry.key << entry.rightOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocator::BTree::Node::Entry &entry) {
            serializer >> entry.key >> entry.rightOffset;
            // Because of the way we allocate the BTree::Node the
            // Entry ctor is never called. In a way this extraction
            // operator is our ctor. Make sure all members are
            // properly initialized.
            entry.rightNode = nullptr;
            return serializer;
        }

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT_HAVE_CREATE (
            thekogans::util::TransactedFileBTreeAllocator::BTree::Node,
            1,
            0)

        TransactedFileBTreeAllocator::BTree::Node::Node (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset,
                ui32 maxEntries_,
                tk_mem_t mem_) :
                TransactedFile::Object (file, offset),
                maxEntries (maxEntries_),
                count (0),
                leftOffset (0),
                leftNode (nullptr),
                mem (mem_),
                entries ((Entry *)(this + 1)) {
            Reload ();
        }

        TransactedFileBTreeAllocator::BTree::Node::~Node () {
            Reset ();
        }

        std::size_t TransactedFileBTreeAllocator::BTree::Node::FileSize (std::size_t maxEntries) {
                                            // key         rightOffset
            constexpr std::size_t ENTRY_SIZE = Key::SIZE + TransactedFile::AddressTypeSize;
                // magic       maxEntries  count       leftOffset                        entries
            return UI32_SIZE + UI32_SIZE + UI32_SIZE + TransactedFile::AddressTypeSize + maxEntries * ENTRY_SIZE;
        }

        std::size_t TransactedFileBTreeAllocator::BTree::Node::Size (std::size_t maxEntries) {
            return sizeof (Node) + maxEntries * sizeof (Entry);
        }

        DynamicCreatable::SharedPtr TransactedFileBTreeAllocator::BTree::Node::Create (
                DynamicCreatable::ParametersType params) {
            Node::SharedPtr dummy (Alloc (nullptr, 0, DEFAULT_BTREE_ENTRIES_PER_NODE), false);
            if (params == nullptr) {
                return dummy;
            }
            // If we were called by TransactedFile::Object::CreateObject params
            // will endow the dummy with file and offset.
            params (dummy);
            // If not, default dummy is what they get.
            if (dummy->GetFile () == nullptr || dummy->GetOffset () == 0) {
                return dummy;
            }
            // From here on out we assume that TransactedFile::Object::CreateObject
            // called us. Either way valid file and offset signifies that we are
            // looking at a real node. If anything happens from here we throw.
            ui32 maxEntries = DEFAULT_BTREE_ENTRIES_PER_NODE;
            TransactedFile::BlockRange range (dummy->GetFile (), dummy->GetOffset ());
            SerializableHeader header;
            range >> header;
            if (header.type == TYPE) {
                ui32 magic;
                range >> magic;
                if (magic == MAGIC32) {
                    range >> maxEntries;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt Node @" THEKOGANS_UTIL_UI64_FORMAT,
                        dummy->GetOffset ());
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Invalid Node header type: %s", header.type.c_str ());
            }
            // Note: No running of params here. By now they are either
            // dummy or we got what we needed.
            return Alloc (dummy->GetFile (), dummy->GetOffset (), maxEntries);
        }

        TransactedFileBTreeAllocator::BTree::Node *TransactedFileBTreeAllocator::BTree::Node::Alloc (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset,
                ui32 maxEntries) {
            tk_mem_t mem = tk_malloc (Size (maxEntries));
            Node *node = new (mem.ptr) Node (file, offset, maxEntries, mem);
            node->AddRef ();
            return node;
        }

        void TransactedFileBTreeAllocator::BTree::Node::FreeObject (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset) {
            if (offset != 0) {
                TransactedFile::BlockRange range (file, offset);
                SerializableHeader header;
                range >> header;
                if (header.type == TYPE) {
                    ui32 magic;
                    range >> magic;
                    if (magic == MAGIC32) {
                        ui32 maxEntries;
                        ui32 count;
                        range >> maxEntries >> count;
                        if (count > 0) {
                            TransactedFile::AddressType leftOffset;
                            range >> leftOffset;
                            FreeObject (file, leftOffset);
                            for (ui32 i = 0; i < count; ++i) {
                                TransactedFile::AddressType rightOffset;
                                range >> rightOffset;
                                FreeObject (file, rightOffset);
                            }
                        }
                        file->GetAllocator ()->Free (offset);
                        file->Producer<TransactedFileStructureEvents>::Produce (
                            std::bind (
                                &TransactedFileStructureEvents::OnTransactedFileOffsetDestroyed,
                                std::placeholders::_1,
                                file,
                                offset));
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Corrupt BTree::Node @" THEKOGANS_UTIL_UI64_FORMAT,
                            offset);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Header type: %s != TransactedFileBTreeAllocator::BTree::Node type: %s @" THEKOGANS_UTIL_UI64_FORMAT,
                        header.type.c_str (),
                        TYPE,
                        offset);
                }
            }
        }

        TransactedFileBTreeAllocator::BTree::Node *TransactedFileBTreeAllocator::BTree::Node::GetChild (ui32 index) {
            if (index == 0) {
                if (leftNode == nullptr && leftOffset != 0) {
                    leftNode = Alloc (file, leftOffset, maxEntries);
                }
                return leftNode;
            }
            else {
                --index;
                if (entries[index].rightNode == nullptr &&
                    entries[index].rightOffset != 0) {
                    entries[index].rightNode = Alloc (
                        file, entries[index].rightOffset, maxEntries);
                }
                return entries[index].rightNode;
            }
        }

        bool TransactedFileBTreeAllocator::BTree::Node::Find (
            const Key &key,
            ui32 &index) const {
            index = 0;
            ui32 last = count;
            while (index < last) {
                ui32 middle = (index + last) / 2;
                if (key == entries[middle].key) {
                    index = middle;
                    return true;
                }
                if (key < entries[middle].key) {
                    last = middle;
                }
                else {
                    index = middle + 1;
                }
            }
            return false;
        }

        bool TransactedFileBTreeAllocator::BTree::Node::Insert (Entry &entry) {
            ui32 index;
            // This search collapses all duplicate nodes in to one.
            // That means that you can insert the same node all you
            // want, but it will only be inserted the first time with
            // all subsequent ones being a noop.
            if (!Find (entry.key, index)) {
                Node *child = GetChild (index);
                if (child == nullptr || !child->Insert (entry)) {
                    if (!IsFull ()) {
                        InsertEntry (entry, index);
                    }
                    else {
                        // Node is full. Split it and insert the entry
                        // in to the proper daughter node. Return the
                        // entry at the split point to be added up the
                        // parent chain.
                        Node *right = Alloc (file, 0, maxEntries);
                        Split (right);
                        ui32 splitIndex = maxEntries / 2;
                        if (index != splitIndex) {
                            if (index < splitIndex) {
                                InsertEntry (entry, index);
                            }
                            else {
                                right->InsertEntry (entry, index - splitIndex);
                            }
                            entry = right->entries[0];
                            right->RemoveEntry (0);
                        }
                        right->leftOffset = entry.rightOffset;
                        right->leftNode = entry.rightNode;
                        entry.rightOffset = right->offset;
                        entry.rightNode = right;
                        return false;
                    }
                }
            }
            return true;
        }

        bool TransactedFileBTreeAllocator::BTree::Node::Remove (const Key &key) {
            ui32 index;
            bool found = Find (key, index);
            Node *child = GetChild (found ? index + 1 : index);
            if (found) {
                if (child != nullptr) {
                    Node *leaf = child;
                    while (leaf->leftOffset != 0) {
                        leaf = leaf->GetChild (0);
                    }
                    entries[index].key = leaf->entries[0].key;
                    child->Remove (leaf->entries[0].key);
                    SetDirty ();
                    if (child->IsPoor ()) {
                        RestoreBalance (index);
                    }
                }
                else {
                    RemoveEntry (index);
                }
                return true;
            }
            else if (child != nullptr && child->Remove (key)) {
                if (child->IsPoor ()) {
                    RestoreBalance (index);
                }
                return true;
            }
            return false;
        }

        void TransactedFileBTreeAllocator::BTree::Node::RestoreBalance (ui32 index) {
            if (index == count) {
                Node *left = GetChild (index - 1);
                Node *right = GetChild (index);
                if (left != nullptr && right != nullptr) {
                    if (left->IsPlentiful ()) {
                        RotateRight (index - 1, left, right);
                    }
                    else {
                        Merge (index - 1, left, right);
                    }
                }
            }
            else {
                Node *left = GetChild (index);
                Node *right = GetChild (index + 1);
                if (left != nullptr && right != nullptr) {
                    if (left->IsPlentiful ()) {
                        RotateRight (index, left, right);
                    }
                    else if (right->IsPlentiful ()) {
                        RotateLeft (index, left, right);
                    }
                    else {
                        Merge (index, left, right);
                    }
                }
            }
        }

        void TransactedFileBTreeAllocator::BTree::Node::RotateRight (
            ui32 index,
            Node *left,
            Node *right) {
            entries[index].rightOffset = right->leftOffset;
            entries[index].rightNode = right->leftNode;
            right->InsertEntry (entries[index], 0);
            ui32 lastIndex = left->count - 1;
            right->leftOffset = left->entries[lastIndex].rightOffset;
            right->leftNode = left->entries[lastIndex].rightNode;
            left->entries[lastIndex].rightOffset = right->offset;
            left->entries[lastIndex].rightNode = right;
            entries[index] = left->entries[lastIndex];
            left->RemoveEntry (lastIndex);
            SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::RotateLeft (
            ui32 index,
            Node *left,
            Node *right) {
            entries[index].rightOffset = right->leftOffset;
            entries[index].rightNode = right->leftNode;
            right->leftOffset = right->entries[0].rightOffset;
            right->leftNode = right->entries[0].rightNode;
            right->entries[0].rightOffset = right->offset;
            right->entries[0].rightNode = right;
            left->Concatenate (entries[index]);
            entries[index] = right->entries[0];
            right->RemoveEntry (0);
            SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::Merge (
            ui32 index,
            Node *left,
            Node *right) {
            assert (left->count + right->count < maxEntries);
            entries[index].rightOffset = right->leftOffset;
            entries[index].rightNode = right->leftNode;
            left->Concatenate (entries[index]);
            left->Concatenate (right);
            RemoveEntry (index);
            file->GetAllocator ()->Free (right->offset);
            right->Release ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::Split (Node *node) {
            ui32 splitIndex = maxEntries / 2;
            node->count = count - splitIndex;
            std::memcpy (
                node->entries,
                entries + splitIndex,
                node->count * sizeof (Entry));
            count = splitIndex;
            SetDirty ();
            node->SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::Concatenate (Node *node) {
            std::memcpy (
                entries + count,
                node->entries,
                node->count * sizeof (Entry));
            count += node->count;
            node->count = 0;
            SetDirty ();
            node->SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::InsertEntry (
            const Entry &entry,
            ui32 index) {
            std::memmove (
                entries + index + 1,
                entries + index,
                (count++ - index) * sizeof (Entry));
            entries[index] = entry;
            SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::RemoveEntry (ui32 index) {
            std::memcpy (
                entries + index,
                entries + index + 1,
                (--count - index) * sizeof (Entry));
            SetDirty ();
        }

        void TransactedFileBTreeAllocator::BTree::Node::Reset () {
            if (count > 0) {
                if (leftNode != nullptr) {
                    leftNode->Release ();
                    leftNode = nullptr;
                }
                for (ui32 i = 0; i < count; ++i) {
                    if (entries[i].rightNode != nullptr) {
                        entries[i].rightNode->Release ();
                        entries[i].rightNode = nullptr;
                    }
                }
                count = 0;
            }
        }

        void TransactedFileBTreeAllocator::BTree::Node::Harakiri () {
            this->~Node ();
            tk_free (mem);
        }

        void TransactedFileBTreeAllocator::BTree::Node::Read (Serializer &serializer) {
            Reset ();
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> maxEntries >> count;
                if (count > 0) {
                    serializer >> leftOffset;
                    for (ui32 i = 0; i < count; ++i) {
                        serializer >> entries[i];
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFileBTreeAllocator::BTree::Node: @" THEKOGANS_UTIL_UI64_FORMAT,
                    GetOffset ());
            }
        }

        void TransactedFileBTreeAllocator::BTree::Node::Write (Serializer &serializer) {
            serializer << MAGIC32 << maxEntries << count;
            if (count > 0) {
                if (leftNode != nullptr) {
                    leftOffset = leftNode->offset;
                }
                serializer << leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    if (entries[i].rightNode != nullptr) {
                        entries[i].rightOffset = entries[i].rightNode->offset;
                    }
                    serializer << entries[i];
                }
            }
        }

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (
            thekogans::util::TransactedFileBTreeAllocator::BTree,
            1,
            0)

        TransactedFileBTreeAllocator::BTree::BTree (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset,
                std::size_t entriesPerNode) :
                TransactedFile::Object (file, offset),
                header ((ui32)entriesPerNode),
                root (Node::Alloc (file, header.rootOffset, entriesPerNode)) {
            Reload ();
        }

        TransactedFileBTreeAllocator::BTree::~BTree () {
            root->Release ();
        }

        TransactedFileBTreeAllocator::BTree::Key TransactedFileBTreeAllocator::BTree::Find (const Key &key) {
            Key result{UI64_MAX, 0};
            Node *node = root;
            while (node != nullptr) {
                ui32 index;
                if (node->Find (key, index)) {
                    return key;
                }
                if (index < node->count &&
                    node->entries[index].key > key && node->entries[index].key < result) {
                    result = node->entries[index].key;
                }
                node = node->GetChild (index);
            }
            return result;
        }

        void TransactedFileBTreeAllocator::BTree::Insert (const Key &key) {
            Node::Entry entry (key);
            if (!root->Insert (entry)) {
                // The path to the leaf node is full.
                // Create a new root node and make the entry
                // its first.
                Node *node = Node::Alloc (file, 0, header.entriesPerNode);
                node->leftOffset = root->GetOffset ();
                node->leftNode = root;
                node->InsertEntry (entry, 0);
                root = node;
            }
            if (header.rootOffset == 0 || root->GetOffset () != header.rootOffset) {
                SetDirty ();
            }
        }

        bool TransactedFileBTreeAllocator::BTree::Remove (const Key &key) {
            bool removed = root->Remove (key);
            if (removed) {
                if (root->IsEmpty () && root->GetChild (0) != nullptr) {
                    Node *node = root;
                    root = root->GetChild (0);
                    file->GetAllocator ()->Free (node->GetOffset ());
                    node->Release ();
                }
                if (header.rootOffset == 0 || root->GetOffset () != header.rootOffset) {
                    SetDirty ();
                }
            }
            return removed;
        }

        void TransactedFileBTreeAllocator::BTree::FreeObject (
                TransactedFile::SharedPtr file,
                TransactedFile::AddressType offset) {
            TransactedFile::BlockRange range (file, offset);
            SerializableHeader header;
            range >> header;
            if (header.type == TYPE) {
                ui32 magic;
                range >> magic;
                if (magic == MAGIC32) {
                    Header header;
                    range >> header;
                    Node::FreeObject (file, header.rootOffset);
                    Object::FreeObject (file, offset);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt BTree header @" THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Invalid BTree header type: %s", header.type.c_str ());
            }
        }

        void TransactedFileBTreeAllocator::BTree::Read (Serializer &serializer) {
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> header;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFileBTreeAllocator::BTree: @" THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
            root->Release ();
            root = Node::Alloc (file, header.rootOffset, header.entriesPerNode);
        }

        void TransactedFileBTreeAllocator::BTree::Write (Serializer &serializer) {
            header.rootOffset = root->GetOffset ();
            serializer << MAGIC32 << header;
        }

        inline bool operator == (
                const TransactedFileBTreeAllocator::BTree::Key &key1,
                const TransactedFileBTreeAllocator::BTree::Key &key2) {
            return key1.size == key2.size && key1.offset == key2.offset;
        }

        inline bool operator != (
                const TransactedFileBTreeAllocator::BTree::Key &key1,
                const TransactedFileBTreeAllocator::BTree::Key &key2) {
            return key1.size != key2.size || key1.offset != key2.offset;
        }

        inline bool operator < (
                const TransactedFileBTreeAllocator::BTree::Key &key1,
                const TransactedFileBTreeAllocator::BTree::Key &key2) {
            return key1.size < key2.size ||
                (key1.size == key2.size && key1.offset < key2.offset);
        }

        inline bool operator > (
                const TransactedFileBTreeAllocator::BTree::Key &key1,
                const TransactedFileBTreeAllocator::BTree::Key &key2) {
            return key1.size > key2.size ||
                (key1.size == key2.size && key1.offset > key2.offset);
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFileBTreeAllocator::BTree::Header &header) {
            serializer << header.entriesPerNode << header.rootOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTreeAllocator::BTree::Header &header) {
            serializer >> header.entriesPerNode >> header.rootOffset;
            return serializer;
        }

    } // namespace util
} // namespace thekogans
