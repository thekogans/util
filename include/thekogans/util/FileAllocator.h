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
        /// Header header
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
                    std::size_t blocksPerPage = DEFAULT_BTREE_ENTRIES_PER_NODE,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

                /// \brief
                /// Pool is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Pool)
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

            /// \struct FileAllocator::BlockInfo FileAllocator.h thekogans/util/FileAllocator.h
            ///
            /// \brief
            /// BlockInfo encapsulates the structure of the heap. It provides
            /// an api to read and write as well as to navigate (Prev/Next)
            /// heap blocks in chronological order.
            struct _LIB_THEKOGANS_UTIL_DECL BlockInfo {
            private:
                File &file;
                ui64 offset;
                /// \struct FileAllocator::BlockInfo::Header FileAllocator.h thekogans/util/FileAllocator.h
                ///
                /// \brief
                struct _LIB_THEKOGANS_UTIL_DECL Header {
                    Flags32 flags;
                    ui64 size;
                    ui64 nextBlockOffset;

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

                    void Read (
                        File &file,
                        ui64 offset);
                    void Write (
                        File &file,
                        ui64 offset);
                } header;
                /// \struct FileAllocator::BlockInfo::Footer FileAllocator.h thekogans/util/FileAllocator.h
                ///
                /// \brief
                struct _LIB_THEKOGANS_UTIL_DECL Footer {
                    Flags32 flags;
                    ui64 size;

                    static const std::size_t SIZE =
                    #if defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                        UI32_SIZE + // magic
                    #endif // defined (THEKOGANS_UTIL_FILE_ALLOCATOR_BLOCK_INFO_USE_MAGIC)
                        UI32_SIZE + // flags
                        UI64_SIZE;  // size

                    Footer (
                        Flags32 flags_ = 0,
                        ui64 size_ = 0) :
                        flags (flags_),
                        size (size_) {}

                    inline bool IsFree () const {
                        return flags.Test (FLAGS_FREE);
                    }
                    inline void SetFree (bool free) {
                        flags.Set (FLAGS_FREE, free);
                    }
                    inline bool IsFixed () const {
                        return flags.Test (FLAGS_FIXED);
                    }
                    inline void SetFixed (bool fixed) {
                        flags.Set (FLAGS_FIXED, fixed);
                    }

                    void Read (
                        File &file,
                        ui64 offset);
                    void Write (
                        File &file,
                        ui64 offset);
                } footer;

            public:
                static const std::size_t FLAGS_FREE = 1;
                static const std::size_t FLAGS_FIXED = 2;
                static const std::size_t HEADER_SIZE = Header::SIZE;
                static const std::size_t FOOTER_SIZE = Footer::SIZE;
                static const std::size_t SIZE = HEADER_SIZE + FOOTER_SIZE;

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

                inline ui64 GetOffset () const {
                    return offset;
                }
                inline void SetOffset (ui64 offset_) {
                    offset = offset_;
                }
                inline bool IsFirst () const {
                    return GetOffset () == FileAllocator::MIN_BLOCK_OFFSET;
                }
                inline bool IsLast () const {
                    return GetOffset () + GetSize () + FOOTER_SIZE == file.GetSize ();
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

                inline ui64 GetNextBlockOffset () const {
                    return header.nextBlockOffset;
                }
                inline void SetNextBlockOffset (ui64 nextBlockOffset) {
                    header.nextBlockOffset = nextBlockOffset;
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
                FileAllocator &fileAllocator;
                BlockInfo block;

            public:
                BlockBuffer (
                    FileAllocator &fileAllocator_,
                    ui64 offset,
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

            struct BTree;

            /// \brief
            /// Default number of entries per node.
            /// NOTE: This is a tunable parameter that should be used
            /// during system integration to provide the best performance
            /// for your needs. Once the heap is created though, this
            /// value is set in stone and the only way to change it is
            /// to delete the file and try again.
            static const std::size_t DEFAULT_BTREE_ENTRIES_PER_NODE = 256;

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
                std::size_t blocksPerPage = DEFAULT_BTREE_ENTRIES_PER_NODE,
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
            /// FileAllocator is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FileAllocator)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocator_h)
