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

#include <string>
#include <functional>
#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Singleton.h"

namespace thekogans {
    namespace util {

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE(_T)\
        public:\
            static const char *TYPE;\
            virtual const char *Type () const;

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T)
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T)\
            const char *_T::TYPE = #_T;\
            const char *_T::Type () const {\
                return TYPE;\
            }

        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_FUNCTIONS(_T)\
        public:\
            static bool IsType (const std::string &type);\
            static const thekogans::util::DynamicCreatable::TypeMapType &GetTypes ();\
            static _T::SharedPtr CreateType (\
                const std::string &type,\
                thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters = nullptr);

        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_FUNCTIONS(_T)\
            bool _T::IsType (const std::string &type) {\
                static const thekogans::util::DynamicCreatable::TypeMapType &types =\
                    (*thekogans::util::DynamicCreatable::BaseMap::Instance ())[_T::TYPE];\
                return types.find (type) != types.end ();\
            }\
            const thekogans::util::DynamicCreatable::TypeMapType &_T::GetTypes () {\
                static const thekogans::util::DynamicCreatable::TypeMapType &types =\
                    (*thekogans::util::DynamicCreatable::BaseMap::Instance ())[_T::TYPE];\
                return types;\
            }\
            _T::SharedPtr _T::CreateType (\
                    const std::string &type,\
                    thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters) {\
                static const thekogans::util::DynamicCreatable::TypeMapType &types =\
                    (*thekogans::util::DynamicCreatable::BaseMap::Instance ())[_T::TYPE];\
                thekogans::util::DynamicCreatable::TypeMapType::const_iterator it =\
                    types.find (type);\
                if (it != types.end ()) {\
                    return it->second (parameters);\
                }\
                return nullptr;\
            }

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE(_T)
        /// This macro is used in DynamicCreatable base classes. It casts
        /// it's return types to the base they were derived from to make
        /// the code cleaner without the dynamic_refcounted_sharedptr_cast
        /// clutter.
        /// A DynamicCreatable base class is not itself creatable but is a base
        /// for more concrete derived types. An example is \see{Serializable}.
        /// It extends the capabilities of DynamicCreatable in a generic way
        /// yet serves as a virtual base for more concrete DynamicCreatables.
        /// DynamicCreatable bases should be used to statically create known
        /// derived types based on them. This way you can use parametarized
        /// type appropriate ctors. They should at the very least contain
        /// the following;
        /// - This macro.
        /// - Properly scoped void StaticInit (); it will manually register
        /// all known derived classes.
        /// - Some sort of static _T::SharedPtr Create'TYPE' (that potentially
        /// takes parameters).
        /// - Some sort of _DEFINE_ and _IMPLEMENT_ macros.
        /// This architecture is not meant to mimic RTTI. Only one layer of inheritance
        /// is maintained. As bases don't have factory methods and are only used
        /// to create types derived directly from them, you can have as bigger
        /// base higherarchy as you like and it will not reflect in the base map here.
        /// Bases can only create immediate descendants (keep that in mind when
        /// designing higherarchies). DynamicCreatable as the base of all bases
        /// will always contain the registration of all concrete types. You can
        /// always create an instance of a type (provided the type registration exists)
        /// by finding it's factory in the DynamicCreatable::TYPE base.
        /// NOTE: Your DynamicCreatable derived types can eventually wind up
        /// in a bigger system with DynamicCreatable types from other organizations.
        /// To facilitate interoperability and to avoid name space collisions
        /// it's best practice to declare your types as fully qualified including
        /// the namespace they came from (Ex: \see{Serializable}, it's type name
        /// is specified as "thekogans::util::Serializable"). You only need to do
        /// this in two places (THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE and
        /// THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE).
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE(_T)\
        public:\
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE(_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_FUNCTIONS (_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE(_T)
        /// This macro is used in DynamicCreatable base classes. It casts
        /// it's return types to the base they were derived from to make
        /// the code cleaner without the dynamic_refcounted_sharedptr_cast
        /// clutter.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE(_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_FUNCTIONS (_T)

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
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (DynamicCreatable)

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
                /// The Create method below will call this method if
                /// a Parameters derived class is passed to CreateType.
                /// Here's where you apply encapsulated parameters to
                /// the passed in instance.
                /// \param[in] dynamicCreatable DynamicCreatable to apply the
                /// encapsulated parameters to.
                virtual void Apply (DynamicCreatable::SharedPtr /*dynamicCreatable*/) = 0;
            };

            /// \brief
            /// Type factory method.
            using FactoryType = std::function<SharedPtr (Parameters::SharedPtr)>;
            /// \brief
            /// Maps type name to it's factory method.
            using TypeMapType = std::map<std::string, FactoryType>;
            /// \brief
            /// Maps base type to it's derived types.
            using BaseMapType = std::map<std::string, TypeMapType>;
            /// \brief
            /// The one and only base to derived type map.
            using BaseMap = Singleton<BaseMapType>;

            /// \brief
            /// Declare DynamicCreatable TYPE.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE (DynamicCreatable)
            /// \brief
            /// Declare DynamicCreatable base functions.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_FUNCTIONS (DynamicCreatable)

        #if defined (THEKOGANS_UTIL_TYPE_Static)
            /// \brief
            /// Register all known bases. This method is meant to be added
            /// to as new DynamicCreatable bases are added to the system.
            /// NOTE: If you create DynamicCreatable bases (see \see{Hash},
            /// \see{Allocator}...) you should add your own static initializer
            /// to register their derived classes.
            static void StaticInit ();
        #else // defined (THEKOGANS_UTIL_TYPE_Static)
            /// \struct DynamicCreatable::BaseMapInitializer DynamicCreatable.h thekogans/util/DynamicCreatable.h
            ///
            /// \brief
            /// BaseMapInitializer is used to initialize the DynamicCreatable::Map.
            /// It should not be used directly, and instead is included
            /// in THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE/THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE.
            /// If you are deriving from DynamicCreatable, add
            /// THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (for base classes) or
            /// THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (for derived classes) to it's
            /// declaration, and THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE to it's definition.
            struct _LIB_THEKOGANS_UTIL_DECL BaseMapInitializer {
                /// \brief
                /// ctor.
                /// \param[in] base DynamicCreatable base type (it's parents class name).
                /// \param[in] type DynamicCreatable type (it's class name).
                /// \param[in] factory DynamicCreatable creation factory.
                BaseMapInitializer (
                    const std::string bases[],
                    std::size_t basesSize,
                    const std::string &type,
                    FactoryType factory);
            };
        #endif // defined (THEKOGANS_UTIL_TYPE_Shared)

            static bool IsBaseType (
                const std::string &base,
                const std::string &type);
            static const TypeMapType &GetBaseTypes (
                const std::string &base);
            static SharedPtr CreateBaseType (
                const std::string &base,
                const std::string &type,
                Parameters::SharedPtr parameters);

            /// \brief
            /// Pretty print the BaseMap to the std::cout.
            /// Use this to take a look inside your map when debugging.
            static void DumpBaseMap (const std::string &base = std::string ());
        };

        /// \def THEKOGANS_UTIL_DYNAMIC_CREATABLE_BASES_BEGIN(_T)
        #define THEKOGANS_UTIL_DYNAMIC_CREATABLE_BASES_BEGIN(_T)\
            const char *_T::BASES[] = {

        /// \def THEKOGANS_UTIL_DYNAMIC_CREATABLE_BASES_END(_T)
        #define THEKOGANS_UTIL_DYNAMIC_CREATABLE_BASES_END(_T)\
            };\
            const std::size_t _T::BASES_SIZE = THEKOGANS_UTIL_ARRAY_SIZE (_T::BASES);

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASES(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASES(_T)\
        public:\
            static const char *BASES[];\
            static const std::size_t BASES_SIZE;

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT_STATIC(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT_STATIC(_T)\
        public:\
            static void StaticInit ();

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT_STATIC(_T)
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT_STATIC(_T)\
            void _T::StaticInit () {\
                for (std::size_t i = 0; i < _T::BASES_SIZE; ++i) {\
                    (*BaseMap::Instance ())[_T::BASES[i]][_T::TYPE] = _T::Create;\
                }\
            }

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT_SHARED(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT_SHARED(_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT_SHARED(_T)
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT_SHARED(_T)
    #else // defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT_STATIC(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT_STATIC(_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT_STATIC(_T)
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT_STATIC(_T)

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT_SHARED(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT_SHARED(_T)\
        public:\
            static const thekogans::util::DynamicCreatable::BaseMapInitializer mapInitializer;

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT_SHARED(_T)
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT_SHARED(_T)\
            const thekogans::util::DynamicCreatable::BaseMapInitializer _T::mapInitializer (\
                _T::BASES, _T::BASES_SIZE, _T::TYPE, _T::Create);
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_INIT(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT(_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT_STATIC (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT_SHARED (_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_INIT(_T)
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT(_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT_STATIC (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT_SHARED (_T)

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(_T)
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(_T)\
        public:\
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASES (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT (_T)\
            static thekogans::util::DynamicCreatable::SharedPtr Create (\
                thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters);

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE(_T, ...)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (SHA1)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE(_T, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            THEKOGANS_UTIL_DYNAMIC_CREATABLE_BASES_BEGIN (_T)\
                thekogans::util::DynamicCreatable::TYPE,\
                __VA_ARGS__\
            THEKOGANS_UTIL_DYNAMIC_CREATABLE_BASES_END (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT (_T)\
            thekogans::util::DynamicCreatable::SharedPtr _T::Create (\
                    thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters) {\
                thekogans::util::DynamicCreatable::SharedPtr dynamicCreatable (new _T);\
                if (parameters != nullptr) {\
                    parameters->Apply (dynamicCreatable);\
                }\
                return dynamicCreatable;\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON(_T, ...)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON (DefaultAllocator)
        /// \endcode
        /// NOTE: \see{Singleton} does not participate in the dynamic parameterization
        /// (\see{DynamicCreatable::Parameters}) as it has it's own mechanism for static
        /// ctor parameterization (\see{Singleton::CreateInstance}) more appropriate
        /// for template programming.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON(_T, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            THEKOGANS_UTIL_DYNAMIC_CREATABLE_BASES_BEGIN (_T)\
                thekogans::util::DynamicCreatable::TYPE,\
                __VA_ARGS__\
            THEKOGANS_UTIL_DYNAMIC_CREATABLE_BASES_END (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT (_T)\
            thekogans::util::DynamicCreatable::SharedPtr _T::Create (\
                    thekogans::util::DynamicCreatable::Parameters::SharedPtr /*parameters*/) {\
                return _T::Instance ();\
            }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DynamicCreatable_h)
