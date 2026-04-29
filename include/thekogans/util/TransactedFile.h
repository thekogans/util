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
#include "thekogans/util/Flags.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/File.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/Producer.h"
#include "thekogans/util/SecureAllocator.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Forward declaration of \see{TransactedFile} needed by \see{TransactedFileEvents}.
        struct TransactedFile;

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
            /// will have all objects allocate the space they need
            /// to commit themselves to disk. How the space is
            /// allocated is outside the scope of the TransactedFile
            /// (See \see{FileAllocator} for one such example).
            /// This is also the time when all offset pointers
            /// are updated (Again see \see{FileAllocator::Object}
            /// and \see{FileAllocator::ObjectEvents}). The second
            /// phase (usually called the commit phase) will have
            /// all objects flush themselves to disk. This logic is
            /// enshrined in \see{TransactionParticipant} below.

            /// \brief
            /// Commit phase 1 (allocation).
            static const int COMMIT_PHASE_1 = 1;
            /// \brief
            /// Commit phase 2 (flush).
            static const int COMMIT_PHASE_2 = 2;

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

            /// \struct TransactedFile::TransactionParticipant TransactedFile.h
            /// thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// TransactionParticipants are objects that listen to
            /// \see{TransactedFileEvents} and are able to flush and reload
            /// themselves to and from a \see{TransactedFile}.
            struct _LIB_THEKOGANS_UTIL_DECL TransactionParticipant :
                    public Subscriber<TransactedFileEvents> {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (TransactionParticipant)

            protected:
                /// \brief
                /// \see{TransactedFile} whose \see{TransactedFileEvents}
                /// we're participants of.
                TransactedFile::SharedPtr file;
                /// \brief
                /// Set if the internal cache is dirty.
                static const ui32 FLAGS_DIRTY = 1;
                /// \brief
                /// Set if we've been deleted.
                static const ui32 FLAGS_DELETED = 2;
                /// \brief
                /// Combination of the above flags.
                Flags32 flags;

            public:
                /// \brief
                /// ctor.
                /// \param[in] file_ \see{TransactedFile} we're a transaction participant of.
                TransactionParticipant (TransactedFile::SharedPtr file_) :
                    file (file_),
                    flags (0) {}
                /// \brief
                /// dtor.
                virtual ~TransactionParticipant () {}

                /// \brief
                /// Return the file.
                /// \return file.
                inline TransactedFile::SharedPtr GetFile () const {
                    return file;
                }

                /// \brief
                /// Return dirty.
                /// \return dirty.
                inline bool IsDirty () const {
                    return flags.Test (FLAGS_DIRTY);
                }
                /// \brief
                /// Set the dirty flag, preserving the state of the deleted flag.
                /// \param[in] dirty true == dirty, false == clean.
                inline void SetDirty (bool dirty) {
                    SetFlags ((dirty ? FLAGS_DIRTY : 0) | (IsDeleted () ? FLAGS_DELETED : 0));
                }

                /// \brief
                /// Return deleted.
                /// \return deleted.
                inline bool IsDeleted () const {
                    return flags.Test (FLAGS_DELETED);
                }

                /// \brief
                /// Delete the disk image and reset the internal state.
                void Delete ();

            protected:
                // NOTE: The following API abstracts out the protocol called for in
                // OnTransactedFileTransactionCommit, OnTransactedFileTransactionAbort
                // and Delete.

                /// \brief
                /// Allocate space from file.
                virtual void Alloc () = 0;
                /// \brief
                /// Free the on disk image.
                virtual void Free () = 0;
                /// \brief
                /// Flush the internal cache to file.
                virtual void Flush () = 0;
                /// \brief
                /// Reload the internal cache from file.
                virtual void Reload () = 0;
                /// \brief
                /// Reset internal state.
                virtual void Reset () = 0;

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

            private:
                /// \brief
                /// Set the deleted flag, preserving the state of the dirty flag.
                /// \param[in] deleted true == deleted, false == alive.
                inline void SetDeleted (bool deleted) {
                    SetFlags ((IsDirty () ? FLAGS_DIRTY : 0) | (deleted ? FLAGS_DELETED : 0));
                }
                /// \brief
                /// Set the flags.
                /// \param[in] flags_ New flags value.
                void SetFlags (ui32 flags_);

                /// \brief
                /// TransactionParticipant is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TransactionParticipant)
            };

        private:
            /// \brief
            /// Current read/write position.
            i64 position;
            /// \brief
            /// File size on disk.
            ui64 sizeOnDisk;
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
            Allocator::SharedPtr allocator;
            /// \brief
            /// For use by \see{Guard} and \see{Transaction}
            Mutex mutex;

            /// \struct TransactedFile::Buffer TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// Buffer tiles the file address space providing incremental,
            /// sparse access to the data. Use \see{DeleteCache} to manage
            /// memory usage.
            /// WARNING: Spent an hour chasing my tail looking for a stack
            /// overflow bug. Long story short, don't allocate buffers on
            /// the stack. They're too big.
            struct Buffer {
                /// \brief
                /// Buffer has a private heap.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                /// \brief
                /// Buffer offset (multiple of SIZE).
                ui64 offset;
                /// \brief
                /// Buffer length (max SIZE).
                ui64 length;
                /// \brief
                /// Buffer size. This (and the coresponding
                /// \see{SHIFT_COUNT} below) is a tunning parameter.
                /// Set it based on your typical file sizes. Meaning that,
                /// if your files never go above 100KB then a 1MB SIZE
                /// is overkill. But if you typically manage multi gigabyte
                /// (terabyte?) databases you might want to bump this value up.
                static const std::size_t SIZE = 0x00100000;
                /// \brief
                /// Look at \see{SIZE} above. This number
                /// represents the trailing zeros in binary.
                static const std::size_t SHIFT_COUNT = 20;
                /// \brief
                /// Buffer.
                ui8 data[SIZE];
                /// \brief
                /// true == modified.
                bool dirty;

                /// \brief
                /// ctor.
                /// \param[in] offset_ Buffer offset (multiple of SIZE).
                /// \param[in] length_ Buffer length (max SIZE).
                Buffer (
                    ui64 offset_ = 0,
                    ui64 length_ = 0) :
                    offset (offset_),
                    length (length_),
                    dirty (false) {}
            };

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
            struct Node {
                /// \brief
                /// dtor.
                virtual ~Node () {}

                /// \brief
                /// Delete the buffer cache.
                virtual void Delete () = 0;
                /// \brief
                /// Delete the dirty buffers.
                virtual void Clear () = 0;
                /// \brief
                /// Write dirty buffers to log.
                /// \param[in] log Log \see{File} to save to.
                /// \param[out] count Incremental count of the dirty buffers.
                virtual void Save (
                    File &log,
                    ui64 &count) = 0;
                /// \brief
                /// Write dirty buffers to file.
                /// \param[in] file \see{File} to save to.
                virtual void Flush (File &file) = 0;
                /// \brief
                /// Delete all buffers whose offset > newSize.
                /// \param[in] newSize New size to clip the node to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a buffer was encoutered whose offset was < newSize, stop iterating.
                virtual bool SetSize (ui64 newSize) = 0;
                /// \brief
                /// Return either an \see{Internal} scafolding node
                /// or a \see{Segment} leaf node. Create if null.
                /// \param[in] index Index of node to return.
                /// \param[in] segment If null, true == create \see{Segment},
                /// otherwise create \see{Internal}
                /// \retrun \see{Segment} or \see{Internal} node at index.
                virtual Node *GetNode (
                    ui8 index,
                    bool segment = false) = 0;
            };

            /// \struct TransactedFile::Segment TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// Leaf node representing a 4GB chunk of the file.
            struct Segment : public Node {
                /// \brief
                /// Segment has a private heap.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                /// \brief
                /// The segment is split in to 4K of 1MB tiles (\see{Buffer}).
                static const std::size_t BRANCHING_LEVEL = 0x00001000;
                /// \brief
                /// \see{Buffers} tiling the 4GB segment.
                Buffer *buffers[BRANCHING_LEVEL];

                /// \brief
                /// ctor.
                Segment () {
                    SecureZeroMemory (buffers, sizeof (buffers));
                }
                /// \brief
                /// dtor.
                virtual ~Segment ();

                /// \brief
                /// Delete the buffer cache.
                virtual void Delete () override;
                /// \brief
                /// Delete dirty buffers.
                virtual void Clear () override;
                /// \brief
                /// Write dirty buffers to log.
                /// \param[in] log Log \see{File} to save to.
                /// \param[out] count Incremental count of the dirty buffers.
                virtual void Save (
                    File &log,
                    ui64 &count) override;
                /// \brief
                /// Write dirty buffers to file.
                /// \param[in] file \see{File} to save to.
                virtual void Flush (File &file) override;
                /// \brief
                /// Delete all buffers whose offset > newSize.
                /// \param[in] newSize New size to clip the node to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a buffer was encoutered whose offset was < newSize, stop iterating.
                virtual bool SetSize (ui64 newSize) override;
                /// \brief
                /// We're a leaf. We don't have any children.
                virtual Node *GetNode (
                        ui8 /*index*/,
                        bool /*segment*/ = false) override {
                    assert (0);
                    return nullptr;
                }

            private:
                /// \brief
                /// Delete a buffer at index.
                /// \param[in] index Index of buffer to delete.
                inline void DeleteBuffer (std::size_t index) {
                    delete buffers[index];
                    buffers[index] = nullptr;
                }
            };

            /// \struct TransactedFile::Internal TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// Internal structure node representing 4G of 4GB segments.
            struct Internal : public Node {
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
                Node *nodes[BRANCHING_LEVEL];

                /// \brief
                /// ctor.
                Internal () {
                    SecureZeroMemory (nodes, sizeof (nodes));
                }
                /// \brief
                /// dtor.
                virtual ~Internal ();

                /// \brief
                /// Delete the buffer cache.
                virtual void Delete () override;
                /// \brief
                /// Delete dirty buffers.
                virtual void Clear () override;
                /// \brief
                /// Write dirty buffers to log.
                /// \param[in] log Log \see{File} to save to.
                /// \param[out] count Incremental count of the dirty buffers.
                virtual void Save (
                    File &log,
                    ui64 &count) override;
                /// \brief
                /// Write dirty buffers to file.
                /// \param[in] file \see{File} to save to.
                virtual void Flush (File &file) override;
                /// \brief
                /// Delete all buffers whose offset > newSize.
                /// \param[in] newSize New size to clip the node to.
                /// \return true == the entire node was clipped, continue iterating.
                /// false == a buffer was encoutered whose offset was < newSize, stop iterating.
                virtual bool SetSize (ui64 newSize) override;
                /// \brief
                /// Return either an \see{Internal} scaffolding node
                /// or a \see{Segment} leaf node. Create if null.
                /// \param[in] index Index of node to return.
                /// \param[in] segment If null, true == create \see{Segment},
                /// otherwise create \see{Internal}
                /// \retrun \see{Segment} or \see{Internal} node at index.
                virtual Node *GetNode (
                    ui8 index,
                    bool segment = false) override;

            private:
                /// \brief
                /// Delete a node at index.
                /// \param[in] index Index of node to delete.
                inline void DeleteNode (std::size_t index) {
                    delete nodes[index];
                    nodes[index] = nullptr;
                }
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
            /// will be. To that end, you're highly encouraged to use \see{FileAllocator}
            /// and \see{FileAllocator::BlockBuffer}.
            Buffer *currBuffer;

            /// \struct TransactedFile::Range TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// Range provides direct access to TransactedFile's underlying buffers.
            /// By pairing it with \see{Serializer}, Range provides serialization/
            /// deserialization capabilities without the need to copy chunks of data
            /// in to and out of the buffers resulting in better performance. Because
            /// the file's 64 bit address space is chunked in to hierarchical pages,
            /// if the requested range straddles a page boundary, a range buffer is
            /// allocated to gurantee sequential access.
            struct _LIB_THEKOGANS_UTIL_DECL Range : public Serializer {
            protected:
                /// \brief
                ///
                TransactedFile &file;
                /// \brief
                /// Begining of range.
                ui64 offset;
                /// \brief
                /// Length of range.
                std::size_t length;
                /// \brief
                /// If the provided range straddles a page boundary
                /// use this allocaor to allocate a range buffer.
                util::Allocator::SharedPtr allocator;
                /// \brief
                ///
                ui8 *data;
                /// \brief
                ///
                std::size_t position;
                /// \brief
                ///
                TransactedFile::Buffer *buffer;
                /// \brief
                ///
                bool owner;

            public:
                /// \brief
                /// ctor.
                /// \param[in] file_ \see{TransactedFile} to buffer.
                /// \param[in] offset_ File offset.
                /// \param[in] length_ How much of the file we want access too.
                /// \param[in] allocator_ \see{util::Allocator} if we need to allocate.
                Range (
                    TransactedFile &file_,
                    ui64 offset_,
                    std::size_t length_ = 0,
                    util::Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ());
                /// \brief
                /// dtor.
                virtual ~Range ();

                inline ui8 *GetDataPtr () const {
                    return data + position;
                }
                inline std::size_t GetDataAvailable () const {
                    return length;
                }
                std::size_t AdvanceOffset (std::size_t advance);

                virtual std::size_t Read (
                    void *buffer,
                    std::size_t count) override;
                virtual std::size_t Write (
                    const void *buffer,
                    std::size_t count) override;
            };

        public:
            struct _LIB_THEKOGANS_UTIL_DECL ReadOnlyRange : public Range {
                /// \brief
                /// ctor.
                /// \param[in] file_ \see{TransactedFile} to buffer.
                /// \param[in] offset_ File offset.
                /// \param[in] length_ How much of the file we want access too.
                /// \param[in] allocator_ \see{util::Allocator} if we need to allocate.
                ReadOnlyRange (
                    TransactedFile &file,
                    ui64 offset,
                    std::size_t length = 0,
                    util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

                virtual std::size_t Read (
                    void *buffer,
                    std::size_t count) override;
            };

            struct _LIB_THEKOGANS_UTIL_DECL WriteOnlyRange : public Range {
                /// \brief
                /// ctor.
                /// \param[in] file_ \see{TransactedFile} to buffer.
                /// \param[in] offset_ File offset.
                /// \param[in] length_ How much of the file we want access too.
                /// \param[in] allocator_ \see{util::Allocator} if we need to allocate.
                WriteOnlyRange (
                    TransactedFile &file,
                    ui64 offset,
                    std::size_t length = 0,
                    util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                    Range (file, offset, length, allocator) {}
                virtual ~WriteOnlyRange ();

                virtual std::size_t Write (
                    const void *buffer,
                    std::size_t count) override;
            };

            #include "thekogans/util/TransactedFileAllocator.h"

            /// \brief
            /// Forward declaration of \see{Object} needed by \see{ObjectEvents}.
            struct Object;

            /// \struct TransactedFile::ObjectEvents TransactedFile.h
            /// thekogans/util/TransactedFile.h
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
                virtual void OnTranactedFileObjectAlloc (
                    RefCounted::SharedPtr<Object> /*object*/) noexcept {}
                /// \brief
                /// \see{Object} freed its file block.
                /// \param[in] object \see{Object} whose offset has become invalid.
                /// VERY IMPORTANT SEMANTICS: When you get this notification,
                /// object->GetOffset () will tell you which block has been allocated.
                virtual void OnTransacedFileObjectFree (
                    RefCounted::SharedPtr<Object> /*object*/) noexcept {}
            };

            /// \struct TransactedFile::Object TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// A TransactedFile Object is an object that has allocated at least one block
            /// from \see{File::Allocator} and participates in \see{TransactedFileEvents}.
            struct _LIB_THEKOGANS_UTIL_DECL Object :
                    public TransactionParticipant,
                    public Producer<ObjectEvents> {
                /// \brief
                /// Object is a \see{util::DynamicCreatable} abstract base.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Object)

            protected:
                /// \brief
                /// Our address inside the \see{TransactedFile}.
                Allocator::PtrType offset;

            public:
                /// \brief
                /// ctor.
                /// \param[in] allocator_ \see{File::Allocator} where this object resides.
                /// \param[in] offset_ Offset of the \see{File::Allocator::Block}.
                Object (
                    TransactedFile::SharedPtr file,
                    Allocator::PtrType offset_) :
                    TransactionParticipant (file),
                    offset (offset_) {}
                /// \brief
                /// dtor.
                virtual ~Object () {}

                /// \brief
                /// Return the offset.
                /// \return offset.
                inline Allocator::PtrType GetOffset () const {
                    return offset;
                }

                /// \brief
                /// A convenience method used to synchronize the
                /// in memory cache with on disk image.
                /// \return Offset of the on disk image.
                Allocator::PtrType ForceFlush ();

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
                /// \brief
                /// Return the serializable binary size (not including the header).
                /// \return Serializable binary size.
                virtual std::size_t Size () const noexcept = 0;

                /// \brief
                /// Write the serializable from the given serializer.
                /// \param[in] serializer Serializer to read the serializable from.
                virtual void Read (Serializer & /*serializer*/) = 0;
                /// \brief
                /// Write the serializable to the given serializer.
                /// \param[out] serializer Serializer to write the serializable to.
                virtual void Write (Serializer & /*serializer*/) = 0;

                // TransactedFile::TransactionParticipant
                /// \brief
                /// If needed allocate space from \see{File::Allocator}.
                virtual void Alloc () override;
                /// \brief
                /// Default free implementation for single block objects.
                /// If your objects contain internal pointers to other
                /// blocks you will need to implement this method and
                /// properly free the containing blocks.
                virtual void Free () override;
                /// \brief
                /// Flush the internal cache to file.
                virtual void Flush () override;
                /// \brief
                /// Reload the internal cache from file.
                virtual void Reload () override;

                /// \brief
                /// Object is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Object)
            };

            /// \brief
            /// ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            TransactedFile (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path = std::string ()) :
                File (endianness, handle, path),
                position (IsOpen () ? File::Tell () : 0),
                sizeOnDisk (IsOpen () ? File::GetSize () : 0),
                size (sizeOnDisk),
                flags (0),
                currBufferOffset (NOFFS),
                currBuffer (nullptr) {}
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor. Open the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
            TransactedFile (
                Endianness endianness,
                const std::string &path,
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                DWORD dwCreationDisposition = OPEN_ALWAYS,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL) :
                File (
                    endianness,
                    path,
                    dwDesiredAccess,
                    dwShareMode,
                    dwCreationDisposition,
                    dwFlagsAndAttributes),
                position (IsOpen () ? File::Tell () : 0),
                sizeOnDisk (IsOpen () ? File::GetSize () : 0),
                size (sizeOnDisk),
                flags (0),
                currBufferOffset (NOFFS),
                currBuffer (nullptr) {}
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor. Open the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
            TransactedFile (
                Endianness endianness,
                const std::string &path,
                i32 flags = O_RDWR | O_CREAT,
                i32 mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) :
                File (endianness, path, flags, mode),
                position (IsOpen () ? File::Tell () : 0),
                sizeOnDisk (IsOpen () ? File::GetSize () : 0),
                size (sizeOnDisk),
                flags (0),
                currBufferOffset (NOFFS),
                currBuffer (nullptr) {}
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// dtor.
            virtual ~TransactedFile ();

            inline Allocator::SharedPtr GetAllocator () const {
                return allocator;
            }
            inline void SetAllocator (Allocator::SharedPtr allocator_) {
                allocator = allocator_;
            }

            /// \brief
            /// Use the lock to gain exclusive access to the file.
            /// NOTE: TransactedFile does not use mutex (at all). As
            /// noted elsewhere, locking with every seek/read/write
            /// would be prohibitively expensive (not to mention it
            /// would do nothing to preserve the atomicity of seek/read
            /// and seek/write operations). Instead each
            /// TransactedFile exposes a mutex that your threads can
            /// use to synchronize access to the file based on
            /// access patterns that are more appropriate to your
            /// particular situation. Again, the use of the lock
            /// is completely optional and in order for the entire
            /// scheme to work your threads must manually cooperate
            /// by acquiring the lock before use. To help this pattern,
            /// TransactedFile::Transaction should be used for all
            /// writes. LockGuard<Mutex> should be used for all reads.
            /// \return mutex.
            inline Mutex &GetLock () {
                return mutex;
            }

            // Serializer
            /// \brief
            /// Read bytes from a file.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual std::size_t Read (
                void *buffer,
                std::size_t count) override;
            /// \brief
            /// Write bytes to a file.
            /// \param[in] buffer Where the bytes come from.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual std::size_t Write (
                const void *buffer,
                std::size_t count) override;

            // RandomSeekSerializer
            /// \brief
            /// Return the file pointer position.
            /// \return The file pointer position.
            virtual i64 Tell () const override {
                return position;
            }
            /// \brief
            /// Reposition the file pointer.
            /// \param[in] offset Offset to move relative to fromWhere.
            /// \param[in] fromWhere SEEK_SET, SEEK_CUR or SEEK_END.
            /// \return The new file pointer position.
            virtual i64 Seek (
                i64 offset,
                i32 fromWhere) override;

            // File
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Open the file.
            /// \param[in] path Windows CreateFile parameter.
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
            virtual void Open (
                const std::string &path,
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE,
                DWORD dwCreationDisposition = OPEN_EXISTING,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL) override;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Open the file.
            /// \param[in] path POSIX open parameter.
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
            virtual void Open (
                const std::string &path,
                i32 flags = O_RDWR,
                i32 mode = S_IRUSR | S_IWUSR) override;
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Close file.
            virtual void Close () override;
            /// \brief
            /// Flush pending writes to disk.
            virtual void Flush () override;

            /// \brief
            /// Return number of bytes available for reading.
            /// \return Number of bytes available for reading.
            virtual ui64 GetDataAvailableForReading () const override {
                return size > (ui64)position ? size - (ui64)position : 0;
            }

            /// \brief
            /// Return file size in bytes.
            /// \return File size in bytes.
            virtual ui64 GetSize () const override {
                return size;
            }
            /// \brief
            /// Truncates or expands the file.
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

            /// \brief
            /// Flush dirty pages and delete the cache.
            void DeleteCache ();

        private:
            /// \brief
            /// Used by Open to apply the log before opening the file.
            /// \path path File path.
            static void CommitLog (const std::string &path);

            /// \brief
            /// Return true if we have unwriten changes in our cache.
            ///\return true == we have unwriten changes in our cache.
            inline bool IsDirty () const {
                return flags.Test (FLAGS_DIRTY);
            }
            /// \brief
            /// Set/reset the dirty flag.
            /// \param[in] dirty true == set, false == reset.
            inline void SetDirty (bool dirty) {
                flags.Set (FLAGS_DIRTY, dirty);
            }
            /// \brief
            /// Return true if we're in the middle of a transaction.
            ///\return true == we're in the middle of a transaction.
            inline bool IsTransactionPending () const {
                return flags.Test (FLAGS_TRANSACTION);
            }
            /// \brief
            /// Set/reset the transaction flag.
            /// \param[in] transaction true == set, false == reset.
            inline void SetTransactionPending (bool transaction) {
                flags.Set (FLAGS_TRANSACTION, transaction);
            }
            /// \brief
            /// Start a new transaction.
            /// Used by \see{Transaction} ctor to mark a modify scope.
            /// NOTE: Must only be called after acquiring the lock.
            void BeginTransaction ();
            /// \brief
            /// Commit the current transaction.
            /// Used by \see{Transaction} to commit the current changes.
            void CommitTransaction ();
            /// \brief
            /// Abort the current transaction.
            /// Used by \see{Transaction} dtor to abort uncommitted changes.
            void AbortTransaction ();

            /// \brief
            /// Get the buffer that will cover the neighborhood around position.
            /// \return Buffer that covers the neighborhood around position.
            Buffer *GetBuffer ();

            /// \brief
            /// Given a file path, use the full file name to create
            /// a GUID to append to the path to act as the log path.
            /// \param[in] path File path to turn in to log path.
            /// \return Log path based on the passed in file path.
            static std::string GetLogPath (const std::string &path);

            /// \brief
            /// TransactedFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TransactedFile)
        };

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
                const std::string &path = std::string ()) :
                TransactedFile (endianness, handle, path) {}
            /// \brief
            /// ctor. Abstracts most useful functionality from POSIX open.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] flags Most useful POSIX open flags.
            SimpleTransactedFile (
                Endianness endianness,
                const std::string &path,
                Flags32 flags = SimpleFile::ReadWrite | SimpleFile::Create);

            /// \brief
            /// Open the file.
            /// \param[in] path File path.
            /// \param[in] flags File open flags.
            void SimpleOpen (
                const std::string &path,
                Flags32 flags = SimpleFile::ReadWrite | SimpleFile::Create);

            /// \brief
            /// SimpleTransactedFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SimpleTransactedFile)
        };

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_TransactedFile_h
