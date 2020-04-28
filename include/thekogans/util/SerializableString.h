// Copyright 2016 Boris Kogan (boris@thekogans.net)
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

#if !defined (__thekogans_util_SerializableString_h)
#define __thekogans_util_SerializableString_h

#include "thekogans/util/Config.h"
#include "thekogans/util/ValueParser.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/SizeT.h"

namespace thekogans {
    namespace util {

        /// \struct SerializableString SerializableString.h thekogans/util/SerializableString.h
        ///
        /// \brief
        /// SerializableString bridges the gap between std::string serialization and deserialization.
        /// Regular \see{Serializer}::operator << (const std::string &) uses \see{SizeT} to serialize
        /// string length. There are times when you need to control that. SerializableString allows
        /// you to specify the type that will be used to serialize string length.
        /// NOTE: SerializableString is not meant to be a replacement for std::string and as such
        /// only provides ctors for string serialization and deserialization.

        struct _LIB_THEKOGANS_UTIL_DECL SerializableString : public std::string {
        private:
            /// \brief
            /// String length type.
            ValueParser<SizeT>::Type lengthType;

        public:
            /// \brief
            /// ctor. Used for deserialization.
            /// \param[in] lengthType_ String length type.
            explicit SerializableString (ValueParser<SizeT>::Type lengthType_) :
                lengthType (lengthType_) {}
            /// \brief
            /// ctor. Used for serialization.
            /// \param[in] value_ String to serialize.
            /// \param[in] lengthType_ String length type.
            SerializableString (
                const std::string &value,
                ValueParser<SizeT>::Type lengthType_) :
                std::string (value),
                lengthType (lengthType_) {}

            /// \brief
            /// Return the serialized size of this string.
            /// \return Serialized size of this string.
            std::size_t Size () const;

            /// \brief
            /// Needs access to lengthType.
            friend Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const SerializableString &serializableString);
            /// \brief
            /// Needs access to lengthType.
            friend Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                SerializableString &serializableString);
            /// \brief
            /// Needs access to lengthType.
            friend struct ValueParser<SerializableString>;
        };

        /// \brief
        /// Write the given string to the given serializer.
        /// \param[in] serializer Where to write the given buffer.
        /// \param[in] serializableString String to write.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator << (
            Serializer &serializer,
            const SerializableString &serializableString);

        /// \brief
        /// Read a string from the given \see{Serializer}.
        /// \param[in] serializer Where to read the buffer from.
        /// \param[out] serializableString String to read.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
            Serializer &serializer,
            SerializableString &serializableString);

        /// \struct ValueParser<SerializableString> SerializableString.h thekogans/util/SerializableString.h
        ///
        /// \brief
        /// Specialization of ValueParser for SerializableString.

        template<>
        struct _LIB_THEKOGANS_UTIL_DECL ValueParser<SerializableString> {
        private:
            /// \brief
            /// String to parse.
            SerializableString &value;
            /// \brief
            /// String length.
            SizeT length;
            /// \brief
            /// String length parser.
            ValueParser<SizeT> lengthParser;
            /// \brief
            /// Offset in to value where to write the next chunk.
            std::size_t offset;
            /// \enum
            /// SerializableString parser is a state machine. These are it's various states.
            enum {
                /// \brief
                /// Next value is length.
                STATE_LENGTH,
                /// \brief
                /// Next value is string.
                STATE_STRING
            } state;

        public:
            /// \brief
            /// ctor.
            /// \param[out] value_ Value to parse.
            ValueParser (
                SerializableString &value_) :
                value (value_),
                length (0),
                lengthParser (length, value.lengthType),
                offset (0),
                state (STATE_LENGTH) {}

            /// \brief
            /// Reset the members to get them ready for the next value.
            void Reset ();

            /// \brief
            /// Try to parse a SerializableString from the given serializer.
            /// \param[in] serializer Contains a complete or partial SerializableString.
            /// \return true == SerializableString was successfully parsed,
            /// false == call back with more data.
            bool ParseValue (Serializer &serializer);
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SerializableString_h)
