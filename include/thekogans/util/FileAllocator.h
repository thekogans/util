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
        ///     4        2        2         8            8
        ///
        ///    |<----------- version 1 ---------->|
        ///    +---------------------+------------+
        /// ...| freeBTreeNodeOffset | rootOffset |
        ///    +---------------------+------------+
        ///               8                 8
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
        /// * - Can be ommitted by undefining THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC
        ///
        /// Header/Footer::SIZE = 16/12
        ///
        /// Data
        /// +---------------------+-----+
        /// | nextBTreeNodeOffset | ... |
        /// +---------------------+-----+
        ///            8            var
        ///
        /// NOTE: Because of it's design, FileAllocator (including it's creation)
        /// can only be used inside \see{BufferedFile::Transaction}.
        struct _LIB_THEKOGANS_UTIL_DECL FileAllocator :
                public BufferedFile::TransactionParticipant {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (FileAllocator)

            /// \brief
            /// PtrType is \see{ui64}.
            using PtrType = ui64;
            /// \brief
            /// PtrType size on disk.
            static const std::size_t PTR_TYPE_SIZE = UI64_SIZE;

            /// \brief
            /// Forward declaration of \see{Object} needed by \see{ObjectEvents}.
            struct Object;

            /// \struct FileAllocator::ObjectEvents FileAllocator.h
            /// thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// Subscribe to ObjectEvents to receive notifications about file
            /// store lifetime. This is useful when you have containing objects
            /// (objects that contain other objects). They can register for their
            /// 'children' events and be notified when containing child offset changed.
            struct _LIB_THEKOGANS_UTIL_DECL ObjectEvents {
                /// \brief
                /// dtor.
                virtual ~ObjectEvents () {}

                /// \brief
                /// \see{Object} allocated a block in the file.
                /// \param[in] object \see{Object} whose offset has become valid.
                /// VERY IMPORTANT SEMANTICS: When you get this notification,
                /// object->GetOffset () will tell you which block has been freed.
                virtual void OnFileAllocatorObjectAlloc (
                    RefCounted::SharedPtr<Object> /*object*/) noexcept {}
                /// \brief
                /// \see{Object} freed its file block.
                /// \param[in] object \see{Object} whose offset has become invalid.
                /// VERY IMPORTANT SEMANTICS: When you get this notification,
                /// object->GetOffset () will tell you which block has been allocated.
                virtual void OnFileAllocatorObjectFree (
                    RefCounted::SharedPtr<Object> /*object*/) noexcept {}
            };

            /// \struct FileAllocator::Object FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// A FileAllocator Object is an object that has allocated at least one block
            /// from \see{FileAllocator} and participates in \see{BufferedFileEvents}.
            struct _LIB_THEKOGANS_UTIL_DECL Object :
                    public BufferedFile::TransactionParticipant,
                    public Serializable,
                    public Producer<ObjectEvents> {
                /// \brief
                /// Object is a \see{util::DynamicCreatable} abstract base.
                THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (Object)

            #if defined (THEKOGANS_UTIL_TYPE_Static)
                /// \brief
                /// Because Object uses dynamic initialization, when using
                /// it in static builds call this method to have the Object
                /// explicitly include all internal object types. Without
                /// calling this api, the only objects that will be available
                /// to your application are the ones you explicitly link to.
                static void StaticInit ();
            #endif // defined (THEKOGANS_UTIL_TYPE_Static)

            protected:
                /// \brief
                /// \see{FileAllocator} where this object resides.
                FileAllocator::SharedPtr fileAllocator;
                /// \brief
                /// Our address inside the \see{BufferedFile}.
                FileAllocator::PtrType offset;

            public:
                /// \brief
                /// ctor.
                /// \param[in] fileAllocator \see{FileAllocator} where this object resides.
                /// \param[in] offset Offset of the \see{FileAllocator::Block}.
                Object (
                    FileAllocator::SharedPtr fileAllocator_,
                    FileAllocator::PtrType offset_) :
                    BufferedFile::TransactionParticipant (fileAllocator_->GetFile ()),
                    fileAllocator (fileAllocator_),
                    offset (offset_) {}
                /// \brief
                /// dtor.
                virtual ~Object () {}

                /// \brief
                /// Return the fileAllocator.
                /// \return fileAllocator.
                inline FileAllocator::SharedPtr GetFileAllocator () const {
                    return fileAllocator;
                }
                /// \brief
                /// Return the offset.
                /// \return offset.
                inline FileAllocator::PtrType GetOffset () const {
                    return offset;
                }

            protected:
                /// \brief
                /// Optimization for Alloc below. If an object declares
                /// itself as fixed size, Alloc will not check the object
                /// block size, only offset. And if offset == 0, it will allocate
                /// a block once and that's it.
                /// \return true == object is fixed size.
                virtual bool IsFixedSize () const {
                    return false;
                }

                // BufferedFile::TransactionParticipant
                /// \brief
                /// If needed allocate space from \see{FileAllocator}.
                virtual void Alloc () override;
                /// \brief
                /// Default free implementation for single block objects.
                /// If your objects contain internal pointers to other
                /// blocks you will need to implement this method and
                /// properly free the containing blocks.
                virtual void Free () override;
                virtual void Flush () override;
                virtual void Reload () override;

            private:
                // Serializable
                /// \brief
                /// Read the Serializable from an XML DOM.
                /// \param[in] header \see{Serializable::Header}.
                /// \param[in] node XML DOM representation of a Serializable.
                virtual void ReadXML (
                        const Header & /*header*/,
                        const pugi::xml_node &node) override {
                    // FIXME: implement?
                    assert (0);
                }
                /// \brief
                /// Write the Serializable to the XML DOM.
                /// \param[out] node Parent node.
                virtual void WriteXML (pugi::xml_node &node) const override {
                    // FIXME: implement?
                    assert (0);
                }

                /// \brief
                /// Read the Serializable from an JSON DOM.
                /// \param[in] node JSON DOM representation of a Serializable.
                virtual void ReadJSON (
                        const Header & /*header*/,
                        const JSON::Object &object) override {
                    // FIXME: implement?
                    assert (0);
                }
                /// \brief
                /// Write the Serializable to the JSON DOM.
                /// \param[out] node Parent node.
                virtual void WriteJSON (JSON::Object &object) const override {
                    // FIXME: implement?
                    assert (0);
                }

                /// \brief
                /// Object is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Object)
            };

            /// \struct FileAllocator::Block FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// Block encapsulates the structure of the heap. It provides
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
            /// land that Block uses to make sure the block has not
            /// been corrupted by [over/under]flow writes.
            /// 2. Ability to navigate the heap in linear order. This
            /// property is used in Free to help coalesce adjecent free
            /// blocks.
            /// The drawback of this design is that every allocation has
            /// 32 bytes of overhead. Keep that in mind when designing
            /// your objects.
            struct _LIB_THEKOGANS_UTIL_DECL Block {
            private:
                /// \brief
                /// \see{FileAllocator} to read and write.
                FileAllocator &fileAllocator;
                /// \brief
                /// Block offset.
                PtrType offset;
                /// \struct FileAllocator::Block::Header FileAllocator.h
                /// thekogans/util/FileAllocator.h
                ///
                /// \brief
                /// Block header preceeds the user data and forms one half of
                /// it's structure. Block uses the information found in
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
                    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        UI32_SIZE + // magic
                    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        UI32_SIZE + // flags
                        UI64_SIZE;  /* size +
                        PTR_TYPE_SIZE nextBTreeNodeOffset is ommited because it
                                  shares space with user data. This makes Header
                                  and Footer identical as far as Block is concerned. */

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
                /// \struct FileAllocator::Block::Footer FileAllocator.h
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
                    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                        UI32_SIZE + // magic
                    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
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
                /// Block size on disk.
                static const std::size_t SIZE = HEADER_SIZE + FOOTER_SIZE;

            public:
                /// \brief
                /// ctor.
                /// \param[in] fileAllocator_ \see{FileAllocator} to read and write from.
                /// \param[in] offset_ Offset in the file where the Block resides.
                /// \param[in] flags Combination of FLAGS_BTREE_NODE and FLAGS_FREE.
                /// \param[in] size Size of the block (not including the size
                /// of the Block itself).
                /// \param[in] nextBTreeNodeOffset If FLAGS_FREE and FLAGS_BTREE_NODE are
                /// set, this field contains the next free \see{BTree::Node} offset.
                Block (
                    FileAllocator &fileAllocator_,
                    PtrType offset_ = 0,
                    Flags32 flags = 0,
                    ui64 size = 0,
                    PtrType nextBTreeNodeOffset = 0) :
                    fileAllocator (fileAllocator_),
                    offset (offset_),
                    header (flags, size, nextBTreeNodeOffset),
                    footer (flags, size) {}

                /// \brief
                /// Return the offset.
                /// \return Offset to the begining of the block.
                inline PtrType GetOffset () const {
                    return offset;
                }

                /// \brief
                /// Return true if this is the first block in the heap.
                /// \return true == first block in the heap.
                inline bool IsFirst () const {
                    return GetOffset () == fileAllocator.GetFirstBlockOffset ();
                }
                /// \brief
                /// Return true if this is the last block in the heap.
                /// \return true == last block in the heap.
                inline bool IsLast () const {
                    return GetOffset () + GetSize () + FOOTER_SIZE == fileAllocator.GetHeapEnd ();
                }

                /// \brief
                /// Return true if FLAGS_FREE is set.
                /// \return true if FLAGS_FREE is set.
                inline bool IsFree () const {
                    return header.IsFree ();
                }

                /// \brief
                /// Return true if FLAGS_BTREE_NODE set.
                /// \return true == FLAGS_BTREE_NODE set.
                inline bool IsBTreeNode () const {
                    return header.IsBTreeNode ();
                }

                /// \brief
                /// Return the block size (not including Block::SIZE).
                /// \return Block size.
                inline ui64 GetSize () const {
                    return header.size;
                }

                /// \brief
                /// Return the next free \see{FileAllocator::BTree::Node} offset.
                /// \return Next free \see{FileAllocator::BTree::Node} offset.
                inline PtrType GetNextBTreeNodeOffset () const {
                    return header.nextBTreeNodeOffset;
                }

                /// \brief
                /// If not first, return the block right before this one.
                /// \param[out] prev Where to put the previous block.
                /// \return true == prev contains previous block.
                /// false == we're the first block.
                bool Prev (Block &prev) const;
                /// \brief
                /// If not last, return the block right after this one.
                /// \param[out] next Where to put the next block.
                /// \return true == next contains the next block.
                /// false == we're the last block.
                bool Next (Block &next) const;

                /// \brief
                /// Read the block.
                void Read ();

            private:
                /// \brief
                /// Set offset to the given value.
                /// \param[in] offset_ New offset value.
                inline void SetOffset (PtrType offset_) {
                    offset = offset_;
                }

                /// \brief
                /// Set/clear the FLAGS_FREE flag.
                /// \param[in] free true == set, false == clear
                inline void SetFree (bool free) {
                    header.SetFree (free);
                    footer.SetFree (free);
                }

                /// \brief
                /// Set/clear the FLAGS_BTREE_NODE flag.
                /// \param[in] free true == set, false == clear
                inline void SetBTreeNode (bool btreeNode) {
                    header.SetBTreeNode (btreeNode);
                    footer.SetBTreeNode (btreeNode);
                }

                /// \brief
                /// Set block size to the given value.
                /// \param[in] size New block size.
                inline void SetSize (ui64 size) {
                    header.size = size;
                    footer.size = size;
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
                /// Write the block.
                void Write () const;
            #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                /// \brief
                /// If you chose to use magic (a very smart move) to protect
                /// the block data, you get an extra layer of dangling pointer
                /// detection for free. By zeroing out the magic during \see{Free}
                /// the next time you access that block you get an exception
                /// instead of corrupted data.
                void Invalidate () const;
            #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)

                /// \brief
                /// Needs access to \see{Header} and \see{Footer}.
                friend bool operator != (
                    const Header &header,
                    const Footer &footer);

                /// \brief
                /// Needs access to private members.
                friend struct FileAllocator;

                /// \brief
                /// Block is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Block)
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
                /// Block.
                Block block;

            public:
                /// \brief
                /// ctor.
                /// \param[in] fileAllocator \see{FileAllocator} containing the block.
                /// \param[in] offset Block offset.
                /// \param[in] bufferLength How much of the block we want to buffer
                /// (0 == buffer the whole block).
                /// \param[in] allocator \see{Allocator} for \see{Buffer}.
                BlockBuffer (
                    FileAllocator &fileAllocator,
                    PtrType offset,
                    std::size_t bufferLength = 0,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

                /// \brief
                /// Read a block range in to the buffer.
                /// \param[in] blockOffset Logical offset within block.
                /// \param[in] blockLength How much of the block we want to read.
                /// (0 == read the whole block).
                inline std::size_t BlockRead (
                        std::size_t blockOffset = 0,
                        std::size_t blockLength = 0) {
                    return BlockIO (blockOffset, blockLength, true);
                }
                /// \brief
                /// Write a block range from the buffer.
                /// \param[in] blockOffset Logical offset within block.
                /// \param[in] blockLength How much of the block we want to write.
                /// (0 == write the whole block).
                inline std::size_t BlockWrite (
                        std::size_t blockOffset = 0,
                        std::size_t blockLength = 0) {
                    return BlockIO (blockOffset, blockLength, false);
                }

            private:
                /// \brief
                /// Read or write a block. Since most of the body of
                /// BlockRead and BlockWrite is the same, I saved a
                /// bit by combining the two.
                /// \param[in] blockOffset Logical offset within block.
                /// \param[in] blockLength How much of the block we want to read or write.
                /// (0 == read or write the whole block).
                std::size_t BlockIO (
                    std::size_t blockOffset,
                    std::size_t blockLength,
                    bool read);

                /// \brief
                /// BlockBuffer is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BlockBuffer)
            };

        private:
            /// \struct FileAllocator::Header FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// Header contains the global heap vailues.
            struct Header {
                /// \brief
                /// When creating a new heap, this flag will stamp the structure
                /// of \see{Block} so that if you ever try to open it with a
                /// wrongly compiled version of thekogans_util it will complain
                /// instead of corrupting your data.
                static const ui16 FLAGS_BLOCK_USES_MAGIC = 1;
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
                /// Begining of heap (start of first Block::Header).
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
                /// \param[in] flags_ 0 or FLAGS_SECURE.
                Header (ui16 flags_ = 0) :
                        version (CURRENT_VERSION),
                        flags (flags_),
                        heapStart (SIZE),
                        btreeOffset (0),
                        freeBTreeNodeOffset (0),
                        rootOffset (0) {
                #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                    flags.Set (FLAGS_BLOCK_USES_MAGIC, true);
                #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                }

                /// \brief
                /// Return true if this heap was created with a version of thekogans_util
                /// that was built with THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_USE_MAGIC.
                /// \return true == FLAGS_BLOCK_USES_MAGIC is set.
                inline bool IsBlockUsesMagic () const {
                    return flags.Test (FLAGS_BLOCK_USES_MAGIC);
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
            BTree::SharedPtr btree;
            /// \brief
            /// Cache the size of the BTree::Node on disk so that we
            /// don't pay the price of calculating it everytime \see{Alloc}
            /// is called.
            std::size_t btreeNodeFileSize;

        public:
            /// \brief
            /// Minimum user data size.
            static const std::size_t MIN_USER_DATA_SIZE = 32;
            /// \brief
            /// Minimum block size.
            /// Block::SIZE happens to be 32 bytes, together with 32 for
            /// MIN_USER_DATA_SIZE above means that the smallest block we can
            /// allocate is 64 bytes.
            static const std::size_t MIN_BLOCK_SIZE = Block::SIZE + MIN_USER_DATA_SIZE;
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
            /// \param[in] file The file where the heap resides.
            /// \param[in] secure true == zero out free blocks.
            /// \param[in] btreeEntriesPerNode Number of entries per \see{BTree::Node}.
            /// \param[in] btreeNodesPerPage Number of \see{BTree::Node}s that will fit in to
            /// a \see{BlockAllocator} page.
            /// \param[in] allocator \see{Allocator} for \see{BTree}.
            FileAllocator (
                BufferedFile::SharedPtr file,
                bool secure = false,
                std::size_t btreeEntriesPerNode = DEFAULT_BTREE_ENTRIES_PER_NODE,
                std::size_t btreeNodesPerPage = DEFAULT_BTREE_NODES_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

            /// \brief
            /// Return true if secure.
            /// \return true == secure.
            inline bool IsSecure () const {
                return header.IsSecure ();
            }

            /// \brief
            /// Return the header.rootOffset;
            /// \return header.rootOffset;
            inline PtrType GetRootOffset () const {
                return header.rootOffset;
            }
            /// \brief
            /// Set the header.rootOffset.
            /// \param[in] rootOffset New rootOffset to set.
            inline void SetRootOffset (PtrType rootOffset) {
                header.rootOffset = rootOffset;
                SetDirty (true);
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
                return header.heapStart + Block::HEADER_SIZE;
            }

            /// \brief
            /// Debugging helper. Dumps \see{BTree::Node}s to stdout.
            inline void DumpBTree () {
                btree->Dump ();
            }

            /// \brief
            /// Alloc a block.
            /// \param[in] size Size of block to allocate.
            /// \return Offset to the allocated block.
            PtrType Alloc (std::size_t size);
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] offset Offset of block to free.
            void Free (PtrType offset);
            PtrType Realloc (
                PtrType offset,
                std::size_t newSize,
                bool moveData = true);

        protected:
            // BufferedFile::TransactionParticipant
            /// \brief
            /// Nothing for us to allocate.
            /// The header is the first thing in the file.
            virtual void Alloc () override {}
            virtual void Free () override {}
            /// \brief
            /// Flush the header to file.
            virtual void Flush () override;
            /// \brief
            /// Reload the header from file.
            virtual void Reload () override;
            virtual void Reset () override;

        private:
            /// \brief
            /// Common method used by the ctor and Reload.
            void Load ();

            /// \brief
            /// Used to allocate \see{BTree::Node} blocks.
            /// This method is used by the \see{BTree::Node}.
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
