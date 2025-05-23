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

#if !defined (__thekogans_util_BufferedFile_h)
#define __thekogans_util_BufferedFile_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/File.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/Producer.h"
#include "thekogans/util/SecureAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct BufferedFile BufferedFile.h thekogans/util/BufferedFile.h
        ///
        /// \brief
        /// BufferedFile is a drop in replacement for \see{File}. BufferedFile
        /// will accumulate all changes in memory and will commit them all at
        /// once in \see{Flush} or \see{CommitTransaction}. BufferedFile has
        /// support for simple, flat (not nested) transactions (see \see{BeginTransaction}).
        /// BufferedFile design principle is; if you have it, might as well use it.
        /// Meaning today's (early March 2025) state of the art has some servers
        /// sporting up to 6TB of main memory. All that memory is there for a
        /// reason. You paid good money for it. BufferedFile will use as much
        /// as you have (give it). By default, BufferedFile uses 1MB tiles (\see{Buffer}).
        /// It will load and hold available as much of the file as you have room.
        /// That's where \see{Flush}, \see{CommitTransaction} and \see{DeleteCache}
        /// come in. As you eventually start to run out of room (even with 6TB it's
        /// a drop in the bucket compared to 64 bit address space), call \see{Flush}
        /// to commit the changes to disk followed by a call to \see{DeleteCache} to
        /// release the cache. While with proper tuning BufferedFile should work just
        /// fine for 'small' (under 1MB) files, it's strength lies with it's ability
        /// to handle multi GB or even TB files with ease. It's hierarchical address
        /// space partitioning allows for very efficient, sparse file handling.

        struct _LIB_THEKOGANS_UTIL_DECL BufferedFile : public File {
            /// \brief
            /// BufferedFile participates in the \see{DynamicCreatable}
            /// dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (BufferedFile)

            /// \brief
            /// Forward declaration of \see{Transaction}.
            struct Transaction;

            /// \struct BufferedFile::Transaction BufferedFile.h thekogans/util/BufferedFile.h
            ///
            /// \brief
            /// Events broadcast by transactions to its participants.
            struct _LIB_THEKOGANS_UTIL_DECL TransactionEvents {
                /// \brief
                /// dtor.
                virtual ~TransactionEvents () {}

                /// \brief
                /// Transaction is beginning. Time to flush the internal cache.
                /// If your object derives from \see{BufferedFileTransactionParticipant}
                /// all this is done under the hood for you. All you will need
                /// to do is implement Flush.
                /// \param[in] transaction Transaction thats beginning.
                virtual void OnTransactionBegin (
                    RefCounted::SharedPtr<Transaction> transaction) noexcept {}
                /// \brief
                /// Transaction is committing. See OnTransactionBegin above.
                /// \param[in] transaction Transaction thats beginning.
                virtual void OnTransactionCommit (
                    RefCounted::SharedPtr<Transaction> transaction) noexcept {}
                /// \brief
                /// Transaction is aborting. Time to reload the object (if its not a
                /// participant).
                /// \param[in] transaction Transaction thats aborting.
                /// If your object derives from \see{BufferedFileTransactionParticipant}
                /// all this is done under the hood for you. All you will need
                /// to do is implement Reload.
                virtual void OnTransactionAbort (
                    RefCounted::SharedPtr<Transaction> transaction) noexcept {}
            };

            /// \struct BufferedFile::Transaction BufferedFile.h thekogans/util/BufferedFile.h
            ///
            /// \brief
            /// A transaction is the easiest way to perform atomic file writes.
            /// BufferedFile is not thread safe. To make it thread safe would require
            /// a lock to protect internal state. Because BufferedFile is just the
            /// foundation of a much more extensive data management system (see
            /// \see{FileAllocator}), I felt introducing locking at such a low level
            /// would potentially be costly in terms of performance. Insread I introduced
            /// transactions. Transactions allow you to organize your writes in to logical
            /// groups. Most high level application writes (database records) require a
            /// series of related file writes. By wrapping them in transactions you guarantee
            /// database integrity (either all writes succeed or none will). If you limit your
            /// file access through transactions even if you don't plan to write you can make
            /// BufferedFile thread safe. Use Guard within a scope to serialize access to the
            /// underlying file and make transactions \see{Exception} safe.
            struct _LIB_THEKOGANS_UTIL_DECL Transaction : public Producer<TransactionEvents> {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Transaction)

                /// \struct BufferedFile::Transaction::Guard BufferedFile.h
                /// thekogans/util/BufferedFile.h
                ///
                /// \brief
                /// A very simple transaction scope guard. Will call Begin in it's
                /// ctor and Abort in it's dtor. Call commit before the end of the
                /// scope to commit the transaction.
                struct _LIB_THEKOGANS_UTIL_DECL Guard {
                    /// \brief
                    /// Transaction to guard.
                    Transaction::SharedPtr transaction;

                    /// \brief
                    /// ctor.
                    /// \param[in] transaction_ Transaction to guard.
                    Guard (Transaction::SharedPtr transaction_);
                    /// \brief
                    /// dtor.
                    ~Guard () {
                        transaction->Abort ();
                    }

                    /// \brief
                    /// Commit the transaction before the dtor aborts it.
                    inline void Commit () {
                        transaction->Commit ();
                    }
                };

            private:
                /// \brief
                /// \see{BufferedFile} to transact.
                BufferedFile &file;
                /// \brief
                /// Transactions are atomic.
                LockGuard<Mutex> guard;
                /// \brief
                /// List of objects created during the transaction. Their lifetime
                /// is confined to the transaction.
                std::vector<Subscriber<TransactionEvents>::SharedPtr> participants;

            public:
                /// \brief
                /// ctor
                /// \param[in] file_ \see{BufferedFile} to transact.
                explicit Transaction (BufferedFile &file_) :
                    file (file_),
                    guard (file.mutex) {}

                /// \brief
                /// Add an object as a transaction participant. Participants liftimes
                /// are usualy confined to the lifetime of the transaction. Meaning
                /// regardless of the outcome of the transaction (Commit/Abort), the
                /// participant will be destroyed. If the transaction is commited,
                /// the participant will be given a chance to flush itself to file. If
                /// the transaction is aborted it will be as though the participant never
                /// existed.
                /// \param[in] participant Participants are transaction events subscribers.
                void AddParticipant (Subscriber<TransactionEvents>::SharedPtr participant);

                /// \brief
                /// Begin the transaction. Notify all subscribers.
                void Begin ();
                /// \brief
                /// Commit the transaction. Notify all subscribers.
                void Commit ();
                /// \brief
                /// Abort the transaction. Notify all subscribers except participants.
                /// VERY IMPORTANT: If you've created participants (AddParticipant)
                /// and surrendered their lifetime management to transaction (i.e.
                /// you're not holding a reference to them) there's nothing for you to
                /// do. The objects will be delete before OnTransactionAbort is broadcast.
                /// They are not told to flush or reload. Their cache (whatever it may be)
                /// dies with them. If you've kept a reference to the temporary participants
                /// it's very important that you listen to OnTransactionAbort yourself
                /// and delete them. Under no circumstance should you attempt to flush the
                /// cache they've created as its no longer valid and will lead to database
                /// corruption. The only way participants can survive the liftime of their
                /// transactions is if the transaction commits.
                void Abort ();
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
            /// \brief
            /// For use by \see{Transaction}
            Mutex mutex;

            /// \struct BufferedFile::Buffer BufferedFile.h thekogans/util/BufferedFile.h
            ///
            /// \brief
            /// Buffer tiles the file address space providing incremental,
            /// sparse access to the data. Use \see{Flush} to manage memory
            /// usage.
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

            /// \struct BufferedFile::Node BufferedFile.h thekogans/util/BufferedFile.h
            ///
            /// \brief
            /// The file 64 bit address space is partitioned such that it can be
            /// represented using a fixed depth multiway tree. The high order 32 bits
            /// are used to represent 4G of 4GB segments. The low order 32 bits are
            /// used to represent 4GB segments partitioned in to 4K of 1MB tiles.
            /// The entire address space is sparce and fits in to a tree of depth 5
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

            /// \struct BufferedFile::Segment BufferedFile.h thekogans/util/BufferedFile.h
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

            /// \struct BufferedFile::Internal BufferedFile.h thekogans/util/BufferedFile.h
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

        public:
            /// \brief
            /// ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            BufferedFile (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path = std::string ()) :
                File (endianness, handle, path),
                position (IsOpen () ? File::Tell () : 0),
                sizeOnDisk (IsOpen () ? File::GetSize () : 0),
                size (sizeOnDisk),
                flags (0) {}
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor. Open the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
            BufferedFile (
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
                flags (0) {}
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor. Open the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
            BufferedFile (
                Endianness endianness,
                const std::string &path,
                i32 flags = O_RDWR | O_CREAT,
                i32 mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) :
                File (endianness, path, flags, mode),
                position (IsOpen () ? File::Tell () : 0),
                sizeOnDisk (IsOpen () ? File::GetSize () : 0),
                size (sizeOnDisk),
                flags (0) {}
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// dtor.
            virtual ~BufferedFile ();

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
            /// BufferedFile does not support region locking.
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
            /// Return true if we're in the middle of a transaction.
            ///\return true == we're in the middle of a transaction.
            inline bool IsTransactionPending () const {
                return flags.Test (FLAGS_TRANSACTION);
            }

            /// \brief
            /// Start a new transaction. If a transaction is
            /// already in progress do nothing. If the cache
            /// was dirty before the transaction began it will
            /// be \see{Flush}ed first.
            void BeginTransaction ();
            /// \brief
            /// Commit the current transaction.
            void CommitTransaction ();
            /// \brief
            /// Abort the current transaction.
            void AbortTransaction ();

            /// \brief
            /// BufferedFile is very delicate when it comes to it's internal
            /// cache (\see{Buffer}s). After all, its very expensive to create
            /// and maintain. So it goes through great lengths to preserve it
            /// as much as posible. There are times however when you need to
            /// release the cache so that you can start anew (presumably in some
            /// other remote part of the same file). Only you know when you
            /// need to do that. That's where this function comes in.
            /// \param[in] commitChanges true == Commits the open transaction
            /// or flushes dirty buffers. false == Aborts the open transaction
            /// or deletes the dirty buffers.
            void DeleteCache (bool commitChanges = true);

        private:
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
            /// BufferedFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BufferedFile)
        };

        /// \struct SimpleBufferedFile BufferedFile.h thekogans/util/BufferedFile.h
        ///
        /// \brief
        /// SimpleBufferedFile exposes the basic flags supported by the standard
        /// library open that are portable across Windows, Linux and OS X.
        /// NOTE: On Linux and OS X if a file needs to be created, it's
        /// mode will be 0644. This is fine for most cases but might not
        /// be appropriate for some. If you need to control the mode of
        /// the created file use BufferedFile instead.

        struct _LIB_THEKOGANS_UTIL_DECL SimpleBufferedFile : public BufferedFile {
            /// \brief
            /// Default ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            SimpleBufferedFile (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path = std::string ()) :
                BufferedFile (endianness, handle, path) {}
            /// \brief
            /// ctor. Abstracts most useful functionality from POSIX open.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] flags Most useful POSIX open flags.
            SimpleBufferedFile (
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
            /// SimpleBufferedFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SimpleBufferedFile)
        };

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_BufferedFile_h
