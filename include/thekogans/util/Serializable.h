// Copyright 2016 Boris Kogan (boris@thekogans.net)
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

#if !defined (__thekogans_util_Serializable_h)
#define __thekogans_util_Serializable_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct Serializable Serializable.h thekogans/util/Serializable.h
        ///
        /// \brief
        /// Serializable is an abstract base for all supported serializable types (See
        /// \see{Serializer}).

        struct _LIB_THEKOGANS_UTIL_DECL Serializable : public ThreadSafeRefCounted {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<Serializable>.
            typedef ThreadSafeRefCounted::Ptr<Serializable> Ptr;

            /// \brief
            /// typedef for the Serializable factory function.
            typedef Ptr (*Factory) (Serializer &serializer);
            /// \brief
            /// typedef for the Serializable map.
            typedef std::map<std::string, Factory> Map;
            /// \brief
            /// Controls Map's lifetime.
            /// \return Serializable map.
            static Map &GetMap ();
            /// \brief
            /// Used for Serializable dynamic discovery and creation.
            /// \param[in] serializer Serializer containing the Serializable.
            /// \return A deserialized serializable.
            static Ptr Get (Serializer &serializer);
            /// \struct Serializable::MapInitializer Serializable.h thekogans/util/Serializable.h
            ///
            /// \brief
            /// MapInitializer is used to initialize the Serializable::map.
            /// It should not be used directly, and instead is included
            /// in THEKOGANS_UTIL_DECLARE_SERIALIZABLE/THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE.
            /// If you are deriving a serializable from Serializable, and you want
            /// it to be dynamically discoverable/creatable, add
            /// THEKOGANS_UTIL_DECLARE_SERIALIZABLE to it's declaration,
            /// and THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE to it's definition.
            struct _LIB_THEKOGANS_UTIL_DECL MapInitializer {
                /// \brief
                /// ctor. Add serializable of type, and factory for creating it
                /// to the Serializable::map
                /// \param[in] type Serializable type (it's class name).
                /// \param[in] factory Serializable creation factory.
                MapInitializer (
                    const std::string &type,
                    Factory factory);
            };

            /// \brief
            /// dtor.
            virtual ~Serializable () {}

            /// \brief
            /// Return the size of the serializable including it's type.
            /// NOTE: Use this API when you're dealing with a generic Serializable.
            /// \return Size of the serializable including it's type.
            static std::size_t Size (const Serializable &serializable) {
                return Serializer::Size (serializable.Type ()) + serializable.Size ();
            }

            /// \brief
            /// Return serializable type (it's class name).
            /// \return Serializable type.
            virtual std::string Type () const = 0;

            /// \brief
            /// Return the serializable size.
            /// \return Serializable size.
            virtual std::size_t Size () const = 0;

            /// \brief
            /// Write the serializable to the given serializer.
            /// \param[out] serializer Serializer to write the serializable to.
            virtual void Serialize (Serializer &serializer) const = 0;
        };

        /// \def THEKOGANS_UTIL_DECLARE_SERIALIZABLE_COMMON(type, lock)
        /// Common code used by Static and Shared versions THEKOGANS_UTIL_DECLARE_SERIALIZABLE.
        #define THEKOGANS_UTIL_DECLARE_SERIALIZABLE_COMMON(type, lock)\
            typedef thekogans::util::ThreadSafeRefCounted::Ptr<type> Ptr;\
            THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (type, lock)\
        public:\
            static thekogans::util::Serializable::Ptr Create (thekogans::util::Serializer &serializer) {\
                return thekogans::util::Serializable::Ptr (new type (serializer));\
            }\
            static const char *TYPE;\
            virtual std::string Type () const {\
                return TYPE;\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_COMMON(type, lock, minSerializablesInPage, allocator)
        /// Common code used by Static and Shared versions THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_COMMON(type, lock, minSerializablesInPage, allocator)\
            const char *type::TYPE = #type;\
            THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX_AND_ALLOCATOR (\
                type,\
                lock,\
                minSerializablesInPage,\
                allocator)

    #if defined (TOOLCHAIN_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_SERIALIZABLE(type, lock)
        /// Dynamic discovery macro. Add this to your class declaration.
        /// Example:
        /// \code{.cpp}
        /// struct _LIB_THEKOGANS_UTIL_DECL SymmetricKey : public Serializable {
        ///     THEKOGANS_UTIL_DECLARE_SERIALIZABLE (SymmetricKey, thekogans::util::SpinLock)
        ///     ...
        /// };
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_SERIALIZABLE(type, lock)\
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE_COMMON (type, lock)\
            static void StaticInit () {\
                std::pair<Map::iterator, bool> result =\
                    GetMap ().insert (Map::value_type (#type, type::Create));\
                if (!result.second) {\
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                        "'%s' is already registered.", #type);\
                }\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE(type, lock, minSerializablesInPage, allocator)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// #if !defined (THEKOGANS_UTIL_MIN_SYMMETRIC_KEYS_IN_PAGE)
        ///     #define THEKOGANS_UTIL_MIN_SYMMETRIC_KEYS_IN_PAGE 16
        /// #endif // !defined (THEKOGANS_UTIL_MIN_SYMMETRIC_KEYS_IN_PAGE)
        ///
        /// THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
        ///     SymmetricKey,
        ///     thekogans::util::SpinLoc,
        ///     THEKOGANS_UTIL_MIN_SYMMETRIC_KEYS_IN_PAGE,
        ///     thekogans::util::SecureAllocator::Global)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE(type, lock, minSerializablesInPage, allocator)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_COMMON (type, lock, minSerializablesInPage, allocator)
    #else // defined (TOOLCHAIN_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_SERIALIZABLE(type, lock)
        /// Dynamic discovery macro. Add this to your class declaration.
        /// Example:
        /// \code{.cpp}
        /// struct _LIB_THEKOGANS_UTIL_DECL SymmetricKey : public Serializable {
        ///     THEKOGANS_UTIL_DECLARE_SERIALIZABLE (SymmetricKey, thekogans::util::SpinLock)
        ///     ...
        /// };
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_SERIALIZABLE(type, lock)\
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE_COMMON (type, lock)\
            static thekogans::util::Serializable::MapInitializer mapInitializer;

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE(type, lock, minSerializablesInPage, allocator)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// #if !defined (THEKOGANS_UTIL_MIN_SYMMETRIC_KEYS_IN_PAGE)
        ///     #define THEKOGANS_UTIL_MIN_SYMMETRIC_KEYS_IN_PAGE 16
        /// #endif // !defined (THEKOGANS_UTIL_MIN_SYMMETRIC_KEYS_IN_PAGE)
        ///
        /// THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
        ///     SymmetricKey,
        ///     thekogans::util::SpinLock,
        ///     THEKOGANS_UTIL_MIN_SYMMETRIC_SERIALIZABLES_IN_PAGE,
        ///     thekogans::util::SecureAllocator::Global)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE(type, lock, minSerializablesInPage, allocator)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_COMMON (type, lock, minSerializablesInPage, allocator)\
            thekogans::util::Serializable::MapInitializer type::mapInitializer (#type, type::Create);
    #endif // defined (TOOLCHAIN_TYPE_Static)

        /// \brief
        /// Serializable serializer.
        /// \param[in] serializer Where to serialize the frame header.
        /// \param[in] serializable Serializable to serialize.
        /// \return serializer.
        inline Serializer &operator << (
                Serializer &serializer,
                const Serializable::Ptr &serializable) {
            serializer << serializable->Type ();
            serializable->Serialize (serializer);
            return serializer;
        }

        /// \brief
        /// Serializable deserializer.
        /// \param[in] serializer Where to deserialize the frame header.
        /// \param[in] serializable Serializable to deserialize.
        /// \return serializer.
        inline Serializer &operator >> (
                Serializer &serializer,
                Serializable::Ptr &serializable) {
            serializable = Serializable::Get (serializer);
            return serializer;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Serializable_h)
