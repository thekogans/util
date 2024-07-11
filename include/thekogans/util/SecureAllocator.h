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
    defined (THEKOGANS_UTIL_USE_DEFAULT_SECURE_ALLOCATOR)

#include <cstddef>
#if __cplusplus >= 201103L
    #include <utility>
#endif // __cplusplus >= 201103L
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
        /// freeing an allocated block, SecureAllocator clears it.
        ///
        /// This allocator is especially useful for creating heaps of secure
        /// objects. Here is how you would use it:
        ///
        /// \code{.cpp}
        /// struct SecureObject {
        ///     THEKOGANS_UTIL_DECLARE_HEAP (SecureObject)
        ///     or
        ///     THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (
        ///         SecureObject,
        ///         thekogans::util::SpinLock)
        /// };
        ///
        /// THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_ALLOCATOR (
        ///     SecureObject,
        ///     thekogans::util::SecureAllocator::Instance ())
        /// or
        /// THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_AND_ALLOCATOR (
        ///     SecureObject,
        ///     thekogans::util::SpinLock,
        ///     thekogans::util::SecureAllocator::Instance ())
        /// \endcode
        ///
        /// NOTE: Don't forget to call SecureAllocator::ReservePages to
        /// ensure your process has enough physical pages to satisfy
        /// SecureAllocator::Alloc requests.

        struct _LIB_THEKOGANS_UTIL_DECL SecureAllocator :
                public Allocator,
                public Singleton<
                    SecureAllocator,
                    SpinLock,
                    RefCountedInstanceCreator<SecureAllocator>,
                    RefCountedInstanceDestroyer<SecureAllocator>> {
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
            return thekogans::util::SecureAllocator::Instance ().Alloc (size);\
        }\
        void *_T::operator new (\
                std::size_t size,\
                std::nothrow_t) throw () {\
            assert (size == sizeof (_T));\
            return thekogans::util::SecureAllocator::Instance ().Alloc (size);\
        }\
        void *_T::operator new (\
                std::size_t size,\
                void *ptr) {\
            assert (size == sizeof (_T));\
            return ptr;\
        }\
        void _T::operator delete (void *ptr) {\
            thekogans::util::SecureAllocator::Instance ().Free (ptr, sizeof (_T));\
        }\
        void _T::operator delete (\
                void *ptr,\
                std::nothrow_t) throw () {\
            thekogans::util::SecureAllocator::Instance ().Free (ptr, sizeof (_T));\
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
            /// Convenient typedef for T.
            typedef T value_type;
            /// \brief
            /// Convenient typedef for T *.
            typedef T *pointer;
            /// \brief
            /// Convenient typedef for const T *.
            typedef const T *const_pointer;
            /// \brief
            /// Convenient typedef for T &.
            typedef T &reference;
            /// \brief
            /// Convenient typedef for const T &.
            typedef const T &const_reference;
            /// \brief
            /// Convenient typedef for std::size_t.
            typedef std::size_t size_type;
            /// \brief
            /// Convenient typedef for std::ptrdiff_t.
            typedef std::ptrdiff_t difference_type;

            /// \brief
            /// ctor.
            stdSecureAllocator () throw () {}

            /// \brief
            /// ctor.
            /// \param[in] allocator stdSecureAllocator to copy construct.
            template<typename _U>
            stdSecureAllocator (const stdSecureAllocator<_U> & /*allocator*/) throw () {}
            /// \brief
            /// dtor.
            ~stdSecureAllocator () throw () {}

            /// \brief
            /// Return address of value.
            /// \param[in] value Object whose address to return.
            /// \return Address of value.
            pointer address (reference value) const throw () {
                return std::addressof (value);
            }
            /// \brief
            /// Return address of value.
            /// \param[in] value Object whose address to return.
            /// \return Address of value.
            const_pointer address (const_reference value) const throw () {
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
                return (pointer)SecureAllocator::Instance ().Alloc (count * sizeof (T));
            }
            /// \brief
            /// Free a previously allocated buffer.
            /// \param[in] ptr Pointer returned by allocate.
            /// \param[in] count Same count passed to allocate.
            void deallocate (
                    pointer ptr,
                    size_type count) {
                SecureAllocator::Instance ().Free (ptr, count * sizeof (T));
            }

            /// \brief
            /// Returns the largest supported allocation size.
            /// \return Largest supported allocation size.
            size_type max_size () const throw () {
                return static_cast<size_type> (-1) / sizeof (T);
            }

        #if __cplusplus < 201103L
            /// \struct stdSecureAllocator::rebind SecureAllocator.h thekogans/util/SecureAllocator.h
            ///
            /// \brief
            /// Used internally by Visual Studio 10.
            template <class _U>
            struct rebind {
                /// \brief
                /// Convenient typedef for stdSecureAllocator<_U>.
                typedef stdSecureAllocator<_U> other;
            };

            /// \brief
            /// Placement new and copy construct.
            /// \param[in] ptr Where to place the object.
            /// \param[in] value Object to copy construct.
            void construct (
                    pointer ptr,
                    const T &value) {
                ::new ((void *)ptr) T (value);
            }
            /// \brief
            /// Placement new and copy construct template.
            /// \param[in] ptr Where to place the object.
            /// \param[in] value Object to copy construct.
            template<typename _U>
            void construct (
                    pointer ptr,
                    _U &&value) {
                ::new ((void *)ptr) T ((_U &&)value);
            }

            /// \brief
            /// Destroy the object created with constuct.
            void destroy (pointer ptr) {
                ptr->~T ();
            }
        #else // __cplusplus < 201103L
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
        #endif // __cplusplus < 201103L
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
        /// Convenient typedef for std::basic_string<char, std::char_traits<char>, stdSecureAllocator<char>>.
        typedef std::basic_string<char, std::char_traits<char>, stdSecureAllocator<char>> SecureString;
    #if __cplusplus < 201103L
        /// \struct SecureVector SecureAllocator.h thekogans/util/SecureAllocator.h
        ///
        /// \brief
        /// Convenient typedef for std::vector<T, stdSecureAllocator<T>>.
        template<typename T>
        struct SecureVector : public std::vector<T, stdSecureAllocator<T>> {
            /// \brief
            /// Convenient typedef for std::vector<T, stdSecureAllocator<T>>.
            typedef std::vector<T, stdSecureAllocator<T>> Base;
            /// \brief
            /// Convenient typedef for typename Base::value_type.
            typedef typename Base::value_type value_type;
            /// \brief
            /// Convenient typedef for typename Base::size_type.
            typedef typename Base::size_type size_type;
            /// \brief
            /// Convenient typedef for typename Base::difference_type.
            typedef typename Base::difference_type difference_type;
            /// \brief
            /// Convenient typedef for typename Base::reference.
            typedef typename Base::reference reference;
            /// \brief
            /// Convenient typedef for typename Base::value_type.
            typedef typename Base::value_type value_type;
            /// \brief
            /// Convenient typedef for typename Base::const_reference.
            typedef typename Base::const_reference const_reference;
            /// \brief
            /// Convenient typedef for typename Base::pointer.
            typedef typename Base::pointer pointer;
            /// \brief
            /// Convenient typedef for typename Base::const_pointer.
            typedef typename Base::const_pointer const_pointer;
            /// \brief
            /// Convenient typedef for typename Base::iterator.
            typedef typename Base::iterator iterator;
            /// \brief
            /// Convenient typedef for typename Base::const_iterator.
            typedef typename Base::const_iterator const_iterator;
            /// \brief
            /// Convenient typedef for typename Base::reverse_iterator.
            typedef typename Base::reverse_iterator reverse_iterator;
            /// \brief
            /// Convenient typedef for typename Base::const_reverse_iterator.
            typedef typename Base::const_reverse_iterator const_reverse_iterator;

            /// \brief
            /// ctor.
            SecureVector () :
                Base (stdSecureAllocator<T> ()) {}
            /// \brief
            /// ctor.
            /// \param[in] count Number of elements to resize to.
            /// \param[in] value Prototype to initialize new elements with.
            explicit SecureVector (
                size_type count,
                const T &value = T ()) :
                Base (count, value, stdSecureAllocator<T> ()) {}
            /// \brief
            /// ctor.
            /// \param[in] first Beginning of range.
            /// \param[in] last Just past the end of range.
            template<typename InputIt>
            SecureVector (
                InputIt first,
                InputIt last) :
                Base (first, last, stdSecureAllocator<T> ()) {}
            /// \brief
            /// ctor.
            /// \param[in] secureVector SecureVector to copy.
            SecureVector (const SecureVector<T> &secureVector) :
                Base (secureVector) {}
        };
    #else // __cplusplus < 201103L
        /// \brief
        /// Convenient typedef for std::vector<T, stdSecureAllocator<T>>.
        template<typename T> using SecureVector = std::vector<T, stdSecureAllocator<T>>;
    #endif // __cplusplus < 201103L

    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows) ||
       // defined (THEKOGANS_UTIL_HAVE_MMAP) ||
       // defined (THEKOGANS_UTIL_USE_DEFAULT_SECURE_ALLOCATOR)

#endif // !defined (__thekogans_util_SecureAllocator_h)
