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

#if !defined (__thekogans_util_FileAllocator_h)
#define __thekogans_util_FileAllocator_h

#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/File.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/BlockAllocator.h"
#include "thekogans/util/Singleton.h"

namespace thekogans {
    namespace util {

        /// \struct FileAllocator FileAllocator.h thekogans/util/FileAllocator.h
        ///
        /// \brief
        /// FileAllocator

        struct _LIB_THEKOGANS_UTIL_DECL FileAllocator : public RefCounted {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (FileAllocator)

            struct _LIB_THEKOGANS_UTIL_DECL LockedFilePtr {
            private:
                FileAllocator &allocator;

            public:
                LockedFilePtr (FileAllocator &allocator_) :
                        allocator (allocator_) {
                    allocator.spinLock.Acquire ();
                }
                ~LockedFilePtr () {
                    allocator.spinLock.Release ();
                }

                inline File &operator * () const {
                    return allocator.file;
                }
                inline File *operator -> () const {
                    return &allocator.file;
                }

                /// \brief
                /// LockedFilePtr is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (LockedFilePtr)
            };

            struct _LIB_THEKOGANS_UTIL_DECL BlockInfo {
            private:
                File &file;
                ui64 offset;
                struct _LIB_THEKOGANS_UTIL_DECL Header {
                    ui32 flags;
                    ui64 size;
                    ui64 nextOffset;

                    enum {
                        SIZE =
                        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            UI32_SIZE + // magic
                        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            UI32_SIZE +
                            UI64_SIZE /*+
                            UI64_SIZE nextOffset is ommited because it shares space with user data.*/
                    };

                    Header (
                        ui32 flags_ = 0,
                        ui64 size_ = 0,
                        ui64 nextOffset_ = 0) :
                        flags (flags_),
                        size (size_),
                        nextOffset (nextOffset_) {}

                    inline bool IsFree () const {
                        return Flags32 (flags).Test (FLAGS_FREE);
                    }
                    inline void SetFree (bool isFree) {
                        Flags32 (flags).Set (FLAGS_FREE, isFree);
                    }
                    inline bool IsFixed () const {
                        return Flags32 (flags).Test (FLAGS_FIXED);
                    }
                    inline void SetFixed (bool fixed) {
                        Flags32 (flags).Set (FLAGS_FIXED, fixed);
                    }

                    void Read (
                        File &file,
                        ui64 offset);
                    void Write (
                        File &file,
                        ui64 offset);
                } header;
                struct _LIB_THEKOGANS_UTIL_DECL Footer {
                    ui32 flags;
                    ui64 size;

                    enum {
                        SIZE =
                        #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            UI32_SIZE + // magic
                        #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                            UI32_SIZE +
                            UI64_SIZE
                    };

                    Footer (
                        ui32 flags_ = 0,
                        ui64 size_ = 0) :
                        flags (flags_),
                        size (size_) {}

                    inline bool IsFree () const {
                        return Flags32 (flags).Test (FLAGS_FREE);
                    }
                    inline void SetFree (bool isFree) {
                        Flags32 (flags).Set (FLAGS_FREE, isFree);
                    }
                    inline bool IsFixed () const {
                        return Flags32 (flags).Test (FLAGS_FIXED);
                    }
                    inline void SetFixed (bool fixed) {
                        Flags32 (flags).Set (FLAGS_FIXED, fixed);
                    }

                    void Read (
                        File &file,
                        ui64 offset);
                    void Write (
                        File &file,
                        ui64 offset);
                } footer;

            public:
                enum {
                    FLAGS_FREE = 1,
                    FLAGS_FIXED = 2,
                    HEADER_SIZE = Header::SIZE,
                    FOOTER_SIZE = Footer::SIZE,
                    SIZE = HEADER_SIZE + FOOTER_SIZE
                };

                BlockInfo (
                    File &file_,
                    ui64 offset_ = 0,
                    ui32 flags = 0,
                    ui64 size = 0,
                    ui64 nextOffset = 0) :
                    file (file_),
                    offset (offset_),
                    header (flags, size, nextOffset),
                    footer (flags, size) {}

