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

#if !defined (__thekogans_util_Allocator_h)
#define __thekogans_util_Allocator_h

#include <cstddef>
#include <memory>
#include <string>
#include <list>
#include <map>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct Allocator Allocator.h thekogans/util/Allocator.h
        ///
        /// \brief
        /// Allocator is a base class for all allocators. It defines
        /// the interface and lets concrete classes handle implementation
        /// details.

        struct _LIB_THEKOGANS_UTIL_DECL Allocator {
            /// \brief
            /// typedef for the Allocator factory function.
            typedef Allocator *(*Factory) ();
            /// \brief
            /// typedef for the Allocator map.
            typedef std::map<std::string, Factory> Map;
            /// \brief
            /// Controls Map's lifetime.
            /// \return Allocator map.
            static Map &GetMap ();
            /// \brief
            /// Used for Allocator dynamic discovery and creation.
            /// \param[in] type Allocator type (it's name).
            /// \return A Allocator based on the passed in type.
            static Allocator *Get (const std::string &type);
        #if defined (THEKOGANS_UTIL_TYPE_Static)
            /// \brief
            /// Because Allocator uses dynamic initialization, when using
            /// it in static builds call this method to have the Allocator
            /// explicitly include all internal allocator types. Without
            /// calling this api, the only allocatorers that will be available
            /// to your application are the ones you explicitly link to.
            static void StaticInit ();
        #else // defined (THEKOGANS_UTIL_TYPE_Static)
            /// \struct Allocator::MapInitializer Allocator.h thekogans/util/Allocator.h
            ///
            /// \brief
            /// MapInitializer is used to initialize the Allocator::map.
            /// It should not be used directly, and instead is included
            /// in THEKOGANS_UTIL_DECLARE_ALLOCATOR/THEKOGANS_UTIL_IMPLEMENT_ALLOCATOR.
            /// If you are deriving a allocatorer from Allocator, and you want
            /// it to be dynamically discoverable/creatable, add
            /// THEKOGANS_UTIL_DECLARE_ALLOCATOR to it's declaration,
            /// and THEKOGANS_UTIL_IMPLEMENT_ALLOCATOR to it's definition.
            struct _LIB_THEKOGANS_UTIL_DECL MapInitializer {
                /// \brief
                /// ctor. Add allocator of type, and factory for creating it
                /// to the Allocator::map
                /// \param[in] type Allocator type (it's class name).
                /// \param[in] factory Allocator creation factory.
                MapInitializer (
                    const std::string &type,
                    Factory factory);
            };
        #endif // defined (THEKOGANS_UTIL_TYPE_Static)
            /// \brief
            /// Get the list of all allocators registered with the map.
            static void GetAllocators (std::list<std::string> &allocators);

            /// \brief
            /// dtor.
            virtual ~Allocator () {}

            /// \brief
            /// Return allocator name.
            /// \return Allocator name.
            virtual const char *GetName () const = 0;

            /// \brief
            /// Allocate a block.
            /// NOTE: Allocator policy is to return (void *)0 if size == 0.
            /// if size > 0 and an error occurs, Allocator will throw an exception.
            /// \param[in] size Size of block to allocate.
            /// \return Pointer to the allocated block ((void *)0 if size == 0).
            virtual void *Alloc (std::size_t size) = 0;
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// NOTE: Allocator policy is to do nothing if ptr == 0.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Same size parameter previously passed in to Alloc.
            virtual void Free (
                void *ptr,
                std::size_t size) = 0;
        };

        /// \def THEKOGANS_UTIL_DECLARE_ALLOCATOR_COMMON(type)
        /// Common dynamic discovery macro.
        #define THEKOGANS_UTIL_DECLARE_ALLOCATOR_COMMON(type)\
        public:\
            static thekogans::util::Allocator *Create () {\
                return &Instance ();\
            }\
            virtual const char *GetName () const {\
                return #type;\
            }

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_ALLOCATOR(type)
        /// Dynamic discovery macro. Add this to your class declaration.
        /// Example:
        /// \code{.cpp}
        /// struct _LIB_THEKOGANS_UTIL_DECL DefaultAllocator : public Allocator {
        ///     THEKOGANS_UTIL_DECLARE_ALLOCATOR (DefaultAllocator)
        ///     ...
        /// };
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_ALLOCATOR(type)\
        public:\
            THEKOGANS_UTIL_DECLARE_ALLOCATOR_COMMON (type)\
            THEKOGANS_UTIL_STATIC_INIT (type)

        /// \def THEKOGANS_UTIL_IMPLEMENT_ALLOCATOR(type)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_ALLOCATOR (DefaultAllocator)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_ALLOCATOR(type)
    #else // defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_ALLOCATOR(type)
        /// Dynamic discovery macro. Add this to your class declaration.
        /// Example:
        /// \code{.cpp}
        /// struct _LIB_THEKOGANS_UTIL_DECL DefaultAllocator : public Allocator {
        ///     THEKOGANS_UTIL_DECLARE_ALLOCATOR (DefaultAllocator)
        ///     ...
        /// };
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_ALLOCATOR(type)\
        public:\
            THEKOGANS_UTIL_DECLARE_ALLOCATOR_COMMON (type)\
            static const thekogans::util::Allocator::MapInitializer mapInitializer;\

        /// \def THEKOGANS_UTIL_IMPLEMENT_ALLOCATOR(type)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_ALLOCATOR (DefaultAllocator)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_ALLOCATOR(type)\
            const thekogans::util::Allocator::MapInitializer type::mapInitializer (\
                #type, type::Create);
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        /// \def THEKOGANS_UTIL_DECLARE_ALLOCATOR_FUNCTIONS
        /// Macro to declare allocator functions.
        #define THEKOGANS_UTIL_DECLARE_ALLOCATOR_FUNCTIONS\
        public:\
            static void *operator new (std::size_t size);\
            static void *operator new (std::size_t size, std::nothrow_t) throw ();\
            static void *operator new (std::size_t, void *ptr);\
            static void operator delete (void *ptr);\
            static void operator delete (void *ptr, std::nothrow_t) throw ();\
            static void operator delete (void *, void *);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Allocator_h)
