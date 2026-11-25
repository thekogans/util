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
            serializer << header.flags << header.entriesPerNode << header.rootOffset;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                TransactedFileBTree::Header &header) {
            {
                Serializer::ContextGuard guard (serializer);
                serializer >> header.keyContext >> header.valueContext;
            }
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

        bool TransactedFileBTree::Iterator::Next (bool ignorePrefix) {
            if (!finished) {
                ++node.second;
                Node *child = node.first->GetChild (node.second);
                std::size_t savedDepth = parents.size ();
                if (child != nullptr) {
                    parents.push_back (NodeIndex (node.first, node.second));
                    while (child != nullptr) {
                        if (ignorePrefix || prefix == nullptr) {
                            parents.push_back (NodeIndex (child, 0));
                            child = child->GetChild (0);
                        }
                        else {
                            ui32 index = 0;
                            if (child->FindFirstPrefix (*prefix, index)) {
                                parents.push_back (NodeIndex (child, index));
                            }
                            child = child->GetChild (index);
                        }
                    }
                }
                if (parents.size () > savedDepth + 1) {
                    // Found a matching successor inside `child`'s subtree.
                    node = parents.back ();
                    parents.pop_back ();
                }
                else {
                    // Either `child == nullptr` or `child`'s subtree contained no matching prefix entries.
                    if (parents.size () == savedDepth + 1) {
                        // Pop the frame tentatively pushed before descending into `child`.
                        parents.pop_back ();
                    }
                    while (node.second >= node.first->count && !parents.empty ()) {
                        node = parents.back ();
                        parents.pop_back ();
                    }
                }
                if (node.second < node.first->count) {
                    finished = !ignorePrefix && prefix != nullptr &&
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

        bool TransactedFileBTree::Iterator::Prev (bool ignorePrefix) {
            if (!finished) {
                Node *child = node.first->GetChild (node.second);
                std::size_t savedDepth = parents.size ();
                if (child != nullptr) {
                    std::size_t matchDepth = 0;
                    parents.push_back (NodeIndex (node.first, node.second));
                    while (child != nullptr) {
                        if (ignorePrefix || prefix == nullptr) {
                            parents.push_back (NodeIndex (child, child->count));
                            matchDepth = parents.size ();
                            child = child->GetChild (child->count);
                        }
                        else {
                            ui32 index = 0;
                            if (child->FindLastPrefix (*prefix, index)) {
                                parents.push_back (NodeIndex (child, index + 1));
                                matchDepth = parents.size ();
                                child = child->GetChild (index + 1);
                            }
                            else {
                                parents.push_back (NodeIndex (child, index));
                                child = child->GetChild (index);
                            }
                        }
                    }
                    if (matchDepth > 0) {
                        parents.resize (matchDepth);
                    }
                    else {
                        parents.resize (savedDepth + 1);
                    }
                }
                if (parents.size () > savedDepth + 1) {
                    // Found a matching predecessor inside `child`'s subtree.
                    node = parents.back ();
                    parents.pop_back ();
                    --node.second;
                }
                else {
                    // Either `child == nullptr` or `child`'s subtree contained no matching prefix entries.
                    if (parents.size () == savedDepth + 1) {
                        // Pop the frame we tentatively pushed before descending into `child`.
                        parents.pop_back ();
                    }
                    if (node.second > 0) {
                        --node.second;
                    }
                    else {
                        bool found = false;
                        while (!parents.empty ()) {
                            NodeIndex parent = parents.back ();
                            parents.pop_back ();
                            if (parent.second > 0) {
                                node = NodeIndex (parent.first, parent.second - 1);
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            finished = true;
                        }
                    }
                }
                if (!finished) {
                    finished = !ignorePrefix && prefix != nullptr &&
                        prefix->PrefixCompare (*node.first->entries[node.second].key) != 0;
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

        void TransactedFileBTree::Node::Entry::Unsubscribe (
                Node *node,
                bool isValueisObject,
                bool unsubscribeRight) {
            if (value != nullptr) {
                if (!isValueisObject) {
                    value->Producer<SerializableEvents>::Unsubscribe (*node);
                }
                else {
                    TransactedFile::Object::SharedPtr object = Serializable::SharedPtr (value);
                    object->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*node);
                }
            }
            if (unsubscribeRight && right != nullptr) {
                right->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*node);
            }
        }

        void TransactedFileBTree::Node::Entry::Subscribe (
                Node *node,
                bool isValueisObject,
                bool subscribeRight) {
            if (value != nullptr) {
                if (!isValueisObject) {
                    value->Producer<SerializableEvents>::Subscribe (*node);
                }
                else {
                    TransactedFile::Object::SharedPtr object = Serializable::SharedPtr (value);
                    object->Producer<TransactedFile::ObjectEvents>::Subscribe (*node);
                }
            }
            if (subscribeRight && right != nullptr) {
                right->Producer<TransactedFile::ObjectEvents>::Subscribe (*node);
            }
        }

        void TransactedFileBTree::Node::Entry::Destroy (
                Node *node,
                bool isValueisObject) {
            if (isValueisObject) {
                if (value != nullptr) {
                    TransactedFile::Object::SharedPtr object = Serializable::SharedPtr (value);
                    object->FreeBlock ();
                }
                else if (valueOffset != 0) {
                    Object::DestroyObject (node->GetFile (), valueOffset);
                }
            }
            Reset (node, isValueisObject, false);
        }

        void TransactedFileBTree::Node::Entry::Reset (
                Node *node,
                bool isValueisObject,
                bool resetRight) {
            Unsubscribe (node, isValueisObject, resetRight);
            if (THEKOGANS_UTIL_LIKELY (key != nullptr)) {
                key->Release ();
                key = nullptr;
            }
            if (THEKOGANS_UTIL_LIKELY (value != nullptr)) {
                value->Release ();
                value = nullptr;
            }
            if (resetRight && right != nullptr) {
                right->Release ();
                right = nullptr;
            }
        }

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

        std::size_t TransactedFileBTree::Node::Size (std::size_t entriesPerNode) noexcept {
            return sizeof (Node) + entriesPerNode * sizeof (Entry);
        }

        std::size_t TransactedFileBTree::Node::SlotsPerPage (std::size_t entriesPerNode) noexcept {
            std::size_t slots = 32768 / Size (entriesPerNode);
            return slots < 32 ? 32 : (slots > 512 ? 512 : slots);
        }

        std::size_t TransactedFileBTree::Node::TLCSize (std::size_t entriesPerNode) noexcept {
            std::size_t tlc = SlotsPerPage (entriesPerNode) / 2;
            return tlc < 16 ? 16 : (tlc > 128 ? 128 : tlc);
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

        namespace {
            constexpr std::size_t FAULT_IN_STRIPE_COUNT = 64;
            SpinLock faultInStripes[FAULT_IN_STRIPE_COUNT];

            inline SpinLock &GetFaultInStripe (
                const void *node,
                ui32 slot) noexcept {
                std::uintptr_t hash =
                    (reinterpret_cast<std::uintptr_t> (node) >> 6) ^
                    (static_cast<std::uintptr_t> (slot) * 2654435761u);
                return faultInStripes[hash & (FAULT_IN_STRIPE_COUNT - 1)];
            }
        }

        Serializable::SharedPtr TransactedFileBTree::Node::GetValue (ui32 index) {
            Serializable *val = __atomic_load_n (&entries[index].value, __ATOMIC_ACQUIRE);
            if (THEKOGANS_UTIL_UNLIKELY (val == nullptr && entries[index].valueOffset != 0)) {
                LockGuard<SpinLock> guard (GetFaultInStripe (this, index + 1024));
                val = __atomic_load_n (&entries[index].value, __ATOMIC_ACQUIRE);
                if (val == nullptr) {
                    TransactedFile::ObjectRange valueRange (file, entries[index].valueOffset);
                    TransactedFile::Object::SharedPtr value;
                    valueRange >> value;
                    value->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                    val = value.Release ();
                    __atomic_store_n (&entries[index].value, val, __ATOMIC_RELEASE);
                }
            }
            return val;
        }

        TransactedFileBTree::Node *TransactedFileBTree::Node::GetChild (ui32 index) {
            if (THEKOGANS_UTIL_UNLIKELY (index == 0)) {
                Node *child = __atomic_load_n (&left, __ATOMIC_ACQUIRE);
                if (THEKOGANS_UTIL_UNLIKELY (child == nullptr && leftOffset != 0)) {
                    LockGuard<SpinLock> guard (GetFaultInStripe (this, 0));
                    child = __atomic_load_n (&left, __ATOMIC_ACQUIRE);
                    if (child == nullptr) {
                        child = Alloc (btree, file, leftOffset);
                        child->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                        __atomic_store_n (&left, child, __ATOMIC_RELEASE);
                    }
                }
                return child;
            }
            else {
                --index;
                Node *child = __atomic_load_n (&entries[index].right, __ATOMIC_ACQUIRE);
                if (THEKOGANS_UTIL_UNLIKELY (child == nullptr && entries[index].rightOffset != 0)) {
                    LockGuard<SpinLock> guard (GetFaultInStripe (this, index + 1));
                    child = __atomic_load_n (&entries[index].right, __ATOMIC_ACQUIRE);
                    if (child == nullptr) {
                        child = Alloc (btree, file, entries[index].rightOffset);
                        child->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                        __atomic_store_n (&entries[index].right, child, __ATOMIC_RELEASE);
                    }
                }
                return child;
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

        bool TransactedFileBTree::Node::FindLastPrefix (
                const Key &prefix,
                ui32 &index) const {
            index = count;
            if (PrefixFind (prefix, index)) {
                // `index` is a matching entry; binary-search [index + 1 .. count) for the rightmost match.
                ui32 low = index + 1;
                ui32 high = count;
                while (low < high) {
                    ui32 middle = low + ((high - low) >> 1);
                    if (prefix.PrefixCompare (*entries[middle].key) == 0) {
                        index = middle;
                        low = middle + 1;
                    }
                    else {
                        high = middle;
                    }
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

        bool TransactedFileBTree::Node::FindUpperBound (
                const Key &prefix,
                ui32 &index) const {
            ui32 low = 0;
            ui32 high = count;
            while (low < high) {
                ui32 mid = low + ((high - low) >> 1);
                if (prefix.PrefixCompare (*entries[mid].key) < 0) {
                    high = mid;
                }
                else {
                    low = mid + 1;
                }
            }
            index = low;
            return index < count;
        }

        // In Node: first entry >= `prefix` (either matching `prefix` or strictly greater than `prefix`).
        bool TransactedFileBTree::Node::FindLowerBound (
                const Key &prefix,
                ui32 &index) const {
            ui32 low = 0;
            ui32 high = count;
            while (low < high) {
                ui32 mid = low + ((high - low) >> 1);
                if (prefix.PrefixCompare (*entries[mid].key) <= 0) {
                    high = mid;
                }
                else {
                    low = mid + 1;
                }
            }
            index = low;
            return index < count;
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
                ui32 splitIndex = btree->header.entriesPerNode / 2;
                right->MoveEntries (this, splitIndex, 0, count - splitIndex);
                if (THEKOGANS_UTIL_UNLIKELY (index != splitIndex)) {
                    if (index < splitIndex) {
                        InsertEntry (entry, index);
                    }
                    else {
                        right->InsertEntry (entry, index - splitIndex);
                    }
                    entry = right->entries[0];
                    right->RemoveEntry (0);
                }
                right->SetLeft (entry.right, entry.rightOffset);
                entry.rightOffset = right->GetOffset ();
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
                    // 1. Free the deleted entry resources leaving the right child intact.
                    entries[index].Destroy (this, btree->header.IsValueIsObject ());
                    // 2. Promote leaf->entries's key & value into entries[index], preserving entries[index].right!
                    Entry promoted = entries[index];
                    promoted.key = leaf->entries->key;
                    promoted.key->AddRef ();
                    promoted.valueOffset = leaf->entries->valueOffset;
                    promoted.value = leaf->entries->value;
                    SetEntry (index, promoted);
                    // 3. Transfer value ownership out of leaf->entries (unsubscribe from `leaf`, do NOT release
                    //    `value`, and keep `leaf->entries.key` intact so child->Remove can find & delete it).
                    leaf->entries->Unsubscribe (leaf, btree->header.IsValueIsObject (), false);
                    leaf->entries->value = nullptr;
                    leaf->entries->valueOffset = 0;
                    // 4. Delete the promoted successor key from the right subtree and rebalance.
                    Key::SharedPtr successorKey (entries[index].key);
                    child->Remove (*successorKey);
                    if (child->IsPoor ()) {
                        RestoreBalance (index + 1);
                    }
                }
                else {
                    entries[index].Destroy (this, btree->header.IsValueIsObject ());
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
            Node *child = GetChild (index);
            if (child != nullptr && child->IsPoor ()) {
                if (index > 0) {
                    Node *left = GetChild (index - 1);
                    if (left != nullptr && left->IsPlentiful ()) {
                        RotateRight (index - 1, left, child);
                        return;
                    }
                }
                if (index < count) {
                    Node *right = GetChild (index + 1);
                    if (right != nullptr && right->IsPlentiful ()) {
                        RotateLeft (index, child, right);
                        return;
                    }
                }
                if (index > 0) {
                    Merge (index - 1, GetChild (index - 1), child);
                }
                else {
                    Merge (index, child, GetChild (index + 1));
                }
            }
        }

        // BEFORE (Right Child Underflows; Left Sibling has spare key K2)
        //                      Parent
        //                   [ ... | P | ... ]
        //                        /     \
        //          Left Sibling           Right Child (Underflow)
        //         [ K1 | K2 ]               [ (empty / < min) ]
        //        /     |     \             /
        //      C0     C1     C2          C3
        //
        // AFTER RIGHT ROTATION
        // 1. Parent key P moves DOWN into Right Child.
        // 2. Largest key K2 of Left Sibling moves UP to replace P in Parent.
        // 3. Rightmost child pointer C2 of Left Sibling shifts to become leftmost child of Right Child.
        //
        //                      Parent
        //                   [ ... | K2 | ... ]
        //                        /      \
        //          Left Sibling            Right Child (Balanced)
        //            [ K1 ]                  [ P ]
        //           /      \                /     \
        //         C0        C1            C2       C3
        void TransactedFileBTree::Node::RotateRight (
                ui32 index,
                Node *left,
                Node *right) {
            // 1. Demote separator entries[index] into right[0] with right's old left child.
            Entry demoted = entries[index];
            demoted.rightOffset = right->leftOffset;
            demoted.right = right->left;
            right->InsertEntry (demoted, 0);
            // 2. Move left's rightmost child to right->left.
            ui32 lastIndex = left->count - 1;
            right->SetLeft (left->entries[lastIndex].right, left->entries[lastIndex].rightOffset);
            // 3. Promote left[lastIndex] into entries[index] (keeping right as its right child).
            Entry promoted = left->entries[lastIndex];
            promoted.rightOffset = right->GetOffset ();
            promoted.right = right;
            left->RemoveEntry (lastIndex);
            SetEntry (index, promoted);
        }

        // BEFORE (Left Child Underflows; Right Sibling has spare key K3)
        //                      Parent
        //                   [ ... | P | ... ]
        //                        /     \
        //     Left Child (Underflow)     Right Sibling
        //      [ (empty / < min) ]        [ K3 | K4 ]
        //                         \      /     |     \
        //                         C0   C1     C2     C3
        //
        // AFTER LEFT ROTATION
        // 1. Parent key P moves DOWN into Left Child.
        // 2. Smallest key K3 of Right Sibling moves UP to replace P in Parent.
        // 3. Leftmost child pointer C1 of Right Sibling shifts to become rightmost child of Left Child.
        //
        //                      Parent
        //                   [ ... | K3 | ... ]
        //                        /      \
        //       Left Child (Balanced)      Right Sibling
        //             [ P ]                   [ K4 ]
        //            /     \                 /      \
        //          C0       C1             C2        C3
        void TransactedFileBTree::Node::RotateLeft (
                ui32 index,
                Node *left,
                Node *right) {
            // 1. Demote separator entries[index] to end of left with right's old left child.
            Entry demoted = entries[index];
            demoted.rightOffset = right->leftOffset;
            demoted.right = right->left;
            left->InsertEntry (demoted, left->count);
            // 2. Move right[0]'s right child to right->left.
            Entry promoted = right->entries[0];
            right->SetLeft (promoted.right, promoted.rightOffset);
            // 3. Promote right[0] into entries[index] (keeping right as its right child).
            promoted.rightOffset = right->GetOffset ();
            promoted.right = right;
            right->RemoveEntry (0);
            SetEntry (index, promoted);
        }

        // BEFORE MERGE (Right Child underflows; Left Sibling cannot spare a key)
        //                          Parent
        //                    [ ... | P | Q | ... ]
        //                         /     \     \
        //            Left Sibling         Right Child (Underflow)
        //              [ K1 ]               [ (empty) ]
        //             /      \             /
        //           C0        C1         C2
        //
        // DURING MERGE
        // 1. Pull separator key P DOWN from Parent into Left Sibling.
        // 2. Append all remaining keys and child pointers of Right Child into Left Sibling.
        // 3. Free/reclaim the now-empty Right Child node and shift Parent's remaining keys/pointers left.
        //
        // AFTER MERGE
        //                          Parent
        //                      [ ... | Q | ... ]
        //                           /     \
        //               Merged Node         ...
        //              [ K1 | P ]
        //             /     |     \
        //           C0     C1     C2
        void TransactedFileBTree::Node::Merge (
                ui32 index,
                Node *left,
                Node *right) {
            assert (left->count + right->count < btree->header.entriesPerNode);
            Entry separator = entries[index];
            RemoveEntry (index);
            separator.rightOffset = right->leftOffset;
            separator.right = right->left;
            right->SetLeft (nullptr, 0);
            left->InsertEntry (separator, left->count);
            left->MoveEntries (right, 0, left->count, right->count);
            Object::FreeObject (file, right->GetOffset ());
            right->SetDestroyed ();
            right->Release ();
        }

        void TransactedFileBTree::Node::SetLeft (
                Node *child,
                TransactedFile::AddressType offset) {
            if (left != nullptr) {
                left->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
            }
            left = child;
            leftOffset = offset;
            if (left != nullptr) {
                left->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
            }
        }

        void TransactedFileBTree::Node::SetEntry (
                ui32 index,
                const Entry &entry) {
            entries[index].Unsubscribe (this, btree->header.IsValueIsObject ());
            entries[index] = entry;
            entries[index].Subscribe (this, btree->header.IsValueIsObject ());
            SetDirty ();
        }

        void TransactedFileBTree::Node::InsertEntry (
                const Entry &entry,
                ui32 index) {
            std::memmove (entries + index + 1, entries + index, (count++ - index) * sizeof (Entry));
            entries[index] = entry;
            entries[index].Subscribe (this, btree->header.IsValueIsObject ());
            SetDirty ();
        }

        void TransactedFileBTree::Node::RemoveEntry (ui32 index) {
            entries[index].Unsubscribe (this, btree->header.IsValueIsObject ());
            std::memmove (entries + index, entries + index + 1, (--count - index) * sizeof (Entry));
            std::memset (entries + count, 0, sizeof (Entry));
            SetDirty ();
        }

        void TransactedFileBTree::Node::MoveEntries (
                Node *src,
                ui32 srcIndex,
                ui32 dstIndex,
                ui32 moveCount) {
            if (THEKOGANS_UTIL_LIKELY (moveCount > 0)) {
                assert (srcIndex + moveCount <= src->count);
                assert (dstIndex <= count);
                assert (count + moveCount <= btree->header.entriesPerNode);
                // 1. Open space in `this` at `dstIndex`.
                if (dstIndex < count) {
                    std::memmove (
                        entries + dstIndex + moveCount,
                        entries + dstIndex,
                        (count - dstIndex) * sizeof (Entry));
                }
                // 2. Transfer `moveCount` entries and re-bind subscriptions from `src` to `this`.
                for (ui32 i = 0; i < moveCount; ++i) {
                    src->entries[srcIndex + i].Unsubscribe (src, btree->header.IsValueIsObject ());
                    entries[dstIndex + i] = src->entries[srcIndex + i];
                    entries[dstIndex + i].Subscribe (this, btree->header.IsValueIsObject ());
                }
                count += moveCount;
                // 3. Close the gap in `src` and zero out the vacated tail slots.
                ui32 tailCount = src->count - (srcIndex + moveCount);
                if (tailCount > 0) {
                    std::memmove (
                        src->entries + srcIndex,
                        src->entries + srcIndex + moveCount,
                        tailCount * sizeof (Entry));
                }
                src->count -= moveCount;
                std::memset (src->entries + src->count, 0, moveCount * sizeof (Entry));
                src->SetDirty ();
                SetDirty ();
            }
        }

        void TransactedFileBTree::Node::Harakiri () {
            this->~Node ();
            btree->nodeAllocator->Free (this);
        }

        void TransactedFileBTree::Node::Reset () {
            Node *l = left;
            SetLeft (nullptr, 0);
            if (l != nullptr) {
                l->Release ();
            }
            for (ui32 i = 0; i < count; ++i) {
                entries[i].Reset (this, btree->header.IsValueIsObject ());
            }
            count = 0;
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
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "offset @" THEKOGANS_UTIL_UI64_FORMAT "is not pointing to %s (%s)" ,
                        offset,
                        TYPE.c_str (),
                        range.context.type.c_str ());
                }
            }
        }

        void TransactedFileBTree::Node::SetDirty () {
            THEKOGANS_UTIL_ASSERT (!IsDestroyed (),
                "Calling TransactedFile::Object::SetDirty on a destroyed object.");
            if (THEKOGANS_UTIL_LIKELY (file != nullptr)) {
                if (THEKOGANS_UTIL_UNLIKELY (!file->CheckCanMutate ())) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Structural Logic Error: TransactedFile::Object::SetDirty can only be "
                        "called durring transaction in progress and allocation phases (%s).",
                        TransactedFile::Transaction::phaseName[file->GetTransactionPhase ()]);
                }
                btree->Producer<TransactedFileTransactionEvents>::Subscribe (*this);
                Serializable::SetDirty ();
            }
        }

        void TransactedFileBTree::Node::GetBlockMap (
                BlockMap &allocatedBlockMap,
                BlockMap &referencedBlockMap) {
            RecordReference (file, offset, allocatedBlockMap);
            if (btree->header.IsValueIsObject ()) {
                for (ui32 i = 0; i < count; ++i) {
                    TransactedFile::Object::SharedPtr value = GetValue (i);
                    if (value != nullptr) {
                        value->GetBlockMap (allocatedBlockMap, referencedBlockMap);
                    }
                }
            }
            for (ui32 i = 0; i <= count; ++i) {
                Node *child = GetChild (i);
                if (child != nullptr) {
                    child->GetBlockMap (allocatedBlockMap, referencedBlockMap);
                }
            }
        }

        THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (thekogans::util::TransactedFileBTree, 1, 0)

        TransactedFileBTree::TransactedFileBTree (
                const SerializableHeader &keyContext,
                const SerializableHeader &valueContext,
                bool valueIsObject,
                std::size_t entriesPerNode) :
                header (
                    keyContext,
                    valueContext,
                    valueIsObject ? Header::FLAGS_VALUE_IS_OBJECT : 0,
                    (ui32)entriesPerNode),
                keyFactory (Key::GetTypeFactory (keyContext.type)),
                valueFactory (TransactedFile::Object::GetTypeFactory (valueContext.type)) {
            if (THEKOGANS_UTIL_UNLIKELY ((!keyContext.NeedType () && !Key::IsType (keyContext.type)) ||
                    (!valueContext.NeedType () && !Serializable::IsType (valueContext.type)) || entriesPerNode < 3)) {
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
                RWLockGuard guard (rwLock, true);
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
                RWLockGuard guard (rwLock, false);
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
                Node::Entry entry{key.Get (), valueOffset, value.Get ()};
                Node::InsertResult result = root->Insert (entry);
                if (THEKOGANS_UTIL_UNLIKELY (result == Node::Overflow)) {
                    // The path to the leaf node is full.
                    // Create a new root node and make the entry
                    // its first.
                    Node *newRoot = Node::Alloc (this, file, 0);
                    newRoot->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                    root->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                    newRoot->SetLeft (root, root->GetOffset ());
                    newRoot->InsertEntry (entry, 0);
                    root = newRoot;
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
                RWLockGuard guard (rwLock, false);
                bool removed = root->Remove (key);
                if (removed) {
                    if (THEKOGANS_UTIL_UNLIKELY (root->IsEmpty () && root->GetChild (0) != nullptr)) {
                        Node *newRoot = root->GetChild (0);
                        newRoot->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
                        root->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
                        root->SetLeft (nullptr, 0);
                        Object::FreeObject (file, root->GetOffset ());
                        root->SetDestroyed ();
                        root->Release ();
                        root = newRoot;
                    }
                }
                return removed;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        TransactedFileBTree::Iterator TransactedFileBTree::FindFirst (Key::SharedPtr prefix) {
            if (THEKOGANS_UTIL_LIKELY (prefix == nullptr || prefix->IsKindOf (header.keyContext.type))) {
                RWLockGuard guard (rwLock, true);
                Iterator it (prefix);
                std::size_t matchDepth = 0;
                Node *node = root;
                if (THEKOGANS_UTIL_LIKELY (node != nullptr && node->count > 0)) {
                    if (THEKOGANS_UTIL_UNLIKELY (it.prefix == nullptr)) {
                        do {
                            it.parents.push_back (Iterator::NodeIndex (node, 0));
                            node = node->GetChild (0);
                        } while (node != nullptr);
                        matchDepth = it.parents.size ();
                        it.finished = false;
                    }
                    else {
                        while (node != nullptr) {
                            ui32 index = 0;
                            bool found = node->FindFirstPrefix (*it.prefix, index);
                            it.parents.push_back (Iterator::NodeIndex (node, index));
                            if (found) {
                                matchDepth = it.parents.size ();
                                it.finished = false;
                            }
                            node = node->GetChild (index);
                        }
                    }
                }
                if (!it.finished) {
                    it.parents.resize (matchDepth);
                    it.node = it.parents.back ();
                    it.parents.pop_back ();
                }
                return it;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        TransactedFileBTree::Iterator TransactedFileBTree::FindLast (Key::SharedPtr prefix) {
            if (THEKOGANS_UTIL_LIKELY (prefix == nullptr || prefix->IsKindOf (header.keyContext.type))) {
                RWLockGuard guard (rwLock, true);
                Iterator it (prefix);
                std::size_t matchDepth = 0;
                Node *node = root;
                if (THEKOGANS_UTIL_LIKELY (node != nullptr && node->count > 0)) {
                    if (THEKOGANS_UTIL_UNLIKELY (it.prefix == nullptr)) {
                        do {
                            it.parents.push_back (Iterator::NodeIndex (node, node->count));
                            node = node->GetChild (node->count);
                        } while (node != nullptr);
                        matchDepth = it.parents.size ();
                        it.finished = false;
                    }
                    else {
                        while (node != nullptr) {
                            ui32 index = 0;
                            bool found = node->FindLastPrefix (*it.prefix, index);
                            if (found) {
                                it.parents.push_back (Iterator::NodeIndex (node, index + 1));
                                matchDepth = it.parents.size ();
                                it.finished = false;
                                node = node->GetChild (index + 1);
                            }
                            else {
                                it.parents.push_back (Iterator::NodeIndex (node, index));
                                node = node->GetChild (index);
                            }
                        }
                    }
                }
                if (!it.finished) {
                    it.parents.resize (matchDepth);
                    it.node = it.parents.back ();
                    it.parents.pop_back ();
                    --it.node.second;
                }
                return it;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        TransactedFileBTree::Iterator TransactedFileBTree::LowerBound (Key::SharedPtr prefix) {
            if (THEKOGANS_UTIL_LIKELY (prefix == nullptr || prefix->IsKindOf (header.keyContext.type))) {
                RWLockGuard guard (rwLock, true);
                Iterator it (nullptr);
                std::size_t matchDepth = 0;
                Node *node = root;
                if (THEKOGANS_UTIL_LIKELY (node != nullptr && node->count > 0)) {
                    if (THEKOGANS_UTIL_UNLIKELY (prefix == nullptr)) {
                        do {
                            it.parents.push_back (Iterator::NodeIndex (node, 0));
                            node = node->GetChild (0);
                        } while (node != nullptr);
                        matchDepth = it.parents.size ();
                        it.finished = false;
                    }
                    else {
                        while (node != nullptr) {
                            ui32 index = 0;
                            bool found = node->FindLowerBound (*prefix, index);
                            it.parents.push_back (Iterator::NodeIndex (node, index));
                            if (found) {
                                matchDepth = it.parents.size ();
                                it.finished = false;
                            }
                            node = node->GetChild (index);
                        }
                    }
                }
                if (!it.finished) {
                    it.parents.resize (matchDepth);
                    it.node = it.parents.back ();
                    it.parents.pop_back ();
                }
                return it;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        TransactedFileBTree::Iterator TransactedFileBTree::UpperBound (Key::SharedPtr prefix) {
            if (THEKOGANS_UTIL_LIKELY (prefix == nullptr || prefix->IsKindOf (header.keyContext.type))) {
                RWLockGuard guard (rwLock, true);
                Iterator it (nullptr);
                std::size_t matchDepth = 0;
                Node *node = root;
                if (THEKOGANS_UTIL_LIKELY (node != nullptr && node->count > 0 && prefix != nullptr)) {
                    while (node != nullptr) {
                        ui32 index = 0;
                        bool found = node->FindUpperBound (*prefix, index);
                        it.parents.push_back (Iterator::NodeIndex (node, index));
                        if (found) {
                            matchDepth = it.parents.size ();
                            it.finished = false;
                        }
                        node = node->GetChild (index);
                    }
                }
                if (!it.finished) {
                    it.parents.resize (matchDepth);
                    it.node = it.parents.back ();
                    it.parents.pop_back ();
                }
                return it;
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
                    Node::SlotsPerPage (header.entriesPerNode),
                    Node::TLCSize (header.entriesPerNode)));
            root = Node::Alloc (this, file, header.rootOffset);
            root->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
        }

        void TransactedFileBTree::Write (Serializer &serializer) {
            header.rootOffset = root->GetOffset ();
            serializer << MAGIC32 << header;
        }

        void TransactedFileBTree::Init (
                TransactedFile *file,
                TransactedFile::AddressType offset) {
            Object::Init (file, offset);
            if (offset == 0) {
                nodeAllocator.Reset (
                    new SlabAllocatorImpl (
                        Node::Size (header.entriesPerNode),
                        Node::SlotsPerPage (header.entriesPerNode),
                        Node::TLCSize (header.entriesPerNode)));
                root = Node::Alloc (this, file, 0);
                root->Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
            }
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

        void TransactedFileBTree::GetBlockMap (
                BlockMap &allocatedBlockMap,
                BlockMap &referencedBlockMap) {
            RecordReference (file, offset, allocatedBlockMap);
            if (offset != 0) {
                assert (root != nullptr);
                root->GetBlockMap (allocatedBlockMap, referencedBlockMap);
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