                inline ui64 GetOffset () const {
                    return offset;
                }
                inline void SetOffset (ui64 offset_) {
                    offset = offset_;
                }
                inline bool IsFirst () const {
                    return GetOffset () == FileAllocator::Header::SIZE + Header::SIZE;
                }
                inline bool IsLast () const {
                    return GetOffset () + GetSize () + Footer::SIZE == file.GetSize ();
                }

                inline bool IsFree () const {
                    return header.IsFree ();
                }
                inline void SetFree (bool free) {
                    header.SetFree (free);
                    footer.SetFree (free);
                }
                inline bool IsFixed () const {
                    return header.IsFixed ();
                }
                inline void SetFixed (bool fixed) {
                    header.SetFixed (fixed);
                    footer.SetFixed (fixed);
                }

                inline ui64 GetSize () const {
                    return header.size;
                }
                inline void SetSize (ui64 size) {
                    header.size = size;
                    footer.size = size;
                }

                inline ui64 GetNextOffset () const {
                    return header.nextOffset;
                }
                inline void SetNextOffset (ui64 nextOffset) {
                    header.nextOffset = nextOffset;
                }

                void Prev (BlockInfo &prev);
                void Next (BlockInfo &next);

                void Read ();
                void Write ();

                friend bool operator != (
                    const Header &header,
                    const Footer &footer);

                /// \brief
                /// BlockInfo is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BlockInfo)
            };

            struct _LIB_THEKOGANS_UTIL_DECL BlockBuffer : public Buffer {
            private:
                FileAllocator &allocator;
                ui64 offset;
                BlockInfo block;

            public:
                BlockBuffer (
                    FileAllocator &allocator_,
                    ui64 offset_,
                    std::size_t length = 0);

                std::size_t Read (
                    std::size_t blockOffset = 0,
                    std::size_t length = 0);
                std::size_t Write (
                    std::size_t blockOffset = 0,
                    std::size_t length = 0);

                /// \brief
                /// BlockBuffer is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BlockBuffer)
            };

            /// \struct BlockAllocator::Pool BlockAllocator.h thekogans/util/BlockAllocator.h
            ///
            /// \brief
            /// Each instance of a FileAllocator attached to a particular
            /// file should be treated as a singleton. Use Pool to recycle
            /// and reuse file allocators based on a given path.
            struct _LIB_THEKOGANS_UTIL_DECL Pool : public Singleton<Pool> {
            private:
                /// \brief
                /// FileAllocator map type (keyed on path).
                using Map = std::map<std::string, FileAllocator::SharedPtr>;
                /// \brief
                /// FileAllocator map.
                Map map;
                /// \brief
                /// Synchronization lock.
                SpinLock spinLock;

            public:
                /// \brief
                /// ctor.
                Pool () {}

                /// \brief
                /// Given a block size, return a matching block allocator.
                /// If we don't have one, create it.
                /// \param[in] blockSize Block size.
                /// \return FileAllocator matching the given path.
                FileAllocator::SharedPtr GetFileAllocator (
                    const std::string &path,
                    std::size_t blockSize = 0,
                    std::size_t blocksPerPage = BTree::DEFAULT_ENTRIES_PER_NODE,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

                /// \brief
                /// Pool is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Pool)
            };

        private:
            SimpleFile file;
            struct Header {
                enum {
                    FLAGS_FIXED = 1
                };
                ui32 flags;
                ui32 blockSize;
                ui64 fixedFreeListOffset;
                ui64 btreeOffset;
                ui64 rootOffset;

                enum {
                    SIZE =
                        UI32_SIZE + // magic
                        UI32_SIZE +
                        UI32_SIZE +
                        UI64_SIZE +
                        UI64_SIZE +
                        UI64_SIZE
                };

                Header (
                    ui32 flags_ = 0,
                    ui32 blockSize_ = 0) :
                    flags (flags_),
                    blockSize (blockSize_),
                    fixedFreeListOffset (0),
                    btreeOffset (0),
                    rootOffset (0) {}

                inline bool IsFixed () const {
                    return Flags32 (flags).Test (FLAGS_FIXED);
                }
            } header;
            Allocator::SharedPtr blockAllocator;
            Allocator::SharedPtr fixedAllocator;
            /// \struct BTree BTree.h thekogans/util/BTree.h
            ///
            /// \brief
            struct BTree : public RefCounted {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (BTree)

