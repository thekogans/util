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

#if !defined (__thekogans_util_SecureAllocator_h)
#define __thekogans_util_SecureAllocator_h

#include "thekogans/util/Environment.h"

#if defined (TOOLCHAIN_OS_Windows) || \
    defined (THEKOGANS_UTIL_HAVE_MMAP) || \
    defined (THEKOGANS_UTIL_SECURE_ALLOCATOR_USE_DEFAULT)

#include <cstddef>
#include <utility>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        /// \struct SecureAllocator SecureAllocator.h thekogans/util/SecureAllocator.h
        ///
        /// \brief
        /// SecureAllocator allocates physical pages. The pages are marked
        /// for reading and writing and are not swappable to disk. Before
        /// returning/freeing an allocated block, SecureAllocator clears it.
        ///
        /// This allocator is especially useful for creating heaps of secure
        /// objects. Here is how you would use it:
        ///
        /// \code{.cpp}
        /// struct SecureObject {
        ///     THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS
        /// };
        ///
        /// THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS_EX (
        ///     SecureObject,
        ///     thekogans::util::SpinLock,
        ///     thekogans::util::DEFAULT_HEAP_MIN_ITEMS_IN_PAGE,
        ///     thekogans::util::SecureAllocator::Instance ())
        /// or
        /// THEKOGANS_UTIL_IMPLEMENT_SECURE_ALLOCATOR_FUNCTIONS (SecureObject)
        /// \endcode
        ///
        /// NOTE: Don't forget to call SecureAllocator::ReservePages to
        /// ensure your process has enough physical pages to satisfy
        /// SecureAllocator::Alloc requests.

        struct _LIB_THEKOGANS_UTIL_DECL SecureAllocator :
                public Allocator,
                public RefCountedSingleton<SecureAllocator> {
            /// \brief
            /// SecureAllocator participates in the \see{Allocator} dynamic
            /// discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (SecureAllocator)

            /// \brief
            /// Call this method during process initialization to reserve
            /// enough physical pages to satisfy Alloc requests.
            /// \param[in] minWorkingSetSize Minimum physical pages to reserve.
            /// \param[in] maxWorkingSetSize Maximum physical pages to reserve.
            /// NOTE: Both values are in bytes.
            static void ReservePages (
                ui64 minWorkingSetSize,
                ui64 maxWorkingSetSize);

            /// \brief
            /// Allocate a secure block and pin it.
            /// \param[in] size Size of block to allocate.
            /// \return Pointer to the allocated block (0 if out of memory).
            virtual void *Alloc (std::size_t size) override;
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Same size parameter previously passed in to Alloc.
            virtual void Free (
                void *ptr,
                std::size_t size) override;
        };

        /// \def THEKOGANS_UTIL_IMPLEMENT_SECURE_ALLOCATOR_FUNCTIONS(_T)
        /// Macro to implement SecureAllocator functions.
        #define THEKOGANS_UTIL_IMPLEMENT_SECURE_ALLOCATOR_FUNCTIONS(_T)\
        void *_T::operator new (std::size_t size) {\
            assert (size == sizeof (_T));\
            return thekogans::util::SecureAllocator::Instance ()->Alloc (size);\
        }\
        void *_T::operator new (\
                std::size_t size,\
                std::nothrow_t) noexcept {\
            assert (size == sizeof (_T));\
            return thekogans::util::SecureAllocator::Instance ()->Alloc (size);\
        }\
        void *_T::operator new (\
                std::size_t size,\
                void *ptr) {\
            assert (size == sizeof (_T));\
            return ptr;\
        }\
        void _T::operator delete (void *ptr) {\
            thekogans::util::SecureAllocator::Instance ()->Free (ptr, sizeof (_T));\
        }\
        void _T::operator delete (\
                void *ptr,\
                std::nothrow_t) noexcept {\
            thekogans::util::SecureAllocator::Instance ()->Free (ptr, sizeof (_T));\
        }\
        void _T::operator delete (\
            void *,\
            void *) {}

        /// \struct stdSecureAllocator SecureAllocator.h thekogans/util/SecureAllocator.h
        ///
        /// \brief
        /// Implementation of a std::allocator which uses SecureAllocator::Instance (). Many standard
        /// template containers have an allocator parameter. Use this allocator to have the items
        /// be allocated from a secure heap.
        /// This implementation was borrowed heavily from: https://botan.randombit.net/

        template<typename T>
        struct stdSecureAllocator {
        public:
            /// \brief
            /// Alias for T.
            using value_type = T;
            /// \brief
            /// Alias for T *.
            using pointer = T *;
            /// \brief
            /// Alias for const T *.
            using const_pointer = const T *;
            /// \brief
            /// Alias for T &.
            using reference = T &;
            /// \brief
            /// Alias for const T &.
            using const_reference = const T &;
            /// \brief
            /// Alias for std::size_t.
            using size_type = std::size_t ;
            /// \brief
            /// Alias for std::ptrdiff_t.
            using difference_type = std::ptrdiff_t;

            /// \brief
            /// ctor.
            stdSecureAllocator () noexcept {}

            /// \brief
            /// ctor.
            /// \param[in] allocator stdSecureAllocator to copy construct.
            template<typename _U>
            stdSecureAllocator (const stdSecureAllocator<_U> & /*allocator*/) noexcept {}
            /// \brief
            /// dtor.
            ~stdSecureAllocator () noexcept {}

            /// \brief
            /// Return address of value.
            /// \param[in] value Object whose address to return.
            /// \return Address of value.
            pointer address (reference value) const noexcept {
                return std::addressof (value);
            }
            /// \brief
            /// Return address of value.
            /// \param[in] value Object whose address to return.
            /// \return Address of value.
            const_pointer address (const_reference value) const noexcept {
                return std::addressof (value);
            }

            /// \brief
            /// Allocate count of objects.
            /// \param[in] count Number of objects to allocate.
            /// \param[in] hint Nearby memory location.
            /// \return Buffer with enough space to contain count objects.
            pointer allocate (
                    size_type count,
                    const void * /*hint*/ = nullptr) {
                return (pointer)SecureAllocator::Instance ()->Alloc (count * sizeof (T));
            }
            /// \brief
            /// Free a previously allocated buffer.
            /// \param[in] ptr Pointer returned by allocate.
            /// \param[in] count Same count passed to allocate.
            void deallocate (
                    pointer ptr,
                    size_type count) {
                SecureAllocator::Instance ()->Free (ptr, count * sizeof (T));
            }

            /// \brief
            /// Returns the largest supported allocation size.
            /// \return Largest supported allocation size.
            size_type max_size () const noexcept {
                return static_cast<size_type> (-1) / sizeof (T);
            }

            /// \brief
            /// Placement new and copy construct template.
            /// C++11 provides variadic templates.
            /// \param[in] ptr Where to place the object.
            /// \param[in] args Variable ctor parameters.
            template<
                typename _U,
                typename... Args>
            void construct (
                    _U *ptr,
                    Args &&... args) {
                ::new (static_cast<void *> (ptr)) _U (std::forward<Args> (args)...);
            }

            /// \brief
            /// Destroy the object created with constuct.
            template<typename _U>
            void destroy (_U *ptr) {
                ptr->~_U ();
            }
        };

        /// \brief
        /// stdSecureAllocator is stateless. Therefore operator == always returns true.
        /// \param[in] allocator1 First allocator to compare.
        /// \param[in] allocator2 Second allocator to compare.
        /// \return true
        template<
            typename T,
            typename _U>
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
                const stdSecureAllocator<T> & /*allocator1*/,
                const stdSecureAllocator<_U> & /*allocator2*/) {
            return true;
        }
        /// \brief
        /// stdSecureAllocator is stateless. Therefore operator != always returns false.
        /// \param[in] allocator1 First allocator to compare.
        /// \param[in] allocator2 Second allocator to compare.
        /// \return false
        template<
            typename T,
            typename _U>
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
                const stdSecureAllocator<T> & /*allocator1*/,
                const stdSecureAllocator<_U> & /*allocator2*/) {
            return false;
        }

        /// \brief
        /// Alias for std::basic_string<char, std::char_traits<char>, stdSecureAllocator<char>>.
        using SecureString = std::basic_string<
            char, std::char_traits<char>,
            stdSecureAllocator<char>>;
        /// \brief
        /// Alias for std::basic_string<wchar_t, std::char_traits<wchar_t>, stdSecureAllocator<wchar_t>>.
        using SecureWString = std::basic_string<
            wchar_t, std::char_traits<wchar_t>,
            stdSecureAllocator<wchar_t>>;
        /// \brief
        /// Alias for std::vector<T, stdSecureAllocator<T>>.
        template<typename T> using SecureVector = std::vector<T, stdSecureAllocator<T>>;

        /// \brief
        /// Zero out the given memory block.
        /// Do it in such a way as to not get optimized away.
        /// \param[in] data Block t zero out.
        /// \param[in] size Block size (in bytes).
        /// \return size.
        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API SecureZeroMemory (
            void *data,
            std::size_t size);

    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows) ||
       // defined (THEKOGANS_UTIL_HAVE_MMAP) ||
       // defined (THEKOGANS_UTIL_SECURE_ALLOCATOR_USE_DEFAULT)

#endif // !defined (__thekogans_util_SecureAllocator_h)
