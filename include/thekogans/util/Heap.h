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

#if !defined (__thekogans_util_Heap_h)
#define __thekogans_util_Heap_h

#include <cstddef>
#include <cstring>
#include <cassert>
#include <memory>
#include <map>
#include <iostream>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/DefaultAllocator.h"
#include "thekogans/util/AlignedAllocator.h"
#include "thekogans/util/NullLock.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/XMLUtils.h"

namespace thekogans {
    namespace util {

        /// \struct Heap Heap.h thekogans/util/Heap.h
        ///
        /// \brief
        /// This file implements a heap template for use with C++ structs/classes.
        ///
        /// Standard usage: Embedded in the class declaration.
        ///
        /// in myclass.hpp:
        ///
        /// \code{.cpp}
        /// class MyClass {
        ///     THEKOGANS_UTIL_DECLARE_HEAP (MyClass)
        /// public:
        ///     MyClass ();
        ///     ~MyClass ();
        /// };
        /// \endcode
        ///
        /// in myclass.cpp:
        ///
        /// use THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_HEAP (MyClass)
        /// \endcode
        /// or
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_HEAP_EX (MyClass, minItemsInPage)
        /// \endcode
        ///
        /// Aternate usage: Temporary object sub-allocation.
        ///
        /// \code{.cpp}
        /// using namespace thekogans::util;
        ///
        /// void foo (...) {
        ///     // Bar comes from someplace where its not possible to embed
        ///     // a heap in the class definition.
        ///     Heap<Bar> barHeap ("Bar");
        ///     ...
        ///     Bar *bar = barHeap.Alloc ();
        ///     assert (bar != 0);
        ///     ...
        ///     barHeap.Flush ();
        /// }
        /// \endcode
        ///
        /// Note: In production code barHeap would probably be
        /// sub classed from Heap to encapsulate bar ctor, and
        /// to call Flush in its own dtor.
        ///
        /// FIXME: Add example for template classes.
        ///
        /// Advantages:
        ///
        /// 1) The heap takes advantage of it's knowledge of the size of
        /// the type it is allocating for. Because of that one piece of info,
        /// it is able to do the job 50x faster on average (read that as 5000%).
        ///
        /// 2) Less memory fragmentation. The heap allocates pages in user
        /// specified multiples.
        ///
        /// 3) Built in diagnostics for double free and memory leaks, two
        /// of the nastiest (and most difficult to diagnose) memory bugs.
        ///
        /// 4) heap.Alloc () and heap.Free () execute in constant time. What
        /// this means is that no mater what the global heap looks like,
        /// class heap will always allocate and free quickly.
        ///
        /// 5) The heap is 64 bit safe, and will allocate from a 64 bit
        /// virtual space (must be used on a 64 bit OS).
        ///
        /// 6) The heap is highly tuned for 32/64/128 byte cache lines.
        /// If performance is of concern, make sure types allocated
        /// from this heap are properly aligned. To do that use
        /// pad bytes in the type declaration.
        ///
        /// 7) The heap is thread safe. By default the heap uses NullLock
        /// for performance. Use THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK/
        /// THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK to specify the type of
        /// lock to use. It is highly recommended that you use \see{SpinLock}
        /// for this, as it is very fast, and cheap.
        ///
        /// 8) Perhaps the most important reason for using the heap is,
        /// it's a no brainer to use. The examples above illustrate the
        /// only steps necessary to use the heap for any type.
        ///
        /// Limitations:
        ///
        /// 1) Because of design constraints, sizof (type) >= sizeof (void *).
        ///
        /// 2) As implemented, the heap does not allocate array of objects. This
        /// is not really a limitation, because allocating an array is usually
        /// a one time shot, and does not incur a lot of overhead.
        ///
        /// History:
        ///
        /// 10/09/1995 - version 1.0
        /// 01/30/1996 - version 1.1
        ///            - Added Heap::GetItemCount ()
        ///            - Fixed Heap::Free () to free empty pages.
        /// 10/20/2008 - version 1.2
        ///            - Added support for heaps of templates.
        /// 11/03/2008 - version 1.3
        ///            - Added external page Allocator to extend the heaps
        ///              usefulness to environments that use system allocators
        ///              other then new/delete.
        /// 11/13/2008 - version 2.0
        ///            - Complete overhaul.
        /// 04/18/2009 - version 2.1
        ///              Added Heap::Flush () to allow the heap to be
        ///              used for temporary item sub-allocation.
        /// 05/20/2009 - version 2.1.1
        ///              Added pad to Heap::Page to help with alignment.
        ///              Changed Heap::Page::magic to be type std::size_t
        ///              (for the same reason).
        /// 06/04/2009 - version 2.1.2
        ///              Commented out Flush (); in ~Heap (). See comment below.
        /// 02/06/2010 - version 2.1.3
        ///              Added item footer magic to help debug buffer overruns.
        /// 01/18/2011 - version 2.1.4
        ///              Replaced Heap::Page::pad with magic2.
        /// 01/22/2012 - version 2.2.0
        ///              Added Stats and HeapRegistry.
        /// 09/05/2012 - version 2.2.1
        ///              Changed Heap::Alloc to throw Exception.
        /// 07/12/2013 - version 2.2.2
        ///              Changed Heap::~Heap to use THEKOGANS_UTIL_ASSERT.
        /// 06/11/2014 - version 2.2.3
        ///              Added support for memory clearing. Very useful
        ///              in a security context.
        /// 11/17/2014 - version 2.2.4
        ///              Reworked the *_DECLARE_* and *_IMPLEMENT_* macros
        ///              to allow private sub-classes to implement a private
        ///              heap, and still remain private.
        /// 11/29/2014 - version 2.2.5
        ///              Finished documentation.
        /// 12/27/2014 - version 2.2.6
        ///              Fixed the *_DECLARE_* and *_IMPLEMENT_* macros
        ///              to allow templates to export the new/delete functions
        ///              in Windows. Now template instances can be defined
        ///              in a dll and allocated elsewhere.
        /// 02/09/2015 - version 2.3.0
        ///              Added nothrow versions of new and delete.
        /// 03/29/2015 - version 2.3.1
        ///              Cleaned up Heap::Stats::Dump to use XMLUtils.
        /// 07/29/2015 - version 2.3.2
        ///              Tweaked the Heap::Free logic to help with
        ///              cache utilization and memory thrashing.
        /// 07/30/2015 - version 2.3.3
        ///              Replaced Heap::PageList with IntrusiveList.
        /// 08/05/2015 - version 2.3.4
        ///              Removed clearMemory as the functionality is now
        ///              subsumed in to SecureAllocator.
        /// 08/05/2015 - version 2.4.0
        ///              Replaced the Allocator template parameter with a
        ///              ctor parameter defaulted to DefaultAllocator::Instance ().
        ///              Reworked the affected *_IMPLEMENT_* macros to
        ///              reflect the change. Heap is now usable in situations
        ///              that call for Allocators that have a non trivial
        ///              ctor.
        /// 04/22/2018 - version 2.5.0
        ///              Added HeapRegistry::IsValidPtr (...).
        /// 07/17/2018 - version 2.5.1
        ///              Changed name type from std::string to const char *.
        ///              Added GetName ().
        ///
        /// Author:
        ///
        /// Boris Kogan

