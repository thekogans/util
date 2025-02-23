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
        /// FileAllocator manages a heap on permanent storage.
        /// +--------+---------+-----+---------+
        /// | Header | Block 1 | ... | Block N |
        /// +--------+---------+-----+---------+
        ///
        /// Header
        /// +-------+-------+-----------+-----------------+-------------+------------+
        /// | magic | flags | blockSize | freeBlockOffset | btreeOffset | rootOffset |
        /// +-------+-------+-----------+-----------------+-------------+------------+
        ///    4        4         8              8               8             8
        ///
        /// Header::SIZE = 40
        ///
        /// Block
        /// +--------+------+--------+
        /// | Header | Data | Footer |
        /// +--------+------+--------+
        ///     16     var      16
        ///
        /// Header/Footer
        /// +-------+-------+------+
        /// | magic | flags | size |
        /// +-------+-------+------+
        ///     4       4       8
        ///
        /// Heade/Footer::SIZE = 16
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

            /// \struct FileAllocator::Pool FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// Each instance of a FileAllocator attached to a particular
            /// file should be treated as a singleton. Use Pool to recycle
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
                /// Given a path, return the matching file allocator.
                /// If we don't have one, create it.
                /// \param[in] path FileAllocator path.
                /// \param[in] blockSize If > 0, create a fixed block file allocator.
                /// \param[in] blocksPerPage If blockSize is > 0, this allows you to
                /// parameterize the page size for the containing \see{BlockAllocator}.
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
            ///     util::FileAllocator::Flusher flusher (*allocator);
            ///     // Now you can call Alloc and Free as many times
            ///     // as you want and at the end of the scope, the
            ///     // FileAllocator will be flushed.
            /// }
            /// \endcode
            struct _LIB_THEKOGANS_UTIL_DECL Flusher {
            private:
                /// \brief
                /// FileAllocator to flush in the dtor.
                FileAllocator &fileAllocator;

            public:
                /// \brief
                /// ctor.
                /// \param[in] fileAllocator_ FileAllocator to flush in the dtor.
                Flusher (FileAllocator &fileAllocator_) :
                    fileAllocator (fileAllocator_) {}
                /// \brief
                /// dtor.
                ~Flusher () {
                    fileAllocator.FlushBTree ();
                }

                /// \brief
                /// Flusher is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Flusher)
            };

            /// \struct FileAllocator::LockedFilePtr FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// FileAllocator's public api is thread safe. It uses a \see{SpinLock}
            /// to protect shared resources. Unfortunatelly, past experience indicates
            /// that for objects like FileAllocator, a public api is not enough. That's
            /// why I exposed the file. This has a number of implications; I purposely
            /// chose \see{SpinLock} to avoid a heavy penalty for lock aquisition. But
            /// because other threads sit and spin when the lock is busy, it's very
            /// important to not hold on to the lock too long. Ex:
            ///
            /// \code{.cpp}
            /// using namespace thekogans;
            ///
            /// {
            ///     util::FileAllocator::LockedFilePtr file (*fileAllocator);
            ///     util::FileAllocator::BlockInfo block (*file, offset);
            ///     block.Read ();
            ///     ...
            ///     // Automatically release the lock at end of scope.
            /// }
            /// \endcode
            struct _LIB_THEKOGANS_UTIL_DECL LockedFilePtr {
            private:
                /// \brief
                /// FileAllocator we're about to take out a lock on.
                FileAllocator &fileAllocator;

            public:
                /// \brief
                /// ctor.
                /// \param[in] fileAllocator_ FileAllocator we're about to take out a lock on.
                LockedFilePtr (FileAllocator &fileAllocator_) :
                        fileAllocator (fileAllocator_) {
                    fileAllocator.spinLock.Acquire ();
                }
                /// \brief
                /// dtor.
                ~LockedFilePtr () {
                    fileAllocator.spinLock.Release ();
                }

                /// \brief
                /// Standard pointer operator. Provides access to the file.
                /// \return File &.
                inline File &operator * () const {
                    return fileAllocator.file;
                }
                /// \brief
                /// Standard pointer operator. Provides access to the file.
                /// \return File *.
                inline File *operator -> () const {
                    return &fileAllocator.file;
                }

                /// \brief
                /// LockedFilePtr is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (LockedFilePtr)
            };

            /// \brief
            /// Forward declaration to resolve circular dependence.
            struct BlockBuffer;

            /// \struct FileAllocator::BlockInfo FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// BlockInfo encapsulates the structure of the heap. It provides
            /// an api to read and write as well as to navigate (Prev/Next)
            /// heap blocks in chronological order. Every block has the following
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
            /// 2. Ability to navigate the heap in chronological order.
            /// (call BlockInfo::Prev/Next.)
            struct _LIB_THEKOGANS_UTIL_DECL BlockInfo {
            private:
                /// \brief
                /// FileAllocator file to read and write.
                File &file;
                /// \brief
                /// Block offset.
                ui64 offset;
                /// \struct FileAllocator::BlockInfo::Header FileAllocator.h
                /// thekogans/util/FileAllocator.h
                ///
                /// \brief
                /// Block header preceeds the user data and forms one half of
                /// it's structure. BlockInfo uses the information found in
                /// header to compare against what's fond in footer. If the
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
                    ui64 nextBlockOffset;

                    /// \brief
                    /// Size of header on disk.
                    static const std::size_t SIZE =
                    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                        UI32_SIZE + // magic
                    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                        UI32_SIZE + // flags
                        UI64_SIZE; /* size +
                        UI64_SIZE nextBlockOffset is ommited because it shares space
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
                        ui64 nextBlockOffset_ = 0) :
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
                        ui64 offset);
                    /// \brief
                    /// Write the header to the disk.
                    /// \param[in] file File to write to.
                    /// \param[in] offset Offset where the header begins.
                    void Write (
                        File &file,
                        ui64 offset);
                } header;
                /// \struct FileAllocator::BlockInfo::Footer FileAllocator.h
                /// thekogans/util/FileAllocator.h
                ///
                /// \brief
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
                        ui64 offset);
                    /// \brief
                    /// Write the footer to the disk.
                    /// \param[in] file File to write to.
                    /// \param[in] offset Offset where the footer begins.
                    void Write (
                        File &file,
                        ui64 offset);
                } footer;

            public:
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

                /// \brief
                /// ctor.
                /// \param[in] file_ \see{File} containing the BlockInfo.
                /// \param[in] offset_ Offset in the file where the BlockInfo resides.
                /// \param[in] flags Combination of FLAGS_FIXED and FLAGS_FREE.
                /// \param[in] size Size of the block (not including the size
                /// of the BlockInfo itself).
                /// \param[in] nextBlockOffset
                BlockInfo (
                    File &file_,
                    ui64 offset_ = 0,
                    Flags32 flags = 0,
                    ui64 size = 0,
                    ui64 nextBlockOffset = 0) :
                    file (file_),
                    offset (offset_),
                    header (flags, size, nextBlockOffset),
                    footer (flags, size) {}

                /// \brief
                /// Return the offset.
                /// \return Offset to the begining of the block.
                inline ui64 GetOffset () const {
                    return offset;
                }
                /// \brief
                /// Set offset to the given value.
                /// \param[in] offset_ New offset value.
                inline void SetOffset (ui64 offset_) {
                    offset = offset_;
                }
                /// \brief
                /// Return true if this is the first block in the heap.
                /// \return true == first block in the heap.
                inline bool IsFirst () const {
                    return GetOffset () == FileAllocator::MIN_BLOCK_OFFSET;
                }
                /// \brief
                /// Return true if this is the last block in the heap.
                /// \return true == last block in the heap.
                inline bool IsLast () const {
                    return GetOffset () + GetSize () + FOOTER_SIZE == file.GetSize ();
                }

                /// \brief
                /// Return true if FLAGS_FREE is set.
                /// \return true if FLAGS_FREE is set.
                inline bool IsFree () const {
                    return header.IsFree ();
                }
                /// \brief
                /// Set/unset the FLAGS_FREE flag.
                /// \param[in] free true == set, false == unset
                inline void SetFree (bool free) {
                    header.SetFree (free);
                    footer.SetFree (free);
                }
                /// \brief
                /// Return true if FLAGS_FIXED set.
                /// \return true == FLAGS_FIXED set.
                inline bool IsFixed () const {
                    return header.IsFixed ();
                }
                /// \brief
                /// Set/unset the FLAGS_FIXED flag.
                /// \param[in] free true == set, false == unset
                inline void SetFixed (bool fixed) {
                    header.SetFixed (fixed);
                    footer.SetFixed (fixed);
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
                /// Return the next free fixed block offset.
                /// NOTE: This value is only valid if IsFree and IsFixed.
                /// \return Next free fixed block offset.
                inline ui64 GetNextBlockOffset () const {
                    return header.nextBlockOffset;
                }
                /// \brief
                /// Set the next block offset. This will chain the
                /// free fixed blocks in to a singly linked list.
                /// \param[in] nextBlockOffset Next free fixed block offset.
                inline void SetNextBlockOffset (ui64 nextBlockOffset) {
                    header.nextBlockOffset = nextBlockOffset;
                }

                /// \brief
                /// If !IsFirst, return the block info right before this one.
                /// \param[out] prev Where to put the previous block info.
                /// \return true == prev is valid, false == IsFirst is true.
                bool Prev (BlockInfo &prev);
                /// \brief
                /// If !IsLast, return the block info right after this one.
                /// \param[out] next Where to put the next block info.
                /// \return true == next is valid, false == IsLast is true.
                bool Next (BlockInfo &next);

                /// \brief
                /// Read block info at offset.
                void Read ();
                /// \brief
                /// Write block info at offset.
                void Write ();

                /// \brief
                /// Needs access to \see{Header} and \see{Footer}.
                friend bool operator != (
                    const Header &header,
                    const Footer &footer);

                /// \brief
                /// Needs access to file.
                friend struct BlockBuffer;

                /// \brief
                /// BlockInfo is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BlockInfo)
            };

            /// \struct FileAllocator::BlockBuffer FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// BlockBuffer provides access to the user data stored in the
            /// heap blocks. Because it derives from \see{Buffer} it inherits
            /// all the underlying serialization machinery defined in \see{Serializer}.
            /// BlockBuffer is also flexible enough to provide access to sub
            /// ranges. Making it efficient to update only the parts of the data
            /// that have changed.
            struct _LIB_THEKOGANS_UTIL_DECL BlockBuffer : public Buffer {
            private:
                /// \brief
                /// Block info.
                BlockInfo block;

            public:
                /// \brief
                /// ctro.
                /// \param[in] fileAllocator \see{FileAllocator} containing the block.
                /// \param[in] offset Block offset.
                /// \param[in] length How much of the block do we want (0 == get the whole block).
                BlockBuffer (
                    FileAllocator &fileAllocator,
                    ui64 offset,
                    std::size_t length = 0);

                /// \brief
                /// Read a range in to the buffer.
                /// \param[in] blockOffset Logical offset within block.
                /// \param[in] length How much of the block do we want (0 == get the whole block).
                std::size_t Read (
                    std::size_t blockOffset = 0,
                    std::size_t length = 0);
                /// \brief
                /// Write a range from the buffer.
                /// \param[in] blockOffset Logical offset within block.
                /// \param[in] length How much of the block do we want (0 == write the whole block).
                std::size_t Write (
                    std::size_t blockOffset = 0,
                    std::size_t length = 0);

                /// \brief
                /// BlockBuffer is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BlockBuffer)
            };

        private:
            /// \brief
            /// The file where the heap resides.
            SimpleFile file;
            /// \struct FileAllocator::Header FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// Header contains the global heap vailues.
            struct Header {
                /// \brief
                /// Flag to indicate that this heap is allocating fixed size blocks.
                static const std::size_t FLAGS_FIXED = 1;
                /// \brief
                /// Heap flags.
                Flags32 flags;
                /// \brief
                /// If fixed, contains the block size to allocate. Otherwise
                /// contains the BTree::Node size on disk.
                ui64 blockSize;
                /// \brief
                /// If fixed, contains the head of the first free fixed block list.
                /// Otherwise contains the head of the first free BTree::Node block list.
                ui64 freeBlockOffset;
                /// \brief
                /// If !fixed, contains the offset of the BTree::Header.
                ui64 btreeOffset;
                /// \brief
                /// Contains the offset of the user set root block.
                /// See GetRootOffset ()/SetRootOffset () below.
                ui64 rootOffset;

                /// \brief
                /// The size of the header on disk.
                static const std::size_t SIZE =
                    UI32_SIZE + // magic
                    UI32_SIZE + // flags
                    UI64_SIZE + // blockSize
                    UI64_SIZE + // freeBlockOffset
                    UI64_SIZE + // btreeOffset
                    UI64_SIZE;  // rootOffset

                /// \brief
                /// ctor.
                /// \param[in] flags_ 0 of FLAGS_FIXED.
                /// \param[in] blockSize_ If FLAGS_FIXED, constains the
                /// size of the block to allocate. Otherwise contains the
                /// size of BTree::Node on disk.
                Header (
                    Flags32 flags_ = 0,
                    ui64 blockSize_ = 0) :
                    flags (flags_),
                    blockSize (blockSize_),
                    freeBlockOffset (0),
                    btreeOffset (0),
                    rootOffset (0) {}

                /// \brief
                /// Return true if this FileAllocator is allocating fixed size blocks.
                /// \return true == FLAGS_FIXED is set.
                inline bool IsFixed () const {
                    return flags.Test (FLAGS_FIXED);
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
            /// FileAllocator is meant to be shared between threads allocating
            /// from the same file.
            SpinLock spinLock;

        public:
            /// \brief
            /// Minimum user data size.
            static const std::size_t MIN_USER_DATA_SIZE = 32;
            /// \brief
            /// Based on the layout of the file, the smallest valid user offset is;
            static const std::size_t MIN_BLOCK_OFFSET = Header::SIZE + BlockInfo::HEADER_SIZE;
            /// \brief
            /// Minimum block size.
            /// BlockInfo::SIZE happens to be 32 bytes, together with 32 for
            /// MIN_USER_DATA_SIZE above means that the smallest block we can
            /// allocate is 64 bytes.
            static const std::size_t MIN_BLOCK_SIZE = BlockInfo::SIZE + MIN_USER_DATA_SIZE;

            /// \brief
            /// ctor.
            /// \param[in] blockSize If > 0, this is a fixed size block allocator.
            /// If == 0, this is a random size block allocator.
            /// \param[in] blocksPerPage If fixed contains the number of fixed
            /// blocks per page to initialize \see{BlockAllocator}. If random
            /// size contains the number of entries per node in the btree below.
            /// \param[in] allocator If fixed points to an \see{Allocator} that
            /// will allocate block pages for the internal \see{BlockAllocator}.
            /// If random size points to an \see{Allocaor} that will back up file
            /// allocator blocks with in memory buffers (\see{BlockBuffer} above).
            FileAllocator (
                const std::string &path,
                std::size_t blockSize = 0,
                std::size_t blocksPerPage = BTree::DEFAULT_ENTRIES_PER_NODE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            /// \brief
            /// dtor.
            virtual ~FileAllocator ();

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
            /// Return the root offset.
            /// NOTE: If you hold a LockedFilePtr this function is off limits.
            /// \return header.rootOffset.
            ui64 GetRootOffset ();
            /// \brief
            /// Set the root offset.
            /// NOTE: If you hold a LockedFilePtr this function is off limits.
            /// \param[in] rootOffset New root offset to set.
            void SetRootOffset (ui64 rootOffset);

            /// \brief
            /// Alloc a block. If fixed, size is ignored and instead header.blockSize
            /// is used.
            /// NOTE: If you hold a LockedFilePtr this function is off limits.
            /// \param[in] size Size of block to allocate. Ignored if fixed.
            /// \return Offset to the allocated block.
            ui64 Alloc (std::size_t size);
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// NOTE: If you hold a LockedFilePtr this function is off limits.
            /// \param[in] offset Offset of block to free.
            void Free (ui64 offset);

            /// \brief
            /// Flush the btree cache to disk.
            void FlushBTree ();

            /// \brief
            /// Debugging helper. Dumps \see{Btree::Node}s to stdout.
            void DumpBTree ();

        protected:
            /// \brief
            /// Used to allocate blocks when FLAGS_FIXED is true.
            /// Uses \see{Header::blockSize}. This method is also
            /// used directly by the internal \see{BTree::Node}.
            /// \return Offset of allocated block.
            ui64 AllocFixedBlock ();
            /// \brief
            /// Used to free blocks prviously allocated with AllocFixedBlock.
            /// \param[in] offset Offset of fixed block to free.
            void FreeFixedBlock (ui64 offset);

            /// \brief
            /// Write the \see{Header}.
            void Save ();

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
                const BTree::Key &key1,
                const BTree::Key &key2);
            /// \brief
            /// Needs access to private members.
            friend bool operator != (
                const BTree::Key &key1,
                const BTree::Key &key2);
            /// \brief
            /// Needs access to private members.
            friend bool operator < (
                const BTree::Key &key1,
                const BTree::Key &key2);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator << (
               Serializer &serializer,
               const BTree::Key &key);
            /// \brief
            /// Needs access to private members.
            friend Serializer &operator >> (
                Serializer &serializer,
                BTree::Key &key);

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
