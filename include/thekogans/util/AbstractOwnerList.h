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

#if !defined (__thekogans_util_AbstractOwnerList_h)
#define __thekogans_util_AbstractOwnerList_h

#include <memory>
#include <list>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct AbstractOwnerList AbstractOwnerList.h thekogans/util/AbstractOwnerList.h
        ///
        /// \brief
        /// Use this template to hold heap allocated instances of
        /// objects derived from abstract base classes.
        ///
        /// An example will demonstrate the difference between it and
        /// OwnerList (OwnerList.h).
        ///
        /// Assume an abstract base class foo:
        /// \code{.cpp}
        /// struct foo {
        ///     virtual ~foo () {}
        ///     virtual void MustImplement () = 0;
        /// };
        /// \endcode
        ///
        /// Now, derive a concrete class from foo:
        /// \code{.cpp}
        /// struct bar1 : public foo {
        ///     virtual void MustImplement () {
        ///         ...
        ///     }
        /// };
        /// \endcode
        ///
        /// And another:
        /// \code{.cpp}
        /// struct bar2 : public foo {
        ///     virtual void MustImplement () {
        ///         ...
        ///     }
        /// };
        /// \endcode
        ///
        /// Now, suppose you want an OwnerList of type foo so that you
        /// can stash instances of both bar1, and bar2:
        /// \code{.cpp}
        /// thekogans::util::OwnerList<foo> fooList;
        /// fooList.push_back (new bar1 ());
        /// fooList.push_back (new bar2 ());
        /// \endcode
        /// And have the OwnerList be responsible for the lifetime
        /// management of the objects.
        ///
        /// Unfortunately, this won't work with all compilers. Since
        /// the standard is mum about when templates are to be
        /// instantiated, some compilers are aggressive, and others are
        /// lazy. Aggressive compilers (Visual Studio) will try to
        /// instantiate the OwnerList template, and fail because of
        /// OwnerList's copy ctor and assignment operator. It doesn't
        /// mater that your particular usage of OwnerList won't use
        /// those members explicitly. This is exactly the problem
        /// AbstractOwnerList was designed to solve. Since there are no
        /// members which rely on anything specific (other than the
        /// dtor), you can have this:
        /// \code{.cpp}
        /// thekogans::util::AbstractOwnerList<foo> fooList;
        /// fooList.push_back (new bar1 ());
        /// fooList.push_back (new bar2 ());
        /// \endcode
        /// And all will be well with the universe.

        template<typename T>
        struct AbstractOwnerList : public std::list<T *> {
            /// \brief
            /// Default ctor.
            AbstractOwnerList () {}
            /// \brief
            /// Delete all remaining elements, and clear the list.
            ~AbstractOwnerList () {
                deleteAndClear ();
            }

            /// \brief
            /// Delete all items, and clear the container.
            /// After calling this member, the list is empty.
            void deleteAndClear () {
                typedef THEKOGANS_UTIL_TYPENAME std::list<T *>::const_iterator const_iterator;
                for (const_iterator p = this->begin (); p != this->end (); ++p) {
                    delete *p;
                }
                std::list<T *>::clear ();
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_AbstractOwnerList_h)
