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

#if !defined (__thekogans_util_TransactedFileObjects_h)
#define __thekogans_util_TransactedFileObjects_h

#include <string>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/TransactedFile.h"
#include "thekogans/util/SerializableValues.h"

namespace thekogans {
    namespace util {

        /// \struct StringObject TransactedFileObjects.h thekogans/util/TransactedFileObjects.h
        ///
        /// \brief
        /// Variable size string object.
        struct _LIB_THEKOGANS_UTIL_DECL StringObject :
                public virtual TransactedFile::Object,
                public StringValue {
            /// \brief
            /// StringObject is a \see{TransactedFile}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (StringObject)

            /// \brief
            /// ctor.
            /// \param[in] value std::string to initialize this value with.
            StringObject (
                TransactedFile::SharedPtr file = nullptr,
                TransactedFile::AddressType offset = 0,
                const std::string &value = std::string ()) :
                TransactedFile::Object (file, offset),
                StringValue (value) {}
        };

        /// \struct ArrayObject TransactedFileObjects.h thekogans/util/TransactedFileObjects.h
        ///
        /// \brief
        /// ArrayObject is a template for storing arrays of types.
        template<typename T>
        struct ArrayObject :
                public virtual TransactedFile::Object,
                public ArrayValue<T> {
            /// \brief
            /// StringArrayObject is a \see{TransactedFile}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (ArrayValue<T>)

            /// \brief
            /// ctor.
            /// \param[in] value_ Value to initialize with.
            ArrayObject (
                TransactedFile::SharedPtr file = nullptr,
                TransactedFile::AddressType offset = 0,
                const std::vector<T> &value = std::vector<T> {}) :
                TransactedFile::Object (file, offset),
                ArrayValue<T> (value) {}
        };

        using StringArrayObject = ArrayObject<std::string>;
        using AddressArrayObject = ArrayObject<TransactedFile::AddressType>;
        using GUIDArrayObject = ArrayObject<GUID>;

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileObjects_h)
