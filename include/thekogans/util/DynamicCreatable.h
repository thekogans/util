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

#if !defined (__thekogans_util_DynamicCreatable_h)
#define __thekogans_util_DynamicCreatable_h

#include <cstddef>
#include <string>
#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"

namespace thekogans {
    namespace util {

        /// \struct DynamicCreatable DynamicCreatable.h thekogans/util/DynamicCreatable.h
        ///
        /// \brief
        /// Base class used to represent a dynamically creatable object.

        struct _LIB_THEKOGANS_UTIL_DECL DynamicCreatable :
                public virtual ThreadSafeRefCounted {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<DynamicCreatable>.
            typedef ThreadSafeRefCounted::Ptr<DynamicCreatable> Ptr;

            /// \brief
            /// typedef for the DynamicCreatable factory function.
            typedef Ptr (*Factory) ();
            /// \brief
            /// typedef for the DynamicCreatable map.
            typedef std::map<std::string, Factory> Map;
            /// \brief
            /// Controls Map's lifetime.
            /// \return DynamicCreatable map.
            static Map &GetMap ();
            /// \brief
            /// Used for DynamicCreatable dynamic discovery and creation.
            /// \param[in] type DynamicCreatable type (it's name).
            /// \return A DynamicCreatable based on the passed in type.
            static Ptr Get (const std::string &type);
        #if defined (THEKOGANS_UTIL_TYPE_Shared)
            /// \struct DynamicCreatable::MapInitializer DynamicCreatable.h thekogans/util/DynamicCreatable.h
            ///
            /// \brief
            /// MapInitializer is used to initialize the DynamicCreatable::map.
            /// It should not be used directly, and instead is included
            /// in THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE/THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE.
            /// If you are deriving from DynamicCreatable, add
            /// THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE to it's declaration,
            /// and THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE to it's definition.
            struct _LIB_THEKOGANS_UTIL_DECL MapInitializer {
                /// \brief
                /// ctor.
                /// \param[in] type DynamicCreatable type (it's class name).
                /// \param[in] factory DynamicCreatable creation factory.
                MapInitializer (
                    const std::string &type,
                    Factory factory);
            };
        #endif // defined (THEKOGANS_UTIL_TYPE_Shared)

            /// \brief
            /// Virtual dtor.
            virtual ~DynamicCreatable () {}

            /// \brief
            /// Return DynamicCreatable name.
            /// \return DynamicCreatable name.
            virtual const char *GetName () const = 0;
        };

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_COMMON(type)
        /// Common defines for DynamicCreatable.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_COMMON(type)\
        public:\
            typedef ThreadSafeRefCounted::Ptr<type> Ptr;\
            static thekogans::util::DynamicCreatable::Ptr Create () {\
                return thekogans::util::DynamicCreatable::Ptr (new type);\
            }\
            virtual const char *GetName () const {\
                return #type;\
            }

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_STATIC_INIT(type)
        /// Common dynamic discovery static initializer macro.
        #define THEKOGANS_UTIL_STATIC_INIT(type)\
        public:\
            static void StaticInit () {\
                static volatile bool registered = false;\
                static thekogans::util::SpinLock spinLock;\
                if (!registered) {\
                    thekogans::util::LockGuard<thekogans::util::SpinLock> guard (spinLock);\
                    if (!registered) {\
                        std::pair<Map::iterator, bool> result =\
                            GetMap ().insert (Map::value_type (#type, type::Create));\
                        if (!result.second) {\
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                                "'%s' is already registered.", #type);\
                        }\
                        registered = true;\
                    }\
                }\
            }

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(type)
        /// Dynamic discovery macro. Add this to your class declaration.
        /// Example:
        /// \code{.cpp}
        /// struct _LIB_THEKOGANS_UTIL_DECL SHA1 : public DynamicCreatable {
        ///     THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (SHA1)
        ///     ...
        /// };
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(type)\
        public:\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_COMMON (type)\
            THEKOGANS_UTIL_STATIC_INIT (type)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE(type)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (SHA1)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE(type)
    #else // defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(type)
        /// Dynamic discovery macro. Add this to your class declaration.
        /// Example:
        /// \code{.cpp}
        /// struct _LIB_THEKOGANS_UTIL_DECL SHA1 : public DynamicCreatable {
        ///     THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (SHA1)
        ///     ...
        /// };
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(type)\
        public:\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_COMMON (type)\
            static const thekogans::util::DynamicCreatable::MapInitializer mapInitializer;\

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE(type)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (SHA1)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE(type)\
            const thekogans::util::DynamicCreatable::MapInitializer type::mapInitializer (\
                #type, type::Create);
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DynamicCreatable_h)
