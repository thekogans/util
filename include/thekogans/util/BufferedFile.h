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

        /// \brief
        /// Forward declaration of \see{BufferedFile} needed by \see{BufferedFileEvents}.
        struct BufferedFile;

        /// \struct BufferedFileEvents BufferedFile.h thekogans/util/BufferedFile.h
        ///
        /// \brief
        /// Subscribe to BufferedFileEvents to receive transaction notifications.
        struct _LIB_THEKOGANS_UTIL_DECL BufferedFileEvents {
            /// \brief
            /// dtor.
            virtual ~BufferedFileEvents () {}

            /// \brief
            /// Transaction is beginning. Time to flush the internal cache.
            /// If your object derives from \see{BufferedFile::TransactionParticipant}
            /// all this is done under the hood for you. All you will need
            /// to do is implement Flush.
            virtual void OnBufferedFileTransactionBegin (
                RefCounted::SharedPtr<BufferedFile> /*file*/) noexcept {}
            /// \brief
            /// Transaction is committing. Depending on the phase do what ever
            /// is appropriate.
            /// If your object derives from \see{BufferedFile::TransactionParticipant}
            /// all this is done under the hood for you. All you will need
            /// to do is implement Allocate (phase 1) and Flush (phase 2).
            /// \param[in] phase Either COMMIT_PHASE_1 or COMMIT_PHASE_2.
            virtual void OnBufferedFileTransactionCommit (
                RefCounted::SharedPtr<BufferedFile> /*file*/,
                int phase) noexcept {}
            /// \brief
            /// Transaction is aborting. Time to reload the object.
            /// If your object derives from \see{BufferedFile::TransactionParticipant}
            /// all this is done under the hood for you. All you will need
            /// to do is implement Reload.
            virtual void OnBufferedFileTransactionAbort (
                RefCounted::SharedPtr<BufferedFile> /*file*/) noexcept {}
        };

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
        struct _LIB_THEKOGANS_UTIL_DECL BufferedFile :
                public File,
                public Producer<BufferedFileEvents> {
            /// \brief
            /// BufferedFile participates in the \see{DynamicCreatable}
            /// dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (BufferedFile)

            /// \brief
            /// Commit phase 1.
            static const int COMMIT_PHASE_1 = 1;
            /// \brief
            /// Commit phase 2.
            static const int COMMIT_PHASE_2 = 2;

            /// \struct BufferedFile::Guard BufferedFile.h thekogans/util/BufferedFile.h
            ///
            /// \brief
            /// A very simple scope guard. Will acquire the mutex in the ctor and release
            /// it in the dtor. Useful in read only situations.
            struct _LIB_THEKOGANS_UTIL_DECL Guard {
            private:
                /// \brief
                /// This guard will serialize all access to the \see{BufferedFile}.
                LockGuard<Mutex> guard;

            public:
                /// \brief
                /// ctor.
                /// \param[in] file \see{BufferedFile} to guard.
                explicit Guard (BufferedFile::SharedPtr file);

                /// \brief
                /// Guard is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Guard)
            };

            /// \struct BufferedFile::Transaction BufferedFile.h thekogans/util/BufferedFile.h
            ///
            /// \brief
            /// A very simple transaction scope guard. Will call BeginTransaction in it's
            /// ctor and AbortTransaction in it's dtor. Call Commit before the end of the
            /// scope to commit the transaction.
            struct _LIB_THEKOGANS_UTIL_DECL Transaction {
            private:
                /// \see{BufferedFile} we're guarding.
                BufferedFile::SharedPtr file;
                /// \brief
                /// This guard will serialize all transactions.
                LockGuard<Mutex> guard;

            public:
                /// \brief
                /// ctor.
                /// \param[in] file_ \see{BufferedFile} to transact.
                explicit Transaction (BufferedFile::SharedPtr file_);
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

            /// \struct BufferedFile::TransactionParticipant BufferedFile.h
            /// thekogans/util/BufferedFile.h
            ///
            /// \brief
            /// TransactionParticipants are objects that listen to
            /// \see{BufferedFileEvents} and are able to flush and reload
            /// themselves to and from a \see{BufferedFile}.
            struct _LIB_THEKOGANS_UTIL_DECL TransactionParticipant :
                    public Subscriber<BufferedFileEvents> {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (TransactionParticipant)

            protected:
                /// \brief
                /// \see{BufferedFile} whose \see{BufferedFileEvents}
                /// we're participants of.
                BufferedFile::SharedPtr file;

                /// \brief
                /// ctor.
                /// \param[in] file_ \see{BufferedFile} we're a transaction participant of.
                TransactionParticipant (BufferedFile::SharedPtr file_);
                /// \brief
                /// dtor.
                virtual ~TransactionParticipant () {}

                /// \brief
                /// Allocate space from \see{BufferedFile}.
                virtual void Allocate () = 0;

                /// \brief
                /// Flush the internal cache to file.
                virtual void Flush () = 0;

                /// \brief
                /// Reload from file.
                virtual void Reload () = 0;

                // BufferedFileEvents
                /// \brief
                /// Transaction is beginning. Flush internal cache to file.
                /// \param[in] file \see{BufferedFile} beginning the transaction.
                virtual void OnBufferedFileTransactionBegin (
                        BufferedFile::SharedPtr /*file*/) noexcept override {
                    Flush ();
                }
                /// \brief
                /// Transaction is commiting. Flush internal cache to file.
                /// \param[in] file \see{BufferedFile} commiting the transaction.
                /// \param[in] phase \see{BufferedFile} implements two phase commit.
                virtual void OnBufferedFileTransactionCommit (
                        BufferedFile::SharedPtr /*file*/,
                        int phase) noexcept override {
                    if (phase == COMMIT_PHASE_1) {
                        Allocate ();
                    }
                    else if (phase == COMMIT_PHASE_2) {
                        Flush ();
                    }
                }
                /// \brief
                /// Transaction is aborting. Reload from file.
                /// \param[in] file \see{BufferedFile} aborting the transaction.
                virtual void OnBufferedFileTransactionAbort (
                        BufferedFile::SharedPtr /*file*/) noexcept override {
                    Reload ();
                }

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
            /// \brief
            /// For use by \see{Guard} and \see{Transaction}
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
            bool BeginTransaction ();
            /// \brief
            /// Commit the current transaction.
            bool CommitTransaction ();
            /// \brief
            /// Abort the current transaction.
            bool AbortTransaction ();

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
