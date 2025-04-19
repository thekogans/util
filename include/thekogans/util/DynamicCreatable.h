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
#include <unordered_map>
#include "thekogans/util/Config.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/StringUtils.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Dynamic typing is the practice by which the system creates and interacts
        /// with instances of types it did not see @compile time. This technique
        /// is very useful in systems where services need to be extended with
        /// plugins without bringing the whole system down.
        ///
        /// One of the shortcomings of c++ is it's inability to dynamically
        /// create a type at runtime given a string representation of it's type.
        /// DynamicCreatable and it's supporting macros below are my attempt at
        /// plugging that hole. For any scheme to ultimately be useful it needs
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
        /// NOTE: My design and implementation is very heavily macro based. This design
        /// allows me to reuse macros without duplicating code. The macros have a very
        /// logical structure where _DECLARE_ macros are used in class declaration (*.h)
        /// and _IMPLEMENT_ macros are used in class definition (*.cpp). Most are
        /// private and are labeled as such (you don't need to concern yourself with them).
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
        ///     struct bar : public thekogans::util::DynamicCreatable {
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
        ///     // If bar happens to be a \see{Singleton}.
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_S (foo::bar)
        ///     // If bar happens to be a template.
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_T (foo::bar<some type>)
        ///     // If bar happens to be a \see{Singleton} template.
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ST (foo::bar<some type>)
        ///
        /// } // namespace foo
        /// \endcode
        ///
        /// 2. To create a class that will be an abstract base for a number of derivative types;
        ///
        /// \code{.cpp}
        /// // in bar.h
        ///
        /// namespace foo {
        ///
        ///     struct bar : public thekogans::util::DynamicCreatable {
        ///         THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (bar)
        ///         ...
        ///     };
        ///
        /// } // namespace foo
        ///
        /// // in bar.cpp. Note the fully qualified name.
        ///
        /// namespace foo {
        ///
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (foo::bar)
        ///     // If bar happens to be a template.
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE_T (foo::bar<some type>)
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
        ///     // If baz happens to be a \see{thekogans::util::Singleton}.
        ///     struct baz :
        ///             public bar,
        ///             public thekogans::util::Singleton<baz> {
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
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_S (foo::baz, bar::TYPE)
        ///     // If baz happens to be a template.
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_T (foo::baz<some type>, bar::TYPE)
        ///     // If baz happens to be a \see{Singleton} template.
        ///     THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ST (foo::baz<some type>, bar::TYPE)
        ///     // If bar happens to be a template, substitute bar<some type> for bar above.
        ///
        /// } // namespace foo
        /// \endcode

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_TYPE(_T)
        /// Declare DynamicCreatable::TYPE. This macro is usually private.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_TYPE(_T)\
        public:\
            static const char * const TYPE;

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_TYPE(_T)
        /// Implement DynamicCreatable::TYPE. This macro is usually private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_TYPE(_T)\
            const char * const _T::TYPE = #_T;

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_Type(_T)
        /// Declare DynamicCreatable::Type. This macro is usually private.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_Type(_T)\
        public:\
            virtual const char *Type () const noexcept override;

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Type(_T)
        /// Implement DynamicCreatable::Type. This macro is usually private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Type(_T)\
            const char *_T::Type () const noexcept {\
                return TYPE;\
            }

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASES(_T)
        /// Declare the DynamicCreatable::BASES array. This macro is private.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASES(_T)\
        public:\
            static const char * const BASES[];

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_BEGIN(_T)
        /// Used to start the creation of DynamicCreatable::BASES array. This
        /// macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_BEGIN(_T)\
            const char * const _T::BASES[] = {

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_END(_T)
        /// Used to end the creation of DynamicCreatable bases array. A list
        /// of bases would go between the above and this macro. This
        /// macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_END(_T)\
                nullptr\
            };

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_Bases(_T)
        /// Declare DynamicCreatable::Bases. This macro is usually private.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_Bases(_T)\
        public:\
            virtual const char * const *Bases () const noexcept override;

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Bases(_T)
        /// Implement DynamicCreatable::Bases. This macro is usually private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Bases(_T)\
            const char * const *_T::Bases () const noexcept {\
                return BASES;\
            }

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE(_T)
        /// Declare DynamicCreatable overridables. Unless you have a special need
        /// (\see{FileLogger}), you should not need to directly use this macro.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE(_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_TYPE (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_Type (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASES (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_Bases (_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T, ...)
        /// Implement DynamicCreatable overridables. Unless you have a special need
        /// (\see{FileLogger}), you should not need to directly use this macro.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE(_T, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_TYPE (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Type (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_BEGIN (_T)\
                thekogans::util::DynamicCreatable::TYPE,\
                ##__VA_ARGS__,\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_END (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Bases (_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE_T(_T, ...)
        /// Implement DynamicCreatable overridables for template types. Unless
        /// you have a special need, you should not need to directly use this
        /// macro.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE_T(_T, ...)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_TYPE (_T)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Type (_T)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_BEGIN (_T)\
                thekogans::util::DynamicCreatable::TYPE,\
                ##__VA_ARGS__,\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASES_END (_T)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Bases (_T)

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_FUNCTIONS(_T)
        /// Define the base type functions. This macro is private.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_FUNCTIONS(_T)\
        public:\
            static bool IsType (const char *type);\
            static const thekogans::util::DynamicCreatable::TypeMapType &GetTypes ();\
            static _T::SharedPtr CreateType (\
                const char *type,\
                thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters = nullptr);

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_IS_TYPE(_T)
        /// Implement base type IsType. This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_IS_TYPE(_T)\
            bool _T::IsType (const char *type) {\
                return type != nullptr && GetTypes ().find (type) != GetTypes ().end ();\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_GET_TYPES(_T)
        /// Implement base type GetTypes.
        /// This is the magic that makes the entire scheme so performant.
        /// Because base types know their own names, they can cache their
        /// own type map reference and use it to look up types descendant
        /// from them. This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_GET_TYPES(_T)\
            const thekogans::util::DynamicCreatable::TypeMapType &_T::GetTypes () {\
                static const thekogans::util::DynamicCreatable::TypeMapType &types =\
                    (*thekogans::util::DynamicCreatable::BaseMap::Instance ())[_T::TYPE];\
                return types;\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_CREATE_TYPE(_T)
        /// Implement base type CreateType. This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_CREATE_TYPE(_T)\
            _T::SharedPtr _T::CreateType (\
                    const char *type,\
                    thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters) {\
                if (type != nullptr) {\
                    thekogans::util::DynamicCreatable::TypeMapType::const_iterator it =\
                        GetTypes ().find (type);\
                    if (it != GetTypes ().end ()) {\
                        return it->second (parameters);\
                    }\
                }\
                return nullptr;\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_FUNCTIONS(_T)
        /// Implement base type functions. This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_FUNCTIONS(_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_IS_TYPE (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_GET_TYPES (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_CREATE_TYPE (_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_FUNCTIONS_T(_T)
        /// Implement base type functions. This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_FUNCTIONS_T(_T)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_IS_TYPE (_T)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_GET_TYPES (_T)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_CREATE_TYPE (_T)

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE(_T)
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
        /// - Some sort of _DECLARE_ and _IMPLEMENT_ macros to hide base type registration.
        ///
        /// NOTE: Your DynamicCreatable derived types can eventually wind up
        /// in a bigger system with DynamicCreatable types from other organizations.
        /// To facilitate interoperability and to avoid name space collisions
        /// it's best practice to define your types fully qualified (in the _IMPLEMENT_
        /// macro) including the namespace they came from (Ex: \see{Serializable}, it's
        /// type name is specified as "thekogans::util::Serializable"). Ex:
        ///
        /// \code{.cpp}
        /// namespace thekogans {
        ///     namespace util {
        ///
        ///         struct _LIB_THEKOGANS_UTIL_DECL Serializable : public DynamicCreatable {
        ///             THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (Serializable)
        ///             ...
        ///         };
        ///
        ///     } // namespace thekogans
        /// } // namespace util
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE(_T)\
        public:\
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_TYPE (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_Type (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_FUNCTIONS (_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE(_T)
        /// This macro is used in DynamicCreatable base classes. It casts
        /// it's return types to the base they were derived from to make
        /// the code cleaner without the dynamic_refcounted_sharedptr_cast
        /// clutter. Ex:
        ///
        /// \code{.cpp}
        /// namespace thekogans {
        ///     namespace util {
        ///
        ///         THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE (
        ///             thekogans::util::Serializable)
        ///
        ///     } // namespace thekogans
        /// } // namespace util
        /// \endcode
        ///
        /// VERY IMPORTANT: To twart name space collisions, note the fully qualified name.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE(_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_TYPE (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Type (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_FUNCTIONS(_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE_T(_T)
        /// This macro is used in DynamicCreatable base classes for templete types.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ABSTRACT_BASE_T(_T)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_TYPE (_T)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_Type (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_FUNCTIONS_T (_T)

        /// \struct DynamicCreatableBase DynamicCreatable.h thekogans/util/DynamicCreatable.h
        ///
        /// \brief
        /// DynamicCreatableBase exposes the Type (), Bases () and BasesSize () virtual methods.

        struct _LIB_THEKOGANS_UTIL_DECL DynamicCreatableBase : public virtual RefCounted {
            /// \brief
            /// All DynamicCreatable derived types need to be able to
            /// dynamically identify themselves. Its what makes them
            /// dinamically creatable.
            /// \return TYPE.
            virtual const char *Type () const noexcept = 0;

            /// \brief
            /// Return the list of base types (ancestors).
            /// DynamicCreatable derived types will have at least one
            /// base (DynamicCreatable).
            /// \return BASES.
            virtual const char * const *Bases () const noexcept = 0;
        };

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

        struct _LIB_THEKOGANS_UTIL_DECL DynamicCreatable : public DynamicCreatableBase {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (DynamicCreatable)
            /// \brief
            /// Declare DynamicCreatable overrides.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE (DynamicCreatable)

            /// \struct DynamicCreatable::Parameters DynamicCreatable.h
            /// thekogans/util/DynamicCreatable.h
            ///
            /// \brief
            /// Parameters allow you to parametarize type creation. All
            /// DynamicCreatable derived types must be default constructable,
            /// but there are times when you need to provide specific instance
            /// parameters. By deriving a class from Parameters and passing
            /// it to CreateType you can hook in to the creation pipeline and
            /// change the default behavior.
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
            using FactoryType = std::function<SharedPtr (Parameters::SharedPtr /*parameters*/)>;
            /// \struct DynamicCreatable::CharPtrHash DynamicCreatable.h
            /// thekogans/util/DynamicCreatable.h
            ///
            /// \brief
            /// For compactness we use const char * as map keys. This struct
            /// exposes a operator () that is used to treat the keys like strings.
            struct CharPtrHash {
                /// \brief
                /// Return string hash.
                /// \param[in] str String whos hash to return.
                /// \return String hash.
                inline std::size_t operator () (const char *str) const {
                    return HashString (str);
                }
            };
            /// \struct DynamicCreatable::CharPtrEqual DynamicCreatable.h
            /// thekogans/util/DynamicCreatable.h
            ///
            /// \brief
            /// For compactness we use const char * as map keys. This struct
            /// exposes a operator () that is used to treat the keys like strings.
            struct CharPtrEqual {
                /// \brief
                /// Return true if std::strcmp (str1, str2) == 0.
                /// \param[in] str1 First string to compare.
                /// \param[in] str2 Second string to compare.
                /// \return true if std::strcmp (str1, str2) == 0.
                inline bool operator () (
                        const char *str1,
                        const char *str2) const {
                    return std::strcmp (str1, str2) == 0;
                }
            };
            /// \brief
            /// Maps type name to it's factory method.
            using TypeMapType = std::unordered_map<
                const char *, FactoryType, CharPtrHash, CharPtrEqual>;
            /// \brief
            /// Maps base type name to it's derived types.
            using BaseMapType = std::unordered_map<
                const char *, TypeMapType, CharPtrHash, CharPtrEqual>;
            /// \brief
            /// The one and only base to derived type map. You can access it like this;
            /// thekogans::util::DynamicCreatable::BaseMap::Instance ()->.
            using BaseMap = Singleton<BaseMapType>;

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
                /// \param[in] type DynamicCreatable type (it's class name).
                /// \param[in] factory DynamicCreatable creation factory.
                BaseMapInitializer (
                    const char * const bases[],
                    const char *type,
                    FactoryType factory);
            };
        #endif // defined (THEKOGANS_UTIL_TYPE_Shared)

            /// \brief
            /// Return true if base is found in the BASES list.
            /// NOTE: Unlike 'true' polymorphism this method will
            /// return true only if you setup a relationship between
            /// the given base and this type. Actual inheritance has
            /// nothing to do with it.
            /// \return true == this type is derived from the given base.
            bool IsDerivedFrom (const char *base) const noexcept;

            /// \brief
            /// Pretty print the BaseMap to the std::cout.
            /// Use this to take a look inside your map when debugging.
            /// \param[in] base Optional base whos map to prity print.
            /// If nullptr, print all bases.
            static void DumpBaseMap (const char *base = nullptr);
        };

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
                const char * const *bases = _T::BASES;\
                while (*bases != nullptr) {\
                    (*thekogans::util::DynamicCreatable::BaseMap::Instance ())[\
                        *bases++][_T::TYPE] = _T::Create;\
                    }\
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
                _T::BASES, _T::TYPE, _T::Create);
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        /// \def THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE(_T)
        /// Declaration of the type factory method. This macro is private.
        #define THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_CREATE(_T)\
        public:\
            static thekogans::util::DynamicCreatable::SharedPtr Create (\
                thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters = nullptr);

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_CREATE(_T)
        /// Implementation of the default type factory method. This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_CREATE(_T)\
            thekogans::util::DynamicCreatable::SharedPtr _T::Create (\
                    thekogans::util::DynamicCreatable::Parameters::SharedPtr parameters) {\
                thekogans::util::DynamicCreatable::SharedPtr dynamicCreatable (new _T);\
                if (parameters != nullptr) {\
                    parameters->Apply (dynamicCreatable);\
                }\
                return dynamicCreatable;\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_CREATE_S(_T)
        /// Implementation of the default type factory method for \see{Singleton} types.
        /// This macro is private.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_CREATE_S(_T)\
            thekogans::util::DynamicCreatable::SharedPtr _T::Create (\
                    thekogans::util::DynamicCreatable::Parameters::SharedPtr /*parameters*/) {\
                return _T::Instance ();\
            }\

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
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_CREATE (_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE_MAP_INIT (_T)

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
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (_T, ##__VA_ARGS__)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_CREATE (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT (_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_S(_T, ...)
        /// Dynamic discovery macro for \see{Singleton} types. Instantiate one of these
        /// in the class cpp file.
        ///
        /// Example:
        ///
        /// \code{.cpp}
        /// namespace thekogans {
        ///     namespace util {
        ///
        ///         THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_S (
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
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_S(_T, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE (_T, ##__VA_ARGS__)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_CREATE_S (_T)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT (_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_T(_T, ...)
        /// Dynamic discovery macro for template types. Instantiate one of these
        /// in the class cpp file of the template types you're specializing.
        ///
        /// Example:
        ///
        /// \code{.cpp}
        /// namespace thekogans {
        ///     namespace util {
        ///
        ///         THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_T (
        ///             thekogans::crypto::SymmetricKey::KeyType,
        ///             Serializer::TYPE)
        ///
        ///     } // namespace thekogans
        /// } // namespace util
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_T(_T, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE_T (_T, ##__VA_ARGS__)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_CREATE (_T)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT (_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ST(_T, ...)
        /// Dynamic discovery macro for template types that are \see{Singleton}.
        /// Instantiate one of these in the class cpp file of the template types
        /// you're specializing.
        #define THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_ST(_T, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_OVERRIDE_T (_T, ##__VA_ARGS__)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_CREATE_S (_T)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE_MAP_INIT (_T)

        // NOTE: THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE[_ST] are just
        // the examples I thought were interesting. There's nothing magical
        // about them. As a mater of fact all you need to plug in to the
        // DynamicCreatable machinery is to implement a Create factory
        // method (THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_CREATE[_S]
        // above) and construct your own THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE
        // macro by copying one of the above and replacing the Create macro.

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DynamicCreatable_h)
