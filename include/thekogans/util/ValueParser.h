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

#if !defined (__thekogans_util_ValueParser_h)
#define __thekogans_util_ValueParser_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/Buffer.h"
#include "thekogans/util/SizeT.h"

namespace thekogans {
    namespace util {

        /// \struct ValueParser ValueParser.h thekogans/util/ValueParser.h
        ///
        /// \brief
        /// ValueParser is a template used to incrementally assemble values from stream
        /// like \see{thekogans::Serializer}s.

        template<typename T>
        struct ValueParser {
        private:
            /// \brief
            /// Value to parse.
            T &value;
            /// \brief
            /// Offset in to valueBuffer.
            std::size_t offset;
            /// \brief
            /// Partial value.
            ui8 valueBuffer[sizeof (T)];

        public:
            /// \brief
            /// ctor.
            /// \param[out] value_ Value to parse.
            explicit ValueParser (T &value_) :
                value (value_),
                offset (0) {}

            /// \brief
            /// Rewind the offset to get it ready for the next value.
            inline void Reset () {
                offset = 0;
            }

            /// \brief
            /// Try to parse a value from the given serializer.
            /// \param[in] serializer Contains a complete or partial value.
            /// \return true == Value was successfully parsed,
            /// false == call back with more data.
            bool ParseValue (Serializer &serializer) {
                offset += serializer.Read (
                    valueBuffer + offset,
                    sizeof (T) - offset);
                if (offset == sizeof (T)) {
                    TenantReadBuffer buffer (
                        serializer.endianness,
                        valueBuffer,
                        sizeof (T));
                    buffer >> value;
                    Reset ();
                    return true;
                }
                return false;
            }
        };

        /// \struct ValueParser<ui8 *> ValueParser.h thekogans/util/ValueParser.h
        ///
        /// \brief
        /// Specialization of ValueParser for ui8 *.

        template<>
        struct _LIB_THEKOGANS_UTIL_DECL ValueParser<ui8 *> {
        private:
            /// \brief
            /// String to parse.
            ui8 *value;
            /// \brief
            /// String length.
            std::size_t length;
            /// \brief
            /// Offset in to value where to write the next chunk.
            std::size_t offset;

        public:
            /// \brief
            /// ctor.
            /// \param[out] value_ Value to parse.
            ValueParser (
                    ui8 *value_,
                    std::size_t length_) {
                Reset (value_, length_);
            }

            /// \brief
            /// Rewind the offset to get it ready for the next value.
            void Reset ();
            /// \brief
            ///
            void Reset (
                ui8 *value_,
                std::size_t length_);

            /// \brief
            /// Try to parse a ui8 * from the given serializer.
            /// \param[in] serializer Contains a complete or partial std::string.
            /// \return true == std::string was successfully parsed,
            /// false == call back with more data.
            bool ParseValue (Serializer &serializer);
        };

        /// \struct ValueParser<SizeT> ValueParser.h thekogans/util/SizeT.h
        ///
        /// \brief
        /// Specialization of ValueParser for \see{SizeT}.

        template<>
        struct _LIB_THEKOGANS_UTIL_DECL ValueParser<SizeT> {
        private:
            /// \brief
            /// Value to parse.
            SizeT &value;
            /// \brief
            /// Size of serialized \see{SizeT}
            std::size_t size;
            /// \brief
            /// Offset in to valueBuffer.
            std::size_t offset;
            /// \brief
            /// Partial value.
            ui8 valueBuffer[SizeT::MAX_SIZE];
            /// \enum
            /// SizeT parser is a state machine. These are it's various states.
            enum {
                /// \brief
                /// Next value is the first byte.
                STATE_SIZE,
                /// \brief
                /// Next value is value.
                STATE_VALUE
            } state;

        public:
            /// \brief
            /// ctor.
            /// \param[out] value_ Value to parse.
            explicit ValueParser (SizeT &value_) :
                value (value_),
                size (0),
                offset (0),
                state (STATE_SIZE) {}

            /// \brief
            /// Rewind size and offset to get them ready for the next value.
            void Reset ();

            /// \brief
            /// Try to parse a \see{SizeT} from the given serializer.
            /// \param[in] serializer Contains a complete or partial value.
            /// \return \see{SizeT} was successfully parsed,
            /// false == call back with more data.
            bool ParseValue (Serializer &serializer);
        };

        /// \struct ValueParser<std::string> ValueParser.h thekogans/util/ValueParser.h
        ///
        /// \brief
        /// Specialization of ValueParser for std::string.

        template<>
        struct _LIB_THEKOGANS_UTIL_DECL ValueParser<std::string> {
        private:
            /// \brief
            /// String to parse.
            std::string &value;
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
            /// std::string parser is a state machine. These are it's various states.
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
            explicit ValueParser (std::string &value_) :
                value (value_),
                length (0),
                lengthParser (length),
                offset (0),
                state (STATE_LENGTH) {}

            /// \brief
            /// Rewind the lengthParser to get it ready for the next value.
            void Reset ();

            /// \brief
            /// Try to parse a std::string from the given serializer.
            /// \param[in] serializer Contains a complete or partial std::string.
            /// \return true == std::string was successfully parsed,
            /// false == call back with more data.
            bool ParseValue (Serializer &serializer);
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_ValueParser_h)
