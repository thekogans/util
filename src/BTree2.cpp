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
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/BTree2.h"

namespace thekogans {
    namespace util {

        inline Serializer &operator << (
                Serializer &serializer,
                const BTree2::Node::Entry &entry) {
            serializer << entry.rightOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                BTree2::Node::Entry &entry) {
            serializer >> entry.rightOffset;
            // Because of the way we allocate the BTree2::Node the
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
                const BTree2::Header &header) {
            serializer <<
                header.keyType <<
                header.valueType <<
                header.entriesPerNode <<
                header.rootOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                BTree2::Header &header) {
            serializer >>
                header.keyType >>
                header.valueType >>
                header.entriesPerNode >>
                header.rootOffset;
            return serializer;
        }

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE (thekogans::util::BTree2::Key)
        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE (thekogans::util::BTree2::Value)

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            thekogans::util::BTree2::StringKey,
            1,
            BTree2::Key::TYPE)
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            thekogans::util::BTree2::StringValue,
            1,
            BTree2::Value::TYPE)

        BTree2::Node::Node (
                BTree2 &btree_,
                FileAllocator::PtrType offset_) :
                btree (btree_),
                offset (offset_),
                count (0),
                leftOffset (0),
                leftNode (nullptr),
                dirty (false) {
            if (offset != 0) {
                FileAllocator::BlockBuffer::SharedPtr buffer =
                    btree.fileAllocator->CreateBlockBuffer (offset);
                ui32 magic;
                *buffer >> magic;
                if (magic == MAGIC32) {
                    *buffer >> count;
                    if (count > 0) {
                        *buffer >> leftOffset >> keyValueOffset;
                        FileAllocator::BlockBuffer::SharedPtr keyValueBuffer =
                            btree.fileAllocator->CreateBlockBuffer (keyValueOffset);
                        Serializable::Header keyHeader (btree.header.keyType, 0, 0);
                        Serializable::Header valueHeader (btree.header.valueType, 0, 0);
                        for (ui32 i = 0; i < count; ++i) {
                            *buffer >> entries[i];
                            {
                                entries[i].key =
                                    Key::CreateType (keyHeader.type.c_str ()).Release ();
                                *keyValueBuffer >> keyHeader.version >> keyHeader.size;
                                entries[i].key->Read (keyHeader, *keyValueBuffer);
                            }
                            {
                                entries[i].value =
                                    Value::CreateType (valueHeader.type.c_str ()).Release ();
                                *keyValueBuffer >> valueHeader.version >> valueHeader.size;
                                entries[i].value->Read (valueHeader, *keyValueBuffer);
                            }
                        }
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt BTree2::Node: " THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
            else {
                offset = btree.fileAllocator->Alloc (
                    FileSize (btree.header.entriesPerNode));
                Save ();
            }
        }

        BTree2::Node::~Node () {
            if (dirty) {
                FileAllocator::BlockBuffer::SharedPtr buffer =
                    btree.fileAllocator->CreateBlockBuffer (offset, 0, false);
                *buffer << MAGIC32 << count;
                if (count > 0) {
                    // Calculate key/value sizes.
                    std::size_t totalKeyValueSize = 0;
                    Array<std::pair<SizeT, SizeT>> keyValueSizes (count);
                    std::pair<SizeT, SizeT> largestKeyValueSize (0, 0);
                    for (ui32 i = 0; i < count; ++i) {
                        std::pair<SizeT, SizeT> keyValueSize (
                            entries[i].key->Size (), entries[i].value->Size ());
                        if (largestKeyValueSize.first < keyValueSize.first) {
                            largestKeyValueSize.first = keyValueSize.first;
                        }
                        if (largestKeyValueSize.second < keyValueSize.second) {
                            largestKeyValueSize.second = keyValueSize.second;
                        }
                        keyValueSizes[i] = keyValueSize;
                        totalKeyValueSize +=
                            UI16_SIZE + keyValueSize.first.Size () + keyValueSize.first +
                            UI16_SIZE + keyValueSize.second.Size () + keyValueSize.second;
                    }
                    // Get existing block size.
                    ui64 blockSize = 0;
                    if (keyValueOffset != 0) {
                        blockSize = btree.fileAllocator->GetBlockSize (keyValueOffset);
                    }
                    // If existing block size is less than what we need,
                    // resize it.
                    if (blockSize < totalKeyValueSize) {
                        // First, free the old block.
                        if (keyValueOffset != 0) {
                            btree.fileAllocator->Free (keyValueOffset);
                        }
                        // We mitigate the number of times we need to reallocate
                        // the key/value block by trying to allocate enough space
                        // for all entries containing the largest key and value.
                        // Depending on the pathology of the combination and the
                        // size of the node this can be very wasteful.
                        totalKeyValueSize =
                            (UI16_SIZE +
                                largestKeyValueSize.first.Size () +
                                largestKeyValueSize.first +
                            UI16_SIZE +
                                largestKeyValueSize.second.Size () +
                                largestKeyValueSize.second) *
                            btree.header.entriesPerNode;
                        // Allocate a new key/value block.
                        keyValueOffset = btree.fileAllocator->Alloc (totalKeyValueSize);
                    }
                    *buffer << leftOffset << keyValueOffset;
                    FileAllocator::BlockBuffer::SharedPtr keyValueBuffer =
                        btree.fileAllocator->CreateBlockBuffer (keyValueOffset, 0, false);
                    for (ui32 i = 0; i < count; ++i) {
                        *buffer << entries[i];
                        *keyValueBuffer <<
                            entries[i].key->Version () <<
                            SizeT (keyValueSizes[i].first);
                        entries[i].key->Write (*keyValueBuffer);
                        *keyValueBuffer <<
                            entries[i].value->Version () <<
                            SizeT (keyValueSizes[i].second);
                        entries[i].value->Write (*keyValueBuffer);
                    }
                    // Zero out the unused portion of the keyValueBuffer to
                    // prevent leaking sensitive data.
                    if (keyValueBuffer->GetDataAvailableForWriting () > 0) {
                        memset (
                            keyValueBuffer->GetWritePtr (),
                            0,
                            keyValueBuffer->GetDataAvailableForWriting ());
                        keyValueBuffer->AdvanceWriteOffset (
                            keyValueBuffer->GetDataAvailableForWriting ());
                    }
                    btree.fileAllocator->WriteBlockBuffer (*keyValueBuffer);
                }
                else if (keyValueOffset != 0) {
                    btree.fileAllocator->Free (keyValueOffset);
                }
                // See comment above keyValueBuffer.
                if (buffer->GetDataAvailableForWriting () > 0) {
                    memset (
                        buffer->GetWritePtr (),
                        0,
                        buffer->GetDataAvailableForWriting ());
                    buffer->AdvanceWriteOffset (buffer->GetDataAvailableForWriting ());
                }
                btree.fileAllocator->WriteBlockBuffer (*buffer);
            }
            if (count > 0) {
                Free (leftNode);
                for (ui32 i = 0; i < count; ++i) {
                    entries[i].key->Release ();
                    entries[i].value->Release ();
                    Free (entries[i].rightNode);
                }
            }
        }

        std::size_t BTree2::Node::FileSize (std::size_t entriesPerNode) {
            const std::size_t ENTRY_SIZE =
                FileAllocator::PTR_TYPE_SIZE; // rightOffset
            return
                UI32_SIZE + // magic
                UI32_SIZE + // count
                FileAllocator::PTR_TYPE_SIZE + // leftOffset
                FileAllocator::PTR_TYPE_SIZE + // keyValueOffset
                entriesPerNode * ENTRY_SIZE;   // entries
        }

        std::size_t BTree2::Node::Size (std::size_t entriesPerNode) {
            return sizeof (Node) + (entriesPerNode - 1) * sizeof (Entry);
        }

        BTree2::Node *BTree2::Node::Alloc (
                BTree2 &btree,
                FileAllocator::PtrType offset) {
            return new (
                btree.nodeAllocator->Alloc (
                    Size (btree.header.entriesPerNode))) Node (btree, offset);
        }

        void BTree2::Node::Free (Node *node) {
            if (node != nullptr) {
                node->~Node ();
                node->btree.nodeAllocator->Free (
                    node, Size (node->btree.header.entriesPerNode));
            }
        }

        void BTree2::Node::Delete (Node *node) {
            if (node->count == 0) {
                node->btree.fileAllocator->Free (node->offset);
                // We've just deleted it's block, writting to it now would be a bad idea.
                node->dirty = false;
                Free (node);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Logic error: trying to delete a node with count > 0 "
                    THEKOGANS_UTIL_UI64_FORMAT,
                    node->offset);
            }
        }

        void BTree2::Node::Delete (
                FileAllocator &fileAllocator,
                FileAllocator::PtrType offset) {
            if (offset != 0) {
                ui32 count;
                FileAllocator::PtrType leftOffset = 0;
                FileAllocator::PtrType keyValueOffset = 0;
                Entry entry;
                FileAllocator::BlockBuffer::SharedPtr buffer =
                    fileAllocator.CreateBlockBuffer (offset);
                ui32 magic;
                *buffer >> magic;
                if (magic == MAGIC32) {
                    *buffer >> count;
                    if (count > 0) {
                        *buffer >> leftOffset >> keyValueOffset;
                        Delete (fileAllocator, leftOffset);
                        fileAllocator.Free (keyValueOffset);
                        for (ui32 i = 0; i < count; ++i) {
                            *buffer >> entry;
                            Delete (fileAllocator, entry.rightOffset);
                        }
                    }
                    fileAllocator.Free (offset);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt BTree2::Node: " THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
        }

        BTree2::Node *BTree2::Node::GetChild (ui32 index) {
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

        bool BTree2::Node::PrefixSearch (
                const Key &prefix,
                ui32 &index) const {
            ui32 last = index;
            index = 0;
            while (index < last) {
                ui32 middle = (index + last) / 2;
                i32 result = prefix->PrefixCompare (*entries[middle].key);
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

        bool BTree2::Node::FindFirstPrefix (
                const Key &prefix,
                ui32 &index) const {
            index = count;
            ui32 lastIndex = index;
            while (PrefixSearch (prefix, lastIndex)) {
                index = lastIndex;
                if (lastIndex-- == 0) {
                    break;
                }
            }
            return index < count;
        }

        bool BTree2::Node::Search (
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

        BTree2::Node::InsertResult BTree2::Node::Insert (Entry &entry) {
            ui32 index = 0;
            if (Search (*entry.key, index)) {
                assert (index < count);
                if (entry.value != nullptr) {
                    entry.value = entries[index].value;
                }
                return Duplicate;
            }
            Node *child = GetChild (index);
            if (child != nullptr) {
                InsertResult result = child->Insert (entry);
                if (result == Inserted || result == Duplicate) {
                    return result;
                }
            }
            if (!IsFull ()) {
                InsertEntry (entry, index);
                Save ();
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
                    }
                    else {
                        right->InsertEntry (entry, index - splitIndex);
                    }
                    entry = right->entries[0];
                    right->RemoveEntry (0);
                }
                Save ();
                right->leftOffset = entry.rightOffset;
                right->leftNode = entry.rightNode;
                right->Save ();
                entry.rightOffset = right->offset;
                entry.rightNode = right;
                return Overflow;
            }
        }

        bool BTree2::Node::Remove (const Key &key) {
            ui32 index = 0;
            bool found = Search (key, index);
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
                Save ();
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

        void BTree2::Node::RestoreBalance (ui32 index) {
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

        void BTree2::Node::RotateRight (
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
            Save ();
            left->Save ();
            right->Save ();
        }

        void BTree2::Node::RotateLeft (
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
            Save ();
            left->Save ();
            right->Save ();
        }

        void BTree2::Node::Merge (
                ui32 index,
                Node *left,
                Node *right) {
            assert (left->count + right->count < btree.header.entriesPerNode);
            entries[index].rightOffset = right->leftOffset;
            entries[index].rightNode = right->leftNode;
            left->Concatenate (entries[index]);
            left->Concatenate (right);
            RemoveEntry (index);
            Save ();
            left->Save ();
            Delete (right);
        }

        void BTree2::Node::Split (Node *node) {
            ui32 splitIndex = btree.header.entriesPerNode / 2;
            node->count = count - splitIndex;
            std::memcpy (
                node->entries,
                entries + splitIndex,
                node->count * sizeof (Entry));
            count = splitIndex;
        }

        void BTree2::Node::Concatenate (Node *node) {
            std::memcpy (
                entries + count,
                node->entries,
                node->count * sizeof (Entry));
            count += node->count;
            node->count = 0;
        }

        void BTree2::Node::InsertEntry (
                const Entry &entry,
                ui32 index) {
            std::memmove (
                entries + index + 1,
                entries + index,
                (count++ - index) * sizeof (Entry));
            entries[index] = entry;
        }

        void BTree2::Node::RemoveEntry (ui32 index) {
            std::memcpy (
                entries + index,
                entries + index + 1,
                (--count - index) * sizeof (Entry));
        }

        void BTree2::Node::Dump () {
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

        bool BTree2::Iterator::Next () {
            if (!finished) {
                finished = true;
                ++node.second;
                if (node.second < node.first->count) {
                    Node *child = node.first->GetChild (node.second);
                    if (child != nullptr) {
                        // We back up an entry because when we're done iterating
                        // over (left) children, we need to check the entry at
                        // current index which has been incremented above.
                        parents.push_back (NodeIndex (node.first, node.second - 1));
                        ui32 index = 0;
                        while (child != nullptr &&
                                (prefix == nullptr || child->FindFirstPrefix (*prefix, index))) {
                            finished = false;
                            parents.push_back (NodeIndex (child, index));
                            child = child->GetChild (index);
                        }
                        node = parents.back ();
                        parents.pop_back ();
                        // If we didn't encounter any eligible children above,
                        // increment the index so that we can check the next
                        // entry below.
                        if (finished) {
                            ++node.second;
                        }
                    }
                    finished &= prefix != nullptr &&
                        !node.first->entries[node.second].key->PrefixCompare (*prefix);
                }
                else if (!parents.empty ()) {
                    node = parents.back ();
                    parents.pop_back ();
                    finished = prefix != nullptr &&
                        !node.first->entries[node.second].key->PrefixCompare (*prefix);
                }
            }
            if (finished) {
                Clear ();
            }
            return !finished;
        }

        BTree2::BTree2 (
                FileAllocator::SharedPtr fileAllocator_,
                FileAllocator::PtrType offset_,
                const std::string &keyType,
                const std::string &valueType,
                std::size_t entriesPerNode,
                std::size_t nodesPerPage,
                Allocator::SharedPtr allocator) :
                fileAllocator (fileAllocator_),
                offset (offset_),
                header (keyType, valueType, (ui32)entriesPerNode),
                root (nullptr) {
            if (offset != 0) {
                FileAllocator::BlockBuffer::SharedPtr buffer =
                    fileAllocator->CreateBlockBuffer (offset);
                ui32 magic;
                *buffer >> magic;
                if (magic == MAGIC32) {
                    *buffer >> header;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Corrupt BTree2: " THEKOGANS_UTIL_UI64_FORMAT,
                        offset);
                }
            }
            else {
                offset = fileAllocator->Alloc (header.Size ());
                Save ();
            }
            nodeAllocator =
                BlockAllocator::Pool::Instance ()->GetBlockAllocator (
                    Node::Size (header.entriesPerNode),
                    nodesPerPage,
                    allocator);
            root = Node::Alloc (*this, header.rootOffset);
            if (header.rootOffset != root->offset) {
                header.rootOffset = root->offset;
                Save ();
            }
        }

        BTree2::~BTree2 () {
            Node::Free (root);
        }

        void BTree2::Delete (
                FileAllocator &fileAllocator,
                FileAllocator::PtrType offset) {
            FileAllocator::BlockBuffer::SharedPtr buffer =
                fileAllocator.CreateBlockBuffer (offset);
            ui32 magic;
            *buffer >> magic;
            if (magic == MAGIC32) {
                Header header;
                *buffer >> header;
                Node::Delete (fileAllocator, header.rootOffset);
                fileAllocator.Free (offset);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt BTree2: " THEKOGANS_UTIL_UI64_FORMAT,
                    offset);
            }
        }

        bool BTree2::Search (
                const Key &key,
                Value::SharedPtr &value) {
            LockGuard<SpinLock> guard (spinLock);
            ui32 index = 0;
            for (Node *node = root; node != nullptr; node = node->GetChild (index)) {
                if (node->Search (key, index)) {
                    value.Reset (node->entries[index].value);
                    return true;
                }
            }
            return false;
        }

        bool BTree2::Add (
                Key::SharedPtr key,
                Value::SharedPtr &value) {
            LockGuard<SpinLock> guard (spinLock);
            Node::Entry entry (key.Get (), value.Get ());
            Node::InsertResult result = root->Insert (entry);
            if (result == Node::Overflow) {
                // The path to the leaf node is full.
                // Create a new root node and make the entry
                // its first.
                Node *node = Node::Alloc (*this);
                node->leftOffset = root->offset;
                node->leftNode = root;
                node->InsertEntry (entry, 0);
                node->Save ();
                SetRoot (node);
                result = Node::Inserted;
            }
            // If we inserted the key/value pair, take ownership.
            if (result == Node::Inserted) {
                key.Release ();
                value.Release ();
            }
            else {
                // Otherwise it's a duplicate.
                assert (result == Node::Duplicate);
                value.Reset (entry.value);
            }
            return result == Node::Inserted;
        }

        bool BTree2::Delete (const Key &key) {
            LockGuard<SpinLock> guard (spinLock);
            bool removed = root->Remove (key);
            if (removed && root->IsEmpty () && root->GetChild (0) != nullptr) {
                Node *node = root;
                SetRoot (root->GetChild (0));
                Node::Delete (node);
            }
            return removed;
        }

        bool BTree2::FindFirst (Iterator &it) {
            it.Clear ();
            LockGuard<SpinLock> guard (spinLock);
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

        void BTree2::Flush () {
            LockGuard<SpinLock> guard (spinLock);
            Node::Free (root);
            root = Node::Alloc (*this, header.rootOffset);
        }

        void BTree2::Dump () {
            LockGuard<SpinLock> guard (spinLock);
            if (root != nullptr) {
                root->Dump ();
            }
        }

        void BTree2::Save () {
            FileAllocator::BlockBuffer::SharedPtr buffer =
                fileAllocator->CreateBlockBuffer (offset, 0, false);
            *buffer << MAGIC32 << header;
            fileAllocator->WriteBlockBuffer (*buffer);
        }

        void BTree2::SetRoot (Node *node) {
            root = node;
            header.rootOffset = root->offset;
            Save ();
        }

    } // namespace util
} // namespace thekogans
