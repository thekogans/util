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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cassert>
#include <memory>
#include "pugixml/pugixml.hpp"
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/SizeT.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/JSON.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/SecureAllocator.h"
#include "thekogans/util/NullAllocator.h"
#include "thekogans/util/StringUtils.h"

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
        /// occasions (See \see{SecureBuffer} and \see{TenantBuffer}
        /// below).
        /// NOTE: Buffer maintains distinct read and write positions
        /// (readOffset and writeOffset). It allows you to continue
        /// filling the buffer without disturbing the current read
        /// position. This is very useful in streaming operations.
        /// The following diagram represents various buffer regions:
        ///
        /// |--- consumed ---+--- available for reading ---+--- available for writing ---|
        /// 0            readOffset                   writeOffset                     length
        ///
        /// NOTE: Buffer is not thread safe. The need for a thread
        /// safe buffer is rare and, for performance reasons, it's
        /// better to leave the buffer lock free. That said, even
        /// if buffer access was thread safe, because of it's design
        /// (read and write offsets), maintaining proper read/write
        /// synchronization between threads would require careful
        /// planning and execution. To that end, if you're using
        /// a buffer in a multithreaded (async) environment (to pass
        /// as argument to a \see{Producer} event), you should treat
        /// any buffer passed to you as const and use \see{TenantReadBuffer}
        /// as your own local set of buffer variables. This way any
        /// other event consumers that will receive the same buffer
        /// pointer will get correct read/write offsets as the caller
        /// intended. If you can't process the buffer during the
        /// callback and you don't know if there are other event
        /// recipients it's best to make a copy.

        struct _LIB_THEKOGANS_UTIL_DECL Buffer : public Serializer {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (Buffer)

            /// \brief
            /// Buffer has a private heap to help with memory
            /// management, performance, and global heap fragmentation.
            THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

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
            Allocator::SharedPtr allocator;

            /// \brief
            /// Copy ctor.
            /// \param[in] other Buffer to copy.
            Buffer (const Buffer &other);
            /// \brief
            /// Move ctor.
            /// \param[in,out] other Buffer to move.
            Buffer (Buffer &&other) :
                    Serializer (HostEndian),
                    data (nullptr),
                    length (0),
                    readOffset (0),
                    writeOffset (0),
                    allocator (DefaultAllocator::Instance ()) {
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
                void *data_ = nullptr,
                std::size_t length_ = 0,
                std::size_t readOffset_ = 0,
                std::size_t writeOffset_ = 0,
                Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ()) :
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
                Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ()) :
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
                Allocator::SharedPtr allocator_ = DefaultAllocator::Instance ());
            /// \brief
            /// dtor.
            virtual ~Buffer () {
                Resize (0);
            }

            /// \brief
            /// Copy assignment operator.
            /// \param[in] other Buffer to copy.
            /// \return *this.
            Buffer &operator = (const Buffer &other);
            /// \brief
            /// Move assignment operator.
            /// \param[in,out] other Buffer to move.
            /// \return *this.
            Buffer &operator = (Buffer &&other);

            /// \brief
            /// std::swap for Buffer.
            /// \param[in,out] buffer Buffer to swap.
            void swap (Buffer &buffer);

            // Serializer
            /// \brief
            /// Read raw bytes from a buffer.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual std::size_t Read (
                void *buffer,
                std::size_t count) override;
            /// \brief
            /// Write raw bytes to a buffer.
            /// \param[in] buffer Bytes to write.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual std::size_t Write (
                const void *buffer,
                std::size_t count) override;

            /// \brief
            /// Clear the buffer (set to '\0').
            /// NOTE: This method does not respect read and write offsets.
            /// It clears from data to data + length.
            /// \param[in] rewind true == Reset the read (and possibly write) offsets.
            /// \param[in] readOnly true == Only rewind the read offset.
            void Clear (
                bool rewind = true,
                bool readOnly = false);

            /// \brief
            /// Reset the readOffset and the writeOffset to prepare the
            /// buffer for reuse.
            /// \param[in] readOnly true == Reset for reading only.
            void Rewind (bool readOnly = false);

            /// \brief
            /// Resize the buffer. Adjust readOffset and writeOffset to stay within [0, length).
            /// \param[in] length_ New buffer length.
            /// \param[in] allocator_ Allocator to use to allocate new data.
            /// If nullptr than use the buffers allocator.
            virtual void Resize (
                std::size_t length_,
                Allocator::SharedPtr allocator_ = nullptr);

            /// \brief
            /// Clone the buffer.
            /// \param[in] allocator_ Allocator for the returned buffer.
            /// If nullptr than use the buffers allocator.
            /// \return A clone of this buffer.
            virtual SharedPtr Clone (
                Allocator::SharedPtr allocator_ = nullptr) const;

            /// \brief
            /// Return subset of the buffer.
            /// \param[in] offset Beginning of sub-buffer.
            /// \param[in] count Size of sub-buffer.
            /// \param[in] allocator_ Allocator for the returned buffer.
            /// If nullptr than use the buffers allocator.
            /// \return A subset of this buffer.
            /// NOTE: Unlike other methods, this one does NOT take
            /// readOffset and writeOffset in to account. A straight
            /// subset of [data, data + length) is returned.
            virtual SharedPtr Subset (
                std::size_t offset,
                std::size_t count = SIZE_T_MAX,
                Allocator::SharedPtr allocator_ = nullptr) const;

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

        #if defined (THEKOGANS_UTIL_HAVE_ZLIB)
            /// \brief
            /// Use zlib to compress the buffer.
            /// \param[in] allocator_ Allocator for the returned buffer.
            /// If nullptr than use the buffers allocator.
            /// \return A buffer containing deflated data.
            virtual SharedPtr Deflate (
                Allocator::SharedPtr allocator_ = nullptr) const;
            /// \brief
            /// Use zlib to decompress the buffer.
            /// \param[in] allocator_ Allocator for the returned buffer.
            /// If nullptr than use the buffers allocator.
            /// \return A buffer containing inflated data.
            virtual SharedPtr Inflate (
                Allocator::SharedPtr allocator_ = nullptr) const;
        #endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)

            /// \brief
            /// Given a hex encoded string and length, convert to Buffer.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] hexBuffer Hex encoded string.
            /// \param[in] hexBufferLength hexBuffer length (must be even).
            /// \param[in] allocator Allocator for the returned buffer.
            /// If nullptr than use \see{DefaultAllocator}::Instance () allocator.
            /// \return Buffer containing the decoded hex string.
            static SharedPtr FromHexBuffer (
                Endianness endianness,
                const char *hexBuffer,
                std::size_t hexBufferLength,
                Allocator::SharedPtr allocator = nullptr);
            /// \brief
            /// Convert the buffer to a hex string.
            /// \return std::string containing the buffers hex encoded contents.
            inline std::string ToHexString () const {
                return GetDataAvailableForReading () > 0 ?
                    HexEncodeBuffer (GetReadPtr (), GetDataAvailableForReading ()) :
                    std::string ();
            }

            /// \brief
            /// Convert the buffer to a std::string.
            /// \return std::string containing the buffers contents.
            inline std::string Tostring () const {
                return GetDataAvailableForReading () > 0 ?
                    std::string (GetReadPtr (), GetReadPtrEnd ()) :
                    std::string ();
            }
            /// \brief
            /// Convert the buffer to a std::wstring.
            /// \return std::wstring containing the buffers contents.
            inline std::wstring Towstring () const {
                return
                    GetDataAvailableForReading () > 0 &&
                    (GetDataAvailableForReading () % WCHAR_T_SIZE) == 0 ?
                    std::wstring (
                        (const wchar_t *)GetReadPtr (),
                        GetDataAvailableForReading () / WCHAR_T_SIZE) :
                    std::wstring ();
            }

            /// \brief
            /// Convert the buffer to a \see{SecureString}.
            /// \return see{SecureString} containing the buffers contents.
            inline SecureString ToSecureString () const {
                return GetDataAvailableForReading () > 0 ?
                    SecureString (GetReadPtr (), GetReadPtrEnd ()) :
                    SecureString ();
            }

            /// \brief
            /// Convert the buffer to a \see{SecureWString}.
            /// \return see{SecureWString} containing the buffers contents.
            inline SecureWString ToSecureWString () const {
                return GetDataAvailableForReading () > 0 ?
                    SecureWString (GetReadPtr (), GetReadPtrEnd ()) :
                    SecureWString ();
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
        };

        /// \struct SecureBuffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// Wraps a \see{SecureAllocator}. SecureBuffers are very useful
        /// in cryptographic contexts as well as times when you need to
        /// ensure buffer data stays away from prying eyes.

        struct _LIB_THEKOGANS_UTIL_DECL SecureBuffer : public Buffer {
            /// \brief
            /// SecureBuffer has a private heap to help with memory
            /// management, performance, and global heap fragmentation.
            THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

            /// \brief
            /// Copy ctor.
            /// \param[in,out] other SecureBuffer to move.
            SecureBuffer (const SecureBuffer &other) :
                Buffer (other) {}
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
                void *data = nullptr,
                std::size_t length = 0,
                std::size_t readOffset = 0,
                std::size_t writeOffset = 0) :
                Buffer (
                    endianness,
                    data,
                    length,
                    readOffset,
                    writeOffset,
                    SecureAllocator::Instance ()) {}
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
                    SecureAllocator::Instance ()) {}
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
                    SecureAllocator::Instance ()) {}
            /// \brief
            /// dtor. Zero out the buffer before releasing.
            virtual ~SecureBuffer ();

            /// \brief
            /// Copy assignment operator.
            /// \param[in,out] other SecureBuffer to move.
            /// \return *this.
            SecureBuffer &operator = (const SecureBuffer &other);
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
                Allocator::SharedPtr /*allocator*/ = nullptr) override;

            /// \brief
            /// Clone the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            /// \return A clone of this buffer.
            virtual SharedPtr Clone (
                Allocator::SharedPtr /*allocator*/ = nullptr) const override;

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
            virtual SharedPtr Subset (
                std::size_t offset,
                std::size_t count,
                Allocator::SharedPtr /*allocator*/ = nullptr) const override;

        #if defined (THEKOGANS_UTIL_HAVE_ZLIB)
            /// \brief
            /// Use zlib to compress the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            /// \return A buffer containing deflated data.
            virtual SharedPtr Deflate (
                Allocator::SharedPtr /*allocator*/ = nullptr) const override;
            /// \brief
            /// Use zlib to decompress the buffer.
            /// \param[in] allocator Allocator for the returned buffer.
            /// NOTE: The allocator paramater is ignored as SecureBuffer uses the SecureAllocator.
            /// \return A buffer containing inflated data.
            virtual SharedPtr Inflate (
                Allocator::SharedPtr /*allocator*/ = nullptr) const override;
        #endif // defined (THEKOGANS_UTIL_HAVE_ZLIB)
        };

        /// \struct TenantReadBuffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// TenantReadBuffer is used to wrap a raw byte stream for reading.

        struct TenantReadBuffer : public Buffer {
            /// \brief
            /// ctor.
            /// \param[in] buffer \see{Buffer} to wrap.
            TenantReadBuffer (const Buffer &buffer) :
                Buffer (
                    buffer.endianness,
                    const_cast<void *> ((const void *)buffer.data),
                    buffer.length,
                    buffer.readOffset,
                    buffer.writeOffset,
                    NullAllocator::Instance ()) {}
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
                    NullAllocator::Instance ()) {}

            /// \brief
            /// Copy assignment operator.
            /// \param[in] buffer Buffer to wrap.
            /// \return *this.
            TenantReadBuffer &operator = (const Buffer &buffer) {
                if (&buffer != this) {
                    data = buffer.data;
                    length = buffer.length;
                    readOffset = buffer.readOffset;
                    writeOffset = buffer.writeOffset;
                    allocator = NullAllocator::Instance ();
                }
                return *this;
            }

            /// \brief
            /// Write raw bytes to a buffer.
            /// \param[in] buffer Bytes to write.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual std::size_t Write (
                    const void * /*buffer*/,
                    std::size_t /*count*/) override {
                assert (0);
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "TenantReadBuffer can't Write.");
                return -1;
            }
        };

        /// \struct TenantWriteBuffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// TenantWriteBuffer is used to wrap a raw byte stream for writing.

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
                    NullAllocator::Instance ()) {}
        };

        /// \struct NetworkBuffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// A convenience class to obviate the need to provide NetworkEndian all the time.

        struct _LIB_THEKOGANS_UTIL_DECL NetworkBuffer : public Buffer {
            /// \brief
            /// Copy ctor.
            /// \param[in,out] other NetworkBuffer to move.
            NetworkBuffer (const NetworkBuffer &other) :
                Buffer (other) {}
            /// \brief
            /// Move ctor.
            /// \param[in,out] other NetworkBuffer to move.
            NetworkBuffer (NetworkBuffer &&other) :
                Buffer (std::move (other)) {}
            /// \brief
            /// ctor for wrapping a raw data pointer.
            /// \param[in] endianness How multi-byte values are stored.
            /// \param[in] data Pointer to wrap.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            NetworkBuffer (
                void *data = nullptr,
                std::size_t length = 0,
                std::size_t readOffset = 0,
                std::size_t writeOffset = 0,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                Buffer (NetworkEndian, data, length, readOffset, writeOffset, allocator) {}
            /// \brief
            /// ctor for creating a buffer of a given length.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            NetworkBuffer (
                std::size_t length,
                std::size_t readOffset = 0,
                std::size_t writeOffset = 0,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                Buffer (NetworkEndian, length, readOffset, writeOffset, allocator) {}
            /// \brief
            /// ctor for creating a buffer from a given range.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] begin Start of range.
            /// \param[in] end Just past the end of range.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            NetworkBuffer (
                const void *begin,
                const void *end,
                std::size_t readOffset = 0,
                std::size_t writeOffset = SIZE_T_MAX,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                Buffer (NetworkEndian, begin, end, readOffset, writeOffset, allocator) {}
        };

        /// \struct SecureNetworkBuffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// A convenience class to obviate the need to provide NetworkEndian all the time.

        struct _LIB_THEKOGANS_UTIL_DECL SecureNetworkBuffer : public SecureBuffer {
            /// \brief
            /// Copy ctor.
            /// \param[in,out] other SecureNetworkBuffer to move.
            SecureNetworkBuffer (const SecureNetworkBuffer &other) :
                SecureBuffer (other) {}
            /// \brief
            /// Move ctor.
            /// \param[in,out] other SecureNetworkBuffer to move.
            SecureNetworkBuffer (SecureNetworkBuffer &&other) :
                SecureBuffer (std::move (other)) {}
            /// \brief
            /// ctor for wrapping a raw data pointer.
            /// \param[in] endianness How multi-byte values are stored.
            /// \param[in] data Pointer to wrap.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            SecureNetworkBuffer (
                void *data = nullptr,
                std::size_t length = 0,
                std::size_t readOffset = 0,
                std::size_t writeOffset = 0) :
                SecureBuffer (NetworkEndian, data, length, readOffset, writeOffset) {}
            /// \brief
            /// ctor for creating a buffer of a given length.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            SecureNetworkBuffer (
                std::size_t length,
                std::size_t readOffset = 0,
                std::size_t writeOffset = 0) :
                SecureBuffer (NetworkEndian, length, readOffset, writeOffset) {}
            /// \brief
            /// ctor for creating a buffer from a given range.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] begin Start of range.
            /// \param[in] end Just past the end of range.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            SecureNetworkBuffer (
                const void *begin,
                const void *end,
                std::size_t readOffset = 0,
                std::size_t writeOffset = SIZE_T_MAX) :
                SecureBuffer (NetworkEndian, begin, end, readOffset, writeOffset) {}
        };

        /// \struct HostBuffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// A convenience class to obviate the need to provide HostEndian all the time.

        struct _LIB_THEKOGANS_UTIL_DECL HostBuffer : public Buffer {
            /// \brief
            /// Copy ctor.
            /// \param[in,out] other HostBuffer to move.
            HostBuffer (const HostBuffer &other) :
                Buffer (other) {}
            /// \brief
            /// Move ctor.
            /// \param[in,out] other HostBuffer to move.
            HostBuffer (HostBuffer &&other) :
                Buffer (std::move (other)) {}
            /// \brief
            /// ctor for wrapping a raw data pointer.
            /// \param[in] endianness How multi-byte values are stored.
            /// \param[in] data Pointer to wrap.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            HostBuffer (
                void *data = nullptr,
                std::size_t length = 0,
                std::size_t readOffset = 0,
                std::size_t writeOffset = 0,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                Buffer (HostEndian, data, length, readOffset, writeOffset, allocator) {}
            /// \brief
            /// ctor for creating a buffer of a given length.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            HostBuffer (
                std::size_t length,
                std::size_t readOffset = 0,
                std::size_t writeOffset = 0,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                Buffer (HostEndian, length, readOffset, writeOffset, allocator) {}
            /// \brief
            /// ctor for creating a buffer from a given range.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] begin Start of range.
            /// \param[in] end Just past the end of range.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            HostBuffer (
                const void *begin,
                const void *end,
                std::size_t readOffset = 0,
                std::size_t writeOffset = SIZE_T_MAX,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                Buffer (HostEndian, begin, end, readOffset, writeOffset, allocator) {}
        };

        /// \struct SecureHostBuffer Buffer.h thekogans/util/Buffer.h
        ///
        /// \brief
        /// A convenience class to obviate the need to provide HostEndian all the time.

        struct _LIB_THEKOGANS_UTIL_DECL SecureHostBuffer : public SecureBuffer {
            /// \brief
            /// Copy ctor.
            /// \param[in,out] other SecureHostBuffer to move.
            SecureHostBuffer (const SecureHostBuffer &other) :
                SecureBuffer (other) {}
            /// \brief
            /// Move ctor.
            /// \param[in,out] other SecureHostBuffer to move.
            SecureHostBuffer (SecureHostBuffer &&other) :
                SecureBuffer (std::move (other)) {}
            /// \brief
            /// ctor for wrapping a raw data pointer.
            /// \param[in] endianness How multi-byte values are stored.
            /// \param[in] data Pointer to wrap.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            SecureHostBuffer (
                void *data = nullptr,
                std::size_t length = 0,
                std::size_t readOffset = 0,
                std::size_t writeOffset = 0) :
                SecureBuffer (HostEndian, data, length, readOffset, writeOffset) {}
            /// \brief
            /// ctor for creating a buffer of a given length.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] length Length of data.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            SecureHostBuffer (
                std::size_t length,
                std::size_t readOffset = 0,
                std::size_t writeOffset = 0) :
                SecureBuffer (HostEndian, length, readOffset, writeOffset) {}
            /// \brief
            /// ctor for creating a buffer from a given range.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] begin Start of range.
            /// \param[in] end Just past the end of range.
            /// \param[in] readOffset Offset at which to read.
            /// \param[in] writeOffset Offset at which to write.
            SecureHostBuffer (
                const void *begin,
                const void *end,
                std::size_t readOffset = 0,
                std::size_t writeOffset = SIZE_T_MAX) :
                SecureBuffer (HostEndian, begin, end, readOffset, writeOffset) {}
        };

        /// \brief
        /// Write the given buffer to the given \see{Serializer}.
        /// \param[in] serializer Where to write the given buffer.
        /// \param[in] buffer Buffer to write.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
            Serializer &serializer,
            const Buffer &buffer);

        /// \brief
        /// Read a buffer from the given \see{Serializer}.
        /// \param[in] serializer Where to read the buffer from.
        /// \param[out] buffer Buffer to read.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
            Serializer &serializer,
            Buffer &buffer);

        /// \brief
        /// Write the given buffer to the given node.
        /// The node syntax looks like this:
        /// <Buffer Endianness = ""
        ///         Length = ""
        ///         ReadOffset = ""
        ///         WriteOffset = ""
        ///         Allocator = "">
        /// Base 64 encoded buffer contents.
        /// </Buffer>
        /// \param[in] node Where to write the given buffer.
        /// \param[in] buffer Buffer to write.
        /// \return node.
        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
            pugi::xml_node &node,
            const Buffer &buffer);

        /// \brief
        /// Read an buffer from the given node.
        /// The node syntax looks like this:
        /// <Buffer Endianness = ""
        ///         Length = ""
        ///         ReadOffset = ""
        ///         WriteOffset = ""
        ///         Allocator = "">
        /// Base 64 encoded buffer contents.
        /// </Buffer>
        /// \param[in] node Where to read the buffer from.
        /// \param[out] buffer Buffer to read.
        /// \return node.
        _LIB_THEKOGANS_UTIL_DECL pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
            pugi::xml_node &node,
            Buffer &buffer);

        /// \brief
        /// Write the given buffer to the given object.
        /// The object syntax looks like this:
        /// {
        ///   Endianness: 'endianness',
        ///   Length: 'length',
        ///   ReadOffset: 'read offset',
        ///   WriteOffset: 'write offset',
        ///   Allocator: 'allocator name',
        ///   Contents: {
        ///   ...
        ///   }
        /// }
        /// \param[in] object Where to write the given buffer.
        /// \param[in] buffer Buffer to write.
        /// \return object.
        _LIB_THEKOGANS_UTIL_DECL JSON::Object & _LIB_THEKOGANS_UTIL_API operator << (
            JSON::Object &object,
            const Buffer &buffer);

        /// \brief
        /// Read an Exception from the given object.
        /// The object syntax looks like this:
        /// {
        ///   Endianness: 'endianness',
        ///   Length: 'length',
        ///   ReadOffset: 'read offset',
        ///   WriteOffset: 'write offset',
        ///   Allocator: 'allocator name',
        ///   Contents: {
        ///   ...
        ///   }
        /// }
        /// \param[in] object Where to read the buffer from.
        /// \param[out] buffer Buffer to read.
        /// \return node.
        _LIB_THEKOGANS_UTIL_DECL JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
            JSON::Object &object,
            Buffer &buffer);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Buffer_h)
