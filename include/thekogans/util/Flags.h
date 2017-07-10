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

#if !defined (__thekogans_util_Flags_h)
#define __thekogans_util_Flags_h

#include "thekogans/util/Types.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct Flags Flags.h thekogans/util/Flags.h
        ///
        /// \brief
        /// Flags implements a convenient bit flag template. All
        /// standard bit test/set/flip api's are provided. The typedefs
        /// below instantiate the template for four of the most common
        /// types (ui8, ui16, ui32, ui64).

        template<typename T>
        struct Flags {
        private:
            /// \brief
            /// The flags.
            T flags;

        public:
            /// \brief
            /// ctor.
            /// \param[in] flags_ Initial flags value.
            Flags (T flags_ = 0) :
                flags (flags_) {}

            /// \brief
            /// Return serialized size of flags.
            /// \return Serialized size of flags.
            inline ui32 Size () const {
                return Serializer::Size (flags);
            }

            /// \brief
            /// Test if flag is set.
            /// \param[in] flag Flag to test.
            /// \return true = set, false = unset
            inline bool Test (T flag) const {
                return (flags & flag) == flag;
            }
            /// \brief
            /// Test if any of the given flags are set.
            /// \param[in] flags_ Flags to test.
            /// \return true = one or more set, false = none are set
            inline bool TestAny (T flags_) const {
                return (flags & flags_) != 0;
            }
            /// \brief
            /// Set/Unset a given flag.
            /// \param[in] flag Flag to set/unset.
            /// \param[in] on true = set, false = unset.
            /// \return old value of the flag.
            inline bool Set (
                    T flag,
                    bool on) {
                bool old = Test (flag);
                if (on) {
                    flags |= flag;
                }
                else {
                    flags &= ~flag;
                }
                return old;
            }
            /// \brief
            /// Set/Unset a set of flags.
            /// \param[in] flags_ Flags to set/unset.
            /// \param[in] on true = set, false = unset.
            /// \return Old value of the flags.
            inline T SetAll (
                    T flags_,
                    bool on) {
                T old = flags & flags_;
                if (on) {
                    flags |= flags_;
                }
                else {
                    flags &= ~flags_;
                }
                return old;
            }
            /// \brief
            /// Flip the value of a flag.
            /// \param[in] flag Flag whose value to flip.
            /// \return previous value of the flag.
            inline bool Flip (T flag) {
                return Set (flag, !Test (flag));
            }
            /// \brief
            /// Implicit typecast operator.
            inline operator T () const {
                return flags;
            }

            /// \brief
            /// Or assign operator.
            /// \param[in] flag Flag to or with.
            /// \return *this
            inline Flags &operator |= (const T &flag) {
                flags |= flag;
                return *this;
            }
            /// \brief
            /// And assign operator.
            /// \param[in] flag Flag to and with.
            /// \return *this
            inline Flags &operator &= (const T &flag) {
                flags &= flag;
                return *this;
            }
            /// \brief
            /// Xor assign operator.
            /// \param[in] flag Flag to xor with.
            /// \return *this
            inline Flags &operator ^= (const T &flag) {
                flags ^= flag;
                return *this;
            }
            /// \brief
            /// Left shift assign operator.
            /// \param[in] count Bit count to shift.
            /// \return *this
            inline Flags &operator <<= (ui32 count) {
                flags <<= count;
                return *this;
            }
            /// \brief
            /// Right shift assign operator.
            /// \param[in] count Bit count to shift.
            /// \return *this
            inline Flags &operator >>= (ui32 count) {
                flags >>= count;
                return *this;
            }
        };

        /// \brief
        /// Synonym for Flags<ui8>.
        typedef Flags<ui8> Flags8;
        /// \brief
        /// Synonym for Flags<ui16>.
        typedef Flags<ui16> Flags16;
        /// \brief
        /// Synonym for Flags<ui32>.
        typedef Flags<ui32> Flags32;
        /// \brief
        /// Synonym for Flags<ui64>.
        typedef Flags<ui64> Flags64;

        /// \brief
        /// Serialize a Flags<T>. endianness is used to properly
        /// convert between serializer and host byte order.
        /// \param[in] serializer Where to write Flags<T>.
        /// \param[in] flags Flags<T> to serialize.
        /// \return serializer.
        template<typename T>
        inline Serializer &operator << (
                Serializer &serializer,
                const Flags<T> &flags) {
            return serializer << (const T)flags;
        }

        /// \brief
        /// Extract a Flags<T>. endianness is used to properly
        /// convert between serializer and host byte order.
        /// \param[in] serializer Where to read the Flags<T> from.
        /// \param[out] value Where to place the extracted value.
        /// \return serializer.
        template<typename T>
        inline Serializer &operator >> (
                Serializer &serializer,
                Flags<T> &flags) {
            T t;
            serializer >> t;
            flags = Flags<T> (t);
            return serializer;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Flags_h)
