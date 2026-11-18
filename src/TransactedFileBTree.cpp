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
#include "thekogans/util/Heap.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/TransactedFileBTreeKeys.h"
#include "thekogans/util/TransactedFileBTree.h"

namespace thekogans {
    namespace util {

        std::size_t TransactedFileBTree::Header::Size (Serializer &serializer) const {
            std::size_t size = UI32_SIZE; // magic
            {
                Serializer::ContextGuard guard (serializer);
                size +=
                    serializer.Size (keyContext) +
                    serializer.Size (valueContext);
            }
            size +=
                Serializer::Size (flags) +
                Serializer::Size (entriesPerNode) +
                Serializer::Size (nodesPerPage) +
                Serializer::Size (rootOffset);
            return size;
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFileBTree::Header &header) {
            {
                Serializer::ContextGuard guard (serializer);
                serializer << header.keyContext << header.valueContext;
            }
            serializer << header.flags << header.entriesPerNode << header.nodesPerPage << header.rootOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTree::Header &header) {
            {
                Serializer::ContextGuard guard (serializer);
                serializer >> header.keyContext >> header.valueContext;
            }
            serializer >> header.flags >> header.entriesPerNode >> header.nodesPerPage >> header.rootOffset;
            return serializer;
        }

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::TransactedFileBTree::Key)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void TransactedFileBTree::Key::StaticInit () {
            StringKey::StaticInit ();
            GUIDKey::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        void TransactedFileBTree::Iterator::Clear () {
            // Leave the prefix in case they want to reuse the iterator.
            parents.clear ();
            node = NodeIndex (nullptr, 0);
            finished = true;
        }

        bool TransactedFileBTree::Iterator::Next () {
            if (!finished) {
                ++node.second;
                Node *child = node.first->GetChild (node.second);
                if (child != nullptr) {
                    parents.push_back (NodeIndex (node.first, node.second));
                    ui32 index = 0;
                    while (child != nullptr &&
                        (prefix == nullptr || child->FindFirstPrefix (*prefix, index))) {
                        parents.push_back (NodeIndex (child, index));
                        child = child->GetChild (index);
                    }
                    node = parents.back ();
                    parents.pop_back ();
                }
                else {
                    while (node.second >= node.first->count && !parents.empty ()) {
                        node = parents.back ();
                        parents.pop_back ();
                    }
                }
                if (node.second < node.first->count) {
                    finished = prefix != nullptr &&
                        prefix->PrefixCompare (*node.first->entries[node.second].key) != 0;
                }
                else {
                    finished = true;
                }
            }
            if (finished) {
                Clear ();
            }
            return !finished;
        }

        TransactedFileBTree::Key::SharedPtr TransactedFileBTree::Iterator::GetKey () const {
            assert (finished || (node.first != nullptr && node.second < node.first->count));
            return finished ? nullptr : node.first->GetKey (node.second);
        }

        Serializable::SharedPtr TransactedFileBTree::Iterator::GetValue () const {
            assert (finished || (node.first != nullptr && node.second < node.first->count));
            return finished ? nullptr : node.first->GetValue (node.second);
        }

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (thekogans::util::TransactedFileBTree::Node, 1, 0)

        TransactedFileBTree::Node::Node (TransactedFileBTree *btree_) :
                btree (btree_),
                entries ((Entry *)(this + 1)) {
            if (THEKOGANS_UTIL_LIKELY (btree != nullptr)) {
                SecureZeroMemory (entries, btree->header.entriesPerNode * sizeof (Entry));
            }
        }

        TransactedFileBTree::Node::~Node () {
            Reset ();
        }

        std::size_t TransactedFileBTree::Node::Size (std::size_t entriesPerNode) {
            return sizeof (Node) + entriesPerNode * sizeof (Entry);
        }

        TransactedFileBTree::Node *TransactedFileBTree::Node::Alloc (
                TransactedFileBTree *btree,
                TransactedFile *file,
                TransactedFile::AddressType offset) {
            Node *node = new (btree->nodeAllocator->Alloc ()) Node (btree);
            node->AddRef ();
            node->Init (file, offset);
            return node;
        }

        TransactedFileBTree::Key::SharedPtr TransactedFileBTree::Node::GetKey (ui32 index) {
            return entries[index].key;
        }

        Serializable::SharedPtr TransactedFileBTree::Node::GetValue (ui32 index) {
            if (THEKOGANS_UTIL_UNLIKELY (entries[index].value == nullptr && entries[index].valueOffset != 0)) {
                TransactedFile::ObjectRange valueRange (file, entries[index].valueOffset);
                TransactedFile::Object::SharedPtr value;
                valueRange >> value;
                value->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                entries[index].value = value.Release ();
            }
            return entries[index].value;
        }

        TransactedFileBTree::Node *TransactedFileBTree::Node::GetChild (ui32 index) {
            if (THEKOGANS_UTIL_UNLIKELY (index == 0)) {
                if (THEKOGANS_UTIL_UNLIKELY (left == nullptr && leftOffset != 0)) {
                    left = Alloc (btree, file, leftOffset);
                    left->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                }
                return left;
            }
            else {
                --index;
                if (THEKOGANS_UTIL_UNLIKELY (entries[index].right == nullptr && entries[index].rightOffset != 0)) {
                    entries[index].right = Alloc (btree, file, entries[index].rightOffset);
                    entries[index].right->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                }
                return entries[index].right;
            }
        }

        bool TransactedFileBTree::Node::PrefixFind (
                const Key &prefix,
                ui32 &index) const {
            ui32 last = index;
            index = 0;
            while (index < last) {
                ui32 middle = index + ((last - index) >> 1);
                i32 result = prefix.PrefixCompare (*entries[middle].key);
                if (THEKOGANS_UTIL_UNLIKELY (result == 0)) {
                    index = middle;
                    return true;
                }
                if (result < 0) {
                    last = middle;
                }
                else {
                    index = middle + 1;
                }
            }
            return false;
        }

        bool TransactedFileBTree::Node::FindFirstPrefix (
                const Key &prefix,
                ui32 &index) const {
            index = count;
            if (PrefixFind (prefix, index)) {
                ui32 lastIndex = index;
                while (PrefixFind (prefix, lastIndex)) {
                    index = lastIndex;
                }
                return true;
            }
            return false;
        }

        bool TransactedFileBTree::Node::Find (
                const Key &key,
                ui32 &index) const {
            ui32 last = count;
            index = 0;
            while (index < last) {
                ui32 middle = index + ((last - index) >> 1);
                i32 result = key.Compare (*entries[middle].key);
                if (THEKOGANS_UTIL_UNLIKELY (result == 0)) {
                    index = middle;
                    return true;
                }
                if (result < 0) {
                    last = middle;
                }
                else {
                    index = middle + 1;
                }
            }
            return false;
        }

        TransactedFileBTree::Node::InsertResult TransactedFileBTree::Node::Insert (Entry &entry) {
            ui32 index = 0;
            if (Find (*entry.key, index)) {
                assert (index < count);
                return Duplicate;
            }
            Node *child = GetChild (index);
            if (child != nullptr) {
                InsertResult result = child->Insert (entry);
                if (result == Inserted || result == Duplicate) {
                    return result;
                }
            }
            if (THEKOGANS_UTIL_LIKELY (!IsFull ())) {
                InsertEntry (entry, index);
                return Inserted;
            }
            else {
                // Node is full. Split it and insert the entry
                // in to the proper daughter node. Return the
                // entry at the split point to be added up the
                // parent chain.
                Node *right = Alloc (btree, file, 0);
                Split (right);
                ui32 splitIndex = btree->header.entriesPerNode / 2;
                if (THEKOGANS_UTIL_UNLIKELY (index != splitIndex)) {
                    if (index < splitIndex) {
                        InsertEntry (entry, index);
                    }
                    else {
                        right->InsertEntry (entry, index - splitIndex);
                    }
                    if (right->entries[0].value != nullptr) {
                        if (!btree->header.IsValueIsObject ()) {
                            right->entries[0].value->Producer<SerializableEvents>::Unsubscribe (*right);
                        }
                        else {
                            TransactedFile::Object::SharedPtr object =
                                Serializable::SharedPtr (right->entries[0].value);
                            object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*right);
                        }
                    }
                    if (right->entries[0].right != nullptr) {
                        right->entries[0].right->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*right);
                    }
                    entry = right->entries[0];
                    right->RemoveEntry (0);
                }
                right->leftOffset = entry.rightOffset;
                right->left = entry.right;
                if (right->left != nullptr) {
                    right->left->Producer<TransactedFile::ObjectEvents>::Subscribe (*right);
                }
                entry.rightOffset = right->offset;
                entry.right = right;
                return Overflow;
            }
        }

        bool TransactedFileBTree::Node::Remove (const Key &key) {
            ui32 index = 0;
            bool found = Find (key, index);
            Node *child = GetChild (found ? index + 1 : index);
            if (found) {
                if (child != nullptr) {
                    Node *leaf = child;
                    while (Node *leftChild = leaf->GetChild (0)) {
                        leaf = leftChild;
                    }
                    // 1. Clean up and free the deleted entries[index] first.
                    entries[index].key->Release ();
                    if (entries[index].value != nullptr) {
                        if (!btree->header.IsValueIsObject ()) {
                            entries[index].value->Producer<SerializableEvents>::Unsubscribe (*this);
                        }
                        else {
                            TransactedFile::Object::SharedPtr object =
                                Serializable::SharedPtr (entries[index].value);
                            object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                            object->FreeBlock ();
                        }
                        entries[index].value->Release ();
                        entries[index].value = nullptr;
                    }
                    else if (btree->header.IsValueIsObject () && entries[index].valueOffset != 0) {
                        Object::FreeObject (file, entries[index].valueOffset);
                    }
                    // 2. Promote leaf->entries[0] into entries[index] (transferring value ownership).
                    entries[index].key = leaf->entries[0].key;
                    entries[index].key->AddRef ();
                    entries[index].valueOffset = leaf->entries[0].valueOffset;
                    entries[index].value = leaf->entries[0].value;
                    if (entries[index].value != nullptr) {
                        entries[index].value->AddRef ();
                        if (!btree->header.IsValueIsObject ()) {
                            entries[index].value->Producer<SerializableEvents>::Unsubscribe (*leaf);
                            entries[index].value->Producer<SerializableEvents>::Subscribe (*this);
                        }
                        else {
                            TransactedFile::Object::SharedPtr object =
                                Serializable::SharedPtr (entries[index].value);
                            object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*leaf);
                            object->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                        }
                    }
                    // 3. Clear leaf->entries[0]'s value ownership so leaf->Remove won't free its block.
                    leaf->entries[0].valueOffset = 0;
                    if (leaf->entries[0].value != nullptr) {
                        leaf->entries[0].value->Release ();
                        leaf->entries[0].value = nullptr;
                    }
                    SetDirty ();

                    // 4. Delete the promoted successor key from the right subtree and rebalance.
                    Key::SharedPtr successorKey (entries[index].key);
                    child->Remove (*successorKey);
                    if (child->IsPoor ()) {
                        RestoreBalance (index);
                    }
                }
                else {
                    entries[index].key->Release ();
                    entries[index].key = nullptr;
                    if (entries[index].value != nullptr) {
                        if (!btree->header.IsValueIsObject ()) {
                            entries[index].value->Producer<SerializableEvents>::Unsubscribe (*this);
                        }
                        else {
                            TransactedFile::Object::SharedPtr object =
                                Serializable::SharedPtr (entries[index].value);
                            object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                            object->FreeBlock ();
                        }
                        entries[index].value->Release ();
                        entries[index].value = nullptr;
                    }
                    // else if (btree->header.IsValueIsObject () && entries[index].valueOffset != 0) {
                    //     Object::FreeObject (file, entries[index].valueOffset);
                    // }
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

#if 0
        bool TransactedFileBTree::Node::Remove (const Key &key) {
            ui32 index = 0;
            bool found = Find (key, index);
            Node *child = GetChild (found ? index + 1 : index);
            if (found) {
                if (child != nullptr) {
                    Node *leaf = child;
                    while (leaf->leftOffset != 0) {
                        leaf = leaf->GetChild (0);
                    }
                    entries[index].key = leaf->entries[0].key;
                    entries[index].key->AddRef ();
                    entries[index].valueOffset = leaf->entries[0].valueOffset;
                    entries[index].value = leaf->entries[0].value;
                    entries[index].value->AddRef ();
                    if (entries[index].value != nullptr) {
                        if (!btree->header.IsValueIsObject ()) {
                            entries[index].value->Producer<SerializableEvents>::Subscribe (*this);
                        }
                        else {
                            TransactedFile::Object::SharedPtr object =
                                Serializable::SharedPtr (entries[index].value);
                            object->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                        }
                    }
                    SetDirty ();
                    child->Remove (*leaf->entries[0].key);
                    if (child->IsPoor ()) {
                        RestoreBalance (index);
                    }
                }
                else {
                    entries[index].key->Release ();
                    entries[index].key = nullptr;
                    if (entries[index].value != nullptr) {
                        if (!btree->header.IsValueIsObject ()) {
                            entries[index].value->Producer<SerializableEvents>::Unsubscribe (*this);
                        }
                        else {
                            TransactedFile::Object::SharedPtr object =
                                Serializable::SharedPtr (entries[index].value);
                            object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                            object->FreeBlock ();
                        }
                        entries[index].value->Release ();
                        entries[index].value = nullptr;
                    }
                    else if (entries[index].valueOffset != 0) {
                        Object::FreeObject (file, entries[index].valueOffset);
                    }
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
#endif

        void TransactedFileBTree::Node::RestoreBalance (ui32 index) {
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

        void TransactedFileBTree::Node::RotateRight (
                ui32 index,
                Node *left,
                Node *right) {
            // 1. Demote separator entries[index] into right[0], taking right->left as its right child.
            Entry demoted = entries[index];
            demoted.rightOffset = right->leftOffset;
            demoted.right = right->left;
            if (demoted.value != nullptr) {
                if (!btree->header.IsValueIsObject ()) {
                    demoted.value->Producer<SerializableEvents>::Unsubscribe (*this);
                }
                else {
                    TransactedFile::Object::SharedPtr object = Serializable::SharedPtr (demoted.value);
                    object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                }
            }
            if (demoted.right != nullptr) {
                demoted.right->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*right);
            }
            right->InsertEntry (demoted, 0);
            // 2. Move left's rightmost child subtree to become right->left.
            ui32 lastIndex = left->count - 1;
            right->leftOffset = left->entries[lastIndex].rightOffset;
            right->left = left->entries[lastIndex].right;
            if (right->left != nullptr) {
                right->left->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*left);
                right->left->Producer<TransactedFile::ObjectEvents>::Subscribe (*right);
            }
            // 3. Promote left[lastIndex] into entries[index] (keeping right as entries[index].right).
            entries[index] = left->entries[lastIndex];
            entries[index].rightOffset = right->offset;
            entries[index].right = right;
            if (entries[index].value != nullptr) {
                if (!btree->header.IsValueIsObject ()) {
                    entries[index].value->Producer<SerializableEvents>::Unsubscribe (*left);
                    entries[index].value->Producer<SerializableEvents>::Subscribe (*this);
                }
                else {
                    TransactedFile::Object::SharedPtr object = Serializable::SharedPtr (entries[index].value);
                    object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*left);
                    object->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                }
            }
            left->RemoveEntry (lastIndex);
            SetDirty ();
            const Key *prevKey = 0;
            btree->AssertSorted (this, prevKey);
        }

        void TransactedFileBTree::Node::RotateLeft (
                ui32 index,
                Node *left,
                Node *right) {
            // 1. Demote separator entries[index] to the end of left, taking right->left as its right child.
            Entry demoted = entries[index];
            demoted.rightOffset = right->leftOffset;
            demoted.right = right->left;
            if (demoted.value != nullptr) {
                if (!btree->header.IsValueIsObject ()) {
                    demoted.value->Producer<SerializableEvents>::Unsubscribe (*this);
                }
                else {
                    TransactedFile::Object::SharedPtr object = Serializable::SharedPtr (demoted.value);
                    object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                }
            }
            if (demoted.right != nullptr) {
                demoted.right->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*right);
            }
            left->Concatenate (demoted);
            // 2. Move right[0]'s right child subtree to become right->left (still owned by right).
            right->leftOffset = right->entries[0].rightOffset;
            right->left = right->entries[0].right;
            // 3. Promote right[0] into entries[index] (keeping right as entries[index].right).
            entries[index] = right->entries[0];
            entries[index].rightOffset = right->offset;
            entries[index].right = right;
            if (entries[index].value != nullptr) {
                if (!btree->header.IsValueIsObject ()) {
                    entries[index].value->Producer<SerializableEvents>::Unsubscribe (*right);
                    entries[index].value->Producer<SerializableEvents>::Subscribe (*this);
                }
                else {
                    TransactedFile::Object::SharedPtr object = Serializable::SharedPtr (entries[index].value);
                    object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*right);
                    object->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                }
            }
            right->RemoveEntry (0);
            SetDirty ();
            const Key *prevKey = 0;
            btree->AssertSorted (this, prevKey);
        }

        void TransactedFileBTree::Node::Merge (
                ui32 index,
                Node *left,
                Node *right) {
            assert (left->count + right->count < btree->header.entriesPerNode);
            if (entries[index].value != nullptr) {
                if (!btree->header.IsValueIsObject ()) {
                    entries[index].value->Producer<SerializableEvents>::Unsubscribe (*this);
                }
                else {
                    TransactedFile::Object::SharedPtr object =
                        Serializable::SharedPtr (entries[index].value);
                    object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                }
            }
            entries[index].rightOffset = right->leftOffset;
            entries[index].right = right->left;
            if (entries[index].right != nullptr) {
                entries[index].right->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*right);
            }
            left->Concatenate (entries[index]);
            left->Concatenate (right);
            RemoveEntry (index);
            const Key *prevKey = 0;
            btree->AssertSorted (this, prevKey);
        }

        void TransactedFileBTree::Node::Split (Node *right) {
            assert (IsFull ());
            ui32 splitIndex = count / 2;
            right->count = count - splitIndex;
            std::memcpy (right->entries, entries + splitIndex, right->count * sizeof (Entry));
            for (ui32 i = 0; i < right->count; ++i) {
                if (right->entries[i].value != nullptr) {
                    if (!btree->header.IsValueIsObject ()) {
                        right->entries[i].value->Producer<SerializableEvents>::Unsubscribe (*this);
                        right->entries[i].value->Producer<SerializableEvents>::Subscribe (*right);
                    }
                    else {
                        TransactedFile::Object::SharedPtr object =
                            Serializable::SharedPtr (right->entries[i].value);
                        object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                        object->Producer<TransactedFile::ObjectEvents>::Subscribe (*right);
                    }
                }
                if (right->entries[i].right != nullptr) {
                    right->entries[i].right->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                    right->entries[i].right->Producer<TransactedFile::ObjectEvents>::Subscribe (*right);
                }
            }
            count = splitIndex;
            right->SetDirty ();
            SetDirty ();
        }

        void TransactedFileBTree::Node::Concatenate (Node *right) {
            std::memcpy (entries + count, right->entries, right->count * sizeof (Entry));
            ui32 oldCount = count;
            count += right->count;
            for (ui32 i = oldCount; i < count; ++i) {
                if (entries[i].value != nullptr) {
                    if (!btree->header.IsValueIsObject ()) {
                        entries[i].value->Producer<SerializableEvents>::Unsubscribe (*right);
                        entries[i].value->Producer<SerializableEvents>::Subscribe (*this);
                    }
                    else {
                        TransactedFile::Object::SharedPtr object =
                            Serializable::SharedPtr (entries[i].value);
                        object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*right);
                        object->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                    }
                }
                if (entries[i].right != nullptr) {
                    entries[i].right->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*right);
                    entries[i].right->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                }
            }
            right->count = 0;
            Object::FreeObject (file, right->GetOffset ());
            right->Release ();
            SetDirty ();
        }

        void TransactedFileBTree::Node::InsertEntry (
                const Entry &entry,
                ui32 index) {
            std::memmove (entries + index + 1, entries + index, (count++ - index) * sizeof (Entry));
            entries[index] = entry;
            if (entries[index].value != nullptr) {
                if (!btree->header.IsValueIsObject ()) {
                    entries[index].value->Producer<SerializableEvents>::Subscribe (*this);
                }
                else {
                    TransactedFile::Object::SharedPtr object =
                        Serializable::SharedPtr (entries[index].value);
                    object->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                }
            }
            if (entries[index].right != nullptr) {
                entries[index].right->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
            }
            SetDirty ();
        }

        void TransactedFileBTree::Node::RemoveEntry (ui32 index) {
            if (entries[index].value != nullptr) {
                if (!btree->header.IsValueIsObject ()) {
                    entries[index].value->Producer<SerializableEvents>::Unsubscribe (*this);
                }
                else {
                    TransactedFile::Object::SharedPtr object =
                        Serializable::SharedPtr (entries[index].value);
                    object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                }
            }
            if (entries[index].right != nullptr) {
                entries[index].right->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
            }
            std::memmove (entries + index, entries + index + 1, (--count - index) * sizeof (Entry));
            SetDirty ();
        }

        void TransactedFileBTree::Node::Harakiri () {
            this->~Node ();
            btree->nodeAllocator->Free (this);
        }

        void TransactedFileBTree::Node::Reset () {
            return;
            if (THEKOGANS_UTIL_LIKELY (count > 0)) {
                if (left != nullptr) {
                    left->Release ();
                    left = nullptr;
                }
                for (ui32 i = 0; i < count; ++i) {
                    if (THEKOGANS_UTIL_LIKELY (entries[i].key != nullptr)) {
                        entries[i].key->Release ();
                        entries[i].key = nullptr;
                    }
                    if (THEKOGANS_UTIL_LIKELY (entries[i].value != nullptr)) {
                        if (!btree->header.IsValueIsObject ()) {
                            entries[i].value->Producer<SerializableEvents>::Unsubscribe (*this);
                        }
                        else {
                            TransactedFile::Object::SharedPtr object =
                                Serializable::SharedPtr (entries[i].value);
                            object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                        }
                        entries[i].value->Release ();
                        entries[i].value = nullptr;
                    }
                    if (entries[i].right != nullptr) {
                        entries[i].right->Release ();
                        entries[i].right = nullptr;
                    }
                }
                count = 0;
            }
        }

        std::size_t TransactedFileBTree::Node::Size (Serializer &serializer) noexcept {
                            // magic       count
            std::size_t size = UI32_SIZE + UI32_SIZE;
            if (THEKOGANS_UTIL_LIKELY (count > 0)) {
                     // flags       leftOffset
                size += UI32_SIZE + TransactedFile::AddressTypeSize;
                for (ui32 i = 0; i < count; ++i) {
                    if (btree->header.IsValueIsObject ()) {
                             // valueOffset
                        size += TransactedFile::AddressTypeSize;
                    }
                         // rightOffset
                    size += TransactedFile::AddressTypeSize;
                }
                {
                    Serializer::ContextGuard guard (serializer, btree->header.keyContext, btree->keyFactory);
                    for (ui32 i = 0; i < count; ++i) {
                              // key
                        size += entries[i].key->GetSize (serializer);
                    }
                }
                if (!btree->header.IsValueIsObject ()) {
                    Serializer::ContextGuard guard (serializer, btree->header.valueContext, btree->valueFactory);
                    for (ui32 i = 0; i < count; ++i) {
                             // value
                        size += entries[i].value->GetSize (serializer);
                    }
                }
            }
            return size;
        }

        void TransactedFileBTree::Node::Read (Serializer &serializer) {
            Reset ();
            ui32 magic;
            serializer >> magic;
            if (THEKOGANS_UTIL_LIKELY (magic == MAGIC32)) {
                serializer >> count;
                if (THEKOGANS_UTIL_LIKELY (count > 0)) {
                    ui32 flags;
                    serializer >> flags >> leftOffset;
                    assert (flags == btree->header.flags);
                    for (ui32 i = 0; i < count; ++i) {
                        if (btree->header.IsValueIsObject ()) {
                            serializer >> entries[i].valueOffset;
                        }
                        else {
                            entries[i].valueOffset = 0;
                        }
                        entries[i].value = nullptr;
                        serializer >> entries[i].rightOffset;
                    }
                    {
                        Serializer::ContextGuard guard (serializer, btree->header.keyContext, btree->keyFactory);
                        for (ui32 i = 0; i < count; ++i) {
                            Key::SharedPtr key;
                            serializer >> key;
                            entries[i].key = key.Release ();
                        }
                    }
                    if (!btree->header.IsValueIsObject ()) {
                        Serializer::ContextGuard guard (serializer, btree->header.valueContext, btree->valueFactory);
                        for (ui32 i = 0; i < count; ++i) {
                            Serializable::SharedPtr value;
                            serializer >> value;
                            value->Producer<SerializableEvents>::Subscribe (*this);
                            entries[i].value = value.Release ();
                        }
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFileBTree::Node @" THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        }

        void TransactedFileBTree::Node::Write (Serializer &serializer) {
            serializer << MAGIC32 << count;
            if (THEKOGANS_UTIL_LIKELY (count > 0)) {
                if (left != nullptr) {
                    leftOffset = left->GetOffset ();
                }
                serializer << btree->header.flags << leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    if (btree->header.IsValueIsObject ()) {
                        if (entries[i].value != nullptr) {
                            entries[i].valueOffset =
                                dynamic_cast<TransactedFile::Object *> (entries[i].value)->GetOffset ();
                        }
                        serializer << entries[i].valueOffset;
                    }
                    if (entries[i].right != nullptr) {
                        entries[i].rightOffset = entries[i].right->GetOffset ();
                    }
                    serializer << entries[i].rightOffset;
                }
                {
                    Serializer::ContextGuard guard (serializer, btree->header.keyContext);
                    for (ui32 i = 0; i < count; ++i) {
                        serializer << *entries[i].key;
                    }
                }
                if (!btree->header.IsValueIsObject ()) {
                    Serializer::ContextGuard guard (serializer, btree->header.valueContext);
                    for (ui32 i = 0; i < count; ++i) {
                        serializer << *entries[i].value;
                    }
                }
            }
        }

        void TransactedFileBTree::Node::FreeObject (
                TransactedFile *file,
                TransactedFile::AddressType offset) {
            if (THEKOGANS_UTIL_LIKELY (file != nullptr && offset != 0)) {
                TransactedFile::ObjectRange range (file, offset);
                if (THEKOGANS_UTIL_LIKELY (range.context.WeakCompare ({TYPE, VERSION}))) {
                    ui32 magic;
                    range >> magic;
                    if (THEKOGANS_UTIL_LIKELY (magic == MAGIC32)) {
                        ui32 count;
                        range >> count;
                        if (THEKOGANS_UTIL_LIKELY (count > 0)) {
                            Flags32 flags;
                            TransactedFile::AddressType leftOffset;
                            range >> flags >> leftOffset;
                            Node::FreeObject (file, leftOffset);
                            for (ui32 i = 0; i < count; ++i) {
                                if (flags.Test (TransactedFileBTree::Header::FLAGS_VALUE_IS_OBJECT)) {
                                    TransactedFile::AddressType valueOffset;
                                    range >> valueOffset;
                                    DestroyObject (file, valueOffset);
                                }
                                TransactedFile::AddressType rightOffset;
                                range >> rightOffset;
                                Node::FreeObject (file, rightOffset);
                            }
                        }
                        Object::FreeObject (file, offset);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Corrupt TransactedFileBTree::Node @" THEKOGANS_UTIL_UI64_FORMAT,
                            offset);
                    }
                }
                else {
                }
            }
        }

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (thekogans::util::TransactedFileBTree, 1, 0)

        TransactedFileBTree::TransactedFileBTree (
                const SerializableHeader &keyContext,
                const SerializableHeader &valueContext,
                bool valueIsObject,
                std::size_t entriesPerNode,
                std::size_t nodesPerPage) :
                header (
                    keyContext,
                    valueContext,
                    valueIsObject ? Header::FLAGS_VALUE_IS_OBJECT : 0,
                    (ui32)entriesPerNode,
                    (ui32)nodesPerPage),
                keyFactory (Key::GetTypeFactory (keyContext.type)),
                valueFactory (TransactedFile::Object::GetTypeFactory (valueContext.type)) {
            if (THEKOGANS_UTIL_UNLIKELY ((!keyContext.NeedType () && !Key::IsType (keyContext.type)) ||
                    (!valueContext.NeedType () && !Serializable::IsType (valueContext.type)) ||
                    entriesPerNode < 3 || nodesPerPage < 4)) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "key (%s) / value (%s) types are not valid.",
                    keyContext.type.c_str (), valueContext.type.c_str ());
            }
        }

        TransactedFileBTree::~TransactedFileBTree () {
            if (THEKOGANS_UTIL_LIKELY (root != nullptr)) {
                root->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                root->Release ();
            }
        }

        Serializable::SharedPtr TransactedFileBTree::Find (const Key &key) {
            if (THEKOGANS_UTIL_LIKELY (key.IsKindOf (header.keyContext.type))) {
                ui32 index = 0;
                for (Node *node = root; node != nullptr; node = node->GetChild (index)) {
                    if (node->Find (key, index)) {
                        return node->GetValue (index);
                    }
                }
                return nullptr;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool TransactedFileBTree::Insert (
                Key::SharedPtr key,
                Serializable::SharedPtr value) {
            if (THEKOGANS_UTIL_LIKELY (key != nullptr && value != nullptr &&
                    key->IsKindOf (header.keyContext.type) &&
                    ((header.IsValueIsObject () &&
                        dynamic_refcounted_sharedptr_cast<TransactedFile::Object> (value) != nullptr) ||
                        header.valueContext.NeedType () || value->IsKindOf (header.valueContext.type)))) {
                key = key->Clone ();
                TransactedFile::AddressType valueOffset = 0;
                if (!header.IsValueIsObject ()) {
                    Clonable::SharedPtr clonable = value;
                    if (clonable != nullptr) {
                        value = clonable->Clone ();
                    }
                }
                else {
                    TransactedFile::Object::SharedPtr object = value;
                    if (object != nullptr) {
                        valueOffset = object->GetOffset ();
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }
                Node::Entry entry (key.Get (), valueOffset, value.Get ());
                Node::InsertResult result = root->Insert (entry);
                if (THEKOGANS_UTIL_UNLIKELY (result == Node::Overflow)) {
                    // The path to the leaf node is full.
                    // Create a new root node and make the entry
                    // its first.
                    root->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                    Node *node = Node::Alloc (this, file, 0);
                    node->leftOffset = root->GetOffset ();
                    node->left = root;
                    node->left->Producer<TransactedFile::ObjectEvents>::Subscribe (*node);
                    node->InsertEntry (entry, 0);
                    root = node;
                    root->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                    result = Node::Inserted;
                }
                // If we inserted the key/value pair, take ownership.
                if (THEKOGANS_UTIL_LIKELY (result == Node::Inserted)) {
                    key.Release ();
                    value.Release ();
                }
                return result == Node::Inserted;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool TransactedFileBTree::Remove (const Key &key) {
            if (THEKOGANS_UTIL_LIKELY (key.IsKindOf (header.keyContext.type))) {
                bool removed = root->Remove (key);
                if (removed) {
                    if (THEKOGANS_UTIL_UNLIKELY (root->IsEmpty () && root->GetChild (0) != nullptr)) {
                        root->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                        Node *node = root;
                        root = root->GetChild (0);
                        root->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                        Object::FreeObject (file, node->GetOffset ());
                        node->Release ();
                    }
                }
                return removed;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool TransactedFileBTree::FindFirst (Iterator &it) {
            if (THEKOGANS_UTIL_LIKELY (it.prefix == nullptr || it.prefix->IsKindOf (header.keyContext.type))) {
                it.Clear ();
                Node *node = root;
                if (THEKOGANS_UTIL_LIKELY (node != nullptr && node->count > 0)) {
                    if (THEKOGANS_UTIL_UNLIKELY (it.prefix == nullptr)) {
                        do {
                            it.parents.push_back (Iterator::NodeIndex (node, 0));
                            node = node->GetChild (0);
                        } while (node != nullptr);
                        it.finished = false;
                    }
                    else {
                        while (node != nullptr) {
                            ui32 index = 0;
                            if (node->FindFirstPrefix (*it.prefix, index)) {
                                it.parents.push_back (Iterator::NodeIndex (node, index));
                                it.finished = false;
                            }
                            else if (!it.finished) {
                                break;
                            }
                            node = node->GetChild (index);
                        }
                    }
                }
                if (!it.finished) {
                    it.node = it.parents.back ();
                    it.parents.pop_back ();
                }
                return !it.finished;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void TransactedFileBTree::Read (Serializer &serializer) {
            ui32 magic;
            serializer >> magic;
            if (THEKOGANS_UTIL_LIKELY (magic == MAGIC32)) {
                Header header_;
                serializer >> header_;
                if (THEKOGANS_UTIL_LIKELY ((header.keyContext.NeedType () ||
                        header.keyContext == header_.keyContext) &&
                        (header.valueContext.NeedType () ||
                            header.valueContext == header_.valueContext))) {
                    header = header_;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Requested key (%s)/value (%s) types do not "
                        "match existing key (%s)/value (%s) types @" THEKOGANS_UTIL_UI64_FORMAT,
                        header.keyContext.type.c_str (), header.valueContext.type.c_str (),
                        header_.keyContext.type.c_str (), header_.valueContext.type.c_str (),
                        GetOffset ());
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt TransactedFileBTree @" THEKOGANS_UTIL_UI64_FORMAT,
                    GetOffset ());
            }
            if (root != nullptr) {
                root->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                root->Release ();
            }
            nodeAllocator.Reset (
                new SlabAllocatorImpl (
                    Node::Size (header.entriesPerNode),
                    header.nodesPerPage,
                    header.nodesPerPage / 4));
            root = Node::Alloc (this, file, header.rootOffset);
            root->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
        }

        void TransactedFileBTree::Write (Serializer &serializer) {
            header.rootOffset = root->GetOffset ();
            serializer << MAGIC32 << header;
        }

        void TransactedFileBTree::FreeObject (
                TransactedFile *file,
                TransactedFile::AddressType offset) {
            if (THEKOGANS_UTIL_LIKELY (file != nullptr && offset != 0)) {
                TransactedFile::ObjectRange range (file, offset);
                if (range.context.WeakCompare ({TYPE, VERSION})) {
                    ui32 magic;
                    range >> magic;
                    if (THEKOGANS_UTIL_LIKELY (magic == MAGIC32)) {
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
            }
        }

        void TransactedFileBTree::AssertSorted (
                Node *node,
                const Key *&prevKey) {
            if (node != nullptr) {
                AssertSorted (node->GetChild (0), prevKey);
                for (ui32 i = 0; i < node->count; ++i) {
                    if (prevKey != nullptr && prevKey->Compare (*node->entries[i].key) >= 0) {
                        THEKOGANS_UTIL_DEBUG_BREAK
                    }
                    prevKey = node->entries[i].key;
                    AssertSorted (node->GetChild (i + 1), prevKey);
                }
            }
        }

    } // namespace util
} // namespace thekogans
