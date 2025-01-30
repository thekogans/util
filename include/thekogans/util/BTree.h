#if !defined (__thekogans_util_BTree_h)
#define __thekogans_util_BTree_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/GUID.h"
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
            std::string path;
            struct _LIB_THEKOGANS_UTIL_DECL Header {
                ui32 entriesPerNode;
                GUID rootId;
                Header (ui32 entriesPerNode_ = DEFAULT_ENTRIES_PER_NODE) :
                    entriesPerNode (entriesPerNode_),
                    rootId (GUID::Empty) {}
            } header;
            struct _LIB_THEKOGANS_UTIL_DECL Node {
                BTree &btree;
                GUID id;
                ui32 count;
                GUID leftId;
                Node *leftNode;
                struct Entry {
                    Key key;
                    GUID rightId;
                    Node *rightNode;

                    Entry (const Key &key_ = Key (NIDX64, NIDX64)) :
                        key (key_),
                        rightId (GUID::Empty),
                        rightNode (nullptr) {}
                };
                Entry entries[1];

                Node (
                    BTree &btree_,
                    const GUID &id_ = GUID::FromRandom ());
                ~Node ();

                static std::size_t Size (ui32 entriesPerNode);
                static Node *Alloc (
                    BTree &btree,
                    const GUID &id_ = GUID::FromRandom ());
                static void Free (
                    BTree &btree,
                    Node *node);
                static void Delete (
                    BTree &btree,
                    Node *node);

                void Save ();
                Node *GetChild (ui32 index);
                bool Search (
                    const Key &key,
                    ui32 &index) const;
                void Split (
                    Node *node,
                    ui32 index);
                void Concatenate (Node *node);
                inline void Concatenate (const Entry &entry) {
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

        public:
            BTree (
                const std::string &path_,
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
            void Save ();
            void SetRoot (Node *node);

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
