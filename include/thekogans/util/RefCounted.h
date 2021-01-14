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

#include <typeinfo>
#include <atomic>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"

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
            /// \brief
            /// Forward declaration of WeakPtr so that we can define the list below.
            template<typename T>
            struct WeakPtr;

        private:
            /// \brief
            /// Reference count.
            Count count;
            enum {
                /// \brief
                /// WeakPtrList ID.
                WEAK_PTR_LIST_ID
            };
            /// \brief
            /// Convenient typedef for WeakPtr<RefCounted<Count>>.
            typedef WeakPtr<RefCounted<Count>> WeakPtrType;
            /// \brief
            /// Convenient typedef for IntrusiveList<WeakPtrType, WEAK_PTR_LIST_ID>.
            typedef IntrusiveList<WeakPtrType, WEAK_PTR_LIST_ID> WeakPtrList;
            /// \brief
            /// List of weak pointers.
            WeakPtrList weakPtrList;
            /// \brief
            /// Access to weakPtrList needs to be protected.
            SpinLock spinLock;

        public:
            /// \brief
            /// ctor.
            RefCounted () :
                count (0) {}
            /// \brief
            /// dtor.
            virtual ~RefCounted () {
                // We're going out of scope. If there are still
                // references remaining, we have a problem.
                if (count != 0) {
                    // Here we both log the problem and assert to give the
                    // engineer the best chance of figuring out what happened.
                    std::string message =
                        FormatString ("%s : %u\n", typeid (*this).name (), (ui32)count);
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
                    THEKOGANS_UTIL_ASSERT (count == 0, message);
                }
                // Invalidate all weak pointers.
                // WARNING: There exists a subtle race between the
                // dtor invalidating the weak pointers and the
                // \see{WeakPtr::Reset} below. Imagine a thread
                // releases the last reference on this object and
                // triggering \see{Harakiri} and eventually the call
                // in to the dtor. While the dtor is invalidating it's
                // list of weak pointers another thread decides to
                // copy one of the weak pointers associated with this
                // object. That weak pointer will either crash for trying
                // to insert itself in to a deleted object or, will now
                // hold on to a dangling raw pointer to this object.
                struct CleaarObjectCallback : public WeakPtrList::Callback {
                    virtual typename WeakPtrList::Callback::result_type operator () (
                            typename WeakPtrList::Callback::argument_type weakPtr) {
                        assert (weakPtr != 0);
                        weakPtr->Reset ();
                        return true;
                    }
                } clearObjectCallback;
                LockGuard<SpinLock> guard (spinLock);
                weakPtrList.clear (clearObjectCallback);
            }

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
                        "AddRef/Release mismatch in %s.",
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
            /// way it deals with an object's reference count. It incrementes it
            /// in it's ctor, and decrements it in it's dtor. In other words, Ptr
            /// manages it's own reference, not one taken out by someone else.
            /// This behavior can be modified by passing addRef = false to the
            /// ctor. This way Ptr can take ownership of an object Release(d) by
            /// another.
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
                /// \param[in] addRef true == take out a new reference on the passed in object,
                /// false == this object was probably Release(d) by another Ptr.
                Ptr (
                        T *object_ = 0,
                        bool addRef = true) :
                        object (0) {
                    Reset (object_, addRef);
                }
                /// \brief
                /// copy ctor.
                /// \param[in] ptr Pointer to copy.
                Ptr (const Ptr &ptr) :
                        object (0) {
                    Reset (ptr.object);
                }
                /// \brief
                /// dtor.
                ~Ptr () {
                    Reset ();
                }

                /// \brief
                /// Check the pointer for nullness.
                /// \return true if object != 0.
                operator bool () const {
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
                /// \param[in] addRef true == take out a new reference on the passed in object,
                /// false == this object was probably Release(d) by another Ptr.
                void Reset (
                        T *object_ = 0,
                        bool addRef = true) {
                    if (object != object_) {
                        if (object != 0) {
                            object->Release ();
                        }
                        object = object_;
                        if (object != 0 && addRef) {
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

            /// \struct RefCounted::WeakPtr RefCounted.h thekogans/util/RefCounted.h
            ///
            /// \brief
            /// A weak pointer template allowing the user to hold on to a 'smart' raw
            /// pointer to the object. In order to use the object encapsulated by this pointer
            /// you must first call WeakPtr::GetPtr to get a ref counted smart pointer.
            template<typename T>
            struct WeakPtr : public WeakPtrList::Node {
            protected:
                /// \brief
                /// Referemce counted object.
                T *object;

            public:
                /// \brief
                /// ctor.
                /// \param[in] object_ Ptr to referemce counted object.
                WeakPtr (Ptr<T> object_ = Ptr<T> ()) :
                        object (0) {
                    Reset (object_);
                }
                /// \brief
                /// ctor.
                /// \param[in] object_ WeakPtr to referemce counted object.
                WeakPtr (const WeakPtr &object_ = WeakPtr ()) :
                        object (0) {
                    Reset (object_.GetPtr ());
                }
                /// \brief
                /// dtor.
                ~WeakPtr () {
                    Reset ();
                }

                /// \brief
                /// Check the pointer for nullness.
                /// \return true if object != 0.
                operator bool () const {
                    return object != 0;
                }

                /// \brief
                /// Assignemnet operator.
                /// \param[in] ptr Pointer to copy.
                /// \return *this.
                WeakPtr &operator = (Ptr<T> ptr) {
                    if (ptr.Get () != object) {
                        Reset (ptr);
                    }
                    return *this;
                }

                /// \brief
                /// Assignemnet operator.
                /// \param[in] ptr Pointer to copy.
                /// \return *this.
                WeakPtr &operator = (const WeakPtr &ptr) {
                    if (ptr.Get () != object) {
                        Reset (ptr.GetPtr ());
                    }
                    return *this;
                }

                /// \brief
                /// Getter.
                /// \return T *.
                T *Get () const {
                    return object;
                }

                /// \brief
                /// Replace reference counted object.
                /// \param[in] object_ New object to point to.
                void Reset (Ptr<T> object_ = Ptr<T> ()) {
                    if (object != object_.Get ()) {
                        if (object != 0) {
                            object->RemoveWeakPtr ((WeakPtrType *)this);
                        }
                        object = object_.Get ();
                        if (object != 0) {
                            object->AddWeakPtr ((WeakPtrType *)this);
                        }
                    }
                }

                /// \brief
                /// Convert to Ptr<T>.
                /// \return Ptr<T>.
                Ptr<T> GetPtr () const {
                    return Ptr<T> (object);
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
                // We're going out of scope. If there are still
                // references remaining, we have a problem.
                if (count != 0) {
                    // Here we both log the problem and assert to give the
                    // engineer the best chance of figuring out what happened.
                    std::string message =
                        FormatString ("%s : %u", typeid (*this).name (), (ui32)count);
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
                    THEKOGANS_UTIL_ASSERT (count == 0, message);
                }
                delete this;
            }

            /// \brief
            /// Used by WeakPtr above to add itself to the RefCounted::WeakPtrList.
            /// \param[in] weakPtr WeakPtr to add to the list.
            inline void AddWeakPtr (WeakPtrType *weakPtr) {
                LockGuard<SpinLock> guard (spinLock);
                weakPtrList.push_back (weakPtr);
            }
            /// \brief
            /// Used by WeakPtr above to remove itself from the RefCounted::WeakPtrList.
            /// \param[in] weakPtr WeakPtr to remove from the list.
            inline void RemoveWeakPtr (WeakPtrType *weakPtr) {
                LockGuard<SpinLock> guard (spinLock);
                weakPtrList.push_back (weakPtr);
            }
        };

        /// \brief
        /// Convenient typedef for RefCounted<std::atomic<ui32>>.
        typedef RefCounted<std::atomic<ui32>> ThreadSafeRefCounted;
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
        inline ThreadSafeRefCounted::Ptr<To> _LIB_THEKOGANS_UTIL_API static_refcounted_pointer_cast (
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
        inline ThreadSafeRefCounted::Ptr<To> _LIB_THEKOGANS_UTIL_API dynamic_refcounted_pointer_cast (
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
        inline ThreadSafeRefCounted::Ptr<To> _LIB_THEKOGANS_UTIL_API const_refcounted_pointer_cast (
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
        inline ThreadSafeRefCounted::Ptr<To> _LIB_THEKOGANS_UTIL_API reinterpret_refcounted_pointer_cast (
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
        inline SingleThreadedRefCounted::Ptr<To> _LIB_THEKOGANS_UTIL_API static_refcounted_pointer_cast (
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
        inline SingleThreadedRefCounted::Ptr<To> _LIB_THEKOGANS_UTIL_API dynamic_refcounted_pointer_cast (
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
        inline SingleThreadedRefCounted::Ptr<To> _LIB_THEKOGANS_UTIL_API const_refcounted_pointer_cast (
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
        inline SingleThreadedRefCounted::Ptr<To> _LIB_THEKOGANS_UTIL_API reinterpret_refcounted_pointer_cast (
                SingleThreadedRefCounted::Ptr<From> &from) throw () {
            return SingleThreadedRefCounted::Ptr<To> (reinterpret_cast<To *> (from.Get ()));
        }

        /// \brief
        /// Compare two pointers for equality.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == Same object, false == Different objects.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator < (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator <= (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator > (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator >= (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator < (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator <= (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator > (
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
        inline bool _LIB_THEKOGANS_UTIL_API operator >= (
                const SingleThreadedRefCounted::Ptr<T> &item1,
                const SingleThreadedRefCounted::Ptr<T> &item2) throw () {
            return item1.Get () >= item2.Get ();
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RefCounted_h)
