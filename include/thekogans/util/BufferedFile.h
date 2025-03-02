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
#include "thekogans/util/Heap.h"
#include "thekogans/util/File.h"

namespace thekogans {
    namespace util {

        /// \struct BufferedFile BufferedFile.h thekogans/util/BufferedFile.h
        ///
        /// \brief
        /// BufferedFile will accumulate all changes in memory and will commit them
        /// all at once in \see{Flush} or \see{CommitTransaction}. BufferedFile
        /// has support for simple, not nested transactions (see \see{BeginTransaction}).
        /// BufferedFile design principle is; if you have it, might as well use it.
        /// Meaning today's (early March 2025) state of the art has some servers
        /// sporting up to 6TB of main memory. All that memory is there for a
        /// reason. You paid good money for it. BufferedFile will use as much
        /// as you have (give it). By default, BufferedFile uses 1MB tiles (\see{Buffer}).
        /// It will load and hold available as much of the file as you have room.
        /// That's where \see{Flush} and \see{CommitTransaction} come in. As you
        /// eventually start to run out of room (even with 6TB it's a drop in
        /// the bucket compared to 64 bit address space), call \see{Flush} to commit
        /// the changes to disk and release the buffers. While with proper tuning
        /// BufferedFile should work just fine for 'small' (under 1MB) files, it's
        /// strength lies with it's ability to handle multi GB or even TB files
        /// with ease. It's hierarchical address space partitioning alows for
        /// very efficient, sparse file handling.

        struct _LIB_THEKOGANS_UTIL_DECL BufferedFile : public File {
            /// \brief
            /// BufferedFile participates in the \see{DynamicCreatable}
            /// dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (BufferedFile)

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
                /// Buffer offset (multiple of PAGE_SIZE).
                ui64 offset;
                /// \brief
                /// Buffer length (max PAGE_SIZE).
                ui64 length;
                /// \brief
                /// Buffer size. This (and the coresponding
                /// \see{SHIFT_COUNT} below) is a tunning parameter.
                /// Set it based on your typical file sizes. Meaning that,
                /// if your files never go above 100KB then a 1MB PAGE_SIZE
                /// is overkill. But if you typically manage multi gigabyte
                /// (terabyte?) databases you might want to bump this value up.
                static const std::size_t PAGE_SIZE = 0x00100000;
                /// \brief
                /// Look at \see{PAGE_SIZE} above. This number
                /// represents the trailing zeros in binary.
                static const std::size_t SHIFT_COUNT = 20;
                /// \brief
                /// Buffer.
                ui8 data[PAGE_SIZE];
                /// \brief
                /// true == modified.
                bool dirty;

                /// \brief
                /// ctor.
                /// \param[in] offset_ Buffer offset (multiple of PAGE_SIZE).
                /// \param[in] length_ Buffer length (max PAGE_SIZE).
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
                /// Clear the buffer cache.
                virtual void Clear () = 0;
                /// \brief
                /// Write dirty buffers to log.
                /// \param[in] log Log \see{File} to save to.
                /// \param[out] count Incremental count of the dirty buffers.
                virtual void Save (
                    File &log,
                    ui64 &count) = 0;
                /// \brief
                /// Write dirty buffers to file and clear the cache.
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
                    std::memset (buffers, 0, sizeof (buffers));
                }
                /// \brief
                /// dtor.
                virtual ~Segment ();

                /// \brief
                /// Clear the buffer cache.
                virtual void Clear () override;
                /// \brief
                /// Write dirty buffers to log.
                /// \param[in] log Log \see{File} to save to.
                /// \param[out] count Incremental count of the dirty buffers.
                virtual void Save (
                    File &log,
                    ui64 &count) override;
                /// \brief
                /// Write dirty buffers to file and clear the cache.
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
                    std::memset (nodes, 0, sizeof (nodes));
                }
                /// \brief
                /// dtor.
                virtual ~Internal ();

                /// \brief
                /// Clear the buffer cache.
                virtual void Clear () override;
                /// \brief
                /// Write dirty buffers to log.
                /// \param[in] log Log \see{File} to save to.
                /// \param[out] count Incremental count of the dirty buffers.
                virtual void Save (
                    File &log,
                    ui64 &count) override;
                /// \brief
                /// Write dirty buffers to file and clear the cache.
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
            /// \region[in] region region to lock.
            /// \region[in] exclusive lock for exclusive access.
            virtual void LockRegion (
                const Region & /*region*/,
                bool /*exclusive*/) override {}
            /// \brief
            /// Unlock a range of bytes in the file.
            /// \region[in] region region to unlock.
            virtual void UnlockRegion (const Region & /*region*/) override {}

            /// \brief
            /// Used by Open to apply the log before opening the file.
            /// \path path File path.
            static void CommitLog (const std::string &path);

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
            void Open (
                const std::string &path,
                Flags32 flags = SimpleFile::ReadWrite | SimpleFile::Create);

            /// \brief
            /// SimpleBufferedFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SimpleBufferedFile)
        };

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_BufferedFile_h
