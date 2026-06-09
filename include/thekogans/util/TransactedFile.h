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
#include "thekogans/util/AlignedAllocator.h"
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
        /// TransactedFile is the heart of a structured data management system.
        /// Together with \see{TransactedFile::Allocator}, \see{TransactedFile::Registry},
        /// and \see{TransactedFile::Object} they form the core of an OODBMS engine.
        /// TransactedFile is designed to be used in one of two distinct modes;
        /// 1. Fully structured, taking advantage of the classes mentioned above
        /// and 2. As a very powerful and efficient replacement for \see{File} where
        /// you use the built in page caching machinery to speed up reads and writes
        /// but is otherwise unstructured.
        /// ************************** PLEASE READ ****************************
        /// VERY IMPORTANT: In general, TransactedFile is NOT thread safe. Specifically
        /// the legacy \see{File} API is not. I felt introducing a lock with every API
        /// access would be very costly performance wise. And even if I did, the API
        /// design is such that certain common operations (Seek/Read, Seek/Write) would
        /// still not be safe as they would not execute atomicaly. Instead, a higher
        /// level API (ReadEx, WriteEx, Grow, Shrink) is introduced. Together with
        /// \see{TransactedFile::Range} and it's derivatives and \see{TransactedFile::Allocator}
        /// and it's derivatives the following multi-threaded use case is prescribed;
        ///
        /// main thread;
        ///   open/create file.
        ///   release master threads.
        ///   close file.
        ///
        /// master thread;
        ///   if writting
        ///     start transaction. This will lock the file for exclusive access.
        ///   release worker threads that use \see{TransactedFile::Allocator} to
        ///     allocate blocs and \see{TransactedFile::Range} to read and write them.
        ///     It's highly recommended that you use \see{TransactedFile::TransactionParticipant}
        ///     and \see{TransactedFile::Object}. \see{TransactedFile::Regitry}
        ///     can also be used to store global heap objects based on \see{Serializable}.
        ///   wait for workers to enter a barrier (i.e. signal they're done).
        ///   if writting
        ///     optionally delete cache.
        ///     commit transaction.
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
            /// Set if the page cache is dirty.
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
            /// Forward declaration of \see{Page} needed by \see{PageList}.
            struct Page;

        private:
            /// \brief
            /// Alias for \see{IntrusiveList}<Page>.
            using PageList = IntrusiveList<Page>;

        public:
            /// \struct TransactedFile::Page TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// Page tiles the file address space providing incremental,
            /// sparse access to the data. Use \see{DeleteCache} to manage
            /// memory usage.
            /// WARNING: Spent an hour chasing my tail looking for a stack
            /// overflow bug. Long story short, don't allocate pages on
            /// the stack. They're too big.
            struct Page :
                    public RefCounted,
                    public PageList::Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Page)
                /// \brief
                /// Page has a private heap.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                /// \brief
                /// Page index in \see{Segment::pages}.
                const std::size_t index;
                /// \brief
                /// Page offset (multiple of SIZE).
                const ui64 offset;
                util::Allocator &allocator;
                /// \brief
                /// Page size. This is a tuning parameter.
                /// Set it based on your typical file sizes. Meaning that,
                /// if your files never go above 100KB then a 1MB SIZE
                /// is overkill. But if you typically manage multi gigabyte
                /// (terabyte?) databases you might want to bump this value up.
                /// The way you do it is by going up or down a power of 2 on
                /// SIZE.
                /// Ex: Let's say you wanted 2MB pages instead of 1MB.
                /// Then SIZE = 0x00200000.
                /// Ex: If you need 16MB pages,
                /// Then SIZE = 0x01000000.
                /// Ex: If you need 256KB pages,
                /// Then SIZE = 0x00040000.
                static constexpr std::size_t SIZE = 0x00100000;
                /// \brief
                /// Page data.
                ui8 *data;
                /// \brief
                /// true == modified.
                bool dirty;

                /// \brief
                /// ctor.
                /// \param[in] index_ Page index in \see{Segment::pages}.
                /// \param[in] offset_ Page offset (multiple of SIZE).
                /// \param[in] allocator_ \see{AlignedAllocator} to allocate page data.
                Page (
                        std::size_t index_,
                        ui64 offset_,
                        util::Allocator &allocator_) :
                        index (index_),
                        offset (offset_),
                        allocator (allocator_),
                        dirty (false) {
                    data = (ui8 *)allocator.Alloc (SIZE);
                }
                ~Page () {
                    allocator.Free (data, SIZE);
                }
            };

        private:
            AlignedAllocator pageAllocator;
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
            ///   of 4K 20 bit, 4GB segment tiles (Page). (depth = 5)
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
                /// Delete pages.
                /// \param[in] all true == clear all, false == dirty only.
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (bool all = false) = 0;
                /// \brief
                /// Write dirty pages to either their source or the log.
                /// \param[in] serializer \see{RandomSeekSerializer} to flush to.
                /// \param[in] log true == the serializer is a log and should be treated as such.
                /// false == the serializer is the destination and should be dealt with accordingly.
                virtual void Flush (
                    RandomSeekSerializer &serializer,
                    bool log) = 0;
                /// \brief
                /// Delete all pages whose offset > newSize.
                /// \param[in] newSize New size to clip the address space to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a page was encoutered whose offset was < newSize, stop iterating.
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
                /// The number of \see{Page}s tilling the segments 4GB address space.
                static const std::size_t BRANCHING_LEVEL = ((std::size_t)1 << 32) / Page::SIZE;
                /// \brief
                /// \see{Pages} tiling the 4GB segment.
                Page::SharedPtr pages[BRANCHING_LEVEL];
                /// \brief
                /// \see{IntrusiveList} of linked \see{Page}s.
                PageList pageList;

                /// \brief
                /// cor.
                /// \param[in] index Segment index.
                Segment (std::size_t index) :
                    Node (index) {}

                /// \brief
                /// Delete pages.
                /// \param[in] all true == clear all, false == dirty only.
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (bool all = false) override;
                /// \brief
                /// Write dirty pages to either their source or the log.
                /// \param[in] serializer \see{RandomSeekSerializer} to flush to.
                /// \param[in] log true == serializer is a log and should be treated as such.
                virtual void Flush (
                    RandomSeekSerializer &serializer,
                    bool log) override;
                /// \brief
                /// Delete all pages whose offset > newSize.
                /// \param[in] newSize New size to clip the address space to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a page was encoutered whose offset was < newSize, stop iterating.
                virtual bool Shrink (ui64 newSize) override;

                /// \brief
                /// Return the \see{Page} @index. Create if null.
                /// \param[in] pageIndex Page index in the pages array.
                /// \param[in] pageOffset Page offset (multiple of Page::SIZE).
                /// \param[in] serializer \see{RandomSeekSerializer} to read the page data from.
                /// \return The new page.
                Page::SharedPtr GetPage (
                    ui32 pageIndex,
                    ui64 pageOffset,
                    util::Allocator &pageAllocator,
                    RandomSeekSerializer &serializer);
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
                /// Delete pages.
                /// \param[in] all true == clear all, false == dirty only.
                /// \return true == the node is empty,
                /// false == the node has clean pages remaining.
                virtual bool Clear (bool all = false) override;
                /// \brief
                /// Write dirty pages to either their source or the log.
                /// \param[in] serializer \see{RandomSeekSerializer} to flush to.
                /// \param[in] log true == serializer is a log and should be treated as such.
                virtual void Flush (
                    RandomSeekSerializer &serializer,
                    bool log) override;
                /// \brief
                /// Delete all pages whose offset > newSize.
                /// \param[in] newSize New size to clip the address space to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a page was encoutered whose offset was < newSize, stop iterating.
                virtual bool Shrink (ui64 newSize) override;

                /// \brief
                /// Return (posibly creating) the \see{Page} that contains the given offset.
                /// \param[in] serializer \see{RandomSeekSerializer} the pages data will come from.
                /// \param[in] offset Offset of the page in the file.
                /// \param[in] pageAllocator \see{util::Allocator} where the page buffer will come from.
                /// \return \see{Page} that contains the given offset.
                Page::SharedPtr GetPage (
                    RandomSeekSerializer &serializer,
                    ui64 offset,
                    util::Allocator &pageAllocator);

            private:
                /// \brief
                /// Return either an \see{Internal} scaffolding node
                /// or a \see{Segment} leaf node. Create if null.
                /// \param[in] index Index of node to return.
                /// \param[in] segment If null, true == create \see{Segment},
                /// otherwise create \see{Internal}
                /// \retrun \see{Segment} or \see{Internal} node @index.
                /// NOTE: Unlike pages which are shared outside the file api,
                /// nodes are used internally only by GetPage below which
                /// uses them and lets them go. It's therefore unnecessary
                /// overhead to return a SharedPtr. A raw pointer will do
                /// just fine.
                Node *GetNode (
                    std::size_t index,
                    bool segment = false);
            } root;
            /// \brief
            /// Current page offset.
            /// This is the page offset of the last call to \see{GetPage (offset)}.
            /// (offset & ~(Page::SIZE - 1))
            ui64 currPageOffset;
            /// \brief
            /// Current page cache.
            /// NOTE: The design of \see{GetPage} is such that it takes ~5 shfts and ~5
            /// ands to do it's job. Unfortunatelly this function is called every time
            /// you call Read or Write. If you're doing a lot of inserting/extracting
            /// of small types, that can add up. That's why this cache exists. If you're
            /// going to do a lot of 'local' io, GetPage will be reduced to a single
            /// and and compare. Keep that in mind when designing your data structures
            /// and access patterns. The less you Seek, the better your performance
            /// will be. To that end, you're highly encouraged to use \see{Allocator}
            /// and \see{Range}.
            Page::SharedPtr currPage;

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
            /// Since allocator is constant after Init, this API
            /// is thread safe.
            /// \return allocator.
            inline Allocator::SharedPtr GetAllocator () const {
                return allocator;
            }
            /// \brief
            /// Return the \see{Registry} attached to thi file.
            /// Since registry is constant after Init, this API
            /// is thread safe.
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
            /// NOT thread safe.
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
            /// would not be atomic without locking across multiple system calls. Therefore,
            /// I provide the *Ex versions of Read and Write. These apis are left as being
            /// NOT thread safe.

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
            /// This is a wrapper around GetPageHelper.
            /// Thread safe.
            /// \param[in] offset Offset whose page to return.
            /// \return Page that covers the neighborhood around the given offset.
            Page::SharedPtr GetPage (ui64 offset);
            /// \brief
            /// Get the page that will cover the neighborhood around the given offset.
            /// NOT thread safe.
            /// \param[in] offset Offset whose page to return.
            /// \return Page that covers the neighborhood around the given offset.
            Page::SharedPtr GetPageHelper (ui64 offset);

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
