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

        inline Serializer &operator << (
                Serializer &serializer,
                const TransactedFileBTree::Header &header) {
            header.keyContext.Write (serializer);
            header.valueContext.Write (serializer);
            serializer << header.flags << header.entriesPerNode << header.rootOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTree::Header &header) {
            header.keyContext.Read (serializer);
            header.valueContext.Read (serializer);
            serializer >> header.flags >> header.entriesPerNode >> header.rootOffset;
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
                finished = true;
                while (node.second < node.first->count || !parents.empty ()) {
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
                    if (node.second < node.first->count) {
                        finished = prefix != nullptr &&
                            prefix->PrefixCompare (*node.first->entries[node.second].key) != 0;
                        break;
                    }
                    else if (!parents.empty ()) {
                        node = parents.back ();
                        parents.pop_back ();
                    }
                }
            }
            if (finished) {
                Clear ();
            }
            return !finished;
        }

        TransactedFileBTree::Key::SharedPtr TransactedFileBTree::Iterator::GetKey () const {
            if (!finished) {
                assert (node.first != nullptr && node.second < node.first->count);
                return node.first->entries[node.second].key;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Iterator is not pointing to a valid entry.");
            }
        }

        Serializable::SharedPtr TransactedFileBTree::Iterator::GetValue () const {
            if (!finished) {
                assert (node.first != nullptr && node.second < node.first->count);
                return node.first->GetValue (node.second);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Iterator is not pointing to a valid entry.");
            }
        }

        void TransactedFileBTree::Iterator::SetValue (Serializable::SharedPtr value) {
            if (value != nullptr) {
                if (!finished) {
                    assert (node.first != nullptr && node.second < node.first->count);
                    node.first->SetValue (node.second, value);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Iterator is not pointing to a valid entry.");
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void TransactedFileBTree::Iterator::Reset (
                Node *node_,
                ui32 index) {
            prefix = node_->entries[index].key;
            node = NodeIndex (node_, index);
            finished = false;
        }

        TransactedFileBTree::Node::Node (
                TransactedFileBTree &btree_,
                TransactedFile::Allocator::PtrType offset) :
                TransactedFile::Object (btree_.GetFile (), offset),
                btree (btree_),
                count (0),
                leftOffset (0),
                left (nullptr),
                keyValueOffset (0),
                entries ((Entry *)(this + 1)) {
            Reload ();
        }

        TransactedFileBTree::Node::~Node () {
            Reset ();
        }

        std::size_t TransactedFileBTree::Node::Size (std::size_t entriesPerNode) {
            return sizeof (Node) + entriesPerNode * sizeof (Entry);
        }

        TransactedFileBTree::Node *TransactedFileBTree::Node::Alloc (
                TransactedFileBTree &btree,
                TransactedFile::Allocator::PtrType offset) {
            Node *node = new (
                btree.nodeAllocator->Alloc (
                    Size (btree.header.entriesPerNode))) Node (btree, offset);
            node->AddRef ();
            return node;
        }

        void TransactedFileBTree::Node::FreeSubtree (
                TransactedFileBTree &btree,
                TransactedFile::Allocator::PtrType offset) {
            if (offset != 0) {
                TransactedFile::BlockRange range (*btree.file, offset);
                ui32 magic;
                range >> magic;
                if (magic == MAGIC32) {
                    ui32 count;
                    range >> count;
                    if (count > 0) {
                        TransactedFile::Allocator::PtrType leftOffset;
                        TransactedFile::Allocator::PtrType keyValueOffset;
                        range >> leftOffset >> keyValueOffset;
                        btree.GetAllocator ()->Free (keyValueOffset);
                        FreeSubtree (btree, leftOffset);
                        for (ui32 i = 0; i < count; ++i) {
                            if (btree.header.IsValueAsObject ()) {
                                TransactedFile::Allocator::PtrType valueOffset;
                                range >> valueOffset;
                                btree.GetAllocator ()->Free (valueOffset);
                            }
                            TransactedFile::Allocator::PtrType rightOffset;
                            range >> rightOffset;
                            FreeSubtree (btree, rightOffset);
                        }
                    }
                    btree.GetAllocator ()->Free (offset);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt TransactedFileBTree::Node @" THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
        }

        Serializable::SharedPtr TransactedFileBTree::Node::GetValue (ui32 index) {
            return entries[index].value->GetSerializable ();
        }

        void TransactedFileBTree::Node::SetValue (
                ui32 index,
                Serializable::SharedPtr value) {
            entries[index].value->SetSerializable (value);
            if (btree.header.IsValueAsObject ()) {
                entries[index].value->SetDirty (true);
            }
            SetDirty (true);
        }

        TransactedFileBTree::Node *TransactedFileBTree::Node::GetChild (ui32 index) {
            if (index == 0) {
                if (left == nullptr && leftOffset != 0) {
                    left = Alloc (btree, leftOffset);
                }
                return left;
            }
            else {
                --index;
                if (entries[index].right == nullptr &&
                        entries[index].rightOffset != 0) {
                    entries[index].right = Alloc (
                        btree, entries[index].rightOffset);
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
                ui32 middle = (index + last) / 2;
                i32 result = prefix.PrefixCompare (*entries[middle].key);
                if (result == 0) {
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
                ui32 middle = (index + last) / 2;
                i32 result = key.Compare (*entries[middle].key);
                if (result == 0) {
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

        TransactedFileBTree::Node::InsertResult TransactedFileBTree::Node::Insert (
                Entry &entry,
                Iterator &it) {
            ui32 index = 0;
            if (Find (*entry.key, index)) {
                assert (index < count);
                if (it.IsFinished ()) {
                    it.Reset (this, index);
                }
                return Duplicate;
            }
            Node *child = GetChild (index);
            if (child != nullptr) {
                InsertResult result = child->Insert (entry, it);
                if (result == Inserted || result == Duplicate) {
                    return result;
                }
            }
            if (!IsFull ()) {
                InsertEntry (entry, index);
                if (it.IsFinished ()) {
                    it.Reset (this, index);
                }
                return Inserted;
            }
            else {
                // Node is full. Split it and insert the entry
                // in to the proper daughter node. Return the
                // entry at the split point to be added up the
                // parent chain.
                Node *right = Alloc (btree);
                Split (right);
                ui32 splitIndex = btree.header.entriesPerNode / 2;
                if (index != splitIndex) {
                    if (index < splitIndex) {
                        InsertEntry (entry, index);
                        if (it.IsFinished ()) {
                            it.Reset (this, index);
                        }
                    }
                    else {
                        right->InsertEntry (entry, index - splitIndex);
                        if (it.IsFinished ()) {
                            it.Reset (right, index - splitIndex);
                            // -1 because we will be removing the 0'th entry below.
                            --it.node.second;
                        }
                    }
                    entry = right->entries[0];
                    right->RemoveEntry (0);
                }
                right->leftOffset = entry.rightOffset;
                right->left = entry.right;
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
                // Release the key and value. They will
                // either be overwritten or removed below.
                entries[index].key->Release ();
                entries[index].value->Release ();
                if (child != nullptr) {
                    Node *leaf = child;
                    while (leaf->leftOffset != 0) {
                        leaf = leaf->GetChild (0);
                    }
                    entries[index].key = leaf->entries[0].key;
                    entries[index].valueOffset = leaf->entries[0].valueOffset;
                    entries[index].value = leaf->entries[0].value;
                    SetDirty (true);
                    child->Remove (*leaf->entries[0].key);
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
            entries[index].rightOffset = right->leftOffset;
            entries[index].right = right->left;
            right->InsertEntry (entries[index], 0);
            ui32 lastIndex = left->count - 1;
            right->leftOffset = left->entries[lastIndex].rightOffset;
            right->left = left->entries[lastIndex].right;
            left->entries[lastIndex].rightOffset = right->offset;
            left->entries[lastIndex].right = right;
            entries[index] = left->entries[lastIndex];
            left->RemoveEntry (lastIndex);
            SetDirty (true);
        }

        void TransactedFileBTree::Node::RotateLeft (
                ui32 index,
                Node *left,
                Node *right) {
            entries[index].rightOffset = right->leftOffset;
            entries[index].right = right->left;
            right->leftOffset = right->entries[0].rightOffset;
            right->left = right->entries[0].right;
            right->entries[0].rightOffset = right->offset;
            right->entries[0].right = right;
            left->Concatenate (entries[index]);
            entries[index] = right->entries[0];
            right->RemoveEntry (0);
            SetDirty (true);
        }

        void TransactedFileBTree::Node::Merge (
                ui32 index,
                Node *left,
                Node *right) {
            assert (left->count + right->count < btree.header.entriesPerNode);
            entries[index].rightOffset = right->leftOffset;
            entries[index].right = right->left;
            left->Concatenate (entries[index]);
            left->Concatenate (right);
            RemoveEntry (index);
            right->Delete ();
            right->Release ();
        }

        void TransactedFileBTree::Node::Split (Node *node) {
            assert (IsFull ());
            ui32 splitIndex = count / 2;
            node->count = count - splitIndex;
            std::memcpy (
                node->entries,
                entries + splitIndex,
                node->count * sizeof (Entry));
            count = splitIndex;
            node->SetDirty (true);
            SetDirty (true);
        }

        void TransactedFileBTree::Node::Concatenate (Node *node) {
            std::memcpy (
                entries + count,
                node->entries,
                node->count * sizeof (Entry));
            count += node->count;
            node->count = 0;
            node->SetDirty (true);
            SetDirty (true);
        }

        void TransactedFileBTree::Node::InsertEntry (
                const Entry &entry,
                ui32 index) {
            std::memmove (
                entries + index + 1,
                entries + index,
                (count++ - index) * sizeof (Entry));
            entries[index] = entry;
            SetDirty (true);
        }

        void TransactedFileBTree::Node::RemoveEntry (ui32 index) {
            std::memcpy (
                entries + index,
                entries + index + 1,
                (--count - index) * sizeof (Entry));
            SetDirty (true);
        }

        void TransactedFileBTree::Node::Harakiri () {
            this->~Node ();
            btree.nodeAllocator->Free (this, Size (btree.header.entriesPerNode));
        }

        void TransactedFileBTree::Node::Reset () {
            if (count > 0) {
                if (left != nullptr) {
                    left->Release ();
                    left = nullptr;
                }
                for (ui32 i = 0; i < count; ++i) {
                    if (entries[i].key != nullptr) {
                        entries[i].key->Release ();
                        entries[i].key = nullptr;
                    }
                    if (entries[i].value != nullptr) {
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

        void TransactedFileBTree::Node::Read (Serializer &serializer) {
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> count;
                if (count > 0) {
                    serializer >> leftOffset >> keyValueOffset;
                    TransactedFile::BlockRange keyValueRange (*file, keyValueOffset);
                    keyValueRange.context = btree.header.keyContext;
                    keyValueRange.factory = btree.keyFactory;
                    for (ui32 i = 0; i < count; ++i) {
                        Key::SharedPtr key;
                        keyValueRange >> key;
                        entries[i].key = key.Release ();
                        if (btree.header.IsValueAsObject ()) {
                            serializer >> entries[i].valueOffset;
                        }
                        else {
                            entries[i].valueOffset = 0;
                        }
                        entries[i].value = new TransactedFile::SerializableObject (
                            file,
                            entries[i].valueOffset,
                            btree.header.valueContext,
                            btree.valueFactory);
                        entries[i].value->AddRef ();
                        serializer >> entries[i].rightOffset;
                    }
                    if (!btree.header.IsValueAsObject ()) {
                        keyValueRange.context = btree.header.valueContext;
                        keyValueRange.factory = btree.valueFactory;
                        for (ui32 i = 0; i < count; ++i) {
                            Serializable::SharedPtr value;
                            keyValueRange >> value;
                            entries[i].value->SetSerializable (value);
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
            if (count > 0) {
                // Calculate key/value sizes.
                std::size_t keyValueSize = 0;
                for (ui32 i = 0; i < count; ++i) {
                    keyValueSize += entries[i].key->GetSize (btree.header.keyContext);
                    if (!btree.header.IsValueAsObject ()) {
                        keyValueSize += entries[i].value->GetSerializable ()->GetSize (btree.header.valueContext);
                    }
                }
                keyValueOffset = GetAllocator ()->Realloc (keyValueOffset, keyValueSize, false);
                if (left != nullptr) {
                    leftOffset = left->GetOffset ();
                }
                serializer << leftOffset << keyValueOffset;
                TransactedFile::BlockRange keyValueRange (*file, keyValueOffset, false);
                keyValueRange.context = btree.header.keyContext;
                keyValueRange.factory = btree.keyFactory;
                for (ui32 i = 0; i < count; ++i) {
                    keyValueRange << *entries[i].key;
                    if (btree.header.IsValueAsObject ()) {
                        if (entries[i].value != nullptr) {
                            entries[i].valueOffset = entries[i].value->GetOffset ();
                        }
                        serializer << entries[i].valueOffset;
                    }
                    if (entries[i].right != nullptr) {
                        entries[i].rightOffset = entries[i].right->GetOffset ();
                    }
                    serializer << entries[i].rightOffset;
                }
                if (!btree.header.IsValueAsObject ()) {
                    keyValueRange.context = btree.header.valueContext;
                    keyValueRange.factory = btree.valueFactory;
                    for (ui32 i = 0; i < count; ++i) {
                        keyValueRange << *entries[i].value->GetSerializable ();
                    }
                }
                if (GetAllocator ()->IsSecure ()) {
                    // Zero out the unused portion of the keyValueRange to
                    // prevent leaking sensitive data.
                    keyValueRange.Seek (
                        SecureZeroMemory (
                            keyValueRange.GetDataPtr (),
                            keyValueRange.GetDataAvailable ()),
                        SEEK_CUR);
                }
            }
            else if (keyValueOffset != 0) {
                GetAllocator ()->Free (keyValueOffset);
                keyValueOffset = 0;
            }
        }

        TransactedFileBTree::TransactedFileBTree (
                TransactedFile::SharedPtr file,
                TransactedFile::Allocator::PtrType offset,
                const SerializableHeader &keyContext,
                const SerializableHeader &valueContext,
                bool valueAsObject,
                std::size_t entriesPerNode,
                std::size_t nodesPerPage,
                Allocator::SharedPtr allocator) :
                TransactedFile::Object (file, offset),
                header (
                    keyContext,
                    valueContext,
                    valueAsObject ? Header::FLAGS_VALUE_AS_OBJECT : 0,
                    (ui32)entriesPerNode),
                keyFactory (Key::GetTypeFactory (keyContext.type.c_str ())),
                valueFactory (Serializable::GetTypeFactory (valueContext.type.c_str ())),
                nodeAllocator (
                    new BlockAllocator (
                        Node::Size (entriesPerNode),
                        nodesPerPage,
                        allocator)),
                root (Node::Alloc (*this, header.rootOffset)) {
            if (!Key::IsType (keyContext.type.c_str ()) ||
                    (!valueContext.NeedType () &&
                        !Serializable::IsType (valueContext.type.c_str ()))) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "key (%s) / value (%s) types are not valid.",
                    keyContext.type.c_str (), valueContext.type.c_str ());
            }
            Reload ();
        }

        TransactedFileBTree::~TransactedFileBTree () {
            root->Release ();
        }

        bool TransactedFileBTree::Find (
                const Key &key,
                Iterator &it) {
            if (key.IsKindOf (header.keyContext.type.c_str ())) {
                it.Clear ();
                ui32 index = 0;
                for (Node *node = root; node != nullptr;
                        node = node->GetChild (index)) {
                    if (node->Find (key, index)) {
                        it.Reset (node, index);
                        break;
                    }
                }
                return !it.finished;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool TransactedFileBTree::Insert (
                Key::SharedPtr key,
                Serializable::SharedPtr value,
                Iterator &it) {
            if (key != nullptr && value != nullptr &&
                    key->IsKindOf (header.keyContext.type.c_str ()) &&
                    (header.valueContext.NeedType () ||
                        value->IsKindOf (header.valueContext.type.c_str ()))) {
                it.Clear ();
                TransactedFile::SerializableObject::SharedPtr valueObject (
                    new TransactedFile::SerializableObject (
                        file,
                        0,
                        header.valueContext,
                        valueFactory,
                        nullptr,
                        value));
                Node::Entry entry (key.Get (), valueObject.Get ());
                Node::InsertResult result = root->Insert (entry, it);
                if (result == Node::Overflow) {
                    // The path to the leaf node is full.
                    // Create a new root node and make the entry
                    // its first.
                    Node *node = Node::Alloc (*this);
                    node->leftOffset = root->GetOffset ();
                    node->left = root;
                    node->InsertEntry (entry, 0);
                    root = node;
                    if (it.IsFinished ()) {
                        it.Reset (node, 0);
                    }
                    result = Node::Inserted;
                }
                // If we inserted the key/value pair, take ownership.
                if (result == Node::Inserted) {
                    if (!IsDirty () &&
                            (header.rootOffset == 0 ||
                                root->GetOffset () != header.rootOffset)) {
                        SetDirty (true);
                    }
                    key.Release ();
                    if (header.IsValueAsObject ()) {
                        valueObject->SetDirty (true);
                    }
                    valueObject.Release ();
                }
                return result == Node::Inserted;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool TransactedFileBTree::Remove (const Key &key) {
            if (key.IsKindOf (header.keyContext.type.c_str ())) {
                bool removed = root->Remove (key);
                if (removed) {
                    if (root->IsEmpty () && root->GetChild (0) != nullptr) {
                        Node *node = root;
                        root = root->GetChild (0);
                        node->Delete ();
                        node->Release ();
                    }
                    if (!IsDirty () &&
                            (header.rootOffset == 0 ||
                                root->GetOffset () != header.rootOffset)) {
                        SetDirty (true);
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
            if (it.prefix == nullptr || it.prefix->IsKindOf (header.keyContext.type.c_str ())) {
                it.Clear ();
                Node *node = root;
                if (node != nullptr && node->count > 0) {
                    if (it.prefix == nullptr) {
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

        void TransactedFileBTree::Free () {
            if (GetOffset () != 0) {
                Node::FreeSubtree (*this, header.rootOffset);
                header.rootOffset = 0;
                Object::Free ();
            }
        }

        void TransactedFileBTree::Reset () {
            header.rootOffset = 0;
            root->Release ();
            root = Node::Alloc (*this, header.rootOffset);
        }

        void TransactedFileBTree::Read (Serializer &serializer) {
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                Header header_;
                serializer >> header_;
                if ((header.keyContext.NeedType () ||
                        header.keyContext == header_.keyContext) &&
                        (header.valueContext.NeedType () ||
                            header.valueContext == header_.valueContext)) {
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
            root->Release ();
            nodeAllocator.Reset (
                new BlockAllocator (
                    Node::Size (header.entriesPerNode),
                    nodeAllocator->GetBlocksPerPage (),
                    nodeAllocator->GetAllocator ()));
            root = Node::Alloc (*this, header.rootOffset);
        }

        void TransactedFileBTree::Write (Serializer &serializer) {
            header.rootOffset = root->GetOffset ();
            serializer << MAGIC32 << header;
        }

    } // namespace util
} // namespace thekogans
