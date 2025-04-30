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

#include <unordered_map>
#include "thekogans/util/Config.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/BufferedFile.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/BlockAllocator.h"
#include "thekogans/util/Singleton.h"

namespace thekogans {
    namespace util {

        /// \struct FileAllocator FileAllocator.h thekogans/util/FileAllocator.h
        ///
        /// \brief
        /// FileAllocator manages a heap on permanent storage. It's public API is
        /// thread safe and is meant to be used by different clients from different
        /// threads. The heap physical layout looks like this:
        ///
        /// +--------+---------+-----+---------+
        /// | Header | Block 1 | ... | Block N |
        /// +--------+---------+-----+---------+
        ///
        /// Header            |<------------------ version 1 ------------------>|
        /// +-------+---------+-------+-----------+-----------+-----------------+
        /// | magic | version | flags | blockSize | heapStart | freeBlockOffset |...
        /// +-------+---------+-------+-----------+-----------+-----------------+
        ///     4       2         2         8           8              8
        ///
        ///    |<--------------- version 1 --------------->|
        ///    +-------------+----------------+------------+
        /// ,,,| btreeOffset | registryOffset | rootOffset |
        ///    +-------------+----------------+------------+
        ///        8               8              8
        ///
        /// Header::SIZE = 56 (version 1)
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
        /// +-----------------+-----------------------+
        /// | nextBlockOffset |          ...          |
        /// +-----------------+-----------------------+
        ///          8                   var

        struct _LIB_THEKOGANS_UTIL_DECL FileAllocator : public RefCounted {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (FileAllocator)

            /// \brief
            /// PtrType is \see{ui64}.
            using PtrType = ui64;
            /// \brief
            /// PtrType size on disk.
            static const std::size_t PTR_TYPE_SIZE = UI64_SIZE;

            /// \struct FileAllocator::Pool FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// Each instance of a FileAllocator attached to a particular
            /// file should be treated as a singleton. This is why the
            /// \see{FileAllocator} ctor is private. Use Pool to recycle
            /// and reuse file allocators based on a given path. Ex:
            ///
            /// \code{.cpp}
            /// using namespace thekogans;
            ///
            /// util::FileAllocator::SharedPtr allocator =
            ///     util::FileAllocator::Pool::Instance ()->GetFileAllocator ("test.allocator");
            /// \endcode
            struct _LIB_THEKOGANS_UTIL_DECL Pool : public Singleton<Pool> {
            private:
                /// \brief
                /// FileAllocator map type (keyed on path).
                using Map = std::unordered_map<std::string, FileAllocator::SharedPtr>;
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
                /// Given a path, return the matching file allocator.
                /// If we don't have one, create it.
                /// \param[in] path FileAllocator path.
                /// \param[in] blockSize If > 0, create a fixed block file allocator.
                /// \param[in] blocksPerPage If blockSize is > 0, this allows you to
                /// parameterize the page size for the containing \see{BlockAllocator}.
                /// \param[in] allocator If fixed points to an \see{Allocator} that
                /// will allocate block pages for the internal \see{BlockAllocator}.
                /// If random size points to an \see{Allocaor} that will back up file
                /// allocator blocks with in memory buffers (\see{BlockBuffer} above).
                /// \return FileAllocator matching the given path.
                FileAllocator::SharedPtr GetFileAllocator (
                    const std::string &path,
                    std::size_t blockSize = 0,
                    std::size_t blocksPerPage = BTree::DEFAULT_ENTRIES_PER_NODE,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

                /// \brief
                /// Given a FileAllocator path, flush it's changes to disk. If the
                /// given path is empty, flush them all.
                /// \paam[in] path FileAllocator path to flush or empty for all.
                void FlushFileAllocator (const std::string &path = std::string ());

                /// \brief
                /// Pool is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Pool)
            };