        /// \brief
        /// Default number of items per page.
        const std::size_t THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE = 256;

        /// \def THEKOGANS_UTIL_DECLARE_HEAP_FUNCTIONS
        /// Macro to declare heap functions. This is an
        /// internal macro used by various THEKOGANS_UTIL_DECLARE_HEAP*
        /// macros below, and is not meant for public consumption.
        #define THEKOGANS_UTIL_DECLARE_HEAP_FUNCTIONS\
        public:\
            static void *operator new (std::size_t size);\
            static void *operator new (std::size_t size, std::nothrow_t) throw ();\
            static void *operator new (std::size_t, void *ptr);\
            static void operator delete (void *ptr);\
            static void operator delete (void *ptr, std::nothrow_t) throw ();\
            static void operator delete (void *, void *);

        /// \def THEKOGANS_UTIL_DECLARE_HEAP(type)
        /// Use this macro to declare a heap with
        /// a given type, a DefaultAllocator and a NullLock.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        #define THEKOGANS_UTIL_DECLARE_HEAP(type)\
        private:\
            static thekogans::util::Heap<type> &GetHeap ();\
            THEKOGANS_UTIL_DECLARE_HEAP_FUNCTIONS

        /// \def THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK(type, lock)
        /// Use this macro to declare a heap with
        /// a given type, a DefaultAllocator and a custom lock.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] lock Custom heap protection lock \see{NullLock}.
        #define THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK(type, lock)\
        private:\
            static thekogans::util::Heap<type, lock> &GetHeap ();\
            THEKOGANS_UTIL_DECLARE_HEAP_FUNCTIONS

