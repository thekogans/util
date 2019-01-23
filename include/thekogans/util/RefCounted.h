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

#if !defined (__thekogans_util_RefCounted_h)
#define __thekogans_util_RefCounted_h

#include <cassert>
#include <typeinfo>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        /// \struct RefCounted RefCounted.h thekogans/util/RefCounted.h
        ///
        /// \brief
        /// RefCounted is a base class for reference counted objects.
        /// It's designed to be useful for objects that are heap as well
        /// as stack allocated. On construction, the reference count is
        /// set to 0. Use RefCounted::Ptr to deal with heap object lifetimes.
        /// Unlike more complicated reference count classes, I purposely
        /// designed this one not to deal with polymorphism and construction
        /// and destruction issues. The default behavior is: All RefCounted
        /// objects that are allocated on the heap will be allocated with new,
        /// and destroyed with delete. I provide a virtual void Harakiri () to
        /// give class designers finer control over the lifetime management of
        /// their classes. If you need more control over heap placement, that's
        /// what \see{Heap} is for.
        /// NOTE: When inheriting from RefCounted, consider making it 'public virtual'.
        /// This will go a long way towards resolving multiple inheritance ambiguities.

        template<typename Count>
        struct _LIB_THEKOGANS_UTIL_DECL RefCounted {
        private:
            /// \brief
            /// Reference count.
            Count count;

        public:
            /// \brief
            /// ctor.
            RefCounted () :
                count (0) {}
            /// \brief
            /// dtor.
            virtual ~RefCounted () {}

            /// \brief
            /// Increment the reference count.
            /// \return Incremented reference count.
            ui32 AddRef () {
                return ++count;
            }
            /// \brief
            /// Decrement the reference count, and if 0, delete the object.
            /// \return Decremented reference count.
            ui32 Release () {
                if (count > 0) {
                    // We save count in a local variable because after
                    // Harakiri it wont be there to return.
                    ui32 newCount = --count;
                    if (newCount == 0) {
                        Harakiri ();
                    }
                    return newCount;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "AddRef/Release (%u) mismatch in %s.",
                        (ui32)count,
                        typeid (*this).name ());
                }
            }

            /// \brief
            /// Return the count of references held on this object.
            /// \return The count of references held on this object.
            ui32 GetRefCount () const {
                return count;
            }

            /// \struct RefCounted::Ptr RefCounted.h thekogans/util/RefCounted.h
            ///
            /// \brief
            /// RefCounted object smart pointer template.
            /// NOTE: Unlike other ref counted pointers, Ptr is symmetric in the
            /// way it deals with an objects reference count. It incrementes it in
            /// it's ctor, and decrements it in it's dtor. In other words, Ptr
            /// manages it's own reference, not one taken out by someone else.
            template<typename T>
            struct Ptr {
            protected:
                /// \brief
                /// Reference counted object.
                T *object;

            public:
                /// \brief
                /// \ctor.
                /// \param[in] object_ Reference counted object.
                Ptr (T *object_ = 0) :
                        object (object_) {
                    if (object != 0) {
                        object->AddRef ();
                    }
                }
                /// \brief
                /// copy ctor.
                /// \param[in] ptr Pointer to copy.
                Ptr (const Ptr &ptr) :
                        object (ptr.object) {
                    if (object != 0) {
                        object->AddRef ();
                    }
                }
                /// \brief
                /// dtor.
                ~Ptr () {
                    if (object != 0) {
                        object->Release ();
                    }
                }

                /// \brief
                /// Check the pointer for nullness.
                /// \return true if object != 0.
                explicit operator bool () const {
                    return object != 0;
                }

                /// \brief
                /// Assignemnet operator.
                /// \param[in] ptr Pointer to copy.
                /// \return *this.
                Ptr &operator = (const Ptr &ptr) {
                    if (&ptr != this) {
                        Reset (ptr.object);
                    }
                    return *this;
                }
                /// \brief
                /// Assignemnet operator.
                /// \param[in] object_ Object to assign.
                /// \return *this.
                Ptr &operator = (T *object_) {
                    Reset (object_);
                    return *this;
                }

                /// \brief
                /// Dereference operator.
                /// \return T &.
                T &operator * () const {
                    return *object;
                }

                /// \brief
                /// Dereference operator.
                /// \return T *.
                T *operator -> () const {
                    return object;
                }

                /// \brief
                /// Getter.
                /// \return T *.
                T *Get () const {
                    return object;
                }
                /// \brief
                /// Release the object without calling object->Release ().
                /// \return T *.
                T *Release () {
                    T *object_ = object;
                    object = 0;
                    return object_;
                }

                /// \brief
                /// Replace reference counted object.
                /// \param[in] object_ New object to point to.
                void Reset (T *object_ = 0) {
                    if (object != object_) {
                        if (object != 0) {
                            object->Release ();
                        }
                        object = object_;
                        if (object != 0) {
                            object->AddRef ();
                        }
                    }
                }

                /// \brief
                /// Swap objects with the given pointers.
                /// \param[in] ptr Pointer to swap objects with.
                void Swap (Ptr &ptr) {
                    std::swap (object, ptr.object);
                }
            };

        protected:
            /// \brief
            /// Default method of suicide. Derived classes can
            /// override this method to provide whatever means
            /// are appropriate for them.
            /// IMPORTANT: If you're using Ptr (above) with stack
            /// or static RefCounted, overriding this method is
            /// compulsory, as they were never 'allocated' to
            /// begin with.
            virtual void Harakiri () {
                assert (count == 0);
                delete this;
            }
        };

        /// \brief
        /// Convenient typedef for RefCounted<THEKOGANS_UTIL_ATOMIC<ui32>>.
        typedef RefCounted<THEKOGANS_UTIL_ATOMIC<ui32>> ThreadSafeRefCounted;
        /// \brief
        /// Convenient typedef for RefCounted<ui32>.
        typedef RefCounted<ui32> SingleThreadedRefCounted;

        /// \brief
        /// \see{ThreadSafeRefCounted::Ptr} static cast operator.
        /// \param[in] from Type to cast from.
        /// \return Pointer to destination type.
        template<
            typename To,
            typename From>
        inline ThreadSafeRefCounted::Ptr<To> static_refcounted_pointer_cast (
                const ThreadSafeRefCounted::Ptr<From> &from) throw () {
            return ThreadSafeRefCounted::Ptr<To> (static_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{ThreadSafeRefCounted::Ptr} dynamic cast operator.
        /// \param[in] from Type to cast from.
        /// \return Pointer to destination type.
        template<
            typename To,
            typename From>
        inline ThreadSafeRefCounted::Ptr<To> dynamic_refcounted_pointer_cast (
                const ThreadSafeRefCounted::Ptr<From> &from) throw () {
            return ThreadSafeRefCounted::Ptr<To> (dynamic_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{ThreadSafeRefCounted::Ptr} const cast operator.
        /// \param[in] from Type to cast from.
        /// \return Pointer to destination type.
        template<
            typename To,
            typename From>
        inline ThreadSafeRefCounted::Ptr<To> const_refcounted_pointer_cast (
                ThreadSafeRefCounted::Ptr<From> &from) throw () {
            return ThreadSafeRefCounted::Ptr<To> (const_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{ThreadSafeRefCounted::Ptr} reinterpret cast operator.
        /// \param[in] from Type to cast from.
        /// \return Pointer to destination type.
        template<
            typename To,
            typename From>
        inline ThreadSafeRefCounted::Ptr<To> reinterpret_refcounted_pointer_cast (
                ThreadSafeRefCounted::Ptr<From> &from) throw () {
            return ThreadSafeRefCounted::Ptr<To> (reinterpret_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{SingleThreadedRefCounted::Ptr} static cast operator.
        /// \param[in] from Type to cast from.
        /// \return Pointer to destination type.
        template<
            typename To,
            typename From>
        inline SingleThreadedRefCounted::Ptr<To> static_refcounted_pointer_cast (
                const SingleThreadedRefCounted::Ptr<From> &from) throw () {
            return SingleThreadedRefCounted::Ptr<To> (static_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{SingleThreadedRefCounted::Ptr} dynamic cast operator.
        /// \param[in] from Type to cast from.
        /// \return Pointer to destination type.
        template<
            typename To,
            typename From>
        inline SingleThreadedRefCounted::Ptr<To> dynamic_refcounted_pointer_cast (
                const SingleThreadedRefCounted::Ptr<From> &from) throw () {
            return SingleThreadedRefCounted::Ptr<To> (dynamic_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{SingleThreadedRefCounted::Ptr} const cast operator.
        /// \param[in] from Type to cast from.
        /// \return Pointer to destination type.
        template<
            typename To,
            typename From>
        inline SingleThreadedRefCounted::Ptr<To> const_refcounted_pointer_cast (
                SingleThreadedRefCounted::Ptr<From> &from) throw () {
            return SingleThreadedRefCounted::Ptr<To> (const_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{SingleThreadedRefCounted::Ptr} reinterpret cast operator.
        /// \param[in] from Type to cast from.
        /// \return Pointer to destination type.
        template<
            typename To,
            typename From>
        inline SingleThreadedRefCounted::Ptr<To> reinterpret_refcounted_pointer_cast (
                SingleThreadedRefCounted::Ptr<From> &from) throw () {
            return SingleThreadedRefCounted::Ptr<To> (reinterpret_cast<To *> (from.Get ()));
        }

        /// \brief
        /// Compare two pointers for equality.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == Same object, false == Different objects.
        template<typename T>
        inline bool operator == (
                const ThreadSafeRefCounted::Ptr<T> &item1,
                const ThreadSafeRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () == item2.Get ();
        }

        /// \brief
        /// Compare two pointers for inequality.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == Different objects, false == Same object.
        template<typename T>
        inline bool operator != (
                const ThreadSafeRefCounted::Ptr<T> &item1,
                const ThreadSafeRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () != item2.Get ();
        }

        /// \brief
        /// Compare two pointers for order.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == item1 < item2.
        template<typename T>
        inline bool operator < (
                const ThreadSafeRefCounted::Ptr<T> &item1,
                const ThreadSafeRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () < item2.Get ();
        }

        /// \brief
        /// Compare two pointers for order.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == item1 <= item2.
        template<typename T>
        inline bool operator <= (
                const ThreadSafeRefCounted::Ptr<T> &item1,
                const ThreadSafeRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () <= item2.Get ();
        }

        /// \brief
        /// Compare two pointers for order.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == item1 > item2.
        template<typename T>
        inline bool operator > (
                const ThreadSafeRefCounted::Ptr<T> &item1,
                const ThreadSafeRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () > item2.Get ();
        }

        /// \brief
        /// Compare two pointers for order.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == item1 >= item2.
        template<typename T>
        inline bool operator >= (
                const ThreadSafeRefCounted::Ptr<T> &item1,
                const ThreadSafeRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () >= item2.Get ();
        }

        /// \brief
        /// Compare two pointers for equality.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == Same object, false == Different objects.
        template<typename T>
        inline bool operator == (
                const SingleThreadedRefCounted::Ptr<T> &item1,
                const SingleThreadedRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () == item2.Get ();
        }

        /// \brief
        /// Compare two pointers for inequality.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == Different objects, false == Same object.
        template<typename T>
        inline bool operator != (
                const SingleThreadedRefCounted::Ptr<T> &item1,
                const SingleThreadedRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () != item2.Get ();
        }

        /// \brief
        /// Compare two pointers for order.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == item1 < item2.
        template<typename T>
        inline bool operator < (
                const SingleThreadedRefCounted::Ptr<T> &item1,
                const SingleThreadedRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () < item2.Get ();
        }

        /// \brief
        /// Compare two pointers for order.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == item1 <= item2.
        template<typename T>
        inline bool operator <= (
                const SingleThreadedRefCounted::Ptr<T> &item1,
                const SingleThreadedRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () <= item2.Get ();
        }

        /// \brief
        /// Compare two pointers for order.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == item1 > item2.
        template<typename T>
        inline bool operator > (
                const SingleThreadedRefCounted::Ptr<T> &item1,
                const SingleThreadedRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () > item2.Get ();
        }

        /// \brief
        /// Compare two pointers for order.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == item1 >= item2.
        template<typename T>
        inline bool operator >= (
                const SingleThreadedRefCounted::Ptr<T> &item1,
                const SingleThreadedRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () >= item2.Get ();
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RefCounted_h)
