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

#if !defined (__thekogans_util_SerializableHeader_h)
#define __thekogans_util_SerializableHeader_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/SizeT.h"

namespace thekogans {
    namespace util {

        /// \struct SerializableHeader SerializableHeader.h thekogans/util/SerializableHeader.h
        ///
        /// \brief
        /// SerializableHeader is a variable size header containing the metadata
        /// needed to extract a \see{Serializable} instance from a \see{Serializer}
        /// without knowing it's concrete type. It's variable size because the members
        /// inserted in to or extracted out of the \see{Serializer} depend on the current
        /// context. This context comes in the form of \see{Serializer::context} which
        /// tells operators << and >> whats missing and needs to be inserted or extracted.
        /// The more members the context has filled in, the fewer the header will need
        /// to insert or extract. This design allows for creation of containers aggregating
        /// like \see{Serializable} types saving space and insertion/extraction time.
        struct _LIB_THEKOGANS_UTIL_DECL SerializableHeader {
            /// \brief
            /// Serializable type (see \see{DynamicCreatable::Type}).
            std::string type;
            /// \brief
            /// \see{Serializable} version.
            ui16 version;
            /// \brief
            /// \see{Serializable} size in bytes (not including the header).
            SizeT size;

            /// \brief
            /// ctor.
            /// \param[in] type_ \see{Serializable} type (see \see{DynamicCreatable::Type}).
            /// \param[in] version_ \see{Serializable} version.
            /// \param[in] size_ \see{Serializable} size in bytes (not including the header).
            explicit SerializableHeader (
                const std::string &type_ = std::string (),
                ui16 version_ = 0,
                std::size_t size_ = 0) :
                type (type_),
                version (version_),
                size (size_) {}

            /// \brief
            /// Return true if type is empty.
            /// \return type.empty ().
            inline bool NeedType () const {
                return type.empty ();
            }
            /// \brief
            /// Return true if version is 0.
            /// \return version == 0.
            inline bool NeedVersion () const {
                return version == 0;
            }
            /// \brief
            /// Return true if size is 0.
            /// \return size == 0.
            inline bool NeedSize () const {
                return size == 0;
            }

            /// \brief
            /// Return true if all members are empty or 0.
            /// \return NeedType () && NeedVersion () && NeedSize ().
            inline bool IsEmpty () const {
                return NeedType () && NeedVersion () && NeedSize ();
            }
            /// \brief
            /// Return true if all members are filled in.
            /// \return !NeedType () && !NeedVersion () && !NeedSize ().
            inline bool IsFull () const {
                return !NeedType () && !NeedVersion () && !NeedSize ();
            }
        };

        /// \brief
        /// \see{SerializableHeader} equality operator.
        /// \param[in] header1 First \see{SerializableHeader} to compare for equality.
        /// \param[in] header2 Second \see{SerializableHeader} to compare for equality.
        /// \return true if header1 == header2.
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
                const SerializableHeader &header1,
                const SerializableHeader &header2) {
            return
                header1.type == header2.type &&
                header1.version == header2.version &&
                header1.size == header2.size;
        }
        /// \brief
        /// \see{SerializableHeader} inequality operator.
        /// \param[in] header1 First \see{SerializableHeader} to compare for inequality.
        /// \param[in] header2 Second \see{SerializableHeader} to compare for inequality.
        /// \return true if header1 != header2.
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
                const SerializableHeader &header1,
                const SerializableHeader &header2) {
            return
                header1.type != header2.type ||
                header1.version != header2.version ||
                header1.size != header2.size;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SerializableHeader_h)
