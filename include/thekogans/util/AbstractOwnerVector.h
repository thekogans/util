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

#if !defined (__thekogans_util_AbstractOwnerVector_h)
#define __thekogans_util_AbstractOwnerVector_h

#include <cstddef>
#include <memory>
#include <vector>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct AbstractOwnerVector AbstractOwnerVector.h thekogans/util/AbstractOwnerVector.h
        ///
        /// \brief
        /// See \see{AbstractOwnerList} for the rationale behind this class.

        template<typename T>
        struct AbstractOwnerVector : public std::vector<T *> {
            /// \brief
            /// Default ctor.
            AbstractOwnerVector () {}
            /// \brief
            /// ctor to create a vector with count elements.
            /// \param[in] count Number of elements to create.
            explicit AbstractOwnerVector (std::size_t count) :
                std::vector<T *> (count, 0) {}
            /// \brief
            /// Delete all remaining elements, and clear the vector.
            ~AbstractOwnerVector () {
                deleteAndClear ();
            }

            /// \brief
            /// Delete all items, and clear the container.
            /// After calling this member, the vector is empty.
            void deleteAndClear () {
                typedef THEKOGANS_UTIL_TYPENAME std::vector<T *>::const_iterator const_iterator;
                for (const_iterator p = this->begin (); p != this->end (); ++p) {
                    delete *p;
                }
                std::vector<T *>::clear ();
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_AbstractOwnerVector_h)
