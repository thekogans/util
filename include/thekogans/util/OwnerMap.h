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

#if !defined (__thekogans_util_OwnerMap_h)
#define __thekogans_util_OwnerMap_h

#include <memory>
#include <map>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct OwnerMap OwnerMap.h thekogans/util/OwnerMap.h
        ///
        /// \brief
        /// See \see{OwnerList} for the rational behind this template.

        template<
            typename Key,
            typename T,
            typename Compare = std::less<Key>>
        struct OwnerMap : public std::map<Key, T *, Compare> {
        public:
            /// \brief
            /// Convenient typedef to reduce code clutter.
            typedef THEKOGANS_UTIL_TYPENAME std::map<Key, T *, Compare>::iterator iterator;
            /// \brief
            /// Convenient typedef to reduce code clutter.
            typedef THEKOGANS_UTIL_TYPENAME std::map<Key, T *, Compare>::const_iterator const_iterator;

            /// \brief
            /// Default ctor.
            OwnerMap () {}
            /// \brief
            /// Move ctor.
            /// \param[in] other Map to move.
            OwnerMap (OwnerMap<Key, T, Compare> &&other) {
                swap (other);
            }
            /// \brief
            /// dtor. Delete all map elements.
            ~OwnerMap () {
                deleteAndClear ();
            }

            /// \brief
            /// Move assignemnt operator.
            /// \param[in] other Map to move.
            /// \return *this
            OwnerMap<Key, T, Compare> &operator = (OwnerMap<Key, T, Compare> &&other) {
                if (this != &other) {
                    OwnerMap<Key, T, Compare> temp (std::move (other));
                    swap (temp);
                }
                return *this;
            }

            /// \brief
            /// Delete the element pointed to by the iterator,
            /// and erase the iterator from the map.
            /// \param[in] p Itrator to element to delete and erase.
            /// \return Next element in the map.
            iterator deleteAndErase (const_iterator p) {
                delete p->second;
                return this->erase (p);
            }

            /// \brief
            /// Delete a range of elements,
            /// and erase them from the map.
            /// \param[in] p1 Itrator to beginning of range.
            /// \param[in] p2 Itrator to end of range.
            /// \return Next element in the map.
            iterator deleteAndErase (
                    const_iterator p1,
                    const_iterator p2) {
                for (const_iterator it = p1; it != p2; ++it) {
                    delete it->second;
                }
                return this->erase (p1, p2);
            }

            /// \brief
            /// Delete all elements, and clear the map.
            /// After calling this method, the map is empty.
            void deleteAndClear () {
                deleteAndErase (this->begin (), this->end ());
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_OwnerMap_h)
