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
#include <functional>
#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"

namespace thekogans {
    namespace util {

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE(_T)
        /// This macro is used in DynamicCreatable base classes. It cast's
        /// it's return types to the base they were derived from to make
        /// the code cleaner without the dynamic_refcounted_sharedptr_cast
        /// clutter.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE(_T)\
        public:\
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (_T)\
            static void GetTypes (std::list<std::string> &types);\
            static SharedPtr CreateType (const std::string &type);

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE(_T)
        /// This macro is used in DynamicCreatable base classes. It cast's
        /// it's return types to the base they were derived from to make
        /// the code cleaner without the dynamic_refcounted_sharedptr_cast
        /// clutter.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE(_T)\
            void _T::GetTypes (std::list<std::string> &types) {\
                for (thekogans::util::DynamicCreatable::MapType::const_iterator it =\
                        thekogans::util::DynamicCreatable::Map::Instance ()->begin (),\
                        end = thekogans::util::DynamicCreatable::Map::Instance ()->end ();\
                        it != end; ++it) {\
                    if (thekogans::util::dynamic_refcounted_sharedptr_cast<_T> (\
                            it->second ()) != nullptr) {\
                        types.push_back (it->first);\
                    }\
                }\
            }\
            _T::SharedPtr _T::CreateType (const std::string &type) {\
                thekogans::util::DynamicCreatable::MapType::iterator it =\
                    thekogans::util::DynamicCreatable::Map::Instance ()->find (type);\
                return it != thekogans::util::DynamicCreatable::Map::Instance ()->end () ?\
                    thekogans::util::dynamic_refcounted_sharedptr_cast<_T> (it->second ()) :\
                    _T::SharedPtr ();\
            }

        /// \struct DynamicCreatable DynamicCreatable.h thekogans/util/DynamicCreatable.h
        ///
        /// \brief
        /// Base class used to represent a dynamically creatable object.

        struct _LIB_THEKOGANS_UTIL_DECL DynamicCreatable : public virtual RefCounted {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (DynamicCreatable)

            /// \brief
            /// typedef for the DynamicCreatable factory function.
            typedef std::function<SharedPtr ()> Factory;
            /// \brief
            /// typedef for the DynamicCreatable map.
            typedef std::map<std::string, Factory> MapType;
            /// \brief
            /// Controls Map's lifetime.
            typedef Singleton<MapType> Map;

        #if defined (THEKOGANS_UTIL_TYPE_Static)
            /// \brief
            /// Register all known bases. This method is meant to be added
            /// to as new DynamicCreatable bases are added to the system.
            /// NOTE: If you create DynamicCreatable bases (see \see{Hash},
            /// \see{Allocator}...) you should add your own static initializer
            /// to register their derived classes.
            static void StaticInit ();
        #else // defined (THEKOGANS_UTIL_TYPE_Static)
            /// \struct DynamicCreatable::MapInitializer DynamicCreatable.h thekogans/util/DynamicCreatable.h
            ///
            /// \brief
            /// MapInitializer is used to initialize the DynamicCreatable::Map.
            /// It should not be used directly, and instead is included
            /// in THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE/THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE.
            /// If you are deriving from DynamicCreatable, add
            /// THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (for base classes) or
            /// THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (for derived classes) to it's
            /// declaration, and THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE to it's definition.
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
            /// Return DynamicCreatable type (it's class name).
            /// \return DynamicCreatable type (it's class name).
            virtual const char *Type () const = 0;
        };

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_STATIC(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_STATIC(_T)\
        public:\
            static void StaticInit ();

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_SHARED(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_SHARED(_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_STATIC(_T)
        /// Common code for static initialization of a DynamicCreatable.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_STATIC(_T)\
            void _T::StaticInit () {\
                thekogans::util::DynamicCreatable::Map::Instance ()->insert (\
                    thekogans::util::DynamicCreatable::Map::value_type (#_T, _T::Create));\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SHARED(_T)
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SHARED(_T)
    #else // defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_STATIC(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_STATIC(_T)

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_SHARED(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_SHARED(_T)\
        public:\
            static const thekogans::util::DynamicCreatable::MapInitializer mapInitializer;

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_STATIC(_T)
        /// Common code for static initialization of a DynamicCreatable.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_STATIC(_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SHARED(_T)
        /// Common code for shared initialization of a DynamicCreatable.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SHARED(_T)\
            const thekogans::util::DynamicCreatable::MapInitializer _T::mapInitializer (\
                #_T, _T::Create);
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE(_T)\
        public:\
            static const char *TYPE;\
            virtual const char *Type () const;

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(_T)\
        public:\
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_STATIC (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_SHARED (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            static thekogans::util::DynamicCreatable::SharedPtr Create ();

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T)
        /// Common defines for DynamicCreatable.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T)\
            const char *_T::TYPE = #_T;\
            const char *_T::Type () const {\
                return TYPE;\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE(_T)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (SHA1)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE(_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_STATIC (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SHARED (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T)\
            thekogans::util::DynamicCreatable::SharedPtr _T::Create () {\
                return thekogans::util::DynamicCreatable::SharedPtr (new _T);\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON(_T)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON (DefaultAllocator)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON(_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_STATIC (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SHARED (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T)\
            thekogans::util::DynamicCreatable::SharedPtr _T::Create () {\
                return thekogans::util::DynamicCreatable::SharedPtr (_T::Instance ().Get ());\
            }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DynamicCreatable_h)
