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

#if !defined (__thekogans_util_Buffer_h)
#define __thekogans_util_Buffer_h

#include <memory>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/SecureAllocator.h"
#include "thekogans/util/NullAllocator.h"

namespace thekogans {
    namespace util {

        /// \struct Buffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// Buffer is a convenient in memory serializer. It's main
        /// purpose is to provide serialization/deserialization
        /// services for binary protocols, but can be used anywhere
        /// lightweight sharing and lifetime management is
        /// appropriate. One of Buffer's more novel features is
        /// outsourcing memory management to an \see{Allocator}.
        /// This feature allows you to create buffers for all
        /// occasions (See \see{SecureBuffer}, \see{TenantReadBuffer}
        /// and \see{TenantWriteBuffer} below).
        /// NOTE: Buffer maintains distinct read and write positions
        /// (readOffset and writeOffset). It allows you to continue
        /// filling the buffer without disturbing the current read
        /// position. This is very useful in streaming operations.

        struct _LIB_THEKOGANS_UTIL_DECL Buffer : public Serializer {
            /// \brief
            /// Convenient typedef for std::unique_ptr<Buffer>.
            typedef std::unique_ptr<Buffer> UniquePtr;
            /// \brief
            /// Convenient typedef for std::shared_ptr<Buffer>.
            typedef std::shared_ptr<Buffer> SharedPtr;

            /// \brief
            /// Buffer has a private heap to help with memory
            /// management, performance, and global heap fragmentation.
            THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Buffer, SpinLock)

            /// \brief
            /// Buffer data.
            ui8 *data;
            /// \brief
            /// Length of data.
            ui32 length;
            /// \brief
            /// Current read position.
            ui32 readOffset;
            /// \brief
            /// Current write position.
            ui32 writeOffset;
            /// \brief
            /// Allocator used for memory management.
            Allocator *allocator;

            /// \brief
            /// ctor for wrapping a raw data pointer.
            /// \param[in] endianness How multi-byte values are stored.
            /// \param[in] data_ Pointer to wrap.
            /// \param[in] length_ Length of data.
            /// \param[in] readOffset_ Offset at which to read.
            /// \param[in] writeOffset_ Offset at which to write.
            /// \param[in] allocator_ Allocator used for memory management.
            Buffer (
                Endianness endianness = HostEndian,
                ui8 *data_ = 0,
                ui32 length_ = 0,
                ui32 readOffset_ = 0,
                ui32 writeOffset_ = 0,
                Allocator *allocator_ = &DefaultAllocator::Global) :
                Serializer (endianness),
                data (data_),
                length (length_),
                readOffset (readOffset_),
                writeOffset (writeOffset_),
                allocator (allocator_) {}
            /// \brief
            /// ctor for creating a buffer of a given length.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] length_ Length of data.
            /// \param[in] readOffset_ Offset at which to read.
            /// \param[in] writeOffset_ Offset at which to write.
            /// \param[in] allocator_ Allocator used for memory management.
            Buffer (
                Endianness endianness,
                ui32 length_,
                ui32 readOffset_ = 0,
                ui32 writeOffset_ = 0,
                Allocator *allocator_ = &DefaultAllocator::Global);
            /// \brief
            /// ctor for creating a buffer from a given range.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] begin Start of range.
            /// \param[in] end Just past the end of range.
            /// \param[in] readOffset_ Offset at which to read.
            /// \param[in] writeOffset_ Offset at which to write.
            /// \param[in] allocator_ Allocator used for memory management.
            Buffer (
                Endianness endianness,
                const ui8 *begin,
                const ui8 *end,
                ui32 readOffset_ = 0,
                ui32 writeOffset_ = UI32_MAX,
                Allocator *allocator_ = &DefaultAllocator::Global);
            /// \brief
            /// dtor.
            virtual ~Buffer () {
                Resize (0);
            }