                /// \brief
                /// Keys are structured on block {size, offset}
                using Key = std::pair<ui64, ui64>;
                static const std::size_t KEY_SIZE = UI64_SIZE + UI64_SIZE;

                enum {
                    /// \brief
                    /// Default number of entries per node.
                    DEFAULT_ENTRIES_PER_NODE = 32
                };

            private:
                /// \brief
                FileAllocator &fileAllocator;
                ui64 offset;
                /// \struct BTree::Header BTree.h thekogans/util/BTree.h
                ///
                /// \brief
                /// Header contains global btree info.
                struct Header {
                    /// \brief
                    /// Entries per node.
                    ui32 entriesPerNode;
                    /// \brief
                    /// Root node offset.
                    ui64 rootOffset;

                    enum {
                        SIZE =
                            UI32_SIZE + // magic
                            UI32_SIZE +
                            UI64_SIZE
                    };

                    /// \brief
                    /// ctor.
                    /// \param[in] entriesPerNode_ Entries per node.
                    Header (ui32 entriesPerNode_ = DEFAULT_ENTRIES_PER_NODE) :
                        entriesPerNode (entriesPerNode_),
                        rootOffset (0) {}
                } header;
                Allocator::SharedPtr nodeAllocator;
                /// \struct BTree::Node BTree.h thekogans/util/BTree.h
                ///
                /// \brief
                /// BTree nodes store sorted keys and pointers to children nodes.
                struct Node {
                    /// \brief
                    /// BTree to whch this node belongs.
                    BTree &btree;
                    /// \brief
                    /// Node block offset.
                    ui64 offset;
                    /// \brief
                    /// Count of entries.
                    ui32 count;
                    /// \brief
                    /// Left most child node offset.
                    ui64 leftOffset;
                    /// \brief
                    /// Left most child node.
                    Node *leftNode;
                    /// \struct BTree::Node::Entry BTree.h thekogans/util/BTree.h
                    ///
                    /// \brief
                    /// Node entries contain keys and right (grater then) children.
                    struct Entry {
                        /// \brief
                        /// Entry key.
                        Key key;
                        /// \brief
                        /// Right child node offset.
                        ui64 rightOffset;
                        /// \brief
                        /// Right child node.
                        Node *rightNode;

                        /// \brief
                        /// ctor.
                        /// \param[in] key_ Entry key.
                        Entry (const Key &key_ = Key (0, 0)) :
                            key (key_),
                            rightOffset (0),
                            rightNode (0) {}
                    };
                    /// \brief
                    /// Entry array. The rest of the entries are
                    /// allocated when the node is allocated.
                    Entry entries[1];

                    /// \brief
                    /// ctor.
                    /// \param[in] btree_ BTree to whch this node belongs.
                    /// \param[in] offset_ Node offset.
                    Node (
                        BTree &btree_,
                        ui64 offset_ = 0);
                    /// \brief
                    /// dtor.
                    ~Node ();

                    /// \brief
                    /// Given the number of entries, return the node file size in bytes.
                    static std::size_t FileSize (std::size_t entriesPerNode);
                    /// \brief
                    /// Given the number of entries, return the node size in bytes.
                    static std::size_t Size (std::size_t entriesPerNode);
                    /// \brief
                    /// Allocate a node.
                    /// \param[in] btree BTree to which this node belongs.
                    /// \param[in] offset Node offset.
                    static Node *Alloc (
                        BTree &btree,
                        ui64 offset = 0);
                    /// \brief
                    /// Free the given node.
                    /// \param[in] node Node to free.
                    static void Free (Node *node);
                    /// \brief
                    /// Delete the file associated with and free the given empty node.
                    /// If the node is not empty, throw exception.
                    /// \param[in] node Node to delete.
                    static void Delete (Node *node);

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
                    /// \brief
                    /// Insert the given entry at the given index.
                    void InsertEntry (
                        const Entry &entry,
                        ui32 index);
                    /// \brief
                    /// Remove the entry at the given index.
                    void RemoveEntry (ui32 index);
                    /// \brief
                    /// Return true if the node is empty.
                    inline bool IsEmpty () const {
                        return count == 0;
                    }
                    /// \brief
                    /// Return true if the node is full.
                    inline bool IsFull () const {
                        return count == btree.header.entriesPerNode;
                    }
                    /// \brief
                    /// Return true if less than half the node is occupied.
                    inline bool IsPoor () const {
                        return count < btree.header.entriesPerNode / 2;
                    }
                    /// \brief
                    /// Return true if more than half the node is occupied.
                    inline bool IsPlentiful () const {
                        return count > btree.header.entriesPerNode / 2;
                    }
                } *root;

