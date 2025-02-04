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

#if !defined (__thekogans_util_FixedBuffer_h)
#define __thekogans_util_FixedBuffer_h

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/SizeT.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/SecureAllocator.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)

namespace thekogans {
    namespace util {

        /// \struct FixedBuffer FixedBuffer.h thekogans/util/FixedBuffer.h
        ///
        /// \brief
        /// FixedBuffer is a convenient in memory fixed length serializer. It's strength
        /// comes from it's ability to be 1. Defined inline and 2. Constructed like any
        /// other first class object (unlike c arrays). The following diagram represents
        /// the various buffer regions:
        ///
        /// |--- consumed ---+--- available for reading ---+--- available for writing ---|
        /// 0            readOffset                   writeOffset                     length

        template<std::size_t length>
        struct FixedBuffer : public Serializer {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (FixedBuffer)

            /// \brief
            /// FixedBuffer data.
            ui8 data[length];
            /// \brief
            /// Current read position.
            SizeT readOffset;
            /// \brief
            /// Current write position.
            SizeT writeOffset;

            /// \brief
            /// ctor.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] data_ Pointer to wrap.
            /// \param[in] length_ Length of data.
            /// \param[in] clearUnused Clear unused array elements to 0.
            FixedBuffer (
                    Endianness endianness = HostEndian,
                    const void *data_ = nullptr,
                    std::size_t length_ = 0,
                    bool clearUnused = false) :
                    Serializer (endianness),
                    readOffset (0),
                    writeOffset (length_) {
                if (writeOffset <= length) {
                    if (data_ != nullptr && length_ > 0) {
                        memcpy (data, data_, length_);
                    }
                    if (clearUnused) {
                        memset (GetWritePtr (), 0, GetDataAvailableForWriting ());
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
            /// \brief
            /// ctor for creating a fixed buffer from a given range.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] begin Start of range.
            /// \param[in] end Just past the end of range.
            /// \param[in] clearUnused Clear unused array elements to 0.
            FixedBuffer (
                    Endianness endianness,
                    const void *begin,
                    const void *end,
                    bool clearUnused = false) :
                    Serializer (endianness),
                    readOffset (0),
                    writeOffset ((const ui8 *)end - (const ui8 *)begin) {
                if (writeOffset <= length) {
                    if (writeOffset > 0) {
                        memcpy (data, begin, writeOffset);
                    }
                    if (clearUnused) {
                        memset (GetWritePtr (), 0, GetDataAvailableForWriting ());
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }

            // Serializer
            /// \brief
            /// Read raw bytes from a fixed buffer.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual std::size_t Read (
                    void *buffer,
                    std::size_t count) override {
                if (buffer != nullptr && count > 0) {
                    std::size_t availableForReading = GetDataAvailableForReading ();
                    if (count > availableForReading) {
                        count = availableForReading;
                    }
                    if (count != 0) {
                        memcpy (buffer, GetReadPtr (), count);
                        AdvanceReadOffset (count);
                    }
                    return count;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
            /// \brief
            /// Write raw bytes to a fixed buffer.
            /// \param[in] buffer Bytes to write.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual std::size_t Write (
                    const void *buffer,
                    std::size_t count) override {
                if (buffer != nullptr && count > 0) {
                    std::size_t availableForWriting = GetDataAvailableForWriting ();
                    if (count > availableForWriting) {
                        count = availableForWriting;
                    }
                    if (count != 0) {
                        memcpy (GetWritePtr (), buffer, count);
                        AdvanceWriteOffset (count);
                    }
                    return count;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }

            /// \brief
            /// Return serialized size of FixedBuffer<length>.
            /// \return Serialized size of FixedBuffer<length>.
            inline std::size_t Size () const {
                return
                    Serializer::Size () +
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
                return (ui8 *)data;
            }
            /// \brief
            /// Return the current read data position.
            /// \return The current read data position.
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
            /// Return the current write data position.
            /// \return The current write data position.
            inline ui8 *GetWritePtr () const {
                return (ui8 *)(data + writeOffset);
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
            std::size_t AdvanceReadOffset (std::size_t advance) {
                std::size_t availableForReading = GetDataAvailableForReading ();
                if (advance > availableForReading) {
                    advance = availableForReading;
                }
                readOffset += advance;
                return advance;
            }
            /// \brief
            /// Advance the write offset taking care not to overflow.
            /// \param[in] advance Amount to advance the writeOffset.
            /// \return Number of bytes actually advanced.
            std::size_t AdvanceWriteOffset (std::size_t advance) {
                std::size_t availableForWriting = GetDataAvailableForWriting ();
                if (advance > availableForWriting) {
                    advance = availableForWriting;
                }
                writeOffset += advance;
                return advance;
            }

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
            inline HGLOBAL ToHGLOBAL (UINT flags = GMEM_MOVEABLE) const {
                if (GetDataAvailableForReading () > 0) {
                    HGLOBALPtr global (flags, GetDataAvailableForReading ());
                    if (global != nullptr) {
                        memcpy (
                            global,
                            GetReadPtr (),
                            GetDataAvailableForReading ());
                        return global.Release ();
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                }
                return 0;
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        };

        /// \brief
        /// Serialize a FixedBuffer<length>.
        /// \param[in] serializer Where to write the given guid.
        /// \param[in] fixedbuffer FixedBuffer<length> to serialize.
        /// \return serializer.
        template<std::size_t length>
        Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const FixedBuffer<length> &fixedBuffer) {
            serializer <<
                fixedBuffer.endianness <<
                fixedBuffer.readOffset <<
                fixedBuffer.writeOffset;
            if (serializer.Write (fixedBuffer.data, length) != length) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "serializer.Write (fixedBuffer.data, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    length,
                    length);
            }
            return serializer;
        }

        /// \brief
        /// Extract a FixedBuffer<length>.
        /// \param[in] serializer Where to read the guid from.
        /// \param[out] fixedBuffer Where to place the extracted FixedBuffer<length>.
        /// \return serializer.
        template<std::size_t length>
        Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                FixedBuffer<length> &fixedBuffer) {
            serializer >>
                fixedBuffer.endianness >>
                fixedBuffer.readOffset >>
                fixedBuffer.writeOffset;
            if (serializer.Read (fixedBuffer.data, length) != length) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "serializxer.Read (fixedBuffer.data, "
                    THEKOGANS_UTIL_SIZE_T_FORMAT ") != " THEKOGANS_UTIL_SIZE_T_FORMAT,
                    length,
                    length);
            }
            return serializer;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FixedBuffer_h)
