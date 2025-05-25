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
                    node.first->entries[node.second].value->Release ();
                    node.first->entries[node.second].value = value.Release ();
                    node.first->dirty = true;
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
                FileAllocator::PtrType offset_) :
                btree (btree_),
                offset (offset_),
                count (0),
                leftOffset (0),
                leftNode (nullptr),
                keyValueOffset (0),
                dirty (false) {
            if (offset != 0) {
                FileAllocator::BlockBuffer buffer (btree.fileAllocator, offset);
                buffer.BlockRead ();
                ui32 magic;
                buffer >> magic;
                if (magic == MAGIC32) {
                    buffer >> count;
                    if (count > 0) {
                        buffer >> leftOffset >> keyValueOffset;
                        FileAllocator::BlockBuffer keyValueBuffer (
                            btree.fileAllocator, keyValueOffset);
                        keyValueBuffer.BlockRead ();
                        Serializable::Header keyHeader (btree.header.keyType, 0, 0);
                        Serializable::Header valueHeader (btree.header.valueType, 0, 0);
                        for (ui32 i = 0; i < count; ++i) {
                            buffer >> entries[i];
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
            else {
                offset = btree.fileAllocator.Alloc (
                    FileSize (btree.header.entriesPerNode));
                dirty = true;
            }
        }

        BTree::Node::~Node () {
            if (count > 0) {
                Free (leftNode);
                for (ui32 i = 0; i < count; ++i) {
                    entries[i].key->Release ();
                    entries[i].value->Release ();
                    Free (entries[i].rightNode);
                }
            }
        }

        std::size_t BTree::Node::FileSize (std::size_t entriesPerNode) {
            const std::size_t ENTRY_SIZE =
                FileAllocator::PTR_TYPE_SIZE; // rightOffset
            return
                UI32_SIZE + // magic
                UI32_SIZE + // count
                FileAllocator::PTR_TYPE_SIZE + // leftOffset
                FileAllocator::PTR_TYPE_SIZE + // keyValueOffset
                entriesPerNode * ENTRY_SIZE;   // entries
        }

        std::size_t BTree::Node::Size (std::size_t entriesPerNode) {
            return sizeof (Node) + (entriesPerNode - 1) * sizeof (Entry);
        }

        BTree::Node *BTree::Node::Alloc (
                BTree &btree,
                FileAllocator::PtrType offset) {
            return new (
                btree.nodeAllocator->Alloc (
                    Size (btree.header.entriesPerNode))) Node (btree, offset);
        }

        void BTree::Node::Free (Node *node) {
            if (node != nullptr) {
                node->~Node ();
                node->btree.nodeAllocator->Free (
                    node, Size (node->btree.header.entriesPerNode));
            }
        }

        void BTree::Node::Delete (Node *node) {
            if (node->count == 0) {
                node->btree.fileAllocator.Free (node->offset);
                Free (node);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Logic error: trying to delete a node with count > 0 @"
                    THEKOGANS_UTIL_UI64_FORMAT,
                    node->offset);
            }
        }

        void BTree::Node::Delete (
                FileAllocator &fileAllocator,
                FileAllocator::PtrType offset) {
            if (offset != 0) {
                ui32 count;
                FileAllocator::PtrType leftOffset = 0;
                FileAllocator::PtrType keyValueOffset = 0;
                Entry entry;
                FileAllocator::BlockBuffer buffer (fileAllocator, offset);
                buffer.BlockRead ();
                ui32 magic;
                buffer >> magic;
                if (magic == MAGIC32) {
                    buffer >> count;
                    if (count > 0) {
                        buffer >> leftOffset >> keyValueOffset;
                        fileAllocator.Free (keyValueOffset);
                        Delete (fileAllocator, leftOffset);
                        for (ui32 i = 0; i < count; ++i) {
                            buffer >> entry;
                            Delete (fileAllocator, entry.rightOffset);
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

        void BTree::Node::Flush () {
            if (dirty) {
                FileAllocator::BlockBuffer buffer (btree.fileAllocator, offset);
                buffer << MAGIC32 << count;
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
                            // construct a Serializable::Header from the pieces we
                            // already have to take it's size. A bit of insider
                            // trading and potentially dangerous if the Serializable::Header
                            // would ever to change.
                            totalKeyValueSize +=
                                Serializable::Header (
                                    entries[i].value->Type (),
                                    entries[i].value->Version (),
                                    keyValueSize.second).Size () + keyValueSize.second;
                        }
                    }
                    // Get existing block size.
                    ui64 blockSize = 0;
                    if (keyValueOffset != 0) {
                        FileAllocator::BlockInfo block (keyValueOffset);
                        btree.fileAllocator.GetBlockInfo (block);
                        blockSize = block.GetSize ();
                    }
                    // If existing block size is less than what we need,
                    // resize it.
                    if (blockSize < totalKeyValueSize) {
                        // First, free the old block.
                        if (keyValueOffset != 0) {
                            btree.fileAllocator.Free (keyValueOffset);
                        }
                        keyValueOffset = btree.fileAllocator.Alloc (totalKeyValueSize);
                    }
                    buffer << leftOffset << keyValueOffset;
                    FileAllocator::BlockBuffer keyValueBuffer (
                        btree.fileAllocator, keyValueOffset);
                    for (ui32 i = 0; i < count; ++i) {
                        buffer << entries[i];
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
                                Serializable::Header (
                                    entries[i].value->Type (),
                                    entries[i].value->Version (),
                                    keyValueSizes[i].second);
                        }
                        entries[i].value->Write (keyValueBuffer);
                    }
                    if (btree.fileAllocator.IsSecure ()) {
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
                    btree.fileAllocator.Free (keyValueOffset);
                }
                if (btree.fileAllocator.IsSecure ()) {
                    // See comment above keyValueBuffer.
                    buffer.AdvanceWriteOffset (
                        SecureZeroMemory (
                            buffer.GetWritePtr (),
                            buffer.GetDataAvailableForWriting ()));
                }
                buffer.BlockWrite ();
                dirty = false;
            }
            if (count > 0) {
                if (leftNode != nullptr) {
                    leftNode->Flush ();
                }
                for (ui32 i = 0; i < count; ++i) {
                    if (entries[i].rightNode != nullptr) {
                        entries[i].rightNode->Flush ();
                    }
                }
            }
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
                    it.prefix.Reset (entries[index].key);
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
                    it.prefix.Reset (entries[index].key);
                    it.node = Iterator::NodeIndex (this, index);
                    it.finished = false;
                }
                dirty = true;
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
                            it.prefix.Reset (entries[index].key);
                            it.node = Iterator::NodeIndex (this, index);
                            it.finished = false;
                        }
                    }
                    else {
                        right->InsertEntry (entry, index - splitIndex);
                        if (it.IsFinished ()) {
                            it.prefix.Reset (entries[index - splitIndex].key);
                            // -1 because we will be removing the 0'th entry below.
                            it.node = Iterator::NodeIndex (right, index - splitIndex - 1);
                            it.finished = false;
                        }
                    }
                    entry = right->entries[0];
                    right->RemoveEntry (0);
                }
                dirty = true;
                right->leftOffset = entry.rightOffset;
                right->leftNode = entry.rightNode;
                right->dirty = true;
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
                    entries[index].key = leaf->entries[0].key;
                    entries[index].value = leaf->entries[0].value;
                    child->Remove (*leaf->entries[0].key);
                    if (child->IsPoor ()) {
                        RestoreBalance (index);
                    }
                }
                else {
                    entries[index].key->Release ();
                    entries[index].value->Release ();
                    RemoveEntry (index);
                }
                dirty = true;
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
            dirty = true;
            left->dirty = true;
            right->dirty = true;
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
            dirty = true;
            left->dirty = true;
            right->dirty = true;
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
            dirty = true;
            left->dirty = true;
            Delete (right);
        }

        void BTree::Node::Split (Node *node) {
            // This assert checks IsFull as well.
            assert (count == btree.header.entriesPerNode);
            ui32 splitIndex = count / 2;
            node->count = count - splitIndex;
            std::memcpy (
                node->entries,
                entries + splitIndex,
                node->count * sizeof (Entry));
            count = splitIndex;
        }

        void BTree::Node::Concatenate (Node *node) {
            std::memcpy (
                entries + count,
                node->entries,
                node->count * sizeof (Entry));
            count += node->count;
            node->count = 0;
        }

        void BTree::Node::InsertEntry (
                const Entry &entry,
                ui32 index) {
            std::memmove (
                entries + index + 1,
                entries + index,
                (count++ - index) * sizeof (Entry));
            entries[index] = entry;
        }

        void BTree::Node::RemoveEntry (ui32 index) {
            std::memcpy (
                entries + index,
                entries + index + 1,
                (--count - index) * sizeof (Entry));
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

        BTree::BTree (
                FileAllocator &fileAllocator,
                FileAllocator::PtrType offset,
                BufferedFile::Transaction::SharedPtr transaction,
                const std::string &keyType,
                const std::string &valueType,
                std::size_t entriesPerNode,
                std::size_t nodesPerPage,
                Allocator::SharedPtr allocator) :
                FileAllocatorObject (fileAllocator, offset, transaction),
                header (keyType, valueType, (ui32)entriesPerNode),
                keyFactory (Key::GetTypeFactory (keyType.c_str ())),
                valueFactory (Value::GetTypeFactory (valueType.c_str ())),
                root (nullptr),
                dirty (false) {
            if (offset != 0) {
                FileAllocator::BlockBuffer buffer (fileAllocator, offset);
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
                            offset);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt BTree @" THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
            else if (Key::IsType (header.keyType.c_str ()) &&
                    (header.valueType.empty () || Value::IsType (header.valueType.c_str ()))) {
                offset = fileAllocator.Alloc (header.Size ());
                dirty = true;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "key (%s) / value (%s) types are not valid.",
                    keyType.c_str (), valueType.c_str ());
            }
            nodeAllocator = new BlockAllocator (
                Node::Size (header.entriesPerNode),
                nodesPerPage,
                allocator);
            root = Node::Alloc (*this, header.rootOffset);
            if (header.rootOffset == 0) {
                assert (dirty);
                header.rootOffset = root->offset;
            }
        }

        BTree::~BTree () {
            Node::Free (root);
        }

        void BTree::Delete (
                FileAllocator &fileAllocator,
                FileAllocator::PtrType offset) {
            FileAllocator::BlockBuffer buffer (fileAllocator, offset);
            buffer.BlockRead ();
            ui32 magic;
            buffer >> magic;
            if (magic == MAGIC32) {
                Header header;
                buffer >> header;
                Node::Delete (fileAllocator, header.rootOffset);
                fileAllocator.Free (offset);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt BTree @" THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        }

        bool BTree::Find (
                const Key &key,
                Iterator &it) {
            if (key.IsKindOf (header.keyType.c_str ())) {
                it.Clear ();
                ui32 index = 0;
                for (Node *node = root; node != nullptr; node = node->GetChild (index)) {
                    if (node->Find (key, index)) {
                        it.prefix.Reset (node->entries[index].key);
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
            if (key->IsKindOf (header.keyType.c_str ()) &&
                    (header.valueType.empty () || value->IsKindOf (header.valueType.c_str ()))) {
                it.Clear ();
                Node::Entry entry (key.Get (), value.Get ());
                Node::InsertResult result = root->Insert (entry, it);
                if (result == Node::Overflow) {
                    // The path to the leaf node is full.
                    // Create a new root node and make the entry
                    // its first.
                    Node *node = Node::Alloc (*this);
                    node->leftOffset = root->offset;
                    node->leftNode = root;
                    node->InsertEntry (entry, 0);
                    if (it.IsFinished ()) {
                        it.prefix.Reset (node->entries[0].key);
                        it.node = Iterator::NodeIndex (node, 0);
                        it.finished = false;
                    }
                    node->dirty = true;
                    SetRoot (node);
                    result = Node::Inserted;
                }
                // If we inserted the key/value pair, take ownership.
                if (result == Node::Inserted) {
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

        bool BTree::Delete (const Key &key) {
            if (key.IsKindOf (header.keyType.c_str ())) {
                bool removed = root->Remove (key);
                if (removed && root->IsEmpty () && root->GetChild (0) != nullptr) {
                    Node *node = root;
                    SetRoot (root->GetChild (0));
                    Node::Delete (node);
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

        void BTree::Dump () {
            if (root != nullptr) {
                root->Dump ();
            }
        }

        void BTree::Flush () {
            if (dirty) {
                FileAllocator::BlockBuffer buffer (fileAllocator, offset);
                buffer << MAGIC32 << header;
                buffer.BlockWrite ();
                dirty = false;
            }
            root->Flush ();
        }

        void BTree::Reload () {
            if (dirty) {
                FileAllocator::BlockBuffer buffer (fileAllocator, offset);
                buffer.BlockRead ();
                ui32 magic;
                buffer >> magic >> header;
                dirty = false;
            }
            Node::Free (root);
            root = Node::Alloc (*this, header.rootOffset);
        }

        void BTree::SetRoot (Node *node) {
            root = node;
            header.rootOffset = root->offset;
            dirty = true;
        }

    } // namespace util
} // namespace thekogans