            public:
                /// \brief
                /// ctor.
                BTree (
                    FileAllocator &fileAllocator_,
                    ui64 offset_,
                    std::size_t entriesPerNode = DEFAULT_ENTRIES_PER_NODE,
                    std::size_t nodesPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
                /// \brief
                /// dtor.
                ~BTree ();

                inline ui64 GetOffset () const {
                    return offset;
                }

                /// \brief
                /// Find the given key in the btree.
                /// \param[in] key Key to find.
                /// \return If found the given key will be returned.
                /// If not found, return the nearest larger key.
                Key Search (const Key &key);
                /// \brief
                /// Add the given key to the btree.
                /// \param[in] key Key to add.
                /// NOTE: Duplicate keys are ignored.
                void Add (const Key &key);
                /// \brief
                /// Delete the given key from the btree.
                /// \param[in] key Key whose entry to delete.
                /// \return true == entry deleted. false == entry not found.
                bool Delete (const Key &key);
                /// \brief
                /// Flush the node cache (used in tight memory situations).
                void Flush ();

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

                friend struct FileAllocator;

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

                /// \brief
                /// BTree is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BTree)
            };
            BTree::SharedPtr btree;
            SpinLock spinLock;

        public:
            enum {
                /// \brief
                /// Minimum user data size.
                MIN_USER_DATA_SIZE = 32, // BlockInfo::Header::nextOffset
                MIN_USER_DATA_OFFSET = Header::SIZE + BlockInfo::HEADER_SIZE,
                /// \brief
                /// Minimum block size.
                MIN_BLOCK_SIZE = sizeof (BlockInfo::SIZE) + MIN_USER_DATA_SIZE
            };

            FileAllocator (
                const std::string &path,
                std::size_t blockSize = 0,
                std::size_t blocksPerPage = BTree::DEFAULT_ENTRIES_PER_NODE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

            inline bool IsFixed () const {
                // header.flags is read only after ctor so no need to lock.
                return header.IsFixed ();
            }
            inline std::size_t GetBlockSize () const {
                // header.blockSize is read only after ctor so no need to lock.
                return header.blockSize;
            }
            ui64 GetRootOffset ();
            void SetRootOffset (ui64 rootOffset);

            ui64 Alloc (std::size_t size);
            void Free (ui64 offset);

        protected:
            ui64 AllocFixedBlock ();
            void FreeFixedBlock (ui64 offset);

            void Save ();

            friend Serializer &operator << (
                Serializer &serializer,
                const Header &header);
            friend Serializer &operator >> (
                Serializer &serializer,
                Header &header);

            friend bool operator == (
                const BTree::Key &key1,
                const BTree::Key &key2);
            friend bool operator != (
                const BTree::Key &key1,
                const BTree::Key &key2);
            friend bool operator < (
                const BTree::Key &key1,
                const BTree::Key &key2);

            friend Serializer &operator << (
               Serializer &serializer,
               const BTree::Key &key);
            friend Serializer &operator >> (
                Serializer &serializer,
                BTree::Key &key);

            friend Serializer &operator << (
                Serializer &serializer,
                const BTree::Node::Entry &entry);
            friend Serializer &operator >> (
                Serializer &serializer,
                BTree::Node::Entry &entry);
            friend Serializer &operator << (
                Serializer &serializer,
                const BTree::Header &header);
            friend Serializer &operator >> (
                Serializer &serializer,
                BTree::Header &header);

            /// \brief
            /// FileAllocator is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FileAllocator)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocator_h)
