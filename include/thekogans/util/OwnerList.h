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
        /// OwnerList provides a lifetime management template for heap
        /// allocated objects. Because a deep copy ctor, and an
        /// assignment operator are provided, you can only use this
        /// template with types that provide a copy ctor of their
        /// own. OwnerList cannot be used with abstract base
        /// classes. See AbstractOwnerList for an explanation.

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
            /// Deep copy ctor.
            /// \param[in] ownerList List to copy.
            OwnerList (const OwnerList &ownerList) {
                for (const_iterator p = ownerList.begin (),
                        end = ownerList.end (); p != end; ++p) {
                    std::unique_ptr<T> t (new T (**p));
                    push_back (t.get ());
                    t.release ();
                }
            }
            /// \brief
            /// dtor. Delete all list elements.
            ~OwnerList () {
                deleteAndClear ();
            }

            /// \brief
            /// Deep copy assignemnt operator.
            /// Maintains transactional semantics.
            /// \param[in] ownerList List to copy.
            /// \return *this
            OwnerList &operator = (const OwnerList &ownerList) {
                if (this != &ownerList) {
                    OwnerList temp;
                    for (const_iterator p = ownerList.begin (),
                            end = ownerList.end (); p != end; ++p) {
                        std::unique_ptr<T> t (new T (**p));
                        temp.push_back (t.get ());
                        t.release ();
                    }
                    // Guaranteed not to throw.
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
                while (p1 != p2) {
                    delete *p1;
                    ++p1;
                }
                return erase (p1, p2);
            }

            /// \brief
            /// Delete all elements, and clear the list.
            /// After calling this method, the list is empty.
            void deleteAndClear () {
                for (iterator p = this->begin (), end = this->end (); p != end; ++p) {
                    delete *p;
                }
                OwnerList::clear ();
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_OwnerList_h)
