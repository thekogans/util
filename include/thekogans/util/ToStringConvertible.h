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

#if !defined (__thekogans_util_ToStringConvertible_h)
#define __thekogans_util_ToStringConvertible_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/RefCounted.h"

namespace thekogans {
    namespace util {

        struct _LIB_THEKOGANS_UTIL_DECL ToStringConvertible : public virtual RefCounted {
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (ToStringConvertible)
            virtual std::string ToString () const = 0;
        };

    } // Namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_ToStringConvertible_h)