            /// \struct FileAllocator::Flusher FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// FileAllocator uses a delayed approach to disc writes. To make sure
            /// FileAllocator cache is written out you need to call Flush at the end
            /// of the operation (Alloc/Free). This can get tricky as exceptions
            /// alter return paths. Use this class at the begining of a sequence
            /// of operations and let it call Flush automativally. Ex:
            ///
            /// \code{.cpp}
            /// using namespace thekogans;
            ///
            /// {
            ///     util::FileAllocator::SharedPtr allocator =
            ///         util::FileAllocator::Pool::Instance ()->GetFileAllocator ("test.allocator");
            ///     util::FileAllocator::Flusher flusher (allocator);
            ///     // Now you can call Alloc and Free as many times
            ///     // as you want and at the end of the scope, the
            ///     // FileAllocator will be flushed.
            /// }
            /// \endcode
            struct _LIB_THEKOGANS_UTIL_DECL Flusher {
            private:
                /// \brief
                /// FileAllocator to flush in the dtor.
                FileAllocator::SharedPtr fileAllocator;

            public:
                /// \brief
                /// ctor.
                /// \param[in] fileAllocator_ FileAllocator to flush in the dtor.
                Flusher (FileAllocator::SharedPtr fileAllocator_) :
                    fileAllocator (fileAllocator_) {}
                /// \brief
                /// dtor.
                ~Flusher () {
                    Flush ();
                }

                /// \brief
                /// Call
                inline void Flush () {
                    fileAllocator->Flush ();
                }

                /// \brief
                /// Flusher is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Flusher)
            };

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
            /// (call BlockInfo::Prev/Next.)
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
                    /// A combination of FLAGS_FREE and FLAGS_FIXED.
                    Flags32 flags;
                    /// \brief
                    /// Block size (not including header and footer).
                    ui64 size;
                    /// \brief
                    /// If FLAGS_FIXED and FLAGS_FREE are set this offset
                    /// will point to the next fixed block in the free list.
                    /// Otherwise this field is ignored. This is the only
                    /// difference between header and footer.
                    PtrType nextBlockOffset;

                    /// \brief
                    /// Size of header on disk.
                    static const std::size_t SIZE =
                    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                        UI32_SIZE + // magic
                    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                        UI32_SIZE + // flags
                        UI64_SIZE; /* size +
                        PTR_TYPE_SIZE nextBlockOffset is ommited because it shares space
                                  with user data. This makes Header and Footer
                                  identical as far as BlockInfo is concerned.
                                  nextBlockOffset is maintaned only in
                                  FileAllocator::AllocFixedBlock/FreeFixedBlock. */

                    /// \brief
                    /// ctor.
                    /// \param[in] flags_ Combination of FLAGS_FREE and FLAGS_FIXED.
                    /// \param[in] size_ Block user data size.
                    /// \param[in] nextBlockOffset_ If FLAGS_FREE and FLAGS_FIXED are set,
                    /// this field contains the next free fixed block offset.
                    Header (
                        Flags32 flags_ = 0,
                        ui64 size_ = 0,
                        PtrType nextBlockOffset_ = 0) :
                        flags (flags_),
                        size (size_),
                        nextBlockOffset (nextBlockOffset_) {}

