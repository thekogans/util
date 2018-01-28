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

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        /// \struct FixedBuffer FixedBuffer.h thekogans/util/FixedBuffer.h
        ///
        /// \brief
        /// FixedBuffer is a convenient in memory fixed length serializer. It's strength
        /// comes from it's ability to be 1. Defined inline and 2. Constructed like any
        /// other first class object (unlike c arrays).

        template<ui32 length>
        struct FixedBuffer : public Serializer {
            /// \brief
            /// FixedBuffer data.
            ui8 data[length];
            /// \brief
            /// Current read position.
            ui32 readOffset;
            /// \brief
            /// Current write position.
            ui32 writeOffset;

            /// \brief
            /// ctor.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            FixedBuffer (Endianness endianness = HostEndian) :
                Serializer (endianness),
                readOffset (0),
                writeOffset (0) {}
            /// \brief
            /// ctor for creating a fixed buffer from a given range.
            /// \param[in] endianness Specifies how multi-byte values are stored.
            /// \param[in] begin Start of range.
            /// \param[in] end Just past the end of range.
            /// \param[in] readOffset_ Offset at which to read.
            /// \param[in] writeOffset_ Offset at which to write.
            FixedBuffer (
                    Endianness endianness,
                    const ui8 *begin,
                    const ui8 *end) :
                    Serializer (endianness),
                    readOffset (0),
                    writeOffset ((ui32)(end - begin)) {
                if (writeOffset > length) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
                if (writeOffset > 0) {
                    memcpy (data, begin, writeOffset);
                }
            }

            // Serializer
            /// \brief
            /// Read raw bytes from a fixed buffer.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            virtual ui32 Read (
                    void *buffer,
                    ui32 count) {
                if (count > 0) {
                    if (buffer != 0) {
                        ui32 availableForReading = GetDataAvailableForReading ();
                        if (count > availableForReading) {
                            count = availableForReading;
                        }
                        if (count != 0) {
                            memcpy (buffer, GetReadPtr (), count);
                            AdvanceReadOffset (count);
                        }
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }
                return count;
            }
            /// \brief
            /// Write raw bytes to a fixed buffer.
            /// \param[in] buffer Bytes to write.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            virtual ui32 Write (
                    const void *buffer,
                    ui32 count) {
                if (count > 0) {
                    if (buffer != 0) {
                        ui32 availableForWriting = GetDataAvailableForWriting ();
                        if (count > availableForWriting) {
                            count = availableForWriting;
                        }
                        if (count != 0) {
                            memcpy (GetWritePtr (), buffer, count);
                            AdvanceWriteOffset (count);
                        }
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }
                return count;
            }

            /// \brief
            /// Return serialized size of FixedBuffer<length>.
            /// \return Serialized size of FixedBuffer<length>.
            inline ui32 Size () const {
                return
                    Serializer::Size () +
                    Serializer::Size (readOffset) +
                    Serializer::Size (writeOffset) +
                    length;
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
            /// Return the current read data position.
            /// \return The current read data position.
            inline const ui8 *GetReadPtr () const {
                return data + readOffset;
            }
            /// \brief
            /// Return the current write data position.
            /// \return The current write data position.
            inline ui8 *GetWritePtr () const {
                return (ui8 *)(data + writeOffset);
            }
            /// \brief
            /// Advance the read offset taking care not to overflow.
            /// \param[in] advance Amount to advance the readOffset.
            /// \return Number of bytes actually advanced.
            ui32 AdvanceReadOffset (ui32 advance) {
                ui32 availableForReading = GetDataAvailableForReading ();
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
            ui32 AdvanceWriteOffset (ui32 advance) {
                ui32 availableForWriting = GetDataAvailableForWriting ();
                if (advance > availableForWriting) {
                    advance = availableForWriting;
                }
                writeOffset += advance;
                return advance;
            }
        };

        /// \brief
        /// Serialize a FixedBuffer<length>.
        /// \param[in] serializer Where to write the given guid.
        /// \param[in] fixedbuffer FixedBuffer<length> to serialize.
        /// \return serializer.
        template<ui32 length>
        Serializer &operator << (
                Serializer &serializer,
                const FixedBuffer<length> &fixedBuffer) {
            serializer <<
                fixedBuffer.endianness <<
                fixedBuffer.readOffset <<
                fixedBuffer.writeOffset;
            if (serializer.Write (fixedBuffer.data, length) != length) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "*this.Write (fixedBuffer.data, %u) != %u", length, length);
            }
            return serializer;
        }

        /// \brief
        /// Extract a FixedBuffer<length>.
        /// \param[in] serializer Where to read the guid from.
        /// \param[out] fixedBuffer Where to place the extracted FixedBuffer<length>.
        /// \return serializer.
        template<ui32 length>
        Serializer &operator >> (
                Serializer &serializer,
                FixedBuffer<length> &fixedBuffer) {
            serializer >>
                fixedBuffer.endianness >>
                fixedBuffer.readOffset >>
                fixedBuffer.writeOffset;
            if (serializer.Read (fixedBuffer.data, length) != length) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "serializer.Read (fixedBuffer.data, %u) != %u", length, length);
            }
            return serializer;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FixedBuffer_h)
