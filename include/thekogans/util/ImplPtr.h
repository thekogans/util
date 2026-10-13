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

#if !defined (__thekogans_util_ImplPtr_h)
#define __thekogans_util_ImplPtr_h

#include <memory>
#include <type_traits>

namespace thekogans {
    namespace util {

        template <typename T>
        struct ImplPtr {
            static_assert (!std::is_array_v<T>, "ImplPtr does not support array types!");

        private:
            std::unique_ptr<T> ptr;

        public:
            ImplPtr () noexcept = default;
            explicit ImplPtr (T *p) noexcept :
                ptr (p) {}
            ImplPtr (ImplPtr &&other) noexcept :
                ptr (std::move (other.ptr)) {}

            /// \brief
            /// Return const * for comparison.
            /// \return const *.
            inline operator const T * () const noexcept {
                return ptr.get ();
            }

            inline ImplPtr &operator = (ImplPtr &&other) noexcept {
                if (this != &other) {
                    ptr = std::move (other.ptr);
                }
                return *this;
            }

            inline T *operator -> () const noexcept {
                return ptr.get ();
            }
            inline T &operator * () const noexcept {
                return *ptr;
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_ImplPtr_h)
