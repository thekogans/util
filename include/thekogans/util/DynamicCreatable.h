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

#include <cstring>
#include <functional>
#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Singleton.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// One of the shortcomings of c++ is it's inability to dynamically
        /// create a type at runtime given a string representation of it's name.
        /// DynamicCreatable and it's supporting macros below are my attempt at
        /// pluggin that hole. For any scheme to ultimately be useful it needs
        /// to scale well with addition of more types. One can easily envision
        /// a huge system built out of many libraries, using many types. If each
        /// time you needed to create a type instance you had to search through
        /// thousands of types (even O(log(n)), the system would not scale well.
        ///
        /// After a few itterations, the design I finally chose had the following
        /// possitive attributes;
        ///
        /// 1. Very small bookkeeping overhead. I use const char * instead of
        /// std::string to store the type name. This has a few advantages; small
        /// footprint and because const char * is something a linker understands,
        /// it will calculate and fixup addresses at link time which saves a ton
        /// on static object initialization at runtime. The linker will also fold
        /// all identical strings in to one saving on the application runtime size.
        ///
        /// 2. The two layer map hierarchy design (see below) provides a huge performance
        /// boost as every time you need to create a type derived from a base, you can
        /// call base::CreateType (type) instead of DynamicCreatable::CreateType (type).
        /// That one change alows for much faster searches. While the map higherarchy
        /// is only two layes deep (base, type), its very flexible and if used properly
        /// (i.e. with type namespaces) can service any application type higherarchy.
        ///
        /// 3. After initialization (which is usually done before main is called),
        /// the DynamicCreatable::BaseMap is read only! This means that there's no
        /// need for locking to achieve thread safety. No locking means much faster
        /// performance which means greater scalability.
        ///
        /// Below are a few exmples of how to use the DynamicCreatable machinery.
        ///
        /// 1. To derive a class directly from DynamicCreatable use the following pattern;
        ///
        /// \code{.cpp}
        /// // in bar.h
        ///
        /// namespace foo {
        ///
        ///     struct bar : public DynamicCreatable {
        ///         THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (bar)
        ///         ...
        ///     };
        ///
        /// } // namespace foo
        ///
        /// // in bar.cpp Note the fully qualified name.
        /// namespace foo {
        ///
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (foo::bar)
        ///
        /// } // namespace foo
        /// \endcode
        ///
        /// 2. To create a class that will be a base for a number of derivative types;
        ///
        /// \code{.cpp}
        /// // in bar.h
        ///
        /// namespace foo {
        ///
        ///     struct bar : public DynamicCreatable {
        ///         THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (bar)
        ///         ...
        ///     };
        ///
        /// } // namespace foo
        ///
        /// // in bar.cpp. Note the fully qualified name.
        ///
        /// namespace foo {
        ///
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE (foo::bar)
        ///
        /// } // namespace foo
        /// \endcode
        ///
        /// 3. To create a derived type based on bar above you can do the following;
        ///
        /// \code{.cpp}
        /// // in baz.h
        ///
        /// namespace foo {
        ///
        ///     struct baz : public bar {
        ///     // If baz happens to be a \see{Singleton}.
        ///     struct baz :
        ///             public bar,
        ///             public Singleton<baz> {
        ///         THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (baz)
        ///         ...
        ///     };
        ///
        /// } // namespace foo
        ///
        /// // in baz.cpp. Note the fully qualified name.
        ///
        /// namespace foo {
        ///
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (foo::baz, bar::TYPE)
        ///     // If baz happens to be a \see{Singleton}.
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON (foo::baz, bar::TYPE)
        ///
        /// } // namespace foo
        /// \endcode

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE(_T)
        /// Define DynamicCreatable::TYPE. Unless you have a special need
        /// (\see{FileLogger}), you should not need to directly use this
        /// macro.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE(_T)\
        public:\
            static const char * const TYPE;\
            virtual const char *Type () const;

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T)
        /// Implement DynamicCreatable::TYPE. This macro is usually private
        /// (see ..._DECLARE_..._OVERRIDE above).
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T)\
            const char * const _T::TYPE = #_T;\
            const char *_T::Type () const {\
                return TYPE;\
            }

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_FUNCTIONS(_T)
        /// Define the base type functions. This macro is private.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_FUNCTIONS(_T)\
        public:\
            static bool IsType (const char *type);\
            static const thekogans::util::DynamicCreatable::TypeMapType &GetTypes ();\
            static _T::SharedPtr CreateType (\
                const char *type,\
                thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters = nullptr);

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_FUNCTIONS(_T)
        /// Implement base type functions. This is the magic that makes the
        /// entire scheme so performant. Because base types know their own
        /// names, they can cache their own type map reference and use it to
        /// look up types descendant from them. This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_FUNCTIONS(_T)\
            bool _T::IsType (const char *type) {\
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
                    const char *type,\
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
        /// type appropriate ctors. They should consider containing the following;
        ///
        /// - This macro.
        /// - Properly scoped void StaticInit (); it will manually register
        /// all known derived classes.
        /// - Some sort of static _T::SharedPtr Create'TYPE' (that potentially
        /// takes parameters).
        /// - Some sort of _DEFINE_ and _IMPLEMENT_ macros to hide base type registration.
        ///
        /// NOTE: Your DynamicCreatable derived types can eventually wind up
        /// in a bigger system with DynamicCreatable types from other organizations.
        /// To facilitate interoperability and to avoid name space collisions
        /// it's best practice to declare your types fully qualified including
        /// the namespace they came from (Ex: \see{Serializable}, it's type name
        /// is specified as "thekogans::util::Serializable"). Ex:
        ///
        /// \code{.cpp}
        /// namespace thekogans {
        ///     namespace util {
        ///
        ///         struct _LIB_THEKOGANS_UTIL_DECL Serializable : public DynamicCreatable {
        ///             THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (Serializable)
        ///             ...
        ///         };
        ///
        ///     } // namespace thekogans
        /// } // namespace util
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE(_T)\
        public:\
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_FUNCTIONS (_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE(_T)
        /// This macro is used in DynamicCreatable base classes. It casts
        /// it's return types to the base they were derived from to make
        /// the code cleaner without the dynamic_refcounted_sharedptr_cast
        /// clutter. Ex:
        ///
        /// \code{.cpp}
        /// namespace thekogans {
        ///     namespace util {
        ///
        ///         THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE (thekogans::util::Serializable)
        ///
        ///     } // namespace thekogans
        /// } // namespace util
        /// \endcode
        ///
        /// VERY IMPORTANT: To twart name space collisions, note the fully qualified name.
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
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (DynamicCreatable)

            /// \struct DynamicCreatable::Parameters DynamicCreatable.h
            /// thekogans/util/DynamicCreatable.h
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
            /// \struct DynamicCreatable::StringCompare DynamicCreatable.h
            /// thekogans/util/DynamicCreatable.h
            ///
            /// \brief
            /// For compactness we use const char * as map keys. This struct
            /// exposes a operator () that is used to treat the keys like strings.
            struct StringCompare {
                /// \brief
                /// Return true if a < b.
                /// \return true if a < b.
                inline bool operator () (
                        const char *a,
                        const char *b) const {
                    return std::strcmp (a, b) < 0;
                }
            };
            /// \brief
            /// Maps type name to it's factory method.
            using TypeMapType = std::map<const char *, FactoryType, StringCompare>;
            /// \brief
            /// Maps base type name to it's derived types.
            using BaseMapType = std::map<const char *, TypeMapType, StringCompare>;
            /// \brief
            /// The one and only base to derived type map. You can access it like this;
            /// thekogans::util::DynamicCreatable::BaseMap::Instance ()->.
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
            /// \struct DynamicCreatable::BaseMapInitializer DynamicCreatable.h
            /// thekogans/util/DynamicCreatable.h
            ///
            /// \brief
            /// BaseMapInitializer is used to initialize the DynamicCreatable::Map.
            /// It should not be used directly, and instead is included
            /// in THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE/
            /// THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE.
            /// If you are deriving from DynamicCreatable, add
            /// THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (for base classes) or
            /// THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (for derived classes) to it's
            /// declaration, and THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE to it's definition.
            struct _LIB_THEKOGANS_UTIL_DECL BaseMapInitializer {
                /// \brief
                /// ctor.
                /// \param[in] bases DynamicCreatable bases type names (it's parents class names).
                /// \param[in] basesSize Size of bases.
                /// \param[in] type DynamicCreatable type (it's class name).
                /// \param[in] factory DynamicCreatable creation factory.
                BaseMapInitializer (
                    const char *bases[],
                    std::size_t basesSize,
                    const char *type,
                    FactoryType factory);
            };
        #endif // defined (THEKOGANS_UTIL_TYPE_Shared)

            /// This is the generic catch all interface to the BaseMap. It incurs
            /// the overhead of a search for the base map. Much better to use
            /// base::IsType/GetTypes/CreateType as they cache their base map.

            /// \brief
            /// Check if the given type was derived from the given base.
            /// \param[in] base Base name to check.
            /// \param[in] type Type name to check.
            /// \return true == type is descendant from base.
            static bool IsBaseType (
                const char *base,
                const char *type);
            /// \brief
            /// Return the type -> factory map for a given base.
            /// \param[in] base Base whose type map to return.
            /// \return const & to the map.
            static const TypeMapType &GetBaseTypes (
                const char *base);
            /// \brief
            /// Given a base create a given type and initialize it with optional parameters.
            /// \param[in] base Base name whos derived type to create.
            /// \param[in] type Base derived type to create.
            /// \param[in] parameters Optional pointer to parameters
            /// to initalize the newly created type.
            /// \return An instance of a base derived type.
            static SharedPtr CreateBaseType (
                const char *base,
                const char *type,
                Parameters::SharedPtr parameters = nullptr);

            /// \brief
            /// Pretty print the BaseMap to the std::cout.
            /// Use this to take a look inside your map when debugging.
            /// \param[in] base Optional base whos map to prity print.
            /// If nullptr, print all bases.
            static void DumpBaseMap (const char *base = nullptr);
        };

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASES(_T)
        /// Declare the DynamicCreatable bases array. This macro is private.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASES(_T)\
        public:\
            static const char * const BASES[];\
            static const std::size_t BASES_SIZE;

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_BEGIN(_T)
        /// Used to start the creation of DynamicCreatable bases array. This
        /// macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_BEGIN(_T)\
            const char * const _T::BASES[] = {

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_END(_T)
        /// Used to end the creation of DynamicCreatable bases array. A list
        /// of bases would go between the above and this macro. This
        /// macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_END(_T)\
            };\
            const std::size_t _T::BASES_SIZE = THEKOGANS_UTIL_ARRAY_SIZE (_T::BASES);

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT(_T)
        /// Declare the static base map initializer. This macro is private.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT(_T)\
        public:\
            static void StaticInit ();

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT(_T)
        /// Define the static base map initializer. This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT(_T)\
            void _T::StaticInit () {\
                for (std::size_t i = 0; i < _T::BASES_SIZE; ++i) {\
                    (*thekogans::util::DynamicCreatable::BaseMap::Instance ())[\
                        _T::BASES[i]][_T::TYPE] = _T::Create;\
                }\
            }
    #else // defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT(_T)
        /// Declare the shared base map initializer. This macro is private.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT(_T)\
        public:\
            static const thekogans::util::DynamicCreatable::BaseMapInitializer mapInitializer;

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT(_T)
        /// Define the shared base map initializer. This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT(_T)\
            const thekogans::util::DynamicCreatable::BaseMapInitializer _T::mapInitializer (\
                _T::BASES, _T::BASES_SIZE, _T::TYPE, _T::Create);
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(_T)
        /// Dynamic discovery macro. Instantiate one of these in the class h file.
        ///
        /// Example:
        ///
        /// \code{.cpp}
        /// namespace thekogans {
        ///     namespace util {
        ///
        ///         struct _LIB_THEKOGANS_UTIL_DECL SHA1 : public Hash {
        ///             /// \brief
        ///             /// SHA1 participates in the Hash dynamic discovery and creation.
        ///             THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (SHA1)
        ///             ...
        ///         };
        ///
        ///     } // namespace thekogans
        /// } // namespace util
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(_T)\
        public:\
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASES (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT (_T)\
            static thekogans::util::DynamicCreatable::SharedPtr Create (\
                thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters = nullptr);

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE(_T, ...)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        ///
        /// Example:
        ///
        /// \code{.cpp}
        /// namespace thekogans {
        ///     namespace util {
        ///
        ///         THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (
        ///             thekogans::util::SHA1,
        ///             Hash::TYPE)
        ///
        ///     } // namespace thekogans
        /// } // namespace util
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE(_T, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_BEGIN (_T)\
                thekogans::util::DynamicCreatable::TYPE,\
                __VA_ARGS__\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_END (_T)\
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
        ///
        /// Example:
        ///
        /// \code{.cpp}
        /// namespace thekogans {
        ///     namespace util {
        ///
        ///         THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON (
        ///             thekogans::util::DefaultAllocator,
        ///             Allocator::TYPE)
        ///
        ///     } // namespace thekogans
        /// } // namespace util
        /// \endcode
        ///
        /// NOTE: \see{Singleton} does not participate in the dynamic parameterization
        /// (\see{DynamicCreatable::Parameters}) as it has it's own mechanism for static
        /// ctor parameterization (\see{Singleton::CreateInstance}) more appropriate
        /// for template programming.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_SINGLETON(_T, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_BEGIN (_T)\
                thekogans::util::DynamicCreatable::TYPE,\
                __VA_ARGS__\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_END (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT (_T)\
            thekogans::util::DynamicCreatable::SharedPtr _T::Create (\
                    thekogans::util::DynamicCreatable::Parameters::SharedPtr /*parameters*/) {\
                return _T::Instance ();\
            }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DynamicCreatable_h)
