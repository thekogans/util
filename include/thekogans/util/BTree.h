#if !defined (__thekogans_util_BTree_h)
#define __thekogans_util_BTree_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/ByteSwap.h"
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
                ui64 nodeCounter;
                ui64 rootNumber;
                Header (ui32 entriesPerNode_ = DEFAULT_ENTRIES_PER_NODE) :
                    entriesPerNode (entriesPerNode_),
                    nodeCounter (2),
                    rootNumber (1) {}
            } header;
            struct _LIB_THEKOGANS_UTIL_DECL Node : public RefCounted {
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Node)
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                BTree &btree;
                ui64 number;
                ui32 entriesPerNode;
                ui32 count;
                ui64 leftNumber;
                SharedPtr leftNode;
                struct Entry {
                    Key key;
                    ui64 rightNumber;
                    SharedPtr rightNode;

                    Entry (const Key &key_ = Key (NIDX64, NIDX64)) :
                        key (key_),
                        rightNumber (NIDX64) {}
                };
                std::vector<Entry> entries;

                Node (
                    BTree &btree_,
                    ui64 number_ = NIDX64);

                void Save ();
                void Delete ();
                std::string GetPath () const;
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
            };
            Node::SharedPtr root;

        public:
            BTree (
                const std::string &path_,
                ui32 entriesPerNode = DEFAULT_ENTRIES_PER_NODE);

            Key Search (const Key &key);
            void Add (const Key &key);
            void Delete (const Key &key);

        private:
            bool Insert (
                Node::Entry &entry,
                Node::SharedPtr node);
            bool Remove (
                const Key &key,
                Node::SharedPtr node);
            void RestoreBalance (
                Node::SharedPtr node,
                ui32 index);
            void RotateRight (
                Node::SharedPtr node,
                ui32 index,
                Node::SharedPtr left,
                Node::SharedPtr right);
            void RotateLeft (
                Node::SharedPtr node,
                ui32 index,
                Node::SharedPtr left,
                Node::SharedPtr right);
            void Merge (
                Node::SharedPtr node,
                ui32 index,
                Node::SharedPtr left,
                Node::SharedPtr right);
            void Save ();
            void SetRoot (Node::SharedPtr node);

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
