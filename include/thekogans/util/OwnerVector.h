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

#if !defined (__thekogans_util_OwnerVector_h)
#define __thekogans_util_OwnerVector_h

#include <cstddef>
#include <memory>
#include <vector>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct OwnerVector OwnerVector.h thekogans/util/OwnerVector.h
        ///
        /// \brief
        /// See \see{OwnerList} for the rational behind this template.

        template<typename T>
        struct OwnerVector : public std::vector<T *> {
            /// \brief
            /// Default ctor.
            OwnerVector () {}
            /// \brief
            /// ctor. Create a vector of size count.
            /// \param[in] count Number of '0' elements to initialize the vector to.
            explicit OwnerVector (std::size_t count) :
                std::vector<T *> (count, 0) {}
            /// \brief
            /// Move ctor.
            /// \param[in] ownerVector Vector to move.
            OwnerVector (OwnerVector &&ownerVector) {
                swap (ownerVector);
            }
            /// \brief
            /// dtor. Delete all vector elements.
            ~OwnerVector () {
                deleteAndClear ();
            }

            /// \brief
            /// Move assignemnt operator.
            /// \param[in] ownerVector Vector to move.
            /// \return *this
            OwnerVector &operator = (OwnerVector &&ownerVector) {
                if (this != &ownerVector) {
                    swap (ownerVector);
                }
                return *this;
            }

            /// \brief
            /// Delete all elements, and clear the vector.
            /// After calling this method, the vector is empty.
            void deleteAndClear () {
                typedef THEKOGANS_UTIL_TYPENAME OwnerVector::iterator iterator;
                for (iterator p = this->begin (); p != this->end (); ++p) {
                    delete *p;
                }
                OwnerVector::clear ();
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_OwnerVector_h)
