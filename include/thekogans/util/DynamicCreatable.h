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
#include <list>
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
        /// This macro is used in DynamicCreatable base classes. It casts
        /// it's return types to the base they were derived from to make
        /// the code cleaner without the dynamic_refcounted_sharedptr_cast
        /// clutter.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE(_T)\
        public:\
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (_T)\
            static bool IsType (const std::string &type);\
            static thekogans::util::DynamicCreatable::ListType GetTypes ();\
            static _T::SharedPtr CreateType (\
                const std::string &type,\
                thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters = nullptr);

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE(_T)
        /// This macro is used in DynamicCreatable base classes. It casts
        /// it's return types to the base they were derived from to make
        /// the code cleaner without the dynamic_refcounted_sharedptr_cast
        /// clutter.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE(_T)\
            bool _T::IsType (const std::string &type) {\
                thekogans::util::DynamicCreatable::MapType::iterator it =\
                    thekogans::util::DynamicCreatable::Map::Instance ()->find (type);\
                return it != thekogans::util::DynamicCreatable::Map::Instance ()->end () &&\
                    thekogans::util::dynamic_refcounted_sharedptr_cast<_T> (it->second (nullptr)) !=\
                        nullptr;\
            }\
            thekogans::util::DynamicCreatable::ListType _T::GetTypes () {\
                thekogans::util::DynamicCreatable::ListType types;\
                for (thekogans::util::DynamicCreatable::MapType::const_iterator it =\
                        thekogans::util::DynamicCreatable::Map::Instance ()->begin (),\
                        end = thekogans::util::DynamicCreatable::Map::Instance ()->end ();\
                        it != end; ++it) {\
                    if (thekogans::util::dynamic_refcounted_sharedptr_cast<_T> (\
                            it->second (nullptr)) != nullptr) {\
                        types.push_back (\
                            thekogans::util::DynamicCreatable::TypeInfo (\
                                it->first, it->second));\
                    }\
                }\
                return types;\
            }\
            _T::SharedPtr _T::CreateType (\
                    const std::string &type,\
                    thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters) {\
                thekogans::util::DynamicCreatable::MapType::iterator it =\
                    thekogans::util::DynamicCreatable::Map::Instance ()->find (type);\
                return it != thekogans::util::DynamicCreatable::Map::Instance ()->end () ?\
                    thekogans::util::dynamic_refcounted_sharedptr_cast<_T> (it->second (parameters)) :\
                    nullptr;\
            }

        /// \struct DynamicCreatable DynamicCreatable.h thekogans/util/DynamicCreatable.h
        ///
        /// \brief
        /// Base class used to represent a dynamically creatable object. Dynamically
        /// creatable objects are everywhere. Any time you need to rebuild a structured
        /// data stream from a wire or long term storage (disc), think DynamicCreatable!
        /// A huge chunk of thekogans_util is dedicated to object lifetime management.
        /// That umbrella term includes both immediate as well as long term storage. To
        /// facilitate designing and implementing robust, easy to maintain, well behaved
        /// systems a lot of supporting sub-systems are provided; \see{Serializer} and
        /// its concrete derivatives \see{File}, \see{Buffer} and \see{FixedBuffer}.
        /// \see{Serializable} adds object stream insertion/extraction capabilities for
        /// three distinct protocols; binary, XML and JSON. \see{RefCounted} provides
        /// object lifetime management needed in dynamical systems. \see{RefCountedRegistry}
        /// allows \see{RefCounted} objects to interoperate with async os callback apis
        /// without fear of leakage or corruption.

        struct _LIB_THEKOGANS_UTIL_DECL DynamicCreatable : public virtual RefCounted {
            /// \struct DynamicCreatable::Parameters DynamicCreatable.h thekogans/util/DynamicCreatable.h
            ///
            /// \brief
            /// Parameters allow you to parametarize type creation. All
            /// DynamicCreatable derived types must be default constructable,
            /// but there are times when you need to provide specific instance
            /// parameters. By deriving a class from Parameters and passing
            /// it to CreateType you can shortcircuit the default behavior.
            struct _LIB_THEKOGANS_UTIL_DECL Parameters : public virtual RefCounted {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Parameters)

                /// \brief
                /// dtor.
                virtual ~Parameters () {}

                /// \brief
                /// The Create method below will call this method if
                /// a Parameters derived class is passed to CreateType.
                /// Here's where you apply encapsulated parameters to
                /// the passed in instance.
                /// \param[in] dynamicCreatable DynamicCreatable to apply the
                /// encapsulated parameters to.
                virtual void Apply (
                    util::RefCounted::SharedPtr<DynamicCreatable> /*dynamicCreatable*/) = 0;
            };

            /// \brief
            /// typedef for the DynamicCreatable factory function.
            typedef std::function<util::RefCounted::SharedPtr<DynamicCreatable> (
                Parameters::SharedPtr)> Factory;
            /// \brief
            /// typedef for the DynamicCreatable type info.
            typedef std::pair<std::string, Factory> TypeInfo;
            /// \brief
            /// typedef for the DynamicCreatable list.
            typedef std::list<TypeInfo> ListType;
            /// \brief
            /// typedef for the DynamicCreatable map.
            typedef std::map<std::string, Factory> MapType;
            /// \brief
            /// Controls Map's lifetime.
            typedef Singleton<MapType> Map;

            /// \brief
            /// Declare DynamicCreatable boilerplate.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (DynamicCreatable)

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
            virtual const std::string &Type () const = 0;
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
                    thekogans::util::DynamicCreatable::MapType::value_type (#_T, _T::Create));\
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
        /// Common defines for DynamicCreatable.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE(_T)\
        public:\
            static const std::string TYPE;\
            virtual const std::string &Type () const override;

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(_T)\
        public:\
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_STATIC (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_SHARED (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            static thekogans::util::DynamicCreatable::SharedPtr Create (\
                thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters);

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T)
        /// DynamicCreatable overrides.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T)\
            const std::string _T::TYPE = #_T;\
            const std::string &_T::Type () const {\
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
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            thekogans::util::DynamicCreatable::SharedPtr _T::Create (\
                    thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters) {\
                thekogans::util::DynamicCreatable::SharedPtr dynamicCreatable (new _T);\
                if (parameters != nullptr) {\
                    parameters->Apply (dynamicCreatable);\
                }\
                return dynamicCreatable;\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON(_T)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON (DefaultAllocator)
        /// \endcode
        /// NOTE: \see{Singleton} does not participate in the dynamic parameterization
        /// (\see{DynamicCreatable::Parameters}) as it has it's own mechanism for static
        /// ctor parameterization (\see{Singleton::CreateInstance}) more appropriate
        /// for template programming.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON(_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_STATIC (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SHARED (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            thekogans::util::DynamicCreatable::SharedPtr _T::Create (\
                    thekogans::util::DynamicCreatable::Parameters::SharedPtr /*parameters*/) { \
                return _T::Instance ();\
            }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DynamicCreatable_h)