            // Serializer
            /// \brief
            /// Read raw bytes from a buffer.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual ui32 Read (
                void *buffer,
                ui32 count);
            /// \brief
            /// Write raw bytes to a buffer.
            /// \param[in] buffer Bytes to write.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual ui32 Write (
                const void *buffer,
                ui32 count);

            /// \brief
            /// Resize the buffer. Adjust readOffset and writeOffset to stay within [0, length).
            /// \param[in] length_ New buffer length.
            /// \param[in] allocator_ Allocator to use to allocate new data.
            virtual void Resize (
                ui32 length_,
                Allocator *allocator_ = &DefaultAllocator::Global);

            /// \brief
            /// Clone the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// \return A clone of this buffer.
            virtual UniquePtr Clone (Allocator *allocator = &DefaultAllocator::Global) const;

            /// \brief
            /// Return the serialized size of this buffer.
            /// \return Serialized size of this buffer.
            inline ui32 Size () const {
                return
                    Serializer::Size () +
                    Serializer::Size (length) +
                    Serializer::Size (readOffset) +
                    Serializer::Size (writeOffset) +
                    length;
            }

            /// \brief
            /// Return true if there is no more data available for reading.
            /// \return true if there is no more data available for reading.
            inline bool IsEmpty () const {
                return GetDataAvailableForReading () == 0;
            }
            /// \brief
            /// Return true if there is no more space available for writing.
            /// \return true if there is no more space available for writing.
            inline bool IsFull () const {
                return GetDataAvailableForWriting () == 0;
            }

            /// \brief
            /// Return number of bytes available for reading.
            /// \return Number of bytes available for reading.
            inline ui32 GetDataAvailableForReading () const {
                return writeOffset > readOffset ? writeOffset - readOffset : 0;
            }
            /// \brief
            /// Return number of bytes available for writing.
            /// \return Number of bytes available for writing.
            inline ui32 GetDataAvailableForWriting () const {
                return length > writeOffset ? length - writeOffset : 0;
            }
            /// \brief
            /// Return the current data read position.
            /// \return The current data read position.
            inline const ui8 *GetReadPtr () const {
                return data + readOffset;
            }
            /// \brief
            /// Return just past the end of the current data read position.
            /// \return Just past the end of the current data read position.
            inline const ui8 *GetReadPtrEnd () const {
                return GetReadPtr () + GetDataAvailableForReading ();
            }
            /// \brief
            /// Return the current data write position.
            /// \return The current data write position.
            inline ui8 *GetWritePtr () const {
                return data + writeOffset;
            }
            /// \brief
            /// Return just past the end of the current data write position.
            /// \return Just past the end of the current data write position.
            inline ui8 *GetWritePtrEnd () const {
                return GetWritePtr () + GetDataAvailableForWriting ();
            }
            /// \brief
            /// Advance the read offset taking care not to overflow.
            /// \param[in] advance Amount to advance the readOffset.
            /// \return Number of bytes actually advanced.
            ui32 AdvanceReadOffset (ui32 advance);
            /// \brief
            /// Advance the write offset taking care not to overflow.
            /// \param[in] advance Amount to advance the writeOffset.
            /// \return Number of bytes actually advanced.
            ui32 AdvanceWriteOffset (ui32 advance);

            /// \brief
            /// Reset the readOffset and the writeOffset to prepare the
            /// buffer for reuse.
            inline void Rewind () {
                readOffset = 0;
                writeOffset = 0;
            }

        #if defined (THEKOGANS_UTIL_HAVE_ZLIB)
            /// \brief
            /// Use zlib to compress the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// \return A buffer containing deflated data.
            virtual UniquePtr Deflate (Allocator *allocator = &DefaultAllocator::Global);
            /// \brief
            /// Use zlib to decompress the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// \return A buffer containing inflated data.
            virtual UniquePtr Inflate (Allocator *allocator = &DefaultAllocator::Global);
        #endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)

            /// \brief
            /// Buffer is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Buffer)
        };

        /// \struct DefaultBuffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// Wraps a \see{SecureAllocator}. SecureBuffers are very useful
        /// in cryptographic contexts as well as times when you need to
        /// ensure buffer data stays away from prying eyes.

        struct _LIB_THEKOGANS_UTIL_DECL SecureBuffer : public Buffer {
            /// \brief
            /// ctor for wrapping a raw data pointer.
            /// \param[in] endianness How multi-byte values are stored.
            /// \param[in] data Pointer to wrap.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            SecureBuffer (
                Endianness endianness = HostEndian,
                ui8 *data = 0,
                ui32 length = 0,
                ui32 readOffset = 0,
                ui32 writeOffset = 0) :
                Buffer (
                    endianness,
                    data,
                    length,
                    readOffset,
                    writeOffset,
                    &SecureAllocator::Global) {}
            /// \brief
            /// ctor for creating a buffer of a given length.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            SecureBuffer (
                Endianness endianness,
                ui32 length,
                ui32 readOffset = 0,
                ui32 writeOffset = 0) :
                Buffer (
                    endianness,
                    length,
                    readOffset,
                    writeOffset,
                    &SecureAllocator::Global) {}
            /// \brief
            /// ctor for creating a buffer from a given range.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] begin Start of range.
            /// \param[in] end Just past the end of range.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            SecureBuffer (
                Endianness endianness,
                const ui8 *begin,
                const ui8 *end,
                ui32 readOffset = 0,
                ui32 writeOffset = UI32_MAX) :
                Buffer (
                    endianness,
                    begin,
                    end,
                    readOffset,
                    writeOffset,
                    &SecureAllocator::Global) {}

            /// \brief
            /// Resize the buffer. Adjust readOffset and writeOffset to stay within [0, length).
            /// \param[in] length New buffer length.
            /// \param[in] allocator Allocator to use to allocate new data.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            virtual void Resize (
                ui32 length,
                Allocator * /*allocator*/ = &DefaultAllocator::Global);

            /// \brief
            /// Clone the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            /// \return A clone of this buffer.
            virtual UniquePtr Clone (Allocator * /*allocator*/ = &DefaultAllocator::Global) const;

        #if defined (THEKOGANS_UTIL_HAVE_ZLIB)
            /// \brief
            /// Use zlib to compress the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            /// \return A buffer containing deflated data.
            virtual UniquePtr Deflate (Allocator * /*allocator*/ = &DefaultAllocator::Global);
            /// \brief
            /// Use zlib to decompress the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            /// \return A buffer containing inflated data.
            virtual UniquePtr Inflate (Allocator * /*allocator*/ = &DefaultAllocator::Global);
        #endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)

            /// \brief
            /// SecureBuffer is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SecureBuffer)
        };

        /// \struct TenantReadBuffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// TenantReadBuffer is used to wrap a static buffer for reading.

        struct TenantReadBuffer : public Buffer {
            /// \brief
            /// ctor for wrapping a raw data pointer.
            /// \param[in] endianness How multi-byte values are stored.
            /// \param[in] data Pointer to wrap.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            TenantReadBuffer (
                Endianness endianness,
                const ui8 *data,
                ui32 length,
                ui32 readOffset = 0) :
                Buffer (
                    endianness,
                    const_cast<ui8 *> (data),
                    length,
                    readOffset,
                    length,
                    &NullAllocator::Global) {}

            /// \brief
            /// Write raw bytes to a buffer.
            /// \param[in] buffer Bytes to write.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual ui32 Write (
                    const void * /*buffer*/,
                    ui32 /*count*/) {
                assert (0);
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s", "TenantReadBuffer can't Write.");
                return -1;
            }

            /// \brief
            /// TenantReadBuffer is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TenantReadBuffer)
        };

        /// \struct TenantWriteBuffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// TenantWriteBuffer is used to wrap a static buffer for writing.

        struct TenantWriteBuffer : public Buffer {
            /// \brief
            /// ctor for wrapping a raw data pointer.
            /// \param[in] endianness How multi-byte values are stored.
            /// \param[in] data Pointer to wrap.
            /// \param[in] length Length of data.
            /// \param[in] writeOffset Offset at which to write.
            TenantWriteBuffer (
                Endianness endianness,
                ui8 *data,
                ui32 length,
                ui32 writeOffset = 0) :
                Buffer (
                    endianness,
                    data,
                    length,
                    0,
                    writeOffset,
                    &NullAllocator::Global) {}

            /// \brief
            /// TenantWriteBuffer is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TenantWriteBuffer)
        };

        /// \brief
        /// Write the given buffer to the given serializer.
        /// \param[in] serializer Where to write the given guid.
        /// \param[in] buffer Buffer to write.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
            Serializer &serializer,
            const Buffer &buffer);

        /// \brief
        /// Read a buffer from the given serializer.
        /// \param[in] serializer Where to read the guid from.
        /// \param[in] buffer Buffer to read.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
            Serializer &serializer,
            Buffer &buffer);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Buffer_h)
