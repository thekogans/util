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
        /// See OwnerList for the rational behind this template.
        /// \see{OwnerList}

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
            /// Deep copy ctor.
            /// \param[in] ownerVector Vector to copy.
            OwnerVector (const OwnerVector &ownerVector) {
                this->reserve (ownerVector.size ());
                typedef THEKOGANS_UTIL_TYPENAME
                    OwnerVector::const_iterator const_iterator;
                for (const_iterator p = ownerVector.begin ();
                        p < ownerVector.end (); ++p) {
                    this->push_back (new T (**p));
                }
            }
            /// \brief
            /// dtor. Delete all vector elements.
            ~OwnerVector () {
                deleteAndClear ();
            }

            /// \brief
            /// Deep copy assignemnt operator.
            /// Maintains transactional semantics.
            /// \param[in] ownerVector Vector to copy.
            /// \return *this
            OwnerVector &operator = (const OwnerVector &ownerVector) {
                if (this != &ownerVector) {
                    OwnerVector temp;
                    temp.reserve (ownerVector.size ());
                    typedef THEKOGANS_UTIL_TYPENAME
                        OwnerVector::const_iterator const_iterator;
                    for (const_iterator p = ownerVector.begin ();
                            p < ownerVector.end (); ++p) {
                        // push_back is guaranteed not to throw because of
                        // the reserve above. new and T's ctor might
                        // throw. In either case, temp will cleanup the
                        // objects it has constructed successfully in its
                        // dtor, so we are exception safe.
                        temp.push_back (new T (**p));
                    }
                    // Guaranteed not to throw.
                    this->swap (temp);
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
