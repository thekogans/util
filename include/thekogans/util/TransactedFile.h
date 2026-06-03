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

#if !defined (__thekogans_util_TransactedFile_h)
#define __thekogans_util_TransactedFile_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/File.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Producer.h"
#include "thekogans/util/SecureAllocator.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Forward declaration of \see{TransactedFile} needed by \see{TransactedFileEvents}.
        struct TransactedFile;
        /// \brief
        /// Forward declaration of \see{SimpleTransactedFile} needed by \see{TransactedFile}.
        struct SimpleTransactedFile;

        /// \struct TransactedFileEvents TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        /// Subscribe to TransactedFileEvents to receive transaction notifications.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileEvents {
            /// \brief
            /// dtor.
            virtual ~TransactedFileEvents () {}

            /// \brief
            /// Transaction is beginning. Time to flush the internal cache.
            /// If your object derives from \see{TransactedFile::TransactionParticipant}
            /// all this is done under the hood for you. All you will need
            /// to do is implement Flush.
            virtual void OnTransactedFileTransactionBegin (
                RefCounted::SharedPtr<TransactedFile> /*file*/) noexcept {}
            /// \brief
            /// Transaction is committing. Depending on the phase do whatever
            /// is appropriate.
            /// If your object derives from \see{TransactedFile::TransactionParticipant}
            /// all this is done under the hood for you. All you will need
            /// to do is implement Alloc (phase 1) and Flush (phase 2).
            /// \param[in] phase Either COMMIT_PHASE_1 or COMMIT_PHASE_2.
            virtual void OnTransactedFileTransactionCommit (
                RefCounted::SharedPtr<TransactedFile> /*file*/,
                int phase) noexcept {}
            /// \brief
            /// Transaction is aborting. Time to reload the object.
            /// If your object derives from \see{TransactedFile::TransactionParticipant}
            /// all this is done under the hood for you. All you will need
            /// to do is implement Reload.
            virtual void OnTransactedFileTransactionAbort (
                RefCounted::SharedPtr<TransactedFile> /*file*/) noexcept {}
        };

        /// \struct TransactedFile TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        /// TransactedFile is a drop in replacement for \see{File}. TransactedFile
        /// will accumulate all changes in memory and will commit them all at
        /// once in \see{Flush} or \see{CommitTransaction}. TransactedFile has
        /// support for simple, flat (not nested) transactions (see \see{BeginTransaction}).
        /// TransactedFile design principle is; if you have it, might as well use it.
        /// Meaning today's (early March 2025) state of the art has some servers
        /// sporting up to 6TB of main memory. All that memory is there for a
        /// reason. You paid good money for it. TransactedFile will use as much
        /// as you have (give it). By default, TransactedFile uses 1MB tiles (\see{Buffer}).
        /// It will load and hold available as much of the file as you have room.
        /// That's where \see{Flush}, \see{Transaction::Commit} and \see{DeleteCache}
        /// come in. As you eventually start to run out of room (even with 6TB it's
        /// a drop in the bucket compared to 64 bit address space), call \see{Flush}
        /// to commit the changes to disk followed by a call to \see{DeleteCache} to
        /// release the cache. While with proper tuning TransactedFile should work just
        /// fine for 'small' (under 1MB) files, it's strength lies with it's ability
        /// to handle multi GB or even TB files with ease. It's hierarchical address
        /// space partitioning allows for very efficient, sparse file handling.
        /// ************************** PLEASE READ ****************************
        /// VERY IMPORTANT: TransactedFile is NOT thread safe. I felt introducing
        /// a lock with every file access would be very costly performance wise.
        /// Instead, TransactedFile exposes two types of guards; a read only guard
        /// (\see{LockGuard}<\see{Mutex}>) and a read/write guard (\see{TransactedFile::Transaction}).
        /// Use the first to gain exclusive read access to the file's data. Use
        /// the later for exclusive modify access. This design choice has the
        /// following advantages;
        /// 1. Use the lock to protect a set of related read/write operations
        /// instead of the file locking with every call.
        /// 2. Having a thread complete all it's reads and writes without
        /// interruptions is faster than having different threads contend for the
        /// lock as there is no context switching and less disk head movement.
        /// This also plays nice with the currBuffer cache as it promotes locality
        /// of reference.
        /// The one drawback of this design choice is;
        /// The various threads sharing the same TransactedFile have to consciously
        /// cooperate with eachother (by using the provided guards). On the other
        /// hand, if you only have one thread accessing the file, there's nothing
        /// to do and no need to pay the cost of a lock.
        /// *******************************************************************
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFile :
                public File,
                public Producer<TransactedFileEvents> {
            /// \brief
            /// TransactedFile participates in the \see{DynamicCreatable}
            /// dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (TransactedFile)

            /// \brief
            /// TransactedFile implements a two phase commit. The
            /// first phase (usually called the allocation phase)
            /// will have all \see{Object}s allocate the space they
            /// need to commit themselves to disk. The space is
            /// allocated using ane \see{Allocator}. This is also
            /// the time when all offset pointers are updated (Again
            /// see \see{Object} and \see{ObjectEvents}). The second
            /// phase (usually called the commit phase) will have
            /// all objects flush themselves to disk. This logic is
            /// enshrined in \see{TransactionParticipant} below.

            /// \brief
            /// Commit phase 1 (allocation).
            static const int COMMIT_PHASE_1;
            /// \brief
            /// Commit phase 2 (flush).
            static const int COMMIT_PHASE_2;

            /// \struct TransactedFile::Transaction TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// A very simple transaction scope guard. Will call
            /// \see{BeginTransaction} in it's ctor and \see{AbortTransaction}
            /// in it's dtor. Call Commit before the end of the
            /// scope to commit the transaction.
            /// NOTE: If you're only reading the file, this is
            /// overkill. Use \see{LockGuard}<{\see{Mutex}> (file->\see{GetLock} ())
            /// instead.
            struct _LIB_THEKOGANS_UTIL_DECL Transaction {
            private:
                /// \see{TransactedFile} we're guarding.
                TransactedFile::SharedPtr file;
                /// \brief
                /// This guard will serialize all transactions.
                LockGuard<Mutex> guard;

            public:
                /// \brief
                /// ctor.
                /// \param[in] file_ \see{TransactedFile} to transact.
                explicit Transaction (TransactedFile::SharedPtr file_);
                /// \brief
                /// dtor.
                ~Transaction ();

                /// \brief
                /// Commit the transaction before the dtor aborts it.
                void Commit ();

                /// \brief
                /// Transaction is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Transaction)
            };

        private:
            /// \brief
            /// Current read/write position.
            i64 position;
            /// \brief
            /// File size.
            ui64 size;
            /// \brief
            /// Set if the buffer cache is dirty.
            static const ui32 FLAGS_DIRTY = 1;
            /// \brief
            /// Set if we're in the middle of a transaction.
            static const ui32 FLAGS_TRANSACTION = 2;
            /// \brief
            /// Combination of the above flags.
            Flags32 flags;
            /// \brief
            /// For use by \see{Transaction}.
            Mutex mutex;
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            /// \brief
            /// Forward declaration of \see{Buffer} needed by \see{BufferList}.
            struct Buffer;

        private:
            /// \brief
            /// Alias for \see{IntrusiveList}<Buffer>.
            using BufferList = IntrusiveList<Buffer>;

        public:
            /// \struct TransactedFile::Buffer TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// Buffer tiles the file address space providing incremental,
            /// sparse access to the data. Use \see{DeleteCache} to manage
            /// memory usage.
            /// WARNING: Spent an hour chasing my tail looking for a stack
            /// overflow bug. Long story short, don't allocate buffers on
            /// the stack. They're too big.
            struct Buffer :
                    public RefCounted,
                    public BufferList::Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Buffer)
                /// \brief
                /// Buffer has a private heap.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                /// \brief
                /// Buffer index in \see{Segment::buffers}.
                const std::size_t index;
                /// \brief
                /// Buffer offset (multiple of SIZE).
                const ui64 offset;
                /// \brief
                /// Buffer size. This is a tuning parameter.
                /// Set it based on your typical file sizes. Meaning that,
                /// if your files never go above 100KB then a 1MB SIZE
                /// is overkill. But if you typically manage multi gigabyte
                /// (terabyte?) databases you might want to bump this value up.
                /// The way you do it is by going up or down a power of 2 on
                /// SIZE.
                /// Ex: Let's say you wanted 2MB buffers instead of 1MB.
                /// Then SIZE = 0x00200000.
                /// Ex: If you need 16MB buffers,
                /// Then SIZE = 0x01000000.
                /// Ex: If you need 256KB buffers,
                /// Then SIZE = 0x00040000.
                static constexpr std::size_t SIZE = 0x00100000;
                /// \brief
                /// Buffer data.
                ui8 data[SIZE];
                /// \brief
                /// true == modified.
                bool dirty;

                /// \brief
                /// ctor.
                /// \param[in] index_ Buffer index in \see{Segment::buffers}.
                /// \param[in] offset_ Buffer offset (multiple of SIZE).
                Buffer (
                    std::size_t index_,
                    ui64 offset_) :
                    index (index_),
                    offset (offset_),
                    dirty (false) {}
            };

        private:
            /// \brief
            /// Forward declaration of \see{Node} needed by NodeList.
            struct Node;
            /// \brief
            /// Alias for \see{IntrusiveList}<Node>.
            using NodeList = IntrusiveList<Node>;

            /// \struct TransactedFile::Node TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// The file 64 bit address space is partitioned such that it can be
            /// represented using a fixed depth multiway tree. The high order 32 bits
            /// are used to represent 4G of 4GB segments. The low order 32 bits are
            /// used to represent 4GB segments partitioned in to 4K of 1MB tiles.
            /// The entire address space is sparse and fits in to a tree of depth 5
            /// like this;
            /// - The high order 32 bits are split in to 4 8 bit hierarchical indexes.
            ///   Each index is used to access an internal tree node. (depth = 4).
            /// - The low order 32 bits are split in to a 12 bit index to access one
            ///   of 4K 20 bit, 4GB segment tiles (Buffer). (depth = 5)
            struct Node :
                    public RefCounted,
                    public NodeList::Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Node)

                /// \brief
                /// Node index in \see{Internal::nodes}.
                std::size_t index;

                /// \brief
                /// ctor.
                /// \param[in] index_ Node index in \see{Internal::nodes}.
                Node (std::size_t index_) :
                    index (index_) {}

                /// \brief
                /// Delete buffers.
                /// \param[in] all true == clear all, false == dirty only.
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (bool all = false) = 0;
                /// \brief
                /// Write dirty buffers to log.
                /// \param[in] log Log \see{File} to save to.
                virtual void Log (File &log) = 0;
                /// \brief
                /// Write dirty buffers to file.
                /// \param[in] file \see{File} to save to.
                virtual void Flush (
                    File &file,
                    ui64 size) = 0;
                /// \brief
                /// Delete all buffers whose offset > newSize.
                /// \param[in] newSize New size to clip the node to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a buffer was encoutered whose offset was < newSize, stop iterating.
                virtual bool Shrink (ui64 newSize) = 0;
            };

            /// \struct TransactedFile::Segment TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// Leaf node representing a 4GB chunk of the file.
            struct Segment : public Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Segment)
                /// \brief
                /// Segment has a private heap.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                /// \brief
                /// The number of \see{Buffer}s tilling the segments 4GB address space.
                static const std::size_t BRANCHING_LEVEL = ((std::size_t)1 << 32) / Buffer::SIZE;
                /// \brief
                /// \see{Buffers} tiling the 4GB segment.
                Buffer::SharedPtr buffers[BRANCHING_LEVEL];
                /// \brief
                /// \see{IntrusiveList} of linked \see{Buffer}s.
                BufferList bufferList;

                /// \brief
                /// cor.
                /// \param[in] index Segment index.
                Segment (std::size_t index) :
                    Node (index) {}

                /// \brief
                /// Delete buffers.
                /// \param[in] all true == clear all, false == dirty only.
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (bool all = false) override;
                /// \brief
                /// Write dirty buffers to log.
                /// \param[in] log Log \see{File} to save to.
                virtual void Log (File &log) override;
                /// \brief
                /// Write dirty buffers to file.
                /// \param[in] file \see{File} to save to.
                virtual void Flush (
                    File &file,
                    ui64 size) override;
                /// \brief
                /// Delete all buffers whose offset > newSize.
                /// \param[in] newSize New size to clip the node to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a buffer was encoutered whose offset was < newSize, stop iterating.
                virtual bool Shrink (ui64 newSize) override;

                /// \brief
                /// Return the \see{Buffer} @index. Create if null.
                /// \param[in] bufferIndex Buffer index in the buffers array.
                /// \param[in] bufferOffset Buffer offset (multiple of Buffer::SIZE).
                /// \param[in] file File to read the buffer data from.
                /// \return The new buffer.
                Buffer::SharedPtr GetBuffer (
                    ui32 bufferIndex,
                    ui64 bufferOffset,
                    File &file);
            };

            /// \struct TransactedFile::Internal TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// Internal structure node representing 4G of 4GB segments.
            struct Internal : public Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Internal)
                /// \brief
                /// Internal has a private heap.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                /// \brief
                /// The upper 32 bits of the 64 bit address is split
                /// evenly in to 4 8 bit higherarchical indeces. That's
                /// what makes the first 4 layers of the 5 layer deep tree.
                static const std::size_t BRANCHING_LEVEL = 0x00000100;
                /// \brief
                /// These are the internal 4G of 4GB segments.
                Node::SharedPtr nodes[BRANCHING_LEVEL];
                /// \brief
                /// \see{IntrusiveList} of \see{Node}s.
                NodeList nodeList;

                /// \brief
                /// ctor.
                /// \param[in] index Node index.
                Internal (std::size_t index) :
                    Node (index) {}

                /// \brief
                /// Delete buffers.
                /// \param[in] all true == clear all, false == dirty only.
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (bool all = false) override;
                /// \brief
                /// Write dirty buffers to log.
                /// \param[in] log Log \see{File} to save to.
                virtual void Log (File &log) override;
                /// \brief
                /// Write dirty buffers to file.
                /// \param[in] file \see{File} to save to.
                virtual void Flush (
                    File &file,
                    ui64 size) override;
                /// \brief
                /// Delete all buffers whose offset > newSize.
                /// \param[in] newSize New size to clip the node to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a buffer was encoutered whose offset was < newSize, stop iterating.
                virtual bool Shrink (ui64 newSize) override;

                /// \brief
                /// Return either an \see{Internal} scaffolding node
                /// or a \see{Segment} leaf node. Create if null.
                /// \param[in] index Index of node to return.
                /// \param[in] segment If null, true == create \see{Segment},
                /// otherwise create \see{Internal}
                /// \retrun \see{Segment} or \see{Internal} node at index.
                Node::SharedPtr GetNode (
                    ui8 index,
                    bool segment = false);

                Buffer::SharedPtr GetBuffer (
                    File &file,
                    ui64 offset);
            } root;
            /// \brief
            /// Current buffer offset.
            /// This is the buffer offset of the current position. (position & ~(Buffer::SIZE - 1))
            ui64 currBufferOffset;
            /// \brief
            /// Current buffer cache.
            /// NOTE: The design of GetBuffer is such that it takes ~5 shfts and ~5
            /// ands to do it's job. Unfortunatelly this function is called every time
            /// you call Read or Write. If you're doing a lot of inserting/extracting
            /// of small types, that can add up. That's why this cache exists. If you're
            /// going to do a lot of 'local' io, GetBuffer will be reduced to a single
            /// and and compare. Keep that in mind when designing your data structures
            /// and access patterns. The less you Seek, the better your performance
            /// will be. To that end, you're highly encouraged to use \see{Allocator}
            /// and \see{Range}.
            Buffer::SharedPtr currBuffer;

        public:
            #include "thekogans/util/TransactedFileRange.h"
            #include "thekogans/util/TransactedFileAllocator.h"
            #include "thekogans/util/TransactedFileObject.h"
            #include "thekogans/util/TransactedFileRegistry.h"

        private:
            /// \brief
            /// \see{Allocator} associated with this file.
            Allocator::SharedPtr allocator;
            /// \brief
            /// \see{Registry} associated with this file.
            Registry::SharedPtr registry;

        public:
            /// \brief
            /// ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            /// \param[in] allocator \see{Allocator} to attach to this file.
            /// \param[in] registry \see{Registry} to attach to this file.
            TransactedFile (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path = std::string (),
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr);
            /// \brief
            /// ctor. Open or create the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
        #if defined (TOOLCHAIN_OS_Windows)
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] allocator \see{Allocator} to attach to this file.
            /// \param[in] registry \see{Registry} to attach to this file.
            TransactedFile (
                Endianness endianness,
                const std::string &path,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                DWORD dwCreationDisposition = OPEN_ALWAYS,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL,
            #else // defined (TOOLCHAIN_OS_Windows)
                i32 flags = O_RDWR | O_CREAT,
                i32 mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
            #endif // defined (TOOLCHAIN_OS_Windows)
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr);
            /// \brief
            /// dtor. Close the file.
            virtual ~TransactedFile ();

            /// \brief
            /// Return the \see{Allocator} attached to thi file.
            /// \return allocator.
            inline Allocator::SharedPtr GetAllocator () const {
                return allocator;
            }
            /// \brief
            /// Return the \see{Registry} attached to thi file.
            /// \return registry.
            inline Registry::SharedPtr GetRegistry () const {
                return registry;
            }

            /// \brief
            /// Thread safe version of Read.
            /// \param[in] offset Start of range.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            std::size_t ReadEx (
                ui64 offset,
                void *buffer,
                std::size_t count);
            /// \brief
            /// Thread safe version of Write.
            /// \param[in] offset Start of range.
            /// \param[in] buffer Where bytes come from.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            std::size_t WriteEx (
                ui64 offset,
                const void *buffer,
                std::size_t count);

            /// \brief
            /// Open or create the file.
            /// \param[in] path Path to file to open.
        #if defined (TOOLCHAIN_OS_Windows)
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] allocator_ \see{Allocator} to attach to this file.
            /// \param[in] registry_ \see{Registry} to attach to this file.
            /// NOT thread safe.
            void OpenEx (
                const std::string &path,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE,
                DWORD dwCreationDisposition = OPEN_EXISTING,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL,
            #else // defined (TOOLCHAIN_OS_Windows)
                i32 flags = O_RDWR,
                i32 mode = S_IRUSR | S_IWUSR,
            #endif // defined (TOOLCHAIN_OS_Windows)
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr);

            /// \brief
            /// Flush dirty pages and delete the cache.
            /// NOT thread safe.
            void DeleteCache ();
            /// \brief
            /// Grow the file by the given amount.
            /// Thread safe.
            /// \param[in] amount Amount to grow the file by.
            /// \return Old file size.
            ui64 Grow (ui64 amount);
            /// \brief
            /// Shrink the file by the given amount.
            /// Thread safe.
            /// \param[in] amount Amount to shrink the file by.
            /// \return New file size.
            ui64 Shrink (ui64 amount);

            // Serializer
            /// NOTE: This is a legacy \see{File} api which is inherently NOT thread safe.
            /// Even if you would make all apis thread safe, the design (shared position)
            /// is such as to make certain operations (Seek/Read/Write) imposible as they
            /// would not be atomic. Therefore, I provide the *Ex versions of Read and Write.
            /// These apis are left as being NOT thread safe.

            /// \brief
            /// Read bytes from a file.
            /// NOT thread safe.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual std::size_t Read (
                void *buffer,
                std::size_t count) override;
            /// \brief
            /// Write bytes to a file.
            /// NOT thread safe.
            /// \param[in] buffer Where the bytes come from.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual std::size_t Write (
                const void *buffer,
                std::size_t count) override;

            // RandomSeekSerializer
            /// \brief
            /// Return the file pointer position.
            /// NOT thread safe.
            /// \return The file pointer position.
            virtual i64 Tell () const override {
                return position;
            }
            /// \brief
            /// Reposition the file pointer.
            /// NOT thread safe.
            /// \param[in] offset Offset to move relative to fromWhere.
            /// \param[in] fromWhere SEEK_SET, SEEK_CUR or SEEK_END.
            /// \return The new file pointer position.
            virtual i64 Seek (
                i64 offset,
                i32 fromWhere) override;

            // File
            /// \brief
            /// Open the file.
            /// NOT thread safe.
            /// \param[in] path File path.
        #if defined (TOOLCHAIN_OS_Windows)
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
        #endif // defined (TOOLCHAIN_OS_Windows)
            virtual void Open (
                const std::string &path,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE,
                DWORD dwCreationDisposition = OPEN_EXISTING,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL) override;
            #else // defined (TOOLCHAIN_OS_Windows)
                i32 flags = O_RDWR,
                i32 mode = S_IRUSR | S_IWUSR) override;
            #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Close the file.
            /// NOT thread safe.
            virtual void Close () override;
            /// \brief
            /// Flush pending writes to disk.
            /// NOT thread safe.
            virtual void Flush () override;

            /// \brief
            /// Return number of bytes available for reading.
            /// NOT thread safe.
            /// \return Number of bytes available for reading.
            virtual ui64 GetDataAvailableForReading () const override {
                return size > (ui64)position ? size - (ui64)position : 0;
            }

            /// \brief
            /// Return file size in bytes.
            /// NOT thread safe.
            /// \return File size in bytes.
            virtual ui64 GetSize () const override {
                return size;
            }
            /// \brief
            /// Truncates or expands the file.
            /// NOT Thread safe.
            /// \param[in] newSize New size to set the file to.
            virtual void SetSize (ui64 newSize) override;

            /// \brief
            /// Lock a range of bytes in the file.
            /// Since there's no direct access to the file,
            /// TransactedFile does not support region locking.
            /// \region[in] region region to lock.
            /// \region[in] exclusive lock for exclusive access.
            virtual void LockRegion (
                const Region & /*region*/,
                bool /*exclusive*/) override {}
            /// \brief
            /// Unlock a range of bytes in the file.
            /// See comment above LockRegion.
            /// \region[in] region region to unlock.
            virtual void UnlockRegion (const Region & /*region*/) override {}

        private:
            /// \brief
            /// Called during file open. If the file is empty and an
            /// \see{Allocator} and an optional \see{Registry} were
            /// provided, they are used to initialize the file (make
            /// is strucured). If the file is not empty and at least
            /// an \see{Allocator} was provided, the file is assumed
            /// to be structured and the \see{Allocator} and an optional
            /// \see{Registry} will come from the file. If no \see{Allocator}
            /// was provided the file is assumed to be unstrucured.
            /// \param[in] allocator_ \see{Allocator} to attach to this file.
            /// \param[in] registry_ \see{Registry} to attach to this file.
            /// NOT thread safe.
            void Init (
                Allocator::SharedPtr allocator_,
                Registry::SharedPtr registry_);

            /// \brief
            /// Used by Open to apply the log before opening the file.
            /// \path path File path.
            static void CommitLog (const std::string &path);

            /// \brief
            /// Return true if we have unwriten changes in our cache.
            /// NOT thread safe.
            /// \return true == we have unwriten changes in our cache.
            inline bool IsDirty () const {
                return flags.Test (FLAGS_DIRTY);
            }
            /// \brief
            /// Set/reset the dirty flag.
            /// NOT thread safe.
            /// \param[in] dirty true == set, false == reset.
            inline void SetDirty (bool dirty) {
                flags.Set (FLAGS_DIRTY, dirty);
            }
            /// \brief
            /// Return true if we're in the middle of a transaction.
            /// NOT thread safe.
            /// \return true == we're in the middle of a transaction.
            inline bool IsTransactionPending () const {
                return flags.Test (FLAGS_TRANSACTION);
            }
            /// \brief
            /// Set/reset the transaction flag.
            /// NOT thread safe.
            /// \param[in] transaction true == set, false == reset.
            inline void SetTransactionPending (bool transaction) {
                flags.Set (FLAGS_TRANSACTION, transaction);
            }
            /// \brief
            /// Start a new transaction.
            /// Used by \see{Transaction} ctor to mark a modify scope.
            /// NOTE: Must only be called after acquiring the lock.
            /// NOT thread safe.
            void BeginTransaction ();
            /// \brief
            /// Commit the current transaction.
            /// Used by \see{Transaction} to commit the current changes.
            /// NOT thread safe.
            void CommitTransaction ();
            /// \brief
            /// Abort the current transaction.
            /// Used by \see{Transaction} dtor to abort uncommitted changes.
            /// NOT thread safe.
            void AbortTransaction ();

            /// \brief
            /// This is a wrapper around GetBufferHelper.
            /// Thread safe.
            /// \param[in] offset Offset whose buffer to return.
            /// \return Buffer that covers the neighborhood around the given offset.
            Buffer::SharedPtr GetBuffer (ui64 offset);
            /// \brief
            /// Get the buffer that will cover the neighborhood around the given offset.
            /// NOT thread safe.
            /// \param[in] offset Offset whose buffer to return.
            /// \return Buffer that covers the neighborhood around the given offset.
            Buffer::SharedPtr GetBufferHelper (ui64 offset);

            /// \brief
            /// Given a file path, use the full file name to create
            /// a GUID to append to the path to act as the log path.
            /// Thread safe.
            /// \param[in] path File path to turn in to log path.
            /// \return Log path based on the passed in file path.
            static std::string GetLogPath (const std::string &path);

            /// \brief
            /// Needs access to allocator and regitry.
            friend struct SimpleTransactedFile;

            /// \brief
            /// TransactedFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TransactedFile)
        };

        /// \brief
        /// Implement \see{TransactedFile::Allocator} extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS (TransactedFile::Allocator)

        /// \brief
        /// Implement \see{TransactedFile::Registry} extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS (TransactedFile::Registry)

        /// \struct SimpleTransactedFile TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        /// SimpleTransactedFile exposes the basic flags supported by the standard
        /// library open that are portable across Windows, Linux and OS X.
        /// NOTE: On Linux and OS X if a file needs to be created, it's
        /// mode will be 0644. This is fine for most cases but might not
        /// be appropriate for some. If you need to control the mode of
        /// the created file use TransactedFile instead.
        struct _LIB_THEKOGANS_UTIL_DECL SimpleTransactedFile : public TransactedFile {
            /// \brief
            /// Default ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            SimpleTransactedFile (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path = std::string (),
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr) :
                TransactedFile (endianness, handle, path, allocator, registry) {}
            /// \brief
            /// ctor. Abstracts most useful functionality from POSIX open.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] flags Most useful POSIX open flags.
            SimpleTransactedFile (
                Endianness endianness,
                const std::string &path,
                Flags32 flags = SimpleFile::ReadWrite | SimpleFile::Create,
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr);

            /// \brief
            /// Open the file.
            /// \param[in] path File path.
            /// \param[in] flags File open flags.
            void SimpleOpen (
                const std::string &path,
                Flags32 flags = SimpleFile::ReadWrite | SimpleFile::Create,
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr);

            /// \brief
            /// SimpleTransactedFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SimpleTransactedFile)
        };

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_TransactedFile_h