        /// \brief
        /// Use these defines for regular classes (not templates).

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS(type)
        /// Macro to implement heap functions. This is an
        /// internal macro used by various THEKOGANS_UTIL_IMPLEMENT_HEAP*
        /// macros below, and is not meant for public consumption.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS(type)\
        void *type::operator new (std::size_t size) {\
            assert (size == sizeof (type));\
            return GetHeap ().Alloc (false);\
        }\
        void *type::operator new (\
                std::size_t size,\
                std::nothrow_t) throw () {\
            assert (size == sizeof (type));\
            return GetHeap ().Alloc (true);\
        }\
        void *type::operator new (\
                std::size_t size,\
                void *ptr) {\
            assert (size == sizeof (type));\
            return ptr;\
        }\
        void type::operator delete (void *ptr) {\
            GetHeap ().Free (ptr, false);\
        }\
        void type::operator delete (\
                void *ptr,\
                std::nothrow_t) throw () {\
            GetHeap ().Free (ptr, true);\
        }\
        void type::operator delete (\
            void *,\
            void *) {}

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_EX(type, minItemsInPage)
        /// Use this macro to instantiate a heap with
        /// a given items per page, DefaultAllocator
        /// and NullLock.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] minItemsInPage Minimum items per page.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_EX(\
            type, minItemsInPage)\
        thekogans::util::Heap<type> &type::GetHeap () {\
            static thekogans::util::Heap<type> heap (\
                #type, minItemsInPage);\
            return heap;\
        }\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (type)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_EX_WITH_ALLOCATOR(
        ///          type, allocator, minItemsInPage)
        /// Use this macro to instantiate a heap with
        /// a given items per page, a custom page
        /// allocator and a NullLock.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] minItemsInPage Minimum items per page.
        /// \param[in] allocator Custom page allocator \see{Allocator}.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_EX_WITH_ALLOCATOR(\
            type, minItemsInPage, allocator)\
        thekogans::util::Heap<type> &type::GetHeap () {\
            static thekogans::util::Heap<type> heap (\
                #type, minItemsInPage, allocator);\
            return heap;\
        }\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (type)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX(
        ///          type, lock, minItemsInPage)
        /// Use this macro to instantiate a heap with
        /// a DefaultAllocator, a custom lock and a
        /// given items per page.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] lock Custom heap protection lock \see{NullLock}.
        /// \param[in] minItemsInPage Minimum items per page.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX(\
            type, lock, minItemsInPage)\
        thekogans::util::Heap<type, lock> &type::GetHeap () {\
            static thekogans::util::Heap<type, lock> heap (\
                #type, minItemsInPage);\
            return heap;\
        }\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (type)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX_AND_ALLOCATOR(
        ///          type, lock, minItemsInPage, allocator)
        /// Use this macro to instantiate a heap with
        /// a custom page allocator and lock and a
        /// given items per page.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] lock custom Heap protection lock \see{NullLock}.
        /// \param[in] minItemsInPage Minimum items per page.
        /// \param[in] allocator Custom page allocator \see{tAllocator}.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX_AND_ALLOCATOR(\
            type, lock, minItemsInPage, allocator)\
        thekogans::util::Heap<type, lock> &type::GetHeap () {\
            static thekogans::util::Heap<type, lock> heap (\
                #type, minItemsInPage, allocator);\
            return heap;\
        }\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (type)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP(type)
        /// Use this macro to instantiate a heap with
        /// a DefaultAllocator, a NullLock and
        /// THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP(type)\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_EX (type,\
            thekogans::util::THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_ALLOCATOR(type, allocator)
        /// Use this macro to instantiate a heap with
        /// a custom allocator, a NullLock and
        /// THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] allocator Custom page allocator \see{DefaultAllocator}.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_ALLOCATOR(type, allocator)\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_EX_WITH_ALLOCATOR (type,\
            thekogans::util::THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE, allocator)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK(type, lock)
        /// Use this macro to instantiate a heap with
        /// a DefaultAllocator, a custom lock and
        /// THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] lock Custom heap protection lock \see{NullLock}.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK(type, lock)\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX (type, lock,\
            thekogans::util::THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_AND_ALLOCATOR(type, lock, allocator)
        /// Use this macro to instantiate a heap with
        /// a custom allocator, a custom lock and
        /// THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] lock Custom heap protection lock \see{NullLock}.
        /// \param[in] allocator Custom page allocator \see{Allocator}.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_AND_ALLOCATOR(\
            type, lock, allocator)\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX_AND_ALLOCATOR (type,\
            lock, thekogans::util::THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE, allocator)

        /// \brief
        /// Use these defines for templates.

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_T(type)
        /// Macro to implement heap functions. This is an
        /// internal macro used by various THEKOGANS_UTIL_IMPLEMENT_HEAP*
        /// macros below, and is not meant for public consumption.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_T(type)\
        template<>\
        THEKOGANS_UTIL_EXPORT void *type::operator new (std::size_t size) {\
            assert (size == sizeof (type));\
            return GetHeap ().Alloc (false);\
        }\
        template<>\
        THEKOGANS_UTIL_EXPORT void *type::operator new (\
                std::size_t size,\
                std::nothrow_t) throw () {\
            assert (size == sizeof (type));\
            return GetHeap ().Alloc (true);\
        }\
        template<>\
        THEKOGANS_UTIL_EXPORT void *type::operator new (\
                std::size_t size,\
                void *ptr) {\
            assert (size == sizeof (type));\
            return ptr;\
        }\
        template<>\
        THEKOGANS_UTIL_EXPORT void type::operator delete (void *ptr) {\
            GetHeap ().Free (ptr, false);\
        }\
        template<>\
        THEKOGANS_UTIL_EXPORT void type::operator delete (\
                void *ptr,\
                std::nothrow_t) throw () {\
            GetHeap ().Free (ptr, true);\
        }\
        template<>\
        THEKOGANS_UTIL_EXPORT void type::operator delete (\
            void *,\
            void *) {}

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_EX_T(type, minItemsInPage)
        /// Use this macro to instantiate a heap with
        /// a given items per page, DefaultAllocator
        /// and NullLock.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] minItemsInPage Minimum items per page.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_EX_T(\
            type, minItemsInPage)\
        template<>\
        thekogans::util::Heap<type> type::heap (\
            #type, minItemsInPage);\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_T (type)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_EX_WITH_ALLOCATOR_T(
        ///          type, allocator, minItemsInPage)
        /// Use this macro to instantiate a heap with
        /// a NullLock, a given items per page and a custom
        /// page allocator.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] minItemsInPage Minimum items per page.
        /// \param[in] allocator Custom page allocator \see{Allocator}.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_EX_WITH_ALLOCATOR_T(\
            type, minItemsInPage, allocator)\
        template<>\
        thekogans::util::Heap<type> type::heap (\
            #type, minItemsInPage, allocator);\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_T (type)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX_T(
        ///          type, lock, minItemsInPage)
        /// Use this macro to instantiate a heap with
        /// a DefaultAllocator, a custom lock and a
        /// given items per page.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] lock Custom heap protection lock \see{NullLock}.
        /// \param[in] minItemsInPage Minimum items per page.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX_T(\
            type, lock, minItemsInPage)\
        template<>\
        thekogans::util::Heap<type, lock>\
            type::heap (#type, minItemsInPage);\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_T (type)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX_AND_ALLOCATOR_T(
        ///          type, lock, minItemsInPage, allocator)
        /// Use this macro to instantiate a heap with
        /// a lock and a custom page allocator and
        /// given items per page.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] lock Custom heap protection lock \see{NullLock}.
        /// \param[in] minItemsInPage Minimum items per page.
        /// \param[in] allocator Custom page allocator \see{Allocator}.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX_AND_ALLOCATOR_T(\
            type, lock, minItemsInPage, allocator)\
        template<>\
        thekogans::util::Heap<type, lock> type::heap (\
            #type, minItemsInPage, allocator);\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_T (type)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_T(type)
        /// Use this macro to instantiate a heap with
        /// a NullLock, THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE
        /// and a DefaultAllocator.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_T(type)\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_EX_T (type,\
            thekogans::util::THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_ALLOCATOR_T(type, allocator)
        /// Use this macro to instantiate a heap with
        /// a custom allocator, a NullLock and
        /// THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] allocator Custom page allocator \see{DefaultAllocator}.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_ALLOCATOR_T(type, allocator)\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_EX_WITH_ALLOCATOR_T (type,\
            thekogans::util::THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE, allocator)

        /// \brief
        /// Use this macro to instantiate a heap with
        /// a DefaultAllocator, a custom lock and
        /// THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] lock Custom heap protection lock \see{NullLock}.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_T(type, lock)\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX_T (type, lock,\
            thekogans::util::THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE)

