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
            /// Convenient typedef to reduce code clutter.
            typedef THEKOGANS_UTIL_TYPENAME std::vector<T *>::iterator iterator;
            /// \brief
            /// Convenient typedef to reduce code clutter.
            typedef THEKOGANS_UTIL_TYPENAME std::vector<T *>::const_iterator const_iterator;

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
            /// \param[in] other Vector to move.
            OwnerVector (OwnerVector<T> &&other) {
                swap (other);
            }
            /// \brief
            /// dtor. Delete all vector elements.
            ~OwnerVector () {
                deleteAndClear ();
            }

            /// \brief
            /// Move assignemnt operator.
            /// \param[in] other Vector to move.
            /// \return *this
            OwnerVector<T> &operator = (OwnerVector<T> &&other) {
                if (this != &other) {
                    OwnerVector<T> temp (std::move (other));
                    swap (temp);
                }
                return *this;
            }

            /// \brief
            /// Delete the element pointed to by the iterator,
            /// and erase the iterator from the vector.
            /// \param[in] p Itrator to element to delete and erase.
            /// \return Next element in the vector.
            iterator deleteAndErase (const_iterator p) {
                delete *p;
                return erase (p);
            }

            /// \brief
            /// Delete a range of elements,
            /// and erase them from the list.
            /// \param[in] p1 Itrator to beginning of range.
            /// \param[in] p2 Itrator to end of range.
            /// \return Next element in the list.
            iterator deleteAndErase (
                    const_iterator p1,
                    const_iterator p2) {
                for (const_iterator it = p1; it != p2; ++it) {
                    delete *it;
                }
                return this->erase (p1, p2);
            }

            /// \brief
            /// Delete all elements, and clear the vector.
            /// After calling this method, the vector is empty.
            void deleteAndClear () {
                deleteAndErase (this->begin (), this->end ());
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_OwnerVector_h)
