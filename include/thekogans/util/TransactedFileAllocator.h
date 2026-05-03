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

/// \struct TransactedFile::Allocator TransactedFileAllocator.h
/// thekogans/util/TransactedFileAllocator.h
///
/// \brief
/// Allocator manages a heap on permanent storage. The heap physical
/// layout looks like this:
///
/// +--------+---------+-----+---------+
/// | Header | Block 1 | ... | Block N |
/// +--------+---------+-----+---------+
///
/// Header            |<--------- version 1 ---------->|
/// +-------+---------+-------+-----------+------------+
/// | magic | version | flags | heapStart | rootOffset | + allocator spacific data.
/// +-------+---------+-------+-----------+------------+
///     4        2        2         8            8
///
/// Header::SIZE = 24 (version 1)
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
/// * - Can be ommitted by undefining THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC
///
/// Header/Footer::SIZE = 16/12
///
/// Data
/// +-----+
/// | ... |
/// +-----+
///   var
struct _LIB_THEKOGANS_UTIL_DECL Allocator :
        public DynamicCreatable,
        public Subscriber<TransactedFileEvents> {
    /// \brief
    /// Allocator is a \see{util::DynamicCreatable} abstract base.
    THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (Allocator)

#if defined (THEKOGANS_UTIL_TYPE_Static)
    /// \brief
    /// Register all known derivatives. This method is meant to be added
    /// to as new Allocator derivatives are added to the system.
    static void StaticInit ();
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

    /// \brief
    /// PtrType is \see{ui64}.
    using PtrType = ui64;
    /// \brief
    /// PtrType size on disk.
    static const std::size_t PTR_TYPE_SIZE = UI64_SIZE;

    /// \struct TransactedFile::Allocator::Block TransactedFileAllocator.h
    /// thekogans/util/TransactedFileAllocator.h
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
    protected:
        /// \brief
        /// Block offset.
        PtrType offset;
        /// \struct TransactedFileAllocator::Block::Header TransactedFileAllocator.h
        /// thekogans/util/TransactedFileAllocator.h
        ///
        /// \brief
        /// Block header preceeds the user data and forms one half of
        /// it's structure. Block uses the information found in
        /// header to compare against what's found in footer. If the
        /// two match, the block is considered intact. If they don't
        /// an exception is thrown indicating heap corruption.
        struct _LIB_THEKOGANS_UTIL_DECL Header {
            /// \brief
            /// A combination of FLAGS_FREE and allocator specific flags.
            Flags32 flags;
            /// \brief
            /// Block size (not including header and footer).
            ui64 size;

            /// \brief
            /// Size of header on disk.
            static const std::size_t SIZE =
            #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                UI32_SIZE + // magic
            #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
                UI32_SIZE + // flags
                UI64_SIZE;  // size

            /// \brief
            /// ctor.
            /// \param[in] flags_ Combination of FLAGS_FREE and allocator specific flags.
            /// \param[in] size_ Block user data size.
            Header (
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
            /// Read the header from the disk.
            /// \param[in] file File to read from.
            /// \param[in] offset Offset where the header begins.
            void Read (
                TransactedFile &file,
                PtrType offset);
            /// \brief
            /// Write the header to the disk.
            /// \param[in] file TransactedFile to write to.
            /// \param[in] offset Offset where the header begins.
            void Write (
                TransactedFile &file,
                PtrType offset) const;
        } header;

    public:
        /// \brief
        /// If this flag is set, the block is free. Otherwise it's allocated.
        static const ui32 FLAGS_FREE = 1;
        /// \brief
        /// Exposed because header is private.
        static const std::size_t HEADER_SIZE = Header::SIZE;
        /// \brief
        /// Block size on disk.
        static const std::size_t SIZE = HEADER_SIZE + HEADER_SIZE;

        /// \brief
        /// ctor.
        /// \param[in] offset_ Offset in the file where the Block resides.
        /// \param[in] flags Combination of FLAGS_FREE and allocator specific flags.
        /// \param[in] size Size of the block (not including the size
        /// of the Block itself).
        Block (
            PtrType offset_ = 0,
            Flags32 flags = 0,
            ui64 size = 0) :
            offset (offset_),
            header (flags, size) {}

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
        /// Return true if this is the first block in the heap.
        /// \return true == first block in the heap.
        inline bool IsFirst (Allocator &allocator) const {
            return GetOffset () == allocator.GetHeapStart () + HEADER_SIZE;
        }
        /// \brief
        /// Return true if this is the last block in the heap.
        /// \return true == last block in the heap.
        inline bool IsLast (Allocator &allocator) const {
            return GetOffset () + GetSize () + HEADER_SIZE == allocator.GetHeapEnd ();
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
        }

        /// \brief
        /// Return the block size (not including Block::SIZE).
        /// \return Block size.
        inline ui64 GetSize () const {
            return header.size;
        }
        /// \brief
        /// Set block size to the given value.
        /// \param[in] size New block size.
        inline void SetSize (ui64 size) {
            header.size = size;
        }

        /// \brief
        /// If not first, return the block right before this one.
        /// \param[out] prev Where to put the previous block.
        /// \return true == prev contains previous block.
        /// false == we're the first block.
        bool Prev (
            Allocator &allocator,
            Block &prev) const;
        /// \brief
        /// If not last, return the block right after this one.
        /// \param[out] next Where to put the next block.
        /// \return true == next contains the next block.
        /// false == we're the last block.
        bool Next (
            Allocator &allocator,
            Block &next) const;

        /// \brief
        /// Read the block.
        void Read (TransactedFile &file);
        /// \brief
        /// Write the block.
        void Write (TransactedFile &file) const;
    #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
        /// \brief
        /// If you chose to use magic (a very smart move) to protect
        /// the block data, you get an extra layer of dangling pointer
        /// detection for free. By zeroing out the magic during \see{Free}
        /// the next time you access that block you get an exception
        /// instead of corrupted data.
        void Invalidate (TransactedFile &file) const;
    #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)

        /// \brief
        /// Needs access to \see{Header}.
        friend bool operator != (
            const Header &header,
            const Header &footer);
    };

    TransactedFile::SharedPtr file;
    PtrType headerOffset;
    /// \struct TransactedFile::Allocator::Header TransactedFileAllocator.h
    /// thekogans/util/TransactedFileAllocator.h
    ///
    /// \brief
    /// Header contains the global heap vailues.
    struct _LIB_THEKOGANS_UTIL_DECL Header {
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
        /// Contains the offset of the root object.
        PtrType rootOffset;
        // NOTE: If you add new fields, adjust the SIZE and increment
        // the CURRENT_VERSION below and add if statements to operator
        // << and >> to read and write them. And don't forget to rebuild
        // all dependent allocators as it's likely they will have based
        // their header extensions by deriving from this one.

        /// \brief
        /// The size of the header on disk.
        static const std::size_t SIZE =
            UI32_SIZE +     // magic
            UI16_SIZE +     // version
            UI16_SIZE +     // flags
            PTR_TYPE_SIZE + // heapStart
            PTR_TYPE_SIZE;  // rootOffset

        /// \brief
        /// Current version.
        static const ui16 CURRENT_VERSION = 1;

        /// \brief
        /// ctor.
        /// \param[in] flags_ 0 or FLAGS_SECURE.
        /// \param[in] heapStart_ Beginning of heap.
        Header (ui16 flags_ = 0,
                PtrType heapStart_ = SIZE) :
                version (CURRENT_VERSION),
                flags (flags_),
                heapStart (heapStart_),
                rootOffset (0) {
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            flags.Set (FLAGS_BLOCK_USES_MAGIC, true);
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
        }

        /// \brief
        /// Return true if this heap was created with a version of thekogans_util
        /// that was built with THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC.
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
    /// Set if the internal cache is dirty.
    static const ui32 FLAGS_DIRTY = 1;
    /// \brief
    /// Combination of the above flags.
    Flags32 flags;

public:
    /// \brief
    /// ctor.
    /// \param[in] flags_
    /// \param[in] heapStart
    Allocator (
        ui16 flags_ = 0,
        PtrType heapStart = Header::SIZE) :
        headerOffset (0),
        header (flags_, heapStart),
        flags (0) {}

    /// \brief
    /// Minimum user data size.
    static const std::size_t MIN_USER_DATA_SIZE = 32;
    /// \brief
    /// Minimum block size.
    /// Block::SIZE happens to be 32 bytes, together with 32 for
    /// MIN_USER_DATA_SIZE above means that the smallest block we can
    /// allocate is 64 bytes.
    static const std::size_t MIN_BLOCK_SIZE = Block::SIZE + MIN_USER_DATA_SIZE;

    inline TransactedFile::SharedPtr GetFile () const {
        return file;
    }

    /// \brief
    /// Return true if secure.
    /// \return true == secure.
    inline bool IsSecure () const {
        return header.IsSecure ();
    }

    /// \brief
    /// Return the pointer to the start of the heap.
    /// \return Pointer to the start of the heap.
    inline PtrType GetHeapStart () const {
        return header.heapStart;
    }
    /// \brief
    /// Return the offset of the first block in the heap.
    /// \return Offset of the first block in the heap.
    inline PtrType GetFirstBlockOffset () const {
        return header.heapStart + Block::HEADER_SIZE;
    }
    /// \brief
    /// Return the pointer to the end of the heap.
    /// \return Pointer to the end of the heap.
    inline PtrType GetHeapEnd () const {
        return file->GetSize ();
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
    /// Set the dirty flag.
    /// \param[in] dirty true == dirty, false == clean.
    inline void SetDirty (bool dirty) {
        SetFlag (FLAGS_DIRTY, dirty);
    }

    virtual void Init (
            TransactedFile::SharedPtr file_,
            PtrType headerOffset_) {
        file = file_;
        headerOffset = headerOffset_;
    }

    /// \brief
    /// Alloc a block.
    /// \param[in] size Size of block to allocate.
    /// \return Offset to the allocated block.
    virtual PtrType Alloc (std::size_t size) = 0;
    /// \brief
    /// Free a previously Alloc(ated) block.
    /// \param[in] offset Offset of block to free.
    virtual void Free (PtrType offset) = 0;
    /// \brief
    /// Resize a block. Make it bigger or smaller.
    /// \param[in] offset Offset of \see{Block} to resize.
    /// \param[in] newSize New \see{Block} size.
    /// \param[in] moveData true == Copy the data to the new block.
    /// \return If newSize is greater than current size, return the
    /// new \see{Block} offset. If not, return the old \see{Block}
    /// offset.
    virtual PtrType Realloc (
        PtrType offset,
        std::size_t newSize,
        bool moveData = true) = 0;

protected:
    void SetFlag (
        ui32 flag,
        bool on);

    virtual void Read ();
    virtual void Write ();

    // TransactedFileEvents
    /// \brief
    /// Transaction is commiting. Flush the internal cache to file.
    /// \param[in] file \see{TransactedFile} commiting the transaction.
    /// \param[in] phase \see{TransactedFile} implements two phase commit.
    virtual void OnTransactedFileTransactionCommit (
        TransactedFile::SharedPtr /*file*/,
        int phase) noexcept override;
    /// \brief
    /// Transaction is aborting. Reload the internal cache from file.
    /// \param[in] file \see{TransactedFile} aborting the transaction.
    virtual void OnTransactedFileTransactionAbort (
        TransactedFile::SharedPtr /*file*/) noexcept override;
};
