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
            serializer << entry.keyOffset << entry.valueOffset << entry.rightOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                BTree::Node::Entry &entry) {
            serializer >> entry.keyOffset >> entry.valueOffset >> entry.rightOffset;
            // Because of the way we allocate the BTree::Node the
            // Entry cror is never called. In a way this extraction
            // operator is our ctor. Make sure all members are
            // properly initialized.
            entry.key = nullptr;
            entry.value = nullptr;
            entry.rightNode = nullptr;
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
                            prefix->PrefixCompare (*node.first->GetKey (node.second)) != 0;
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
                return node.first->GetKey (node.second);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Iterator is not pointing to a valid entry.");
            }
        }

        BTree::Value::SharedPtr BTree::Iterator::GetValue () const {
            if (!finished) {
                assert (node.first != nullptr && node.second < node.first->count);
                return node.first->GetValue (node.second);
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

        BTree::Node::Node (
                BTree &btree_,
                FileAllocator::PtrType offset) :
                FileAllocator::Object (btree_.GetFileAllocator (), offset),
                btree (btree_),
                count (0),
                leftOffset (0),
                leftNode (nullptr),
                entries ((Entry *)(this + 1)) {
            if (GetOffset () != 0) {
                Load ();
            }
        }

        BTree::Node::~Node () {
            Reset ();
        }

        std::size_t BTree::Node::FileSize (std::size_t entriesPerNode) {
            const std::size_t ENTRY_SIZE =
                FileAllocator::PTR_TYPE_SIZE + // keyOffset
                FileAllocator::PTR_TYPE_SIZE + // valueOffset
                FileAllocator::PTR_TYPE_SIZE;  // rightOffset
            return
                UI32_SIZE + // magic
                UI32_SIZE + // count
                FileAllocator::PTR_TYPE_SIZE + // leftOffset
                entriesPerNode * ENTRY_SIZE;   // entries
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
                        buffer >> leftOffset;
                        FreeSubtree (fileAllocator, leftOffset);
                        for (ui32 i = 0; i < count; ++i) {
                            Entry entry;
                            buffer >> entry;
                            fileAllocator.Free (entry.keyOffset);
                            fileAllocator.Free (entry.valueOffset);
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

        BTree::Key *BTree::Node::GetKey (ui32 index) {
            if (entries[index].key == nullptr) {
                assert (entries[index].keyOffset != 0);
                FileAllocator::BlockBuffer keyBuffer (
                    *fileAllocator, entries[index].keyOffset);
                keyValueBuffer.BlockRead ();
                Serializable::Header keyHeader (btree.header.keyType, 0, 0);
                keyBuffer >> keyHeader.version >> keyHeader.size;
                entries[i].key = (Key *)btree.keyFactory (nullptr).Release ();
                entries[index].key->Read (keyHeader, keyBuffer);
                entries[index].key->Init ();
            }
            return entries[index].key;
        }

        BTree::Value *BTree::Node::GetValue (ui32 index) {
            if (entries[index].value == nullptr) {
                assert (entries[index].valueOffset != 0);
                FileAllocator::BlockBuffer valueBuffer (
                    *fileAllocator, entries[index].valueOffset);
                keyValueBuffer.BlockRead ();
                Serializable::Header valueHeader (btree.header.valueType, 0, 0);
                if (!valueHeader.type.empty ()) {
                    valueBuffer >> valueHeader.version >> valueHeader.size;
                    entries[i].value = (Value *)btree.valueFactory (nullptr).Release ();
                    entries[i].value->Read (valueHeader, valueBuffer);
                    entries[i].value->Init ();
                }
                else {
                    Value::SharedPtr value;
                    valueBuffer >> value;
                    if (value != nullptr) {
                        entries[index].value = value.Release ();
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
            return entries[index].value;
        }

        BTree::Value *BTree::Node::SetValue (
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
                if (leftNode == nullptr && leftOffset != 0) {
                    leftNode = Alloc (btree, leftOffset);
                }
                return leftNode;
            }
            else {
                --index;
                if (entries[index].rightNode == nullptr &&
                        entries[index].rightOffset != 0) {
                    entries[index].rightNode = Alloc (
                        btree, entries[index].rightOffset);
                }
                return entries[index].rightNode;
            }
        }

        bool BTree::Node::PrefixFind (
                const Key &prefix,
                ui32 &index) const {
            ui32 last = index;
            index = 0;
            while (index < last) {
                ui32 middle = (index + last) / 2;
                i32 result = prefix.PrefixCompare (*GetKey (middle));
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
                i32 result = key.Compare (*GetKey (middle));
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
                    it.prefix.Reset (GetKey (index));
                    it.node = Iterator::NodeIndex (this, index);
                    it.finished = false;
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
                    it.prefix.Reset (GetKey (index));
                    it.node = Iterator::NodeIndex (this, index);
                    it.finished = false;
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
                            it.prefix.Reset (GetKey (index));
                            it.node = Iterator::NodeIndex (this, index);
                            it.finished = false;
                        }
                    }
                    else {
                        right->InsertEntry (entry, index - splitIndex);
                        if (it.IsFinished ()) {
                            it.prefix.Reset (GetKey (index - splitIndex));
                            // -1 because we will be removing the 0'th entry below.
                            it.node = Iterator::NodeIndex (right, index - splitIndex - 1);
                            it.finished = false;
                        }
                    }
                    entry = right->entries[0];
                    right->RemoveEntry (0);
                }
                right->leftOffset = entry.rightOffset;
                right->leftNode = entry.rightNode;
                entry.rightOffset = right->offset;
                entry.rightNode = right;
                return Overflow;
            }
        }

        bool BTree::Node::Remove (const Key &key) {
            ui32 index = 0;
            bool found = Find (key, index);
            Node *child = GetChild (found ? index + 1 : index);
            if (found) {
                if (child != nullptr) {
                    Node *leaf = child;
                    while (leaf->leftOffset != 0) {
                        leaf = leaf->GetChild (0);
                    }
                    entries[index].keyOffset = leaf->entries[0].keyOffset;
                    entries[index].key = leaf->entries[0].key;
                    entries[index].valueOffset = leaf->entries[0].valueOffset;
                    entries[index].value = leaf->entries[0].value;
                    SetDirty (true);
                    child->Remove (*leaf->GetKey (0));
                    if (child->IsPoor ()) {
                        RestoreBalance (index);
                    }
                }
                else {
                    if (entries[index].key != nullptr) {
                        entries[index].key->Release ();
                    }
                    if (entries[index].value != nullptr) {
                        entries[index].value->Release ();
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
            entries[index].rightNode = right->leftNode;
            right->InsertEntry (entries[index], 0);
            ui32 lastIndex = left->count - 1;
            right->leftOffset = left->entries[lastIndex].rightOffset;
            right->leftNode = left->entries[lastIndex].rightNode;
            left->entries[lastIndex].rightOffset = right->offset;
            left->entries[lastIndex].rightNode = right;
            entries[index] = left->entries[lastIndex];
            left->RemoveEntry (lastIndex);
            SetDirty (true);
        }

        void BTree::Node::RotateLeft (
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
            SetDirty (true);
        }

        void BTree::Node::Merge (
                ui32 index,
                Node *left,
                Node *right) {
            assert (left->count + right->count < btree.header.entriesPerNode);
            entries[index].rightOffset = right->leftOffset;
            entries[index].rightNode = right->leftNode;
            left->Concatenate (entries[index]);
            left->Concatenate (right);
            RemoveEntry (index);
            right->Delete ();
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
                    std::cout << " ; [" << GetKey (i)->ToString () << ", " <<
                        GetValue (i)->ToString () << "] ; " << entries[i].rightOffset;
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

        void BTree::Node::Free () {
            if (IsEmpty ()) {
                GetFileAllocator ()->Free (GetOffset ());
                Release ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Logic error: trying to free a node with count > 0 @"
                    THEKOGANS_UTIL_UI64_FORMAT,
                    GetOffset ());
            }
        }

        void BTree::Node::Flush () {
            assert (IsDirty ());
            assert (GetOffset () != 0);
            FileAllocator::BlockBuffer buffer (*fileAllocator, GetOffset ());
            buffer << MAGIC32 << count;
            if (count > 0) {
                if (leftNode != nullptr) {
                    leftOffset = leftNode->GetOffset ();
                }
                buffer << leftOffset;
                for (ui32 i = 0; i < count; ++i) {
                    if (entries[i].key != nullptr) {
                        entries[i].keyOffset = entries[i].key->GetOffset ();
                    }
                    if (entries[i].value) {
                        entries[i].valueOffset = entries[i].value->GetOffset ();
                    }
                    if (entries[i].rightNode != nullptr) {
                        entries[i].rightOffset = entries[i].rightNode->GetOffset ();
                    }
                    buffer << entries[i];
                }
            }
            if (fileAllocator->IsSecure ()) {
                buffer.AdvanceWriteOffset (
                    SecureZeroMemory (
                        buffer.GetWritePtr (),
                        buffer.GetDataAvailableForWriting ()));
            }
            buffer.BlockWrite ();
        }

        void BTree::Node::Reload () {
            Reset ();
            if (GetOffset () != 0) {
                Load ();
            }
        }

        void BTree::Node::Reset () {
            if (count > 0) {
                if (leftNode != nullptr) {
                    leftNode->Release ();
                    leftNode = nullptr;
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
                    if (entries[i].rightNode != nullptr) {
                        entries[i].rightNode->Release ();
                        entries[i].rightNode = nullptr;
                    }
                }
                count = 0;
            }
        }

        void BTree::Node::Load () {
            FileAllocator::BlockBuffer buffer (*fileAllocator, offset);
            buffer.BlockRead ();
            ui32 magic;
            buffer >> magic;
            if (magic == MAGIC32) {
                buffer >> count;
                if (count > 0) {
                    buffer >> leftOffset;
                    for (ui32 i = 0; i < count; ++i) {
                        buffer >> entries[i];
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt BTree::Node @" THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
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
                rootNode (Node::Alloc (*this, header.rootOffset)) {
            if (GetOffset () != 0) {
                Load ();
            }
            else if (!Key::IsType (header.keyType.c_str ()) ||
                    (!header.valueType.empty () &&
                        !Value::IsType (header.valueType.c_str ()))) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "key (%s) / value (%s) types are not valid.",
                    keyType.c_str (), valueType.c_str ());
            }
        }

        BTree::~BTree () {
            rootNode->Release ();
        }

        bool BTree::Find (
                const Key &key,
                Iterator &it) {
            if (key.IsKindOf (header.keyType.c_str ())) {
                it.Clear ();
                ui32 index = 0;
                for (Node *node = rootNode; node != nullptr;
                        node = node->GetChild (index)) {
                    if (node->Find (key, index)) {
                        it.prefix.Reset (node->GetKey (index));
                        it.node = Iterator::NodeIndex (node, index);
                        it.finished = false;
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
                Node::InsertResult result = rootNode->Insert (entry, it);
                if (result == Node::Overflow) {
                    // The path to the leaf node is full.
                    // Create a new root node and make the entry
                    // its first.
                    Node *node = Node::Alloc (*this);
                    node->leftOffset = rootNode->GetOffset ();
                    node->leftNode = rootNode;
                    node->InsertEntry (entry, 0);
                    rootNode = node;
                    if (it.IsFinished ()) {
                        it.prefix.Reset (node->GetKey (0));
                        it.node = Iterator::NodeIndex (node, 0);
                        it.finished = false;
                    }
                    result = Node::Inserted;
                }
                // If we inserted the key/value pair, take ownership.
                if (result == Node::Inserted) {
                    if (!IsDirty () &&
                            (header.rootOffset == 0 ||
                                rootNode->GetOffset () != header.rootOffset)) {
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
                bool removed = rootNode->Remove (key);
                if (removed) {
                    if (rootNode->IsEmpty () && rootNode->GetChild (0) != nullptr) {
                        Node *node = rootNode;
                        rootNode = rootNode->GetChild (0);
                        node->Delete ();
                    }
                    if (!IsDirty () &&
                            (header.rootOffset == 0 ||
                                rootNode->GetOffset () != header.rootOffset)) {
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
                Node *node = rootNode;
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
                FileAllocator::BlockBuffer buffer (*fileAllocator, GetOffset ());
                buffer.BlockRead ();
                ui32 magic;
                buffer >> magic;
                if (magic == MAGIC32) {
                    Header header;
                    buffer >> header;
                    Node::FreeSubtree (*fileAllocator, header.rootOffset);
                    fileAllocator->Free (offset);
                    Produce (
                        std::bind (
                            &FileAllocator::ObjectEvents::OnFileAllocatorObjectFree,
                            std::placeholders::_1,
                            this));
                    offset = 0;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt BTree @" THEKOGANS_UTIL_UI64_FORMAT,
                        GetOffset ());
                }
            }
        }

        void BTree::Flush () {
            header.rootOffset = rootNode->GetOffset ();
            FileAllocator::BlockBuffer buffer (*fileAllocator, GetOffset ());
            buffer << MAGIC32 << header;
            buffer.BlockWrite ();
        }

        void BTree::Reload () {
            if (GetOffset () != 0) {
                Load ();
            }
            else {
                Reset ();
            }
        }

        void BTree::Reset () {
            header.rootOffset = 0;
            rootNode->Release ();
            rootNode = Node::Alloc (*this, header.rootOffset);
        }

        void BTree::Load () {
            FileAllocator::BlockBuffer buffer (*fileAllocator, GetOffset ());
            buffer.BlockRead ();
            ui32 magic;
            buffer >> magic;
            if (magic == MAGIC32) {
                Header header_;
                buffer >> header_;
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
            rootNode->Release ();
            nodeAllocator.Reset (
                new BlockAllocator (
                    Node::Size (header.entriesPerNode),
                    nodeAllocator->GetBlocksPerPage (),
                    nodeAllocator->GetAllocator ()));
            rootNode = Node::Alloc (*this, header.rootOffset);
        }

    } // namespace util
} // namespace thekogans
