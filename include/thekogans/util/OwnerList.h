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

#if !defined (__thekogans_util_OwnerList_h)
#define __thekogans_util_OwnerList_h

#include <memory>
#include <list>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct OwnerList OwnerList.h thekogans/util/OwnerList.h
        ///
        /// \brief
        /// OwnerList is a lifetime management template for a list of heap
        /// allocated objects. It has the same semantics as std::unique_ptr.

        template<typename T>
        struct OwnerList : public std::list<T *> {
            /// \brief
            /// Convenient typedef to reduce code clutter.
            typedef THEKOGANS_UTIL_TYPENAME std::list<T *>::iterator iterator;
            /// \brief
            /// Convenient typedef to reduce code clutter.
            typedef THEKOGANS_UTIL_TYPENAME std::list<T *>::const_iterator const_iterator;

            /// \brief
            /// Default ctor.
            OwnerList () {}
            /// \brief
            /// Move ctor.
            /// \param[in] other List to move.
            OwnerList (OwnerList<T> &&other) {
                swap (other);
            }
            /// \brief
            /// dtor. Delete all list elements.
            ~OwnerList () {
                deleteAndClear ();
            }

            /// \brief
            /// Move assignemnt operator.
            /// \param[in] other List to move.
            /// \return *this
            OwnerList<T> &operator = (OwnerList<T> &&other) {
                if (this != &other) {
                    OwnerList<T> temp (std::move (other));
                    swap (temp);
                }
                return *this;
            }

            /// \brief
            /// Delete the element pointed to by the iterator,
            /// and erase the iterator from the list.
            /// \param[in] p Itrator to element to delete and erase.
            /// \return Next element in the list.
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
                return erase (p1, p2);
            }

            /// \brief
            /// Delete all elements, and clear the list.
            /// After calling this method, the list is empty.
            void deleteAndClear () {
                deleteAndErase (this->begin (), this->end ());
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_OwnerList_h)
