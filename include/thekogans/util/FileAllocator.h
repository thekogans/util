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

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/BufferedFile.h"
#include "thekogans/util/BufferedFileTransactionParticipant.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/BlockAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct FileAllocator FileAllocator.h thekogans/util/FileAllocator.h
        ///
        /// \brief
        /// FileAllocator manages a heap on permanent storage. The heap physical
        /// layout looks like this:
        ///
        /// +--------+---------+-----+---------+
        /// | Header | Block 1 | ... | Block N |
        /// +--------+---------+-----+---------+
        ///
        /// Header            |<---------- version 1 ---------->|
        /// +-------+---------+-------+-----------+-------------+
        /// | magic | version | flags | heapStart | btreeOffset |...
        /// +-------+---------+-------+-----------+-------------+
        ///     4       2         2         8            8
        ///
        ///    |<------------- version 1 ------------>|
        ///    +---------------------+------------+
        /// ...| freeBTreeNodeOffset | rootOffset |
        ///    +---------------------+------------+
        ///                8                8
        ///
        /// Header::SIZE = 40 (version 1)
        ///
        /// Block
        /// +--------+------+--------+
        /// | Header | Data | Footer |
        /// +--------+------+--------+
        ///    16/12    var    16/12
        ///
        /// Header/Footer
        /// +-------+-------+------+
        /// | magic | flags | size |
        /// +-------+-------+------+
        ///    *4       4       8
        ///
        /// * - Can be ommitted by undefining THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC
        ///
        /// Header/Footer::SIZE = 16/12
        ///
        /// Data
        /// +---------------------+-----------------------+
        /// | nextBTreeNodeOffset |          ...          |
        /// +---------------------+-----------------------+
        ///            8                     var

        struct _LIB_THEKOGANS_UTIL_DECL FileAllocator :
                public Subscriber<BufferedFileEvents>,
                public BufferedFileTransactionParticipant {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (FileAllocator)

            /// \brief
            /// PtrType is \see{ui64}.
            using PtrType = ui64;
            /// \brief
            /// PtrType size on disk.
            static const std::size_t PTR_TYPE_SIZE = UI64_SIZE;

            /// \struct FileAllocator::BlockInfo FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// BlockInfo encapsulates the structure of the heap. It provides
            /// an api to read and write as well as to navigate (Prev/Next)
            /// heap blocks in linear order. Every block has the following
            /// structure;
            ///
            /// +--------+------+--------+
            /// | Header | Data | Footer |
            /// +--------+------+--------+
            ///
            /// This design provides two benefits;
            /// 1. Heap integrity. Header and Footer provide a no man's
            /// land that BlockInfo uses to make sure the block has not
            /// been corrupted by [over/under]flow writes.
            /// 2. Ability to navigate the heap in linear order.
            struct _LIB_THEKOGANS_UTIL_DECL BlockInfo {
            private:
                /// \brief
                /// Block offset.
                PtrType offset;
                /// \struct FileAllocator::BlockInfo::Header FileAllocator.h
                /// thekogans/util/FileAllocator.h
                ///
                /// \brief
                /// Block header preceeds the user data and forms one half of
                /// it's structure. BlockInfo uses the information found in
                /// header to compare against what's found in footer. If the
                /// two match, the block is considered intact. If they don't
                /// an exception is thrown indicating heap corruption.
                struct _LIB_THEKOGANS_UTIL_DECL Header {
                    /// \brief
                    /// A combination of FLAGS_FREE and FLAGS_BTREE_NODE.
                    Flags32 flags;
                    /// \brief
                    /// Block size (not including header and footer).
                    ui64 size;
                    /// \brief
                    /// If FLAGS_BTREE_NODE and FLAGS_FREE are set this offset
                    /// will point to the next \see{BTree::Node} offset in the free list.
                    /// Otherwise this field is ignored. This is the only
                    /// difference between header and footer.
                    PtrType nextBTreeNodeOffset;

                    /// \brief
                    /// Size of header on disk.
                    static const std::size_t SIZE =
                    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                        UI32_SIZE + // magic
                    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                        UI32_SIZE + // flags
                        UI64_SIZE; /* size +
                        PTR_TYPE_SIZE nextBTreeNodeOffset is ommited because it shares space
                                  with user data. This makes Header and Footer
                                  identical as far as BlockInfo is concerned.
                                  nextBTreeNodeOffset is maintaned only in
                                  FileAllocator::AllocBTreeNode/FreeBTreeNode. */

                    /// \brief
                    /// ctor.
                    /// \param[in] flags_ Combination of FLAGS_FREE and FLAGS_BTREE_NODE.
                    /// \param[in] size_ Block user data size.
                    /// \param[in] nextBTreeNodeOffset_ If FLAGS_FREE and FLAGS_BTREE_NODE are set,
                    /// this field contains the next free \see{BTree::Node} offset.
                    Header (
                        Flags32 flags_ = 0,
                        ui64 size_ = 0,
                        PtrType nextBTreeNodeOffset_ = 0) :
                        flags (flags_),
                        size (size_),
                        nextBTreeNodeOffset (nextBTreeNodeOffset_) {}

                    /// \brief
                    /// Return true if FLAGS_FREE set.
                    /// \return true == FLAGS_FREE set.
                    inline bool IsFree () const {
                        return flags.Test (FLAGS_FREE);
                    }
                    /// \brief
                    /// Set/clear the FLAGS_FREE flag.
                    /// \param[in] free true == set, false == clear
                    inline void SetFree (bool free) {
                        flags.Set (FLAGS_FREE, free);
                    }
                    /// \brief
                    /// Return true if FLAGS_BTREE_NODE set.
                    /// \return true == FLAGS_BTREE_NODE set.
                    inline bool IsBTreeNode () const {
                        return flags.Test (FLAGS_BTREE_NODE);
                    }
                    /// \brief
                    /// Set/clear the FLAGS_BTREE_NODE flag.
                    /// \param[in] free true == set, false == clear
                    inline void SetBTreeNode (bool btreeNode) {
                        flags.Set (FLAGS_BTREE_NODE, btreeNode);
                    }

                    /// \brief
                    /// Read the header from the disk.
                    /// \param[in] file File to read from.
                    /// \param[in] offset Offset where the header begins.
                    void Read (
                        File &file,
                        PtrType offset);
                    /// \brief
                    /// Write the header to the disk.
                    /// \param[in] file File to write to.
                    /// \param[in] offset Offset where the header begins.
                    void Write (
                        File &file,
                        PtrType offset) const;
                } header;
                /// \struct FileAllocator::BlockInfo::Footer FileAllocator.h
                /// thekogans/util/FileAllocator.h
                ///
                /// \brief
                /// Footer follows the user data and forms the second half
                /// of the heap structure.
                struct _LIB_THEKOGANS_UTIL_DECL Footer {
                    /// \brief
                    /// Combination of FLAGS_FREE and FLAGS_BTREE_NODE.
                    Flags32 flags;
                    /// \brief
                    /// Block size (not including header and footer).
                    ui64 size;

                    /// \brief
                    /// Size of footer on disk.
                    static const std::size_t SIZE =
                    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                        UI32_SIZE + // magic
                    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                        UI32_SIZE + // flags
                        UI64_SIZE;  // size

                    /// \brief
                    /// ctor.
                    /// \param[in] flags_ Combination of FLAGS_FREE and FLAGS_BTREE_NODE.
                    /// \param[in] size_ Block user data size.
                    Footer (
                        Flags32 flags_ = 0,
                        ui64 size_ = 0) :
                        flags (flags_),
                        size (size_) {}

                    /// \brief
                    /// Return true if FLAGS_FREE set.
                    /// \return true == FLAGS_FREE set.
                    inline bool IsFree () const {
                        return flags.Test (FLAGS_FREE);
                    }
                    /// \brief
                    /// Set/clear the FLAGS_FREE flag.
                    /// \param[in] free true == set, false == clear
                    inline void SetFree (bool free) {
                        flags.Set (FLAGS_FREE, free);
                    }
                    /// \brief
                    /// Return true if FLAGS_BTREE_NODE set.
                    /// \return true == FLAGS_BTREE_NODE set.
                    inline bool IsBTreeNode () const {
                        return flags.Test (FLAGS_BTREE_NODE);
                    }
                    /// \brief
                    /// Set/clear the FLAGS_BTREE_NODE flag.
                    /// \param[in] btreeNode true == set, false == clear
                    inline void SetBTreeNode (bool btreeNode) {
                        flags.Set (FLAGS_BTREE_NODE, btreeNode);
                    }

                    /// \brief
                    /// Read the footer from the disk.
                    /// \param[in] file File to read from.
                    /// \param[in] offset Offset where the footer begins.
                    void Read (
                        File &file,
                        PtrType offset);
                    /// \brief
                    /// Write the footer to the disk.
                    /// \param[in] file File to write to.
                    /// \param[in] offset Offset where the footer begins.
                    void Write (
                        File &file,
                        PtrType offset) const;
                } footer;

                /// \brief
                /// If this flag is set, the block is free. Otherwise it's allocated.
                static const ui32 FLAGS_FREE = 1;
                /// \brief
                /// If this flag is set the block is \see{BTree::Node}. Otherwise it's random size.
                static const ui32 FLAGS_BTREE_NODE = 2;
                /// \brief
                /// Exposed because header is private.
                static const std::size_t HEADER_SIZE = Header::SIZE;
                /// \brief
                /// Exposed because footer is private.
                static const std::size_t FOOTER_SIZE = Footer::SIZE;
                /// \brief
                /// BlockInfo size on disk.
                static const std::size_t SIZE = HEADER_SIZE + FOOTER_SIZE;

            public:
                /// \brief
                /// ctor.
                /// \param[in] offset_ Offset in the file where the BlockInfo resides.
                /// \param[in] flags Combination of FLAGS_BTREE_NODE and FLAGS_FREE.
                /// \param[in] size Size of the block (not including the size
                /// of the BlockInfo itself).
                /// \param[in] nextBTreeNodeOffset If FLAGS_FREE and FLAGS_BTREE_NODE are
                /// set, this field contains the next free \see{BTree::Node} offset.
                BlockInfo (
                    PtrType offset_ = 0,
                    Flags32 flags = 0,
                    ui64 size = 0,
                    PtrType nextBTreeNodeOffset = 0) :
                    offset (offset_),
                    header (flags, size, nextBTreeNodeOffset),
                    footer (flags, size) {}

                /// \brief
                /// Return true if this is the first block in the heap.
                /// \param[in] heapStart The offset of the first byte in the heap.
                /// \return true == first block in the heap.
                inline bool IsFirst (ui64 heapStart) const {
                    return heapStart + HEADER_SIZE == GetOffset ();
                }
                /// \brief
                /// Return true if this is the last block in the heap.
                /// \param[in] heapEnd The offset of the last byte in the heap.
                /// \return true == last block in the heap.
                inline bool IsLast (ui64 heapEnd) const {
                    return GetOffset () + GetSize () + FOOTER_SIZE == heapEnd;
                }

                /// \brief
                /// Return the offset.
                /// \return Offset to the begining of the block.
                inline PtrType GetOffset () const {
                    return offset;
                }
                /// \brief
                /// Set offset to the given value.
                /// \param[in] offset_ New offset value.
                inline void SetOffset (PtrType offset_) {
                    offset = offset_;
                }

                /// \brief
                /// Return true if FLAGS_FREE is set.
                /// \return true if FLAGS_FREE is set.
                inline bool IsFree () const {
                    return header.IsFree ();
                }
                /// \brief
                /// Set/clear the FLAGS_FREE flag.
                /// \param[in] free true == set, false == clear
                inline void SetFree (bool free) {
                    header.SetFree (free);
                    footer.SetFree (free);
                }

                /// \brief
                /// Return true if FLAGS_BTREE_NODE set.
                /// \return true == FLAGS_BTREE_NODE set.
                inline bool IsBTreeNode () const {
                    return header.IsBTreeNode ();
                }
                /// \brief
                /// Set/clear the FLAGS_BTREE_NODE flag.
                /// \param[in] free true == set, false == clear
                inline void SetBTreeNode (bool btreeNode) {
                    header.SetBTreeNode (btreeNode);
                    footer.SetBTreeNode (btreeNode);
                }

                /// \brief
                /// Return the block size (not including BlockInfo::SIZE).
                /// \return Block size.
                inline ui64 GetSize () const {
                    return header.size;
                }
                /// \brief
                /// Set block size to the given value.
                /// \param[in] size New block size.
                inline void SetSize (ui64 size) {
                    header.size = size;
                    footer.size = size;
                }

                /// \brief
                /// Return the next free \see{FileAllocator::BTree::Node} offset.
                /// \return Next free \see{FileAllocator::BTree::Node} offset.
                inline PtrType GetNextBTreeNodeOffset () const {
                    return header.nextBTreeNodeOffset;
                }
                /// \brief
                /// Set the next free \see{FileAllocator::BTree::Node} offset.
                /// This will chain the free \see{FileAllocator::BTree::Node}
                /// blocks in to a singly linked list.
                /// \param[in] nextBTreeNodeOffset Next free \see{FileAllocator::BTree::Node} offset.
                inline void SetNextBTreeNodeOffset (PtrType nextBTreeNodeOffset) {
                    header.nextBTreeNodeOffset = nextBTreeNodeOffset;
                }

                /// \brief
                /// If !IsFirst, return the block info right before this one.
                /// \param[in] file FileAllocator \see{File} to read prev from.
                /// \param[out] prev Where to put the previous block info.
                void Prev (
                    File &file,
                    BlockInfo &prev) const;
                /// \brief
                /// If !IsLast, return the block info right after this one.
                /// \param[in] file FileAllocator \see{File} to read next from.
                /// \param[out] next Where to put the next block info.
                void Next (
                    File &file,
                    BlockInfo &next) const;

                /// \brief
                /// Read block info.
                /// \param[in] file FileAllocator \see{File} to read.
                void Read (File &file);
                /// \brief
                /// Write block info.
                /// \param[in] file FileAllocator \see{File} to write.
                void Write (File &file) const;
            #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                /// \brief
                /// If you chose to use magic (a very smart move) to protect
                /// the block data, you get an extra layer of dangling pointer
                /// detection for free. By zeroing out the magic during \see{Free}
                /// the next time you access that block you get an exception
                /// instead of corrupted data.
                /// \param[in] file FileAllocator \see{File} to write.
                void Invalidate (File &file) const;
            #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)

                /// \brief
                /// Needs access to \see{Header} and \see{Footer}.
                friend bool operator != (
                    const Header &header,
                    const Footer &footer);

                /// \brief
                /// FileAllocator needs access to private members.
                friend struct FileAllocator;
            };

            /// \struct FileAllocator::BlockBuffer FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// BlockBuffer provides access to the user data stored in heap blocks.
            /// Because it derives from \see{Buffer} it inherits all the underlying
            /// serialization machinery defined in \see{Serializer}. BlockBuffer is
            /// also flexible enough to provide access to sub ranges. Making it
            /// efficient to update only the parts of the data that have changed.
            struct _LIB_THEKOGANS_UTIL_DECL BlockBuffer : public Buffer {
            private:
                /// \brief
                /// \see{FileAllocator} reference.
                File &file;
                /// \brief
                /// Block info.
                BlockInfo block;

            public:
                /// \brief
                /// ctor.
                /// \param[in] file \see{File} containing the block.
                /// \param[in] offset Block offset.
                /// \param[in] bufferLength How much of the block we want to buffer
                /// (0 == buffer the whole block).
                /// \param[in] allocator \see{Allocator} for \see{Buffer}.
                BlockBuffer (
                    File &file_,
                    PtrType offset,
                    std::size_t bufferLength = 0,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

                /// \brief
                /// Read a block range in to the buffer.
                /// \param[in] blockOffset Logical offset within block.
                /// \param[in] blockLength How much of the block we want to read.
                /// (0 == read the whole block).
                std::size_t BlockRead (
                    std::size_t blockOffset = 0,
                    std::size_t blockLength = 0);
                /// \brief
                /// Write a block range from the buffer.
                /// \param[in] blockOffset Logical offset within block.
                /// \param[in] blockLength How much of the block we want to write.
                /// (0 == write the whole block).
                std::size_t BlockWrite (
                    std::size_t blockOffset = 0,
                    std::size_t blockLength = 0);

                /// \brief
                /// BlockBuffer is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BlockBuffer)
            };

        private:
            /// \brief
            /// The file where the heap resides.
            BufferedFile::SharedPtr file;
            /// \struct FileAllocator::Header FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// Header contains the global heap vailues.
            struct Header {
                /// \brief
                /// When creating a new heap, this flag will stamp the structure
                /// of \see{BlockInfo} so that if you ever try to open it with a
                /// wrongly compiled version of thekogans_util it will complain
                /// instead of corrupting your data.
                static const ui16 FLAGS_BLOCK_INFO_USES_MAGIC = 1;
                /// \brief
                /// If set, zero out freed blocks.
                static const ui16 FLAGS_SECURE = 2;
                /// \brief
                /// Heap version.
                ui16 version;
                /// \brief
                /// Heap flags.
                Flags16 flags;
                /// \brief
                /// Begining of heap (start of first BlockInfo::Header).
                PtrType heapStart;
                /// \brief
                /// Contains the offset of the \see{BTree::Header}.
                PtrType btreeOffset;
                /// \brief
                /// Contains the head of the free \see{BTree::Node} list.
                PtrType freeBTreeNodeOffset;
                /// \brief
                /// Contains the offset of the root object.
                PtrType rootOffset;
                // NOTE: If you add new fields, adjust the SIZE and increment
                // the CURRENT_VERSION below and add if statements to operator
                // << and >> to read and write them.

                /// \brief
                /// The size of the header on disk.
                static const std::size_t SIZE =
                    UI32_SIZE +     // magic
                    UI16_SIZE +     // version
                    UI16_SIZE +     // flags
                    PTR_TYPE_SIZE + // heapStart
                    PTR_TYPE_SIZE + // btreeOffset
                    PTR_TYPE_SIZE + // freeBTreeNodeOffset
                    PTR_TYPE_SIZE;  // rootOffset

                /// \brief
                /// Current version.
                static const ui16 CURRENT_VERSION = 1;

                /// \brief
                /// ctor.
                Header (ui16 flags_ = 0) :
                        version (CURRENT_VERSION),
                        flags (flags_),
                        heapStart (SIZE),
                        btreeOffset (0),
                        freeBTreeNodeOffset (0),
                        rootOffset (0) {
                #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                    flags.Set (FLAGS_BLOCK_INFO_USES_MAGIC, true);
                #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                }

                /// \brief
                /// Return true if this heap was created with a version of thekogans_util
                /// that was built with THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC.
                /// \return true == FLAGS_BLOCK_INFO_USES_MAGIC is set.
                inline bool IsBlockInfoUsesMagic () const {
                    return flags.Test (FLAGS_BLOCK_INFO_USES_MAGIC);
                }
                /// \brief
                /// Return true if secure.
                /// \return true == secure.
                inline bool IsSecure () const {
                    return flags.Test (FLAGS_SECURE);
                }
            } header;
            /// \brief
            /// Include the \see{BTree} header.
            /// I split it out because this file was getting too big to maintain.
            #include "thekogans/util/FileAllocatorBTree.h"
            /// \brief
            /// \see{BTree} to manage heap free space.
            BTree *btree;
            /// \brief
            /// Set to indicate that the \see{Header} is dirty and needs
            /// to be written to disk.
            bool dirty;

        public:
            /// \brief
            /// Minimum user data size.
            static const std::size_t MIN_USER_DATA_SIZE = 32;
            /// \brief
            /// Minimum block size.
            /// BlockInfo::SIZE happens to be 32 bytes, together with 32 for
            /// MIN_USER_DATA_SIZE above means that the smallest block we can
            /// allocate is 64 bytes.
            static const std::size_t MIN_BLOCK_SIZE = BlockInfo::SIZE + MIN_USER_DATA_SIZE;
            // NOTE: The following constants are meant to be tuned during
            // system integration to provide the best performance for your
            // needs.
            /// \brief
            /// Default number of entries per \see{BTree::Node}.
            static const std::size_t DEFAULT_BTREE_ENTRIES_PER_NODE = 256;
            /// \brief
            /// Number of \see{BTree::Node}s that will fit in to a
            /// \see{BlockAllocator} page.
            static const std::size_t DEFAULT_BTREE_NODES_PER_PAGE = 10;

            /// \brief
            /// ctor.
            /// \param[in] path Heap file path.
            /// \param[in] secure true == zero out freed blocks.
            /// \param[in] btreeEntriesPerNode Default number of entries
            /// per \see{BTree::Node}.
            /// \param[in] btreeNodesPerPage Number of \see{BTree::Node}s
            /// that will fit in to a \see{BlockAllocator} page.
            /// \param[in] allocator \see{Allocator} for \see{BTree} and
            /// \see{FileAllocator::Registry}.
            FileAllocator (
                BufferedFile::SharedPtr file_,
                bool secure = false,
                std::size_t btreeEntriesPerNode = DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t btreeNodesPerPage = DEFAULT_BTREE_NODES_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            /// \brief
            /// dtor.
            ~FileAllocator ();

            /// \brief
            /// Return true if secure.
            /// \return true == secure.
            inline bool IsSecure () const {
                return header.IsSecure ();
            }

            inline PtrType GetRootOffset () const {
                return header.rootOffset;
            }

            inline void SetRootOffset (PtrType rootOffset) {
                header.rootOffset = rootOffset;
                dirty = true;
            }

            /// \brief
            /// Return the heap file.
            /// \return Heap file.
            inline BufferedFile::SharedPtr GetFile () {
                return file;
            }

            /// \brief
            /// Return the pointer to the start of the heap.
            /// \return Pointer to the start of the heap.
            inline PtrType GetHeapStart () const {
                return header.heapStart;
            }
            /// \brief
            /// Return the pointer to the end of the heap.
            /// \return Pointer to the end of the heap.
            inline PtrType GetHeapEnd () const {
                return file->GetSize ();
            }

            /// \brief
            /// Return the offset of the first block in the heap.
            /// \return Offset of the first block in the heap.
            inline PtrType GetFirstBlockOffset () const {
                // header.heapStart is read only after ctor so no need to lock.
                return header.heapStart + BlockInfo::HEADER_SIZE;
            }

            /// \brief
            /// Given a properly constructed \see{BlockInfo}, return its information.
            /// \param[in, out] block \see{BlockInfo} with properly initialized offset.
            /// On return will contain the block info.
            void GetBlockInfo (BlockInfo &block);
            /// \brief
            /// Given a properly initialized block (offset), get the previous one.
            /// \param[in] block BlockInfo whose previous BlockInfo to return.
            /// \param[out] prev Where to return the previous BlockInfo.
            /// \return true == prev contains the previous block info.
            /// false == block is first in the heap.
            bool GetPrevBlockInfo (
                const BlockInfo &block,
                BlockInfo &prev);
            /// \brief
            /// Given a properly initialized block (offset), get the next one.
            /// \param[in] block BlockInfo whose next BlockInfo to return.
            /// \param[out] next Where to return the next BlockInfo.
            /// \return true == next contains the next block info.
            /// false == block is last in the heap.
            bool GetNextBlockInfo (
                const BlockInfo &block,
                BlockInfo &next);

            /// \brief
            /// Alloc a block.
            /// \param[in] size Size of block to allocate.
            /// \return Offset to the allocated block.
            PtrType Alloc (std::size_t size);
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] offset Offset of block to free.
            void Free (PtrType offset);

            /// \brief
            /// Debugging helper. Dumps \see{BTree::Node}s to stdout.
            void DumpBTree ();

        protected:
            // BufferedFileEvents
            /// \brief
            /// \see{BufferedFile} just created a new \see{BufferedFile::Transaction}.
            /// Subscribe to it's events.
            /// \param[in] file \see{BufferedFile} that created the transaction.
            virtual void OnBufferedFileCreateTransaction (
                    BufferedFile::SharedPtr file) noexcept override {
                BufferedFileTransactionParticipant::Subscribe (*file->GetTransaction ());
            }

            // BufferedFileTransactionParticipant
            /// \brief
            /// Flush the header to file.
            virtual void Flush () override;

            /// \brief
            /// Reload the header from file.
            virtual void Reload () override;

        private:
            /// \brief
            /// Used to allocate BTree::Node blocks.
            /// Uses \see{Header::btreeNodeSize}. This method is
            /// used directly by the internal \see{BTree::Node}.
            /// \return Offset of allocated block.
            PtrType AllocBTreeNode (std::size_t size);
            /// \brief
            /// Used to free blocks prviously allocated with AllocBTreeNode.
            /// \param[in] offset Offset of \see{BTree::Node} to free.
            void FreeBTreeNode (PtrType offset);

            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
                Serializer &serializer,
                const Header &header);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator >> (
                Serializer &serializer,
                Header &header);

            /// \brief
            /// Needs access to private members.
            friend bool operator == (
                const BTree::KeyType &key1,
                const BTree::KeyType &key2);
            /// \brief
            /// Needs access to private members.
            friend bool operator != (
                const BTree::KeyType &key1,
                const BTree::KeyType &key2);
            /// \brief
            /// Needs access to private members.
            friend bool operator < (
                const BTree::KeyType &key1,
                const BTree::KeyType &key2);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
               Serializer &serializer,
               const BTree::KeyType &key);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator >> (
                Serializer &serializer,
                BTree::KeyType &key);

            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
                Serializer &serializer,
                const BTree::Node::Entry &entry);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator >> (
                Serializer &serializer,
                BTree::Node::Entry &entry);

            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
                Serializer &serializer,
                const BTree::Header &header);
            /// \brief
            /// Needs access to private members.
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