        /// \def THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_AND_ALLOCATOR_T(type, lock, allocator)
        /// Use this macro to instantiate a heap with
        /// a custom allocator, a custom lock and
        /// THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE.
        /// \param[in] type Type for which a custom heap
        /// is being declared (same as struct/class type).
        /// \param[in] lock Custom heap protection lock \see{NullLock}.
        /// \param[in] allocator Custom page allocator \see{Allocator}.
        #define THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_AND_ALLOCATOR_T(\
            type, lock, allocator)\
        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX_AND_ALLOCATOR_T (type, lock,\
            thekogans::util::THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE, allocator)

        /// \struct HeapRegistry Heap.h thekogans/util/Heap.h
        ///
        /// \brief
        /// The heap registry provides a convenient place
        /// to examine the state of all heaps in the system.
        /// Heaps register themselves with the registry
        /// during construction, and unregister during
        /// destruction.

        struct _LIB_THEKOGANS_UTIL_DECL HeapRegistry :
                public Singleton<HeapRegistry, SpinLock> {
            /// \struct HeapRegistry::Diagnostics Heap.h thekogans/util/Heap.h
            ///
            /// \brief
            /// An abstract base for the heap template to provide the stats.
            struct Diagnostics {
                /// \brief
                /// dtor.
                virtual ~Diagnostics () {}

                /// \brief
                /// Return true if the given pointer is one of ours.
                /// \param[in] ptr Pointer to check.
                /// \return true == we own the pointer, false == the pointer is not one of ours.
                virtual bool IsValidPtr (void *ptr) throw () = 0;

                /// \struct HeapRegistry::Diagnostics::Stats Heap.h thekogans/util/Heap.h
                ///
                /// \brief
                /// Heap stats.
                struct Stats {
                    /// \brief
                    /// Convenient typedef for std::unique_ptr<Stats>.
                    typedef std::unique_ptr<Stats> UniquePtr;
                    /// \brief
                    /// dtor.
                    virtual ~Stats () {}
                    /// \brief
                    /// Dump stats to std::ostream.
                    /// \param[in] stream std::ostream to dump the stats to.
                    virtual void Dump (std::ostream & /*stream*/ = std::cout) const = 0;
                };

                /// \brief
                /// Return the snapshot of the heap state.
                /// \return Snapshot of the heap state.
                virtual Stats::UniquePtr GetStats () = 0;
            };
            /// \brief
            /// Convenient typedef for std::map<std::string, Diagnostics *>.
            typedef std::map<std::string, Diagnostics *> Map;
            /// \brief
            /// Heap map.
            Map map;
            /// \brief
            /// Synchronization spin lock.
            SpinLock spinLock;

            /// \brief
            /// Add a heap to the registry.
            /// \param[in] name Heap name.
            /// \param[in] heap Heap to add.
            void AddHeap (
                const std::string &name,
                Diagnostics *heap);
            /// \brief
            /// Remove a named heap from the registry.
            /// \param[in] name Heap name.
            void DeleteHeap (const std::string &name);

            /// \brief
            /// Return true if the given pointer belongs to any of the heaps we manage.
            /// NOTE: In order to honor the throw () we cannot de-reference the pointer.
            /// We therefore search all pages in all heaps comparing range. Depending
            /// on the state of your heaps, this could be a very costly operation.
            /// \param[in] ptr Pointer to check.
            /// \return true == heap pointer, false == not a heap pointer.
            bool IsValidPtr (void *ptr) throw ();

            /// \brief
            /// Use this method to dump the state of all
            /// heaps in the system.
            /// \param[in] header Label the dump with an optional header.
            /// \param[in] stream std::ostream stream to dump to.
            void DumpHeaps (
                const std::string &header = std::string (),
                std::ostream &stream = std::cout);
        };

        /// \struct Heap Heap.h thekogans/util/Heap.h
        ///
        /// \brief
        /// Heap template.

        template<
            typename T,
            typename Lock = NullLock>
        struct Heap : public HeapRegistry::Diagnostics {
        protected:
            /// \brief
            /// Forward declaration of Page.
            struct Page;
            enum {
                /// \brief
                /// PageList ID.
                PAGE_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<Page, PAGE_LIST_ID>.
            typedef IntrusiveList<Page, PAGE_LIST_ID> PageList;
            /// \struct Heap::Page Heap.h thekogans/util/Heap.h
            ///
            /// \brief
            /// Items are allocated from pages. Each page has the
            /// following layout:
            ///
            /// +------------------------------------------------+\n
            /// | Page Header | Item 0 | ... | Item maxItems - 1 |\n
            /// +------------------------------------------------+
            struct Page : public PageList::Node {
                /// \brief
                /// A watermark. A way to identify that this
                /// block of memory does indeed point to a
                /// page.
                const std::size_t magic1;
                /// \brief
                /// Size of entire page, including Page
                /// Header, in bytes.
                const std::size_t size;
                /// \brief
                /// Total number of items this page can hold.
                /// The above three are const for a very
                /// simple reason.
                /// Pages are cast in stone. Once allocated,
                /// they are not resizable. Need more memory?
                /// Get another page.
                const std::size_t maxItems;
                /// \brief
                /// Current count of items allocated from
                /// this page.
                std::size_t allocatedItems;
                /// \struct Heap::Page::Item Heap.h thekogans/util/Heap.h
                ///
                /// \brief
                /// Page item info.
                struct Item {
                #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                    /// \brief
                    /// In debug, helps to catch item double
                    /// free, and buffer underrun.
                    std::size_t magic1;
                #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                    union {
                        /// \brief
                        /// Either a pointer to the next item in
                        /// the free list or,
                        Item *next;
                        /// \brief
                        /// the item itself.
                        ui8 block[sizeof (T)];
                    };
                #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                    /// \brief
                    /// In debug, helps to catch item double
                    /// free, and buffer overrun.
                    std::size_t magic2;
                #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                /// Within each page, freed items are stored
                /// in a singly linked list.
                } *freeItem;
                /// \brief
                /// A watermark. A way to identify that this
                /// block of memory does indeed point to a
                /// page.
                /// The header will be 32/64 bytes, depending
                /// on sizeof (std::size_t and Page/Item *).
                const std::size_t magic2;
                /// \brief
                /// The list of items. Directly following
                /// the Page Header.
                /// NOTE: Items are packed without any
                /// regard for alignment. All alignment
                /// concerns should be addressed in item
                /// declaration.
                Item items[1];

                /// \brief
                /// ctor.
                /// \param[in] size_ Page size.
                Page (std::size_t size_) :
                        magic1 (MAGIC),
                        size (size_),
                        maxItems ((size - sizeof (Page)) / sizeof (Item) + 1), // + 1 is for the implicit
                                                                               // item in sizeof (Page)
                        allocatedItems (0),
                        freeItem (0),
                        magic2 (MAGIC) {
                #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                    memset (items, 0, maxItems * sizeof (Item));
                #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                }

                /// \brief
                /// Return true if page is empty.
                /// \return true = empty, false = not empty.
                inline bool IsEmpty () const {
                    return allocatedItems == 0;
                }
                /// \brief
                /// Return true if page is full.
                /// \return true = full, false = not full.
                inline bool IsFull () const {
                    return allocatedItems == maxItems;
                }

                /// \brief
                /// Allocate an item.
                /// \return Pointer to the newly allocated item.
                inline void *Alloc () {
                    if (freeItem != 0) {
                        Item *item = freeItem;
                        // In release, this goes away, and the cost of
                        // allocation is reduced.
                    #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                        item->magic1 = MAGIC;
                        item->magic2 = MAGIC;
                    #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                        freeItem = freeItem->next;
                        ++allocatedItems;
                        return item->block;
                    }
                    else if (!IsFull ()) {
                        Item *item = &items[allocatedItems++];
                        // In release, this goes away, and the assignment
                        // above, and return below get optimized down to:
                        // return &items[allocatedItems++];
                    #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                        item->magic1 = MAGIC;
                        item->magic2 = MAGIC;
                    #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                        return item->block;
                    }
                    // We are allocating from a full page!
                    assert (0);
                    return 0;
                }

                /// \brief
                /// Free previously allocated item.
                /// \param[in] ptr Pointer to item to free.
                inline void Free (void *ptr) {
                #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                    assert (ptr != 0);
                    Item *item = (Item *)((std::size_t *)ptr - 1);
                    assert (IsItem (item));
                    item->magic1 = 0;
                    item->magic2 = 0;
                #else // defined (THEKOGANS_UTIL_CONFIG_Debug)
                    Item *item = (Item *)ptr;
                #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                    item->next = freeItem;
                    freeItem = item;
                    --allocatedItems;
                }

                /// \brief
                /// Return true if the given pointer is one of ours.
                /// \param[in] ptr Pointer to check.
                /// \return true == we own the pointer, false == the pointer is not one of ours.
                inline bool IsValidPtr (void *ptr) {
                #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                    Item *item = (Item *)((std::size_t *)ptr - 1);
                #else // defined (THEKOGANS_UTIL_CONFIG_Debug)
                    Item *item = (Item *)ptr;
                #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                    return IsItem (item);
                }

            private:
                /// \brief
                /// Perform sanity checks on the pointer to
                /// determine if it belongs to this page.
                /// \param[in] item Pointer to item to test.
                /// \return true = ours, false = not ours.
                inline bool IsItem (const Item *item) const {
                    return
                        // Verify that the given pointer points to the
                        // beginning of an item.
                        item >= items && item < &items[maxItems] &&
                        (std::ptrdiff_t)((const std::size_t)item -
                            (const std::size_t)items) % sizeof (Item) == 0 &&
                    #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                        // The test for magic is a lot more convincing
                        // once we verified that this is indeed one of
                        // our pointers.
                        // If this test fails, we have one (or more) of the following.
                        // 1. A double free.
                        // 2. Memory corruption.
                        // Either way, this is an application bug, and
                        // hopefully this will make it easier to find.
                        item->magic1 == MAGIC && item->magic2 == MAGIC;
                    #else // defined (THEKOGANS_UTIL_CONFIG_Debug)
                        1;
                    #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                }

                /// \brief
                /// Page is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Page)
            };

            /// \brief
            /// Heap name.
            const char *name;
            /// \brief
            /// Heap minimum items in page.
            const std::size_t minItemsInPage;
            /// \brief
            /// Heap minimum page size.
            const std::size_t minPageSize;
            /// \brief
            /// Current number of items on the heap.
            std::size_t itemCount;
            /// \brief
            /// Full pages.
            PageList fullPages;
            /// \brief
            /// Partial pages.
            PageList partialPages;
            /// \brief
            /// Pages need to be aligned on a page size boundary.
            AlignedAllocator allocator;
            /// \brief
            /// Synchronization lock.
            Lock lock;

        public:
            /// \brief
            /// ctor.
            /// The heap uses an \see{AlignedAllocator} to allocate its pages.
            /// To maximize memory efficiency, any given page may contain
            /// more or less items then any other (depends on alignment. See
            /// AlignedAllocator.h). Therefore you cannot dictate the exact
            /// count of items per page, only the minimum.
            /// \param[in] name_ Heap name.
            /// \param[in] minItemsInPage_ Heap minimum items in page.
            /// \param[in] allocator_ Page allocator.
            Heap (const char *name_ = 0,
                    std::size_t minItemsInPage_ =
                        THEKOGANS_UTIL_HEAP_DEFAULT_MIN_ITEMS_IN_PAGE,
                    Allocator &allocator_ = DefaultAllocator::Instance ()) :
                    name (name_),
                    minItemsInPage (minItemsInPage_),
                    minPageSize (Align (sizeof (Page) +
                        sizeof (typename Page::Item) * (minItemsInPage - 1))),
                    itemCount (0),
                    allocator (allocator_, minPageSize) {
                assert (minItemsInPage > 0);
                assert (OneBitCount (minPageSize) == 1);
                if (name != 0) {
                    HeapRegistry::Instance ().AddHeap (name, this);
                }
            }
            /// \brief
            /// dtor. Remove the heap from the registrty.
            virtual ~Heap () {
                // We're going out of scope. If there are still
                // pages remaining, we have a memory leak.
                if (!fullPages.empty () || !partialPages.empty ()) {
                    // Here we both log the leak and assert to give the
                    // engineer the best chance of figuring out what happened.
                    std::string message =
                        FormatString (
                            "%s : %s : " THEKOGANS_UTIL_SIZE_T_FORMAT,
                            GetName (),
                            typeid (*this).name (),
                            itemCount);
                    Log (
                        SubsystemAll,
                        THEKOGANS_UTIL,
                        Error,
                        __FILE__,
                        __FUNCTION__,
                        __LINE__,
                        __DATE__ " " __TIME__,
                        "%s",
                        message.c_str ());
                    THEKOGANS_UTIL_ASSERT (fullPages.empty () && partialPages.empty (), message);
                }
                // IMPORTANT: Do not uncomment this Flush. It
                // interferes with static dtors. If you are using a
                // local temp heap, inherit from this one, and call
                // Flush from its dtor.
                //Flush ();
                if (name != 0) {
                    HeapRegistry::Instance ().DeleteHeap (name);
                }
            }

            /// \brief
            /// Return true if the given pointer is one of ours.
            /// \param[in] ptr Pointer to check.
            /// \return true == we own the pointer, false == the pointer is not one of ours.
            virtual bool IsValidPtr (void *ptr) throw ();

            // HeapRegistry::Diagnostics::Stats
            /// \struct Heap::Stats Heap.h thekogans/util/Heap.h
            ///
            /// \brief
            /// Contains a snapshop of the heap stats.
            /// This is very useful for debugging.
            struct Stats : public HeapRegistry::Diagnostics::Stats {
                /// \brief
                /// Heap name.
                const char *name;
                /// \brief
                /// Heap item size.
                std::size_t itemSize;
                /// \brief
                /// Heap minimum item in page.
                std::size_t minItemsInPage;
                /// \brief
                /// Heap minimum page size.
                std::size_t minPageSize;
                /// \brief
                /// Current number of items on the heap.
                std::size_t itemCount;
                /// \brief
                /// Number of full pages on the heap.
                std::size_t fullPagesCount;
                /// \brief
                /// Number of partial pages on the heap.
                std::size_t partialPagesCount;

                /// \brief
                /// ctor.
                /// \param[in] name_ Heap name.
                /// \param[in] itemSize_ Heap item size.
                /// \param[in] minItemsInPage_ Heap minimum item in page.
                /// \param[in] minPageSize_ Heap minimum page size.
                /// \param[in] itemCount_ Current number of items on the heap.
                /// \param[in] fullPagesCount_ Number of full pages on the heap.
                /// \param[in] partialPagesCount_ Number of partial pages on the heap.
                Stats (
                    const char *name_,
                    std::size_t itemSize_,
                    std::size_t minItemsInPage_,
                    std::size_t minPageSize_,
                    std::size_t itemCount_,
                    std::size_t fullPagesCount_,
                    std::size_t partialPagesCount_) :
                    name (name_),
                    itemSize (itemSize_),
                    minItemsInPage (minItemsInPage_),
                    minPageSize (minPageSize_),
                    itemCount (itemCount_),
                    fullPagesCount (fullPagesCount_),
                    partialPagesCount (partialPagesCount_) {}

                /// \brief
                /// Dump heap stats to std::ostream.
                /// \param[in] stream std::ostream stream to dump the stats to.
                virtual void Dump (std::ostream &stream) const {
                    Attributes attributes;
                    attributes.push_back (Attribute ("name", name));
                    attributes.push_back (Attribute ("itemSize", size_tTostring (itemSize)));
                    attributes.push_back (Attribute ("minItemsInPage", size_tTostring (minItemsInPage)));
                    attributes.push_back (Attribute ("minPageSize", size_tTostring (minPageSize)));
                    attributes.push_back (Attribute ("itemCount", size_tTostring (itemCount)));
                    attributes.push_back (Attribute ("fullPagesCount", size_tTostring (fullPagesCount)));
                    attributes.push_back (Attribute ("partialPagesCount", size_tTostring (partialPagesCount)));
                    stream << OpenTag (0, "Heap", attributes, true, true);
                }
            };
            /// \brief
            /// Return a snapshot of the heap state.
            /// \return A snapshot of the heap state.
            virtual HeapRegistry::Diagnostics::Stats::UniquePtr GetStats () {
                LockGuard<Lock> guard (lock);
                return HeapRegistry::Diagnostics::Stats::UniquePtr (
                    new Stats (
                        GetName (),
                        sizeof (T),
                        minItemsInPage,
                        minPageSize,
                        itemCount,
                        fullPages.count,
                        partialPages.count));
            }

            /// \brief
            /// Return count of objects on the heap.
            /// \return Count of objects on the heap.
            inline std::size_t GetItemCount () const {
                return itemCount;
            }

            /// \brief
            /// Allocate an object for the heap.
            /// \param[in] nothrow true = return 0 if can't allocate,
            /// false = throw exception.
            /// \return pointer to newly allocated object.
            void *Alloc (bool nothrow);
            /// \brief
            /// Free a previously Alloc(ated) object.
            /// \param[in] ptr Pointer to object to free.
            /// \param[in] nothrow true = don't throw an exception on error.
            void Free (
                void *ptr,
                bool nothrow);

            /// \brief
            /// Free all objects from the heap.
            /// IMPORTANT NOTE: The heap has a singular job; To
            /// provide raw storage. It is not responsible for
            /// any high level processing on that storage. That
            /// includes construction/destruction of items contained
            /// within. To that end, Flush reclaims the pages, but
            /// does NOT destroy any items they may contain. This
            /// is very important when using the heap for temporary
            /// sub-allocation. If the items being allocated contain
            /// a non trivial dtor, they need to be destroyed before
            /// calling Flush or they might(will) leak.
            void Flush ();

        private:
            /// \brief
            /// If the heap was given a name, return it, otherwise return "unnamed".
            /// \return Heap name.
            inline const char *GetName () const {
                return name != 0  ? name : "unnamed";
            }

            /// \brief
            /// Return first partially allocated page (presumably for allocation).
            /// If no partially allocated pages left, allocate a new one.
            /// \return Pointer to partialPages.head
            inline Page *GetPage () {
                if (partialPages.empty ()) {
                    // AlignedAllocator will return at least minPageSize
                    // (usually more). To maximize efficiency, let the
                    // page sub-allocate as much as available.
                    std::size_t pageSize = minPageSize;
                    void *page = allocator.AllocMax (pageSize);
                    assert (page != 0);
                    if (page != 0) {
                        assert (pageSize >= minPageSize);
                        // This is safe, as neither placement new, nor
                        // Page ctor, nor push_back will throw.
                        partialPages.push_back (new (page) Page (pageSize));
                    }
                }
                return partialPages.front ();
            }

            /// \brief
            /// Get the Page to which a given pointer belongs.
            /// \param[in] ptr Pointer whose page we are asked to return.
            /// \return Page for a given pointer, or 0 if the pointer is not ours.
            inline Page *GetPage (void *ptr) const {
                // Pages are aligned on minPageSize boundary. But
                // because AlignedAllocator can return up to
                // minPageSize - 1 more space (see AlignedAllocator.h),
                // we can't just blindly assume that the page is on
                // the first minPageSize boudary. We use Page::magic1/2
                // to check for signatue, and if it fails, we go down
                // to the next boudary. If that fails, then this
                // pointer is not one of ours.
                Page *page = (Page *)((std::size_t)ptr & ~(minPageSize - 1));
                if (page->magic1 != MAGIC || page->magic2 != MAGIC) {
                    page = (Page *)((std::size_t)page - minPageSize);
                    if (page->magic1 != MAGIC || page->magic2 != MAGIC) {
                        page = 0;
                    }
                }
                return page;
            }

            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Heap)
        };

        template<
            typename T,
            typename Lock>
        bool Heap<T, Lock>::IsValidPtr (void *ptr) throw () {
            if (ptr != 0) {
                LockGuard<Lock> guard (lock);
                // To honor the no throw promise, we can't assume the
                // pointer came from this heap. We can't even assume
                // that it is valid (we cannot de-reference it). We
                // therefore search through our pages to see if the
                // given pointer lies within range.
                struct Callback : public PageList::Callback {
                    void *ptr;
                    explicit Callback (void *ptr_) :
                        ptr (ptr_) {}
                    virtual bool operator () (Page *page) {
                        return !page->IsValidPtr (ptr);
                    }
                } callback (ptr);
                return !fullPages.for_each (callback) || !partialPages.for_each (callback);
            }
            return false;
        }

        template<
            typename T,
            typename Lock>
        void *Heap<T, Lock>::Alloc (bool nothrow) {
            LockGuard<Lock> guard (lock);
            Page *page = GetPage ();
            assert (page != 0);
            if (page != 0) {
                void *ptr = page->Alloc ();
                assert (ptr != 0);
                if (page->IsFull ()) {
                    // GetPage will always return a page from the
                    // partialPages list.
                    partialPages.erase (page);
                    fullPages.push_back (page);
                }
                ++itemCount;
                return ptr;
            }
            if (!nothrow) {
                THEKOGANS_UTIL_THROW_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_ENOMEM,
                    "Out of memory allocating a '%s'.",
                    GetName ());
            }
            return 0;
        }

        template<
            typename T,
            typename Lock>
        void Heap<T, Lock>::Free (
                void *ptr,
                bool nothrow) {
            if (ptr != 0) {
                LockGuard<Lock> guard (lock);
                Page *page = GetPage (ptr);
                assert (page != 0);
                if (page != 0) {
                    // This logic is necessary to accommodate pages
                    // with one item. They become full after one
                    // allocation, and empty after one deletion.
                    if (page->IsFull ()) {
                        fullPages.erase (page);
                        // Put the page at the head of the partial
                        // pages list. If the next allocation happens
                        // soon enough, this page should still be in
                        // cache.
                        partialPages.push_front (page);
                    }
                    page->Free (ptr);
                    --itemCount;
                    if (page->IsEmpty ()) {
                        partialPages.erase (page);
                        page->~Page ();
                        allocator.Free (page, page->size);
                    }
                }
                else if (!nothrow) {
                    // Defensive programming. Nothing should ever go unnoticed.
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
        }

        template<
            typename T,
            typename Lock>
        void Heap<T, Lock>::Flush () {
            LockGuard<Lock> guard (lock);
            itemCount = 0;
            struct Callback : public PageList::Callback {
                typedef typename PageList::Callback::result_type result_type;
                typedef typename PageList::Callback::argument_type argument_type;
                Allocator &allocator;
                explicit Callback (Allocator &allocator_) :
                    allocator (allocator_) {}
                virtual result_type operator () (argument_type page) {
                    page->~Page ();
                    allocator.Free (page, page->size);
                    return true;
                }
            } callback (allocator);
            fullPages.clear (callback);
            partialPages.clear (callback);
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Heap_h)
