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

/// \struct TransactedFile::Block TransactedFileBlock.h thekogans/util/TransactedFileBlock.h
///
/// \brief
/// Block encapsulates the structure of the heap. It provides
/// an api to read and write as well as to navigate (Prev/Next)
/// heap blocks in linear order. Every block has the following
/// structure;
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
///
/// This design provides two benefits;
/// 1. Heap integrity. Header and Footer provide a no man's
/// land that Block uses to make sure the block has not
/// been corrupted by [under|over]flow writes.
/// 2. Ability to navigate the heap in linear order. This
/// property is used in Free to help coalesce adjecent free
/// blocks.
/// TODO: In the future this same ability to navigate the heap
/// forwards and backwards in list like linear order can be
/// used to perform heap checks and potential corruption recovery.
/// The drawback of this design is that every allocation has
/// 32 bytes of overhead. Keep that in mind when designing
/// your objects as this design favors fewer larger objects
/// over many smaller objects.
struct _LIB_THEKOGANS_UTIL_DECL Block {
protected:
    /// \brief
    /// Block \see{TransactedFile}.
    TransactedFile::SharedPtr file;
    /// \brief
    /// Block offset.
    AddressType offset;
    /// \struct TransactedFileAllocator::Block::Header TransactedFileAllocator.h
    /// thekogans/util/TransactedFileAllocator.h
    ///
    /// \brief
    /// Block header preceeds the user data and forms one half of
    /// it's structure. The other half is found in the second copy
    /// of the header located after the user data (footer). Block
    /// uses the information found in header to compare against what's
    /// found in footer. If the two match, the block is considered
    /// intact. If they don't an exception is thrown indicating heap
    /// corruption.
    struct _LIB_THEKOGANS_UTIL_DECL Header {
        /// \brief
        /// Allocator derivatives can use this flag to start their own flag list
        /// without fear of stepping on Allocator's toes.
        static const ui32 FLAGS_FIRST_ALLOCATOR_FLAG = 1 << 16;
        /// \brief
        /// A combination of FLAGS_FREE and allocator specific flags.
        Flags32 flags;
        /// \brief
        /// Block size (not including header and footer). Only user data.
        SizeType size;

        /// \brief
        /// Size of header on disk.
        static const std::size_t SIZE =
            // If this feature is on, every block header and footer will
            // have magic 4 bytes (ui32) embeded. These bytes are used to
            // check block integrity. If you absolutely must save space on
            // your blocks (8 bytes total), turn this feature off.
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            UI32_SIZE + // magic
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            UI32_SIZE + // flags
            SizeTypeSize;  // size

        /// \brief
        /// ctor.
        /// \param[in] flags_ Combination of FLAGS_FREE and allocator specific flags.
        /// \param[in] size_ Block user data size.
        Header (
            Flags32 flags_ = 0,
            SizeType size_ = 0) :
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
            TransactedFile::SharedPtr file,
            AddressType offset);
        /// \brief
        /// Write the header to the disk.
        /// \param[in] file TransactedFile to write to.
        /// \param[in] offset Offset where the header begins.
        void Write (
            TransactedFile::SharedPtr file,
            AddressType offset) const;
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
    /// \param[in] file_ Block \see{TransactedFile}.
    /// \param[in] offset_ Block offset.
    /// \param[in] flags Combination of FLAGS_FREE and allocator specific flags.
    /// \param[in] size Block size (not including the size of the Block itself).
    Block (
        TransactedFile::SharedPtr file_,
        AddressType offset_ = 0,
        Flags32 flags = 0,
        SizeType size = 0) :
        file (file_),
        offset (offset_),
        header (flags, size) {}

    /// \brief
    /// Read the block from \see{TransactedFile} to get it's size.
    /// \param[in] file \see{TransactedFile} containing the block.
    /// \param[in] offset Block offset.
    /// \return Block size.
    static ui64 GetSize (
        TransactedFile::SharedPtr file,
        AddressType offset);

    /// \brief
    /// Return the block offset.
    /// \return Block offset.
    inline AddressType GetOffset () const {
        return offset;
    }
    /// \brief
    /// Set block offset.
    /// \param[in] offset_ New block offset.
    inline void SetOffset (AddressType offset_) {
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
    }

    /// \brief
    /// Return the block size (not including Block::SIZE).
    /// \return Block size.
    inline SizeType GetSize () const {
        return header.size;
    }
    /// \brief
    /// Set block size.
    /// \param[in] size New block size.
    inline void SetSize (SizeType size) {
        header.size = size;
    }

    /// \brief
    /// Return true if this is the first block in the heap.
    /// \return true == first block in the heap.
    inline bool IsFirst () const {
        return GetOffset () == HEADER_SIZE;
    }
    /// \brief
    /// Return true if this is the last block in the heap.
    /// \return true == last block in the heap.
    inline bool IsLast () const {
        return GetOffset () + GetSize () + HEADER_SIZE == file->GetSizeEx ();
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
    /// \brief
    /// Write the block.
    void Write () const;
    /// \brief
    /// Zero out the block (both header and footer).
    void Clear () const;

    /// \brief
    /// Needs access to \see{Header}.
    friend bool operator != (
        const Header &header,
        const Header &footer);

    /// \brief
    /// Block is neither copy or move constructable, nor assignable.
    THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (Block)
};

/// \brief
/// Minimum user data size.
static constexpr std::size_t MIN_USER_DATA_SIZE = 32;
/// \brief
/// Minimum block size.
/// Block::SIZE happens to be 32 bytes, together with 32 for
/// MIN_USER_DATA_SIZE above means that the smallest block we can
/// allocate is 64 bytes.
static constexpr std::size_t MIN_BLOCK_SIZE = Block::SIZE + MIN_USER_DATA_SIZE;

/// \struct TransactedFile::BlockRange TransactedFileAllocator.h
/// thekogans/util/TransactedFileAllocator.h
///
/// \brief
/// A convenience class which uses \see{Allocator::Block::GetSize} for
/// range length.
struct _LIB_THEKOGANS_UTIL_DECL BlockRange : public Range {
    /// \brief
    /// ctor.
    /// \param[in] file \see{TransactedFile} to buffer.
    /// \param[in] offset File offset.
    /// \param[in] reading true == Range will be used for reading.
    /// false == Range will be used for writting.
    /// \param[in] allocator \see{util::Allocator} if we need to allocate.
    BlockRange (
        TransactedFile::SharedPtr file,
        TransactedFile::AddressType offset,
        bool reading = true,
        util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
        Range (file, offset, Block::GetSize (file, offset), reading, allocator) {}
};

/// \struct TransactedFile::SafeBlockRange TransactedFileAllocator.h
/// thekogans/util/TransactedFileAllocator.h
///
/// \brief
/// A convenience class which uses \see{Allocator::Block::GetSize} for
/// range length.
struct _LIB_THEKOGANS_UTIL_DECL SafeBlockRange : public SafeRange {
    /// \brief
    /// ctor.
    /// \param[in] file \see{TransactedFile} to buffer.
    /// \param[in] offset File offset.
    /// \param[in] reading true == Range will be used for reading.
    /// false == Range will be used for writting.
    /// \param[in] allocator \see{util::Allocator} if we need to allocate.
    SafeBlockRange (
        TransactedFile::SharedPtr file,
        TransactedFile::AddressType offset,
        bool reading = true,
        util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
        SafeRange (file, offset, Block::GetSize (file, offset), reading, allocator) {}
};