                    /// \brief
                    /// Return true if FLAGS_FREE set.
                    /// \return true == FLAGS_FREE set.
                    inline bool IsFree () const {
                        return flags.Test (FLAGS_FREE);
                    }
                    /// \brief
                    /// Set/unset the FLAGS_FREE flag.
                    /// \param[in] free true == set, false == unset
                    inline void SetFree (bool free) {
                        flags.Set (FLAGS_FREE, free);
                    }
                    /// \brief
                    /// Return true if FLAGS_FIXED set.
                    /// \return true == FLAGS_FIXED set.
                    inline bool IsFixed () const {
                        return flags.Test (FLAGS_FIXED);
                    }
                    /// \brief
                    /// Set/unset the FLAGS_FIXED flag.
                    /// \param[in] free true == set, false == unset
                    inline void SetFixed (bool fixed) {
                        flags.Set (FLAGS_FIXED, fixed);
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
                    /// Combination of FLAGS_FREE and FLAGS_FIXED.
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
                    /// \param[in] flags_ Combination of FLAGS_FREE and FLAGS_FIXED.
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
                    /// Set/unset the FLAGS_FREE flag.
                    /// \param[in] free true == set, false == unset
                    inline void SetFree (bool free) {
                        flags.Set (FLAGS_FREE, free);
                    }
                    /// \brief
                    /// Return true if FLAGS_FIXED set.
                    /// \return true == FLAGS_FIXED set.
                    inline bool IsFixed () const {
                        return flags.Test (FLAGS_FIXED);
                    }
                    /// \brief
                    /// Set/unset the FLAGS_FIXED flag.
                    /// \param[in] free true == set, false == unset
                    inline void SetFixed (bool fixed) {
                        flags.Set (FLAGS_FIXED, fixed);
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
                static const std::size_t FLAGS_FREE = 1;
                /// \brief
                /// If this flag is set the block is fixed. Otherwise it's random size.
                static const std::size_t FLAGS_FIXED = 2;
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
                /// \param[in] flags Combination of FLAGS_FIXED and FLAGS_FREE.
                /// \param[in] size Size of the block (not including the size
                /// of the BlockInfo itself).
                /// \param[in] nextBlockOffset If FLAGS_FREE and FLAGS_FIXED are
                /// set, this field contains the next free fixed block offset.
                BlockInfo (
                    PtrType offset_ = 0,
                    Flags32 flags = 0,
                    ui64 size = 0,
                    PtrType nextBlockOffset = 0) :
                    offset (offset_),
                    header (flags, size, nextBlockOffset),
                    footer (flags, size) {}

                /// \brief
                /// Return the offset.
                /// \return Offset to the begining of the block.
                inline PtrType GetOffset () const {
                    return offset;
                }

                /// \brief
                /// Return true if FLAGS_FREE is set.
                /// \return true if FLAGS_FREE is set.
                inline bool IsFree () const {
                    return header.IsFree ();
                }
                /// \brief
                /// Return true if FLAGS_FIXED set.
                /// \return true == FLAGS_FIXED set.
                inline bool IsFixed () const {
                    return header.IsFixed ();
                }

                /// \brief
                /// Return the block size (not including BlockInfo::SIZE).
                /// \return Block size.
                inline ui64 GetSize () const {
                    return header.size;
                }

            private:
                /// \brief
                /// Set offset to the given value.
                /// \param[in] offset_ New offset value.
                inline void SetOffset (PtrType offset_) {
                    offset = offset_;
                }
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
                /// Set/unset the FLAGS_FREE flag.
                /// \param[in] free true == set, false == unset
                inline void SetFree (bool free) {
                    header.SetFree (free);
                    footer.SetFree (free);
                }

                /// \brief
                /// Set/unset the FLAGS_FIXED flag.
                /// \param[in] free true == set, false == unset
                inline void SetFixed (bool fixed) {
                    header.SetFixed (fixed);
                    footer.SetFixed (fixed);
                }

                /// \brief
                /// Set block size to the given value.
                /// \param[in] size New block size.
                inline void SetSize (ui64 size) {
                    header.size = size;
                    footer.size = size;
                }

                /// \brief
                /// Return the next free fixed block offset.
                /// NOTE: This value is only valid if IsFree and IsFixed.
                /// \return Next free fixed block offset.
                inline PtrType GetNextBlockOffset () const {
                    return header.nextBlockOffset;
                }
                /// \brief
                /// Set the next block offset. This will chain the
                /// free fixed blocks in to a singly linked list.
                /// \param[in] nextBlockOffset Next free fixed block offset.
                inline void SetNextBlockOffset (PtrType nextBlockOffset) {
                    header.nextBlockOffset = nextBlockOffset;
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
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (BlockBuffer)

                /// \brief
                /// BlockBuffer has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

            private:
                /// \brief
                /// Block info.
                BlockInfo block;

                /// \brief
                /// ctor.
                /// NOTE: The reason its private is that we don't trust the
                /// user to do the right thing and lock the FileAllocator.
                /// Use FileAllocator::CreateBuffer to create input and output
                /// buffers.
                /// \param[in] fileAllocator \see{FileAllocator} containing the block.
                /// \param[in] offset Block offset.
                /// \param[in] bufferLength How much of the block we want to buffer
                /// (0 == buffer the whole block).
                BlockBuffer (
                    FileAllocator &fileAllocator,
                    PtrType offset,
                    std::size_t bufferLength = 0);

                /// \brief
                /// Read a block range in to the buffer.
                /// \param[in] blockOffset Logical offset within block.
                /// \param[in] blockLength How much of the block we want to read.
                /// (0 == read the whole block).
                std::size_t BlockRead (
                    File &file,
                    std::size_t blockOffset = 0,
                    std::size_t blockLength = 0);
                /// \brief
                /// Write a block range from the buffer.
                /// \param[in] blockOffset Logical offset within block.
                /// \param[in] blockLength How much of the block we want to write.
                /// (0 == write the whole block).
                std::size_t BlockWrite (
                    File &file,
                    std::size_t blockOffset = 0,
                    std::size_t blockLength = 0);

                /// \brief
                /// FileAllocator holds the lock so all calls go through it.
                friend struct FileAllocator;
                /// \brief
                /// FileAllocator::BTree is the only class allowed to call in to
                /// BlockBuffer without holding the lock.
                friend struct BTree;

                /// \brief
                /// BlockBuffer is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BlockBuffer)
            };

            /// \brief
            /// \see{FileAllocator::Registry} holds a circular reference to FileAllocator.
            struct Registry;

        private:
            /// \brief
            /// The file where the heap resides.
            SimpleBufferedFile file;
            /// \struct FileAllocator::Header FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// Header contains the global heap vailues.
            struct Header {
                /// \brief
                /// Flag to indicate that this heap is allocating fixed size blocks.
                static const std::size_t FLAGS_FIXED = 1;
                /// \brief
                /// When creating a new heap, this flag will stamp the structure
                /// of \see{BlockInfo} so that if you ever try to open it with a
                /// wrongly compiled version of thekogans_util it will complain
                /// instead of corrupting your data.
                static const std::size_t FLAGS_BLOCK_INFO_USES_MAGIC = 2;
                /// \brief
                /// Heap version.
                ui16 version;
                /// \brief
                /// Heap flags.
                Flags16 flags;
                /// \brief
                /// If fixed, contains the block size to allocate. Otherwise
                /// contains the BTree::Node size on disk.
                ui64 blockSize;
                /// \brief
                /// Begining of heap (start of first BlockInfo::Header).
                PtrType heapStart;
                /// \brief
                /// If fixed, contains the head of the first free fixed block list.
                /// Otherwise contains the head of the first free BTree::Node block list.
                PtrType freeBlockOffset;
                /// \brief
                /// If !fixed, contains the offset of the BTree::Header.
                PtrType btreeOffset;
                /// \brief
                /// If !fixed, contains the offset of the FileAllocatorRegistry::BTree::Header.
                PtrType registryOffset;
                /// \brief
                /// Contains the offset of the user set root block.
                /// See GetRootOffset/SetRootOffset below.
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
                    UI64_SIZE +     // blockSize
                    PTR_TYPE_SIZE + // heapStart
                    PTR_TYPE_SIZE + // freeBlockOffset
                    PTR_TYPE_SIZE + // btreeOffset
                    PTR_TYPE_SIZE + // registryOffset
                    PTR_TYPE_SIZE;  // rootOffset

                /// \brief
                /// Current version.
                static const ui16 CURRENT_VERSION = 1;

                /// \brief
                /// ctor.
                /// \param[in] flags_ 0 of FLAGS_FIXED.
                /// \param[in] blockSize_ If FLAGS_FIXED, constains the
                /// size of the block to allocate. Otherwise contains the
                /// size of BTree::Node on disk.
                Header (
                        Flags32 flags_ = 0,
                        ui64 blockSize_ = 0) :
                        version (CURRENT_VERSION),
                        flags (flags_),
                        blockSize (blockSize_),
                        heapStart (SIZE),
                        freeBlockOffset (0),
                        btreeOffset (0),
                        registryOffset (0),
                        rootOffset (0) {
                #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                    flags.Set (FLAGS_BLOCK_INFO_USES_MAGIC, true);
                #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                }

                /// \brief
                /// Return true if this heap is allocating fixed size blocks.
                /// \return true == FLAGS_FIXED is set.
                inline bool IsFixed () const {
                    return flags.Test (FLAGS_FIXED);
                }
                /// \brief
                /// Return true if this heap was created with a version of thekogans_util
                /// that was built with THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC.
                /// \return true == FLAGS_FIXED is set.
                inline bool IsBlockInfoUsesMagic () const {
                    return flags.Test (FLAGS_BLOCK_INFO_USES_MAGIC);
                }
            } header;
            /// \brief
            /// \see{BlockAllocator} for allocating fixed
            /// block backing store \see{BlockBuffer}s.
            Allocator::SharedPtr fixedAllocator;
            /// \see{Allocator} for allocating random sized
            /// block backing store \see{BlockBuffer}s.
            Allocator::SharedPtr blockAllocator;
            /// \brief
            /// Include the \see{BTree} header.
            /// I split it out because this file was getting too big to maintain.
            #include "thekogans/util/FileAllocatorBTree.h"
            /// \brief
            /// \see{BTree} to manage heap free space (for random size heaps only).
            BTree *btree;
            /// \brief
            /// \see{FileAllocator::Registry} (for random size heaps only).
            Registry *registry;
            /// \brief
            /// Set in Save and indicates that the \see{Header} is dirty and needs
            /// to be written to disk.
            bool dirty;
            /// \brief
            /// FileAllocator is meant to be shared between threads allocating
            /// from the same file.
            SpinLock spinLock;

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

            /// \brief
            /// Return the heap file path.
            /// \return Heap file path.
            inline std::string GetPath () const {
                return file.GetPath ();
            }

            /// \brief
            /// Return true if fixed block allocator.
            /// \return true == Fixed block allocator. false == random size block allocator.
            inline bool IsFixed () const {
                // header.flags is read only after ctor so no need to lock.
                return header.IsFixed ();
            }
            /// \brief
            /// Return the fixed block size.
            /// \return If fixed, fixed block size. if random size, btree node size.
            inline std::size_t GetBlockSize () const {
                // header.blockSize is read only after ctor so no need to lock.
                return header.blockSize;
            }
            /// \brief
            /// Return the size of an allocated block.
            /// \param[in] offset Offset of an allocated block whose size to return.
            /// \return If fixed, fixed block size. if random size, allocated block size.
            /// NOTE: If block is free, returns 0.
            std::size_t GetBlockSize (PtrType offset);

            /// \brief
            /// If !IsFixed, a \see{FileAllocator::Registry} in the form of a \see{BTree}
            /// is available for storing and retrieving associated values. The key type is
            /// \see{StringKey} and the value type is any type derived from \see{BTree::Value}.
            /// If IsFixed, will throw an \see{Exception}.
            /// NOTE: The following parameters are standard \see{BTree} tunning parameters
            /// and have been picked to be suitable for most situations. The entriesPerNode
            /// parameter is only valid during registry creation. Once created, the only
            /// way to change it is to delete the registry (\see{FileAllocator::Registry::Delete}
            /// and to start over. The nodesPerPage and the allocator parameters are a
            /// quirck of the \see{BTree::Node} design. And, unless you're intimately
            /// steeped in \see{BTree} internals, should just be left alone. nodesPerPage
            /// controls how memory is allocated for \see{BTree::Node}s and should be tuned
            /// up or down depending on the number of nodes you think your tree will have.
            /// Since the registry is a global resource, and uses synchronization to protectect
            /// access at every call to Get/SetValue, I don't expect it's traffic to be too
            /// high. Instead, it is designed to let clients store named global data that
            /// they will then use to initialize the rest of the system. If you're going
            /// to be using the registry as your systems primary data storage, perhaps you
            /// might want to bump up nodesPerPage (and entriesPerNode for that mater).
            /// allocator controls where the said node pages come from and unless you have
            /// a very specific need, the default system allocator should work just fine.
            /// \param[in] entriesPerNode Number of entries per btree node.
            /// \param[in] nodesPerPage Number of btree nodes per allocator page.
            /// \param[in] allocator Where allocator node pages come from.
            /// \return Reference to \see{FileAllocator::Registry}. Will create it on first
            /// access.
            Registry &GetRegistry (
                std::size_t entriesPerNode = 32,
                std::size_t nodesPerPage = 5,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

            /// \brief
            /// Return the root offset.
            /// \return header.rootOffset.
            PtrType GetRootOffset ();
            /// \brief
            /// Set the root offset.
            /// \param[in] rootOffset New root block offset.
            void SetRootOffset (PtrType rootOffset);

            /// \brief
            /// Alloc a block. If IsFixed, size is ignored and instead header.blockSize
            /// is used.
            /// \param[in] size Size of block to allocate. Ignored if IsFixed.
            /// \return Offset to the allocated block.
            PtrType Alloc (std::size_t size);
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] offset Offset of block to free.
            void Free (PtrType offset);

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
            /// Create a \see{BlockBuffer} for reading or writing. If for reading
            /// read it's contents.
            /// \param[in] offset Block offset.
            /// \param[in] length Block length (0 == cover the whole block).
            /// \param[in] read true == read block contents (while still holding the lock).
            /// \param[in] blockOffset If read == true, logical offset within block.
            /// \param[in] blockLength If read == true, how much of the block we want
            /// to read (0 == read the whole block).
            /// \return \see{BlockBuffer::SharedPtr}.
            BlockBuffer::SharedPtr CreateBlockBuffer (
                PtrType offset,
                std::size_t bufferLength = 0,
                bool read = true,
                std::size_t blockOffset = 0,
                std::size_t blockLength = 0);
            /// \brief
            /// Read a range of the block in to the given \see{BlockBuffer}.
            /// \param[in] buffer \see{BlockBuffer} to read in to.
            /// \param[in] blockOffset Logical offset within block.
            /// \param[in] blockLength How much of the block we want to read
            /// (0 == read the whole block).
            void ReadBlockBuffer (
                BlockBuffer &buffer,
                std::size_t blockOffset = 0,
                std::size_t blockLength = 0);
            /// \brief
            /// Write the given \see{BlockBuffer} to disk.
            /// \param[in] buffer \see{BlockBuffer} to write to disk.
            /// \param[in] blockOffset Logical offset within block.
            /// \param[in] blockLength How much of the block we want to write
            /// (0 == write the whole block).
            void WriteBlockBuffer (
                BlockBuffer &buffer,
                std::size_t blockOffset = 0,
                std::size_t blockLength = 0);

            /// \brief
            /// Flush the header, btree (if !IsFixed) and file cache
            /// to disk.
            /// IMORTANT: Flush cannot flush unwritten client data as
            /// it has no clue how you're using the heap. To make sure
            /// all data is flushed to disk, make sure you call flush
            /// on all your heap objects first (if they have any) so
            /// that they write to FileAllocator::BlockBuffer and then
            /// call this Flush.
            void Flush ();

            /// \brief
            /// Debugging helper. Dumps \see{Btree::Node}s to stdout.
            void DumpBTree ();

        private:
            /// \brief
            /// ctor.
            /// \param[in] path Heap file path.
            /// \param[in] blockSize If > 0, this is a fixed size block allocator.
            /// If == 0, this is a random size block allocator.
            /// \param[in] blocksPerPage If fixed contains the number of fixed
            /// blocks per page to initialize \see{BlockAllocator}. If random
            /// size contains the number of entries per node in the btree below.
            /// \param[in] allocator If fixed points to an \see{Allocator} that
            /// will allocate block pages for the internal \see{BlockAllocator}.
            /// If random size points to an \see{Allocaor} that will back up file
            /// allocator blocks with in memory buffers (\see{BlockBuffer} above).
            /// NOTE: This ctor is private because you should not create FileAllocator
            /// by yourself. Each path needs to be matched with only one FileAllocator.
            /// Use \see{Pool} above to maintain a map of available allocators and
            /// create new ones when necessary.
            FileAllocator (
                const std::string &path,
                std::size_t blockSize = 0,
                std::size_t blocksPerPage = BTree::DEFAULT_ENTRIES_PER_NODE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            /// \brief
            /// dtor.
            virtual ~FileAllocator ();

            /// \brief
            /// Used to allocate blocks when IsFixed is true.
            /// Uses \see{Header::blockSize}. This method is also
            /// used directly by the internal \see{BTree::Node}.
            /// \return Offset of allocated block.
            PtrType AllocFixedBlock ();
            /// \brief
            /// Used to free blocks prviously allocated with AllocFixedBlock.
            /// \param[in] offset Offset of fixed block to free.
            void FreeFixedBlock (PtrType offset);

            /// \brief
            /// Set dirty to true.
            void Save ();
            /// \brief
            /// Write the \see{Header}.
            void FileAllocator::WriteHeader ();

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

        /// \def THEKOGANS_UTIL_IMPLEMENT_FILE_ALLOCATOR_POOL_FLUSHER
        /// Use this macro at the top of your main to flush the \see{FileAllocator::Pool} on exit.
        #define THEKOGANS_UTIL_IMPLEMENT_FILE_ALLOCATOR_POOL_FLUSHER\
            struct FileAllocatorPoolFlusher {\
                ~FileAllocatorPoolFlusher () {\
                    thekogans::util::FileAllocator::Pool::Instance ()->FlushFileAllocator ();\
                }\
            } fileAllocatorPoolFlusher

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocator_h)
