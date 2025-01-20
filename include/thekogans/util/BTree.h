#if !defined (__thekogans_util_BTree_h)
#define __thekogans_util_BTree_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/File.h"

namespace thekogans {
    namespace util {

        struct _LIB_THEKOGANS_UTIL_DECL BTree {
            // {size, offset}
            using Key = std::pair<ui64, ui64>;

            enum {
                DEFAULT_ENTRIES_PER_NODE = 32
            };

        private:
            File &file;
            ui64 offset;
            struct _LIB_THEKOGANS_UTIL_DECL Header {
                ui32 magic;
                Endian endian;
                ui64 nodeCounter;
                ui32 entriesPerNode;
                ui64 rootOffset;
                Header (ui32 entriesPerNode_ = DEFAULT_ENTRIES_PER_NODE) :
                    magic (MAGIC32),
                    endian (HostEndian),
                    nodeCounter (1),
                    entriesPerNode (entriesPerNode_),
                    rootOffset (NIDX64) {}
            } header;
            struct _LIB_THEKOGANS_UTIL_DECL Node {
                BTree &btree;
                ui64 offset;
                ui32 count;
                ui64 leftOffset;
                Node *leftNode;
                bool dirty;
                struct Entry {
                    Key key;
                    ui64 rightOffset;
                    Node *rightNode;

                    Entry (const Key &key_ = Key (NIDX64, NIDX64)) :
                        key (key_),
                        rightOffset (NIDX64),
                        rightNode (nullptr) {}
                };
                Entry entries[1];

                Node (
                    BTree &btree_,
                    ui64 offset_ = NIDX64);
                ~Node ();

                static std::size_t Size (ui32 entriesPerNode);
                static Node *Alloc (
                    BTree &btree,
                    ui64 offset = NIDX64);
                static void Free (
                    BTree &btree,
                    Node *node);

                Node *GetChild (ui32 index);
                bool Search (
                    const Key &key,
                    ui32 &index) const;
                void Split (
                    Node *node,
                    ui32 index);
                void Concatenate (Node *node);
                inline void Concatenate (Entry &entry) {
                    InsertEntry (entry, count);
                }
                void InsertEntry (
                    const Entry &entry,
                    ui32 index);
                void RemoveEntry (ui32 index);
                inline bool IsEmpty () const {
                    return count == 0;
                }
                inline bool IsFull () const {
                    return count == btree.header.entriesPerNode;
                }
                inline bool IsPoor () const {
                    return count < btree.header.entriesPerNode / 2;
                }
                inline bool IsPlentiful () const {
                    return count > btree.header.entriesPerNode / 2;
                }
            } *root;
            bool dirty;

        public:
            BTree (
                File &file_,
                ui64 offset_ = NIDX64,
                ui32 entriesPerNode = DEFAULT_ENTRIES_PER_NODE);
            ~BTree ();

            Key Search (const Key &key);
            void Add (const Key &key);
            void Delete (const Key &key);

        private:
            bool Insert (
                Node::Entry &entry,
                Node *node);
            bool Remove (
                const Key &key,
                Node *node);
            void RestoreBalance (
                Node *node,
                ui32 index);
            void RotateRight (
                Node *node,
                ui32 index,
                Node *left,
                Node *right);
            void RotateLeft (
                Node *node,
                ui32 index,
                Node *left,
                Node *right);
            void Merge (
                Node *node,
                ui32 index,
                Node *left,
                Node *right);
            void SetRoot (Node *node);
            void DeleteNode (Node *node);

            friend Serializer &operator << (
                Serializer &serializer,
                const Node::Entry &entry);
            friend Serializer &operator >> (
                Serializer &serializer,
                Node::Entry &header);
            friend Serializer &operator << (
                Serializer &serializer,
                const Header &header);
            friend Serializer &operator >> (
                Serializer &serializer,
                Header &header);
        };

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_BTree_h
