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

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cassert>
#include <memory>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/SizeT.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Exception.h"
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
        /// The following diagram represents the various buffer regions:
        ///
        /// |--- consumed ---+--- available for reading ---+--- available for writing ---|
        /// 0            readOffset                   writeOffset                     length

        struct _LIB_THEKOGANS_UTIL_DECL Buffer : public Serializer {
            /// \brief
            /// Buffer data.
            ui8 *data;
            /// \brief
            /// Length of data.
            SizeT length;
            /// \brief
            /// Current read position.
            SizeT readOffset;
            /// \brief
            /// Current write position.
            SizeT writeOffset;
            /// \brief
            /// Allocator used for memory management.
            Allocator *allocator;

            /// \brief
            /// Move ctor.
            /// \param[in,out] other Buffer to move.
            Buffer (Buffer &&other) :
                    Serializer (other.endianness),
                    data (0),
                    length (0),
                    readOffset (0),
                    writeOffset (0),
                    allocator (other.allocator) {
                swap (other);
            }
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
                void *data_ = 0,
                std::size_t length_ = 0,
                std::size_t readOffset_ = 0,
                std::size_t writeOffset_ = 0,
                Allocator *allocator_ = &DefaultAllocator::Global) :
                Serializer (endianness),
                data ((ui8 *)data_),
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
                std::size_t length_,
                std::size_t readOffset_ = 0,
                std::size_t writeOffset_ = 0,
                Allocator *allocator_ = &DefaultAllocator::Global) :
                Serializer (endianness),
                data ((ui8 *)allocator_->Alloc (length_)),
                length (length_),
                readOffset (readOffset_),
                writeOffset (writeOffset_),
                allocator (allocator_) {}
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
                const void *begin,
                const void *end,
                std::size_t readOffset_ = 0,
                std::size_t writeOffset_ = SIZE_T_MAX,
                Allocator *allocator_ = &DefaultAllocator::Global);
            /// \brief
            /// dtor.
            virtual ~Buffer () {
                Resize (0);
            }

            /// \brief
            /// Move assignment operator.
            /// \param[in,out] other Buffer to move.
            /// \return *this.
            Buffer &operator = (Buffer &&other);

            /// \brief
            /// std::swap for Buffer.
            /// \param[in,out] other Buffer to swap.
            void swap (Buffer &other);

            // Serializer
            /// \brief
            /// Read raw bytes from a buffer.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual std::size_t Read (
                void *buffer,
                std::size_t count);
            /// \brief
            /// Write raw bytes to a buffer.
            /// \param[in] buffer Bytes to write.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual std::size_t Write (
                const void *buffer,
                std::size_t count);

            /// \brief
            /// Append the contents of the given buffer to this one.
            /// \param[in] buffer Buffer whose contents to append.
            /// \return *this
            Buffer &operator += (const Buffer &buffer);

            /// \brief
            /// Resize the buffer. Adjust readOffset and writeOffset to stay within [0, length).
            /// \param[in] length_ New buffer length.
            /// \param[in] allocator_ Allocator to use to allocate new data.
            virtual void Resize (
                std::size_t length_,
                Allocator *allocator_ = &DefaultAllocator::Global);

            /// \brief
            /// Clone the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// \return A clone of this buffer.
            virtual Buffer Clone (Allocator *allocator = &DefaultAllocator::Global) const;

            /// \brief
            /// Return subset of the buffer.
            /// \param[in] offset Beginning of sub-buffer.
            /// \param[in] count Size of sub-buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// \return A subset of this buffer.
            /// NOTE: Unlike other methods, this one does NOT take
            /// readOffset and writeOffset in to account. A straight
            /// subset of [data, data + length) is returned.
            virtual Buffer Subset (
                std::size_t offset,
                std::size_t count = SIZE_T_MAX,
                Allocator *allocator = &DefaultAllocator::Global) const;

            /// \brief
            /// Return the serialized size of this buffer.
            /// \return Serialized size of this buffer.
            std::size_t Size () const;

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
            /// Return number of bytes read from the buffer.
            /// \return Number of bytes read from the buffer.
            inline std::size_t GetDataConsumed () const {
                return readOffset;
            }
            /// \brief
            /// Return number of bytes available for reading.
            /// \return Number of bytes available for reading.
            inline std::size_t GetDataAvailableForReading () const {
                return writeOffset > readOffset ? writeOffset - readOffset : 0;
            }
            /// \brief
            /// Return number of bytes available for writing.
            /// \return Number of bytes available for writing.
            inline std::size_t GetDataAvailableForWriting () const {
                return length > writeOffset ? length - writeOffset : 0;
            }
            /// \brief
            /// Return total buffer length.
            /// \return Total buffer length.
            inline std::size_t GetLength () const {
                return length;
            }

            /// \brief
            /// Return the data pointer.
            /// \return The data pointer.
            inline ui8 *GetDataPtr () const {
                return data;
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
            /// NOTE: If advance == 0, the call is a silent noop.
            /// \param[in] advance Amount to advance the readOffset.
            /// \return Number of bytes actually advanced.
            std::size_t AdvanceReadOffset (std::size_t advance);
            /// \brief
            /// Advance the write offset taking care not to overflow.
            /// NOTE: If advance == 0, the call is a silent noop.
            /// \param[in] advance Amount to advance the writeOffset.
            /// \return Number of bytes actually advanced.
            std::size_t AdvanceWriteOffset (std::size_t advance);

            /// \brief
            /// Reset the readOffset and the writeOffset to prepare the
            /// buffer for reuse.
            /// \param[in] readOnly true == Reset for reading only.
            inline void Rewind (bool readOnly = false) {
                readOffset = 0;
                if (!readOnly) {
                    writeOffset = 0;
                }
            }

        #if defined (THEKOGANS_UTIL_HAVE_ZLIB)
            /// \brief
            /// Use zlib to compress the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// \return A buffer containing deflated data.
            virtual Buffer Deflate (Allocator *allocator = &DefaultAllocator::Global);
            /// \brief
            /// Use zlib to decompress the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// \return A buffer containing inflated data.
            virtual Buffer Inflate (Allocator *allocator = &DefaultAllocator::Global);
        #endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)

            /// \brief
            /// Convert the buffer to a std::string.
            /// \return std::string containing the buffers contents.
            inline std::string Tostring () const {
                return GetDataAvailableForReading () > 0 ?
                    std::string (GetReadPtr (), GetReadPtrEnd ()) :
                    std::string ();
            }

            /// \brief
            /// Convert the buffer to a std::vector.
            /// \return std::vector containing the buffers contents.
            inline std::vector<ui8> Tovector () const {
                return GetDataAvailableForReading () > 0 ?
                    std::vector<ui8> (GetReadPtr (), GetReadPtrEnd ()) :
                    std::vector<ui8> ();
            }

            /// \brief
            /// Convert the buffer to a SecureVector.
            /// \return SecureVector containing the buffers contents.
            inline SecureVector<ui8> ToSecureVector () const {
                return GetDataAvailableForReading () > 0 ?
                    SecureVector<ui8> (GetReadPtr (), GetReadPtrEnd ()) :
                    SecureVector<ui8> ();
            }

        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Convert the buffer to a Windows HGLOBAL.
            /// \param[in] flags GlobalAlloc flags.
            /// \return HGLOBAL containing the buffers contents.
            HGLOBAL ToHGLOBAL (UINT flags = GMEM_MOVEABLE) const;
        #endif // defined (TOOLCHAIN_OS_Windows)

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
            /// Move ctor.
            /// \param[in,out] other SecureBuffer to move.
            SecureBuffer (SecureBuffer &&other) :
                Buffer (std::move (other)) {}
            /// \brief
            /// ctor for wrapping a raw data pointer.
            /// \param[in] endianness How multi-byte values are stored.
            /// \param[in] data Pointer to wrap.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            SecureBuffer (
                Endianness endianness = HostEndian,
                void *data = 0,
                std::size_t length = 0,
                std::size_t readOffset = 0,
                std::size_t writeOffset = 0) :
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
                std::size_t length,
                std::size_t readOffset = 0,
                std::size_t writeOffset = 0) :
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
                const void *begin,
                const void *end,
                std::size_t readOffset = 0,
                std::size_t writeOffset = SIZE_T_MAX) :
                Buffer (
                    endianness,
                    begin,
                    end,
                    readOffset,
                    writeOffset,
                    &SecureAllocator::Global) {}

            /// \brief
            /// Move assignment operator.
            /// \param[in,out] other SecureBuffer to move.
            /// \return *this.
            SecureBuffer &operator = (SecureBuffer &&other);

            /// \brief
            /// Resize the buffer. Adjust readOffset and writeOffset to stay within [0, length).
            /// \param[in] length New buffer length.
            /// \param[in] allocator Allocator to use to allocate new data.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            virtual void Resize (
                std::size_t length,
                Allocator * /*allocator*/ = &DefaultAllocator::Global);

            /// \brief
            /// Clone the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            /// \return A clone of this buffer.
            virtual Buffer Clone (Allocator * /*allocator*/ = &DefaultAllocator::Global) const;

            /// \brief
            /// Return subset of the buffer.
            /// \param[in] offset Beginning of sub-buffer.
            /// \param[in] count Size of sub-buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            /// \return A subset of this buffer.
            /// NOTE: Unlike other methods, this one does NOT take
            /// readOffset and writeOffset in to account. A straight
            /// subset of [data, data + length) is returned.
            virtual Buffer Subset (
                std::size_t offset,
                std::size_t count,
                Allocator * /*allocator*/ = &DefaultAllocator::Global) const;

        #if defined (THEKOGANS_UTIL_HAVE_ZLIB)
            /// \brief
            /// Use zlib to compress the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            /// \return A buffer containing deflated data.
            virtual Buffer Deflate (Allocator * /*allocator*/ = &DefaultAllocator::Global);
            /// \brief
            /// Use zlib to decompress the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            /// \return A buffer containing inflated data.
            virtual Buffer Inflate (Allocator * /*allocator*/ = &DefaultAllocator::Global);
        #endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)

            /// \brief
            /// SecureBuffer is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SecureBuffer)
        };

        /// \struct TenantReadBuffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// TenantReadBuffer is used to wrap a raw buffer for reading.

        struct TenantReadBuffer : public Buffer {
            /// \brief
            /// ctor for wrapping a raw data pointer.
            /// \param[in] endianness How multi-byte values are stored.
            /// \param[in] data Pointer to wrap.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            TenantReadBuffer (
                Endianness endianness,
                const void *data,
                std::size_t length,
                std::size_t readOffset = 0) :
                Buffer (
                    endianness,
                    const_cast<void *> (data),
                    length,
                    readOffset,
                    length,
                    &NullAllocator::Global) {}

            /// \brief
            /// Write raw bytes to a buffer.
            /// \param[in] buffer Bytes to write.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual std::size_t Write (
                    const void * /*buffer*/,
                    std::size_t /*count*/) {
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
        /// TenantWriteBuffer is used to wrap a raw buffer for writing.

        struct TenantWriteBuffer : public Buffer {
            /// \brief
            /// ctor for wrapping a raw data pointer.
            /// \param[in] endianness How multi-byte values are stored.
            /// \param[in] data Pointer to wrap.
            /// \param[in] length Length of data.
            /// \param[in] writeOffset Offset at which to write.
            TenantWriteBuffer (
                Endianness endianness,
                void *data,
                std::size_t length,
                std::size_t writeOffset = 0) :
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
        /// NOTE: Insertion does not save the buffer's allocator.
        /// \param[in] serializer Where to write the given buffer.
        /// \param[in] buffer Buffer to write.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
            Serializer &serializer,
            const Buffer &buffer);

        /// \brief
        /// Read a Buffer from the given \see{Serializer}.
        /// NOTE: Extraction does not preserve the buffer's allocator.
        /// After this function completes, the buffer's data will have
        /// been allocated using DefaultAllocator::Global.
        /// \param[in] serializer Where to read the buffer from.
        /// \param[out] buffer Buffer to read.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
            Serializer &serializer,
            Buffer &buffer);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Buffer_h)
