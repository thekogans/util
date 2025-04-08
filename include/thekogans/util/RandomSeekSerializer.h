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

#if !defined (__thekogans_util_RandomSeekSerializer_h)
#define __thekogans_util_RandomSeekSerializer_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct RandomSeekSerializer RandomSeekSerializer.h thekogans/util/RandomSeekSerializer.h
        ///
        /// \brief
        /// RandomSeekSerializer extends the functionality of \see{Serializer} to add
        /// random read/write pointer positioning (Tell and Seek) capabilities.

        struct _LIB_THEKOGANS_UTIL_DECL RandomSeekSerializer : public Serializer {
            /// \brief
            /// RandomSeekSerializer is a \see{util::DynamicCreatable} abstract base.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (RandomSeekSerializer)

        #if defined (THEKOGANS_UTIL_TYPE_Static)
            /// \brief
            /// Register all known bases. This method is meant to be added
            /// to as new RandomSeekSerializer bases are added to the system.
            /// NOTE: If you create RandomSeekSerializer derived bases you
            /// should add your own static initializer to register their
            /// derived classes.
            static void StaticInit ();
        #endif // defined (THEKOGANS_UTIL_TYPE_Static)

            /// \brief
            /// ctor.
            /// \param[in] endianness Serializer endianness.
            RandomSeekSerializer (Endianness endianness = HostEndian) :
                Serializer (endianness) {}

            /// \brief
            /// Return the serializer pointer position.
            /// \return The serializer pointer position.
            virtual i64 Tell () const = 0;
            /// \brief
            /// Reposition the serializer pointer.
            /// \param[in] offset Offset to move relative to fromWhere.
            /// \param[in] fromWhere SEEK_SET, SEEK_CUR or SEEK_END.
            /// \return The new serializer pointer position.
            virtual i64 Seek (
                i64 offset,
                i32 fromWhere) = 0;
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RandomSeekSerializer_h)
