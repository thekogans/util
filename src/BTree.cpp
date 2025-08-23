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
#include "thekogans/util/Array.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/BTreeKeys.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/util/BTree.h"

namespace thekogans {
    namespace util {

        inline Serializer &operator << (
                Serializer &serializer,
                const BTree::Node::Entry &entry) {
            serializer << entry.rightOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                BTree::Node::Entry &entry) {
            serializer >> entry.rightOffset;
            // Because of the way we allocate the BTree::Node the
            // Entry cror is never called. In a way this extraction
            // operator is our ctor. Make sure all members are
            // properly initialized.
            entry.key = nullptr;
            entry.value = nullptr;
            entry.right = nullptr;
            return serializer;
        }

        inline Serializer &operator << (
                Serializer &serializer,
                const BTree::Header &header) {
            serializer <<
                header.keyType <<
                header.valueType <<
                header.entriesPerNode <<
                header.rootOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                BTree::Header &header) {
            serializer >>
                header.keyType >>
                header.valueType >>
                header.entriesPerNode >>
                header.rootOffset;
            return serializer;
        }

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::BTree::Key)
        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (thekogans::util::BTree::Value)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void BTree::Key::StaticInit () {
            StringKey::StaticInit ();
            GUIDKey::StaticInit ();
        }

        void BTree::Value::StaticInit () {
            StringValue::StaticInit ();
            PtrValue::StaticInit ();
            StringArrayValue::StaticInit ();
            GUIDArrayValue::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        void BTree::Iterator::Clear () {
            // Leave the prefix in case they want to reuse the iterator.
            parents.clear ();
            node = NodeIndex (nullptr, 0);
            finished = true;
        }

        bool BTree::Iterator::Next () {
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

        BTree::Key::SharedPtr BTree::Iterator::GetKey () const {
            if (!finished) {
                assert (node.first != nullptr && node.second < node.first->count);
                return node.first->entries[node.second].key;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Iterator is not pointing to a valid entry.");
            }
        }

        BTree::Value::SharedPtr BTree::Iterator::GetValue () const {
            if (!finished) {
                assert (node.first != nullptr && node.second < node.first->count);
                return node.first->entries[node.second].value;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Iterator is not pointing to a valid entry.");
            }
        }

        void BTree::Iterator::SetValue (Value::SharedPtr value) {
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

        void BTree::Iterator::Reset (
                Node *node_,
                ui32 index) {
            prefix = node_->entries[index].key;
            node = NodeIndex (node_, index);
            finished = false;
        }

        BTree::Node::Node (
                BTree &btree_,
                FileAllocator::PtrType offset) :
                FileAllocator::Object (btree_.GetFileAllocator (), offset),
                btree (btree_),
                count (0),
                leftOffset (0),
                left (nullptr),
                keyValueOffset (0),
                keys (btree.header.entriesPerNode),
                entries ((Entry *)(this + 1)) {
            Reload ();
        }

        BTree::Node::~Node () {
            Reset ();
        }

        std::size_t BTree::Node::Size (std::size_t entriesPerNode) {
            return sizeof (Node) + entriesPerNode * sizeof (Entry);
        }

        BTree::Node *BTree::Node::Alloc (
                BTree &btree,
                FileAllocator::PtrType offset) {
            Node *node = new (
                btree.nodeAllocator->Alloc (
                    Size (btree.header.entriesPerNode))) Node (btree, offset);
            node->AddRef ();
            return node;
        }

        void BTree::Node::FreeSubtree (
                FileAllocator &fileAllocator,
                FileAllocator::PtrType offset) {
            if (offset != 0) {
                FileAllocator::BlockBuffer buffer (fileAllocator, offset);
                buffer.BlockRead ();
                ui32 magic;
                buffer >> magic;
                if (magic == MAGIC32) {
                    ui32 count;
                    buffer >> count;
                    if (count > 0) {
                        FileAllocator::PtrType leftOffset;
                        FileAllocator::PtrType keyValueOffset;
                        buffer >> leftOffset >> keyValueOffset;
                        fileAllocator.Free (keyValueOffset);
                        FreeSubtree (fileAllocator, leftOffset);
                        for (ui32 i = 0; i < count; ++i) {
                            Entry entry;
                            buffer >> entry;
                            FreeSubtree (fileAllocator, entry.rightOffset);
                        }
                    }
                    fileAllocator.Free (offset);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt BTree::Node @" THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
        }

        void BTree::Node::SetValue (
                ui32 index,
                Value::SharedPtr value) {
            if (entries[index].value != nullptr) {
                entries[index].value->Release ();
            }
            entries[index].value = value.Release ();
            SetDirty (true);
        }

        BTree::Node *BTree::Node::GetChild (ui32 index) {
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

        bool BTree::Node::PrefixFind (
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

        bool BTree::Node::FindFirstPrefix (
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

        bool BTree::Node::Find (
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

        BTree::Node::InsertResult BTree::Node::Insert (
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

        bool BTree::Node::Remove (const Key &key) {
            ui32 index = 0;
            bool found = Find (key, index);
            Node *child = GetChild (found ? index + 1 : index);
            if (found) {
                // Release the key and the value. They will
                // either be overwritten or removed below.
                entries[index].key->Release ();
                entries[index].value->Release ();
                if (child != nullptr) {
                    Node *leaf = child;
                    while (leaf->leftOffset != 0) {
                        leaf = leaf->GetChild (0);
                    }
                    entries[index].key = leaf->entries[0].key;
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

        void BTree::Node::RestoreBalance (ui32 index) {
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

        void BTree::Node::RotateRight (
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

        void BTree::Node::RotateLeft (
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

        void BTree::Node::Merge (
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

        void BTree::Node::Split (Node *node) {
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

        void BTree::Node::Concatenate (Node *node) {
            std::memcpy (
                entries + count,
                node->entries,
                node->count * sizeof (Entry));
            count += node->count;
            node->count = 0;
            node->SetDirty (true);
            SetDirty (true);
        }

        void BTree::Node::InsertEntry (
                const Entry &entry,
                ui32 index) {
            std::memmove (
                entries + index + 1,
                entries + index,
                (count++ - index) * sizeof (Entry));
            entries[index] = entry;
            SetDirty (true);
        }

        void BTree::Node::RemoveEntry (ui32 index) {
            std::memcpy (
                entries + index,
                entries + index + 1,
                (--count - index) * sizeof (Entry));
            SetDirty (true);
        }

        void BTree::Node::Dump () {
            if (count > 0) {
                std::cout << offset << ": " << leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    std::cout << " ; [" << entries[i].key->ToString () << ", " <<
                        entries[i].value->ToString () << "] ; " << entries[i].rightOffset;
                }
                std::cout << "\n";
                for (ui32 i = 0; i < count; ++i) {
                    Node *child = GetChild (i);
                    if (child != nullptr) {
                        child->Dump ();
                    }
                }
            }
        }

        void BTree::Node::Harakiri () {
            this->~Node ();
            btree.nodeAllocator->Free (this, Size (btree.header.entriesPerNode));
        }

        void BTree::Node::Reset () {
            if (count > 0) {
                if (left != nullptr) {
                    left->Release ();
                    left = nullptr;
                }
                for (ui32 i = 0; i < count; ++i) {
                    entries[i].key->Release ();
                    entries[i].key = nullptr;
                    entries[i].value->Release ();
                    entries[i].value = nullptr;
                    if (entries[i].right != nullptr) {
                        entries[i].right->Release ();
                        entries[i].right = nullptr;
                    }
                }
                count = 0;
            }
        }

        void BTree::Node::Read (Serializer &serializer) {
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> count;
                if (count > 0) {
                    serializer >> leftOffset >> keyValueOffset;
                    FileAllocator::BlockBuffer keyValueBuffer (*fileAllocator, keyValueOffset);
                    keyValueBuffer.BlockRead ();
                    SerializableHeader keyHeader (btree.header.keyType, 0, 0);
                    SerializableHeader valueHeader (btree.header.valueType, 0, 0);
                    for (ui32 i = 0; i < count; ++i) {
                        serializer >> entries[i];
                        {
                            keyValueBuffer >> keyHeader.version >> keyHeader.size;
                            entries[i].key = (Key *)btree.keyFactory (nullptr).Release ();
                            entries[i].key->Read (keyHeader, keyValueBuffer);
                        }
                        if (!valueHeader.type.empty ()) {
                            keyValueBuffer >> valueHeader.version >> valueHeader.size;
                            entries[i].value = (Value *)btree.valueFactory (nullptr).Release ();
                            entries[i].value->Read (valueHeader, keyValueBuffer);
                        }
                        else {
                            Value::SharedPtr value;
                            keyValueBuffer >> value;
                            if (value != nullptr) {
                                entries[i].value = value.Release ();
                            }
                            else {
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                    "Unable to read value from BTree::Node @"
                                    THEKOGANS_UTIL_UI64_FORMAT " @ entry %u",
                                    offset,
                                    i);
                            }
                        }
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt BTree::Node @" THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        }

        void BTree::Node::Write (Serializer &serializer) {
            serializer << MAGIC32 << count;
            if (count > 0) {
                // Calculate key/value sizes.
                Array<std::pair<SizeT, SizeT>> keyValueSizes (count);
                std::size_t totalKeyValueSize = 0;
                for (ui32 i = 0; i < count; ++i) {
                    std::pair<SizeT, SizeT> keyValueSize (
                        entries[i].key->Size (), entries[i].value->Size ());
                    keyValueSizes[i] = keyValueSize;
                    totalKeyValueSize +=
                        // key version + key size + key bytes
                        UI16_SIZE + keyValueSize.first.Size () + keyValueSize.first;
                    if (!btree.header.valueType.empty ()) {
                        totalKeyValueSize +=
                            // value version + value size + value bytes
                            UI16_SIZE + keyValueSize.second.Size () + keyValueSize.second;
                    }
                    else {
                        // It would be nice to just write:
                        // totalKeyValueSize += entries[i].value->GetSize ();
                        // and be done with it. But that would incure a penalty
                        // of yet another call to Serializable::Size (). So we
                        // simmulate what Serializable::GetSize would do and
                        // construct a SerializableHeader from the pieces we
                        // already have to take it's size. A bit of insider
                        // trading and potentially dangerous if the SerializableHeader
                        // would ever to change.
                        totalKeyValueSize +=
                            SerializableHeader (
                                entries[i].value->Type (),
                                entries[i].value->Version (),
                                keyValueSize.second).Size () + keyValueSize.second;
                    }
                }
                // Get existing block size.
                ui64 blockSize = 0;
                if (keyValueOffset != 0) {
                    FileAllocator::Block block (*fileAllocator, keyValueOffset);
                    block.Read ();
                    blockSize = block.GetSize ();
                }
                // If existing block size is less than what we need,
                // resize it.
                if (blockSize < totalKeyValueSize) {
                    // First, free the old block.
                    if (keyValueOffset != 0) {
                        fileAllocator->Free (keyValueOffset);
                    }
                    keyValueOffset = fileAllocator->Alloc (totalKeyValueSize);
                }
                // This is a very important if statement for it
                // stamps the in memory cache on to onfile
                // store. In effect marking the new checkpoint.
                if (left != nullptr) {
                    leftOffset = left->GetOffset ();
                }
                serializer << leftOffset << keyValueOffset;
                FileAllocator::BlockBuffer keyValueBuffer (*fileAllocator, keyValueOffset);
                for (ui32 i = 0; i < count; ++i) {
                    if (entries[i].right != nullptr) {
                        entries[i].rightOffset = entries[i].right->GetOffset ();
                    }
                    serializer << entries[i];
                    keyValueBuffer <<
                        entries[i].key->Version () <<
                        SizeT (keyValueSizes[i].first);
                    entries[i].key->Write (keyValueBuffer);
                    if (!btree.header.valueType.empty ()) {
                        keyValueBuffer <<
                            entries[i].value->Version () <<
                            SizeT (keyValueSizes[i].second);
                    }
                    else {
                        // See comment above totalKeyValueSize +=
                        keyValueBuffer <<
                            SerializableHeader (
                                entries[i].value->Type (),
                                entries[i].value->Version (),
                                keyValueSizes[i].second);
                    }
                    entries[i].value->Write (keyValueBuffer);
                }
                if (fileAllocator->IsSecure ()) {
                    // Zero out the unused portion of the keyValueBuffer to
                    // prevent leaking sensitive data.
                    keyValueBuffer.AdvanceWriteOffset (
                        SecureZeroMemory (
                            keyValueBuffer.GetWritePtr (),
                            keyValueBuffer.GetDataAvailableForWriting ()));
                }
                keyValueBuffer.BlockWrite ();
            }
            else if (keyValueOffset != 0) {
                fileAllocator->Free (keyValueOffset);
            }
        }

        BTree::BTree (
                FileAllocator::SharedPtr fileAllocator,
                FileAllocator::PtrType offset,
                const std::string &keyType,
                const std::string &valueType,
                std::size_t entriesPerNode,
                std::size_t nodesPerPage,
                Allocator::SharedPtr allocator) :
                FileAllocator::Object (fileAllocator, offset),
                header (keyType, valueType, (ui32)entriesPerNode),
                keyFactory (Key::GetTypeFactory (keyType.c_str ())),
                valueFactory (Value::GetTypeFactory (valueType.c_str ())),
                nodeAllocator (
                    new BlockAllocator (
                        Node::Size (entriesPerNode),
                        nodesPerPage,
                        allocator)),
                root (Node::Alloc (*this, header.rootOffset)) {
            if (!Key::IsType (header.keyType.c_str ()) ||
                    (!header.valueType.empty () &&
                        !Value::IsType (header.valueType.c_str ()))) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "key (%s) / value (%s) types are not valid.",
                    keyType.c_str (), valueType.c_str ());
            }
            Reload ();
        }

        BTree::~BTree () {
            root->Release ();
        }

        bool BTree::Find (
                const Key &key,
                Iterator &it) {
            if (key.IsKindOf (header.keyType.c_str ())) {
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

        bool BTree::Insert (
                Key::SharedPtr key,
                Value::SharedPtr value,
                Iterator &it) {
            if (key != nullptr && value != nullptr &&
                    key->IsKindOf (header.keyType.c_str ()) &&
                    (header.valueType.empty () || value->IsKindOf (header.valueType.c_str ()))) {
                it.Clear ();
                Node::Entry entry (key.Get (), value.Get ());
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
                    value.Release ();
                }
                return result == Node::Inserted;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        bool BTree::Remove (const Key &key) {
            if (key.IsKindOf (header.keyType.c_str ())) {
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

        bool BTree::FindFirst (Iterator &it) {
            if (it.prefix == nullptr || it.prefix->IsKindOf (header.keyType.c_str ())) {
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

        void BTree::Free () {
            if (GetOffset () != 0) {
                Node::FreeSubtree (*fileAllocator, header.rootOffset);
                header.rootOffset = 0;
                Object::Free ();
            }
        }

        void BTree::Reset () {
            header.rootOffset = 0;
            root->Release ();
            root = Node::Alloc (*this, header.rootOffset);
        }

        void BTree::Read (Serializer &serializer) {
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                Header header_;
                serializer >> header_;
                if ((header.keyType.empty () || header.keyType == header_.keyType) &&
                        (header.valueType.empty () || header.valueType == header_.valueType)) {
                    header = header_;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Requested key (%s)/value (%s) types do not "
                        "match existing key (%s)/value (%s) types @" THEKOGANS_UTIL_UI64_FORMAT,
                        header.keyType.c_str (), header.valueType.c_str (),
                        header_.keyType.c_str (), header_.valueType.c_str (),
                        GetOffset ());
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt BTree @" THEKOGANS_UTIL_UI64_FORMAT,
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

        void BTree::Write (Serializer &serializer) {
            header.rootOffset = root->GetOffset ();
            serializer << MAGIC32 << header;
        }

    } // namespace util
} // namespace thekogans
