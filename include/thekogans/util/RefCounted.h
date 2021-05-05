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

#include <memory>
#include <typeinfo>
#include <atomic>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        /// \struct RefCounted RefCounted.h thekogans/util/RefCounted.h
        ///
        /// \brief
        /// RefCounted is a base class for reference counted objects.
        /// It's designed to be useful for objects that are heap as well
        /// as stack or statically allocated. On construction, the shared
        /// reference count is set to 0. Use RefCounted::SharedPtr to deal
        /// with heap object lifetimes. Unlike more complicated reference
        /// count classes, I purposely designed this one not to deal with
        /// polymorphism and construction and destruction issues. The default
        /// behavior is: All RefCounted objects that are allocated on the
        /// heap will be allocated with new, and destroyed with delete. I
        /// provide a virtual void Harakiri () to give class designers finer
        /// control over the lifetime management of their classes. If you
        /// need more control over heap placement, that's what \see{Heap}
        /// is for.
        /// NOTE: When inheriting from RefCounted, consider making it 'public virtual'.
        /// This will go a long way towards resolving multiple inheritance ambiguities.
        /// VERY IMPORTANT: If you're going to create RefCounted::SharedPtrs on
        /// stack or static (see \see{Singleton}) objects, it's important that the
        /// object itself take out a reference in it's ctor. This way the reference
        /// count will never reach 0 and the object will not try to delete itself.
        /// For \see{Singleton}s, I provide \see{RefCountedInstanceCreator} and
        /// \see{RefCountedInstanceDestroyer} that will do just that.
        ///
        /// VERY IMPORTANT: Please read the question and understand the accepted answer in the following;
        /// https://stackoverflow.com/questions/35314297/mixing-virtual-and-non-virtual-inheritance-of-a-base-class
        /// Basically, to summarize it here; If you're going to directly inherit
        /// from classes like RefCounted, and you expect your classes to be
        /// inherited from, than your classes' inheritance must be virtual
        /// to ward off the dreaded diamond pattern that can result from
        /// multiple inheritance.

        struct _LIB_THEKOGANS_UTIL_DECL RefCounted {
        private:
            /// \struct RefCounted::References RefCounted.h thekogans/util/RefCounted.h
            ///
            /// \brief
            /// Control block for the lifetime of RefCounted as well as \see{WeakPtr}.
            struct _LIB_THEKOGANS_UTIL_DECL References {
                /// \brief
                /// References has a private heap to help with performance and memory fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (References, SpinLock)

            private:
                /// \brief
                /// Count of weak references.
                std::atomic<ui32> weak;
                /// \brief
                /// Count of shared references.
                std::atomic<ui32> shared;

            public:
                /// \brief
                /// ctor.
                References () :
                    weak (1),
                    shared (0) {}

                /// \brief
                /// Increment the weak reference count.
                /// \return Incremented weak reference count.
                inline ui32 AddWeakRef () {
                    return ++weak;
                }
                /// \brief
                /// Decrement the weak reference count, and if 0, call delete.
                /// \return Decremented weak reference count.
                ui32 ReleaseWeakRef ();
                /// \brief
                /// Return the count of weak references held.
                /// \return Count of weak references held.
                inline ui32 GetWeakCount () const {
                    return weak;
                }

                /// \brief
                /// Increment the shared reference count.
                /// \return Incremented shared reference count.
                inline ui32 AddSharedRef () {
                    return ++shared;
                }
                /// \brief
                /// Decrement the shared reference count, and if 0, call object->Harakiri ().
                /// \return Decremented shared reference count.
                ui32 ReleaseSharedRef (RefCounted *object);
                /// \brief
                /// Return the count of shared references held.
                /// \return Count of shared references held.
                inline ui32 GetSharedCount () const {
                    return shared;
                }
                /// \brief
                /// Used by \see{WeakPtr<T>::GetSharedPtr} below to atomically
                /// take out a shared reference on a weak pointer.
                /// \return true == object was successfuly locked,
                /// false == there are no more shared references (shared == 0).
                bool LockObject ();

                /// \brief
                /// References is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (References)
            } *references;

        public:
            /// \brief
            /// ctor.
            RefCounted () :
                references (new References) {}
            /// \brief
            /// dtor.
            virtual ~RefCounted ();

            /// \brief
            /// Increment the shared reference count.
            /// \return Incremented shared reference count.
            inline ui32 AddRef () {
                return references->AddSharedRef ();
            }
            /// \brief
            /// Decrement the shared reference count, and if 0, call Harakiri.
            /// \return Decremented shared reference count.
            inline ui32 Release () {
                return references->ReleaseSharedRef (this);
            }
            /// \brief
            /// Return the count of shared references held on this object.
            /// \return The count of shared references held on this object.
            inline ui32 GetRefCount () const {
                return references->GetSharedCount ();
            }

            /// \brief
            /// This function template allows you to coexist with libraries that use
            /// std::shared_ptr.
            /// \return std::shared_ptr<T>.
            template<typename T>
            std::shared_ptr<T> CreateStdSharedPtr () {
                AddRef ();
                struct Deleter {
                    void operator () (T *object) const {
                        if (object != 0) {
                            object->Release ();
                        }
                    }
                };
                return std::shared_ptr<T> (this, Deleter ());
            }

            /// \struct RefCounted::SharedPtr RefCounted.h thekogans/util/RefCounted.h
            ///
            /// \brief
            /// RefCounted object shared pointer template.
            /// NOTE: Unlike other ref counted pointers, SharedPtr is symmetric in the
            /// way it deals with an object's shared reference count. It incrementes it
            /// in it's ctor, and decrements it in it's dtor. In other words, SharedPtr
            /// manages it's own reference, not one taken out by someone else. This behavior
            /// can be modified by passing addRef = false to the ctor. This way SharedPtr
            /// can take ownership of an object Released by another.
            template<typename T>
            struct SharedPtr {
                /// \brief
                /// There are many times where we interact with system APIs that take a void *userData.
                /// In a multi-threaded environment passing a raw pointer is dangerous as it can lead
                /// to dangling pointers which lead to crashes. Use this patern instead to deal with
                /// object lifetimes;
                ///
                /// \code{.cpp}
                /// struct foo : public thekogans::util::RefCounted {
                ///     typedef thekogans::util::RefCounted::SharedPtr<foo> SharedPtr;
                ///     ...
                ///     void bar ();
                /// };
                ///
                /// foo::SharedPtr::UniquePtr fooPtr (new foo::SharedPtr (&fooInstance));
                /// if (SomeSystemAPI (..., fooPtr.get ())) {
                ///     fooPtr.release ();
                /// }
                ///
                /// // If SomeSystemAPI fails, the shared pointer will be deleted when fooPtr dtor
                /// // is called. When the system calls you back with the userData pointer you passed
                /// // in above, do the following;
                ///
                /// void MyCallback (..., void *userData) {
                ///     foo::SharedPtr::UniquePtr fooPtr ((foo::SharedPtr *)userData);
                ///     (*fooPtr)->bar ();
                /// }
                /// \endcode
                typedef std::unique_ptr<SharedPtr<T>> UniquePtr;

            protected:
                /// \brief
                /// Reference counted object.
                T *object;

            public:
                /// \brief
                /// \ctor.
                /// \param[in] object_ Reference counted object.
                /// \param[in] addRef true == take out a new reference on the passed in object,
                /// false == this object was probably Release(d) by another SharedPtr.
                SharedPtr (
                        T *object_ = 0,
                        bool addRef = true) :
                        object (0) {
                    Reset (object_, addRef);
                }
                /// \brief
                /// copy ctor.
                /// \param[in] ptr Pointer to copy.
                SharedPtr (const SharedPtr &ptr) :
                        object (0) {
                    Reset (ptr.object);
                }
                /// \brief
                /// dtor.
                ~SharedPtr () {
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
                SharedPtr &operator = (const SharedPtr &ptr) {
                    if (&ptr != this) {
                        Reset (ptr.object);
                    }
                    return *this;
                }
                /// \brief
                /// Assignemnet operator.
                /// \param[in] object_ Object to assign.
                /// \return *this.
                SharedPtr &operator = (T *object_) {
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
                /// false == this object was probably Release(d) by another SharedPtr.
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
                /// Swap objects with the given pointer.
                /// \param[in] ptr Pointer to swap objects with.
                void Swap (SharedPtr &ptr) {
                    std::swap (object, ptr.object);
                }
            };

            /// \struct RefCounted::WeakPtr RefCounted.h thekogans/util/RefCounted.h
            ///
            /// \brief
            /// RefCounted object weak pointer template.
            /// A weak pointer template allows the user to hold on to a 'smart' raw
            /// pointer to the object. What that means in practice is that the RefCounted
            /// object the WeakPtr points to can be deleted and it will not result in a
            /// dangling pointer. Weak pointers are used to break mutual reference cycles
            /// that can occur when two objects hold on to a shared pointer to one another.
            /// In order to use the object encapsulated by the WeakPtr you must first call
            /// WeakPtr<T>::GetSharedPtr to get a SharedPtr<T>.
            /// NOTE: The SharedPtr<T> returned by WeakPtr<T>::GetSharedPtr can be null.
            /// That's why it's important that you use the following pattern;
            ///
            /// \code{.cpp}
            /// SharedPtr<T> shared = weak.GetSharedPtr ();
            /// if (shared.Get () != 0) {
            ///    // Do something productive with shared.
            /// }
            /// \endcode
            ///
            /// IMPORTANT: The Get method provided by this class should not be used to
            /// dereference the contained object. It's there in case you want to compare
            /// two raw pointers. Dereferencing the raw pointer returned by Get can lead
            /// to races and crashes. If you need to dereference the object poonted to
            /// by the raw pointer, again, you must first call GetSharedPtr and check it's
            /// return value for nullness before using it.
            template<typename T>
            struct WeakPtr {
                /// \brief
                /// There are many times where we interact with system APIs that take a void *userData.
                /// In a multi-threaded environment passing a raw pointer is dangerous as it can lead
                /// to dangling pointers which lead to crashes. Use this patern instead to deal with
                /// object lifetimes;
                ///
                /// \code{.cpp}
                /// struct foo : public thekogans::util::RefCounted {
                ///     THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (foo)
                ///     ...
                ///     void bar ();
                /// };
                ///
                /// foo::WeakPtr::UniquePtr fooPtr (new foo::WeakPtr (&fooInstance));
                /// if (SomeSystemAPI (..., fooPtr.get ())) {
                ///     fooPtr.release ();
                /// }
                ///
                /// // If SomeSystemAPI fails, the weak pointer will be deleted when fooPtr dtor
                /// // is called. When the system calls you back with the userData pointer you passed
                /// // in above, do the following;
                ///
                /// void MyCallback (..., void *userData) {
                ///     foo::WeakPtr::UniquePtr weakPtr ((foo::WeakPtr *)userData);
                ///     foo::SharedPtr fooPtr = weakPtr->GetSharedPtr ();
                ///     // If the object is stil alive, use it.
                ///     if (fooPtr.Get () != 0) {
                ///         fooPtr->bar ();
                ///     }
                /// }
                /// \endcode
                typedef std::unique_ptr<WeakPtr<T>> UniquePtr;

            protected:
                /// \brief
                /// Reference counted object.
                T *object;
                /// \brief
                /// WeakPtr takes a reference out on RefCounted::References
                /// and must hang on to it's own copy of the pointer because
                /// we cannot relly on the object being there for dereferencing.
                References *references;

            public:
                /// \brief
                /// ctor.
                /// \paramin] object_ Raw pointer to RefCounted object.
                explicit WeakPtr (T *object_ = 0) :
                        object (0),
                        references (0) {
                    Reset (object_);
                }
                /// \brief
                /// ctor.
                /// \param[in] ptr SharedPtr<T> to reference counted object.
                explicit WeakPtr (const SharedPtr<T> &ptr) :
                        object (0),
                        references (0) {
                    Reset (ptr.Get ());
                }
                /// \brief
                /// ctor.
                /// \param[in] ptr WeakPtr<T> to reference counted object.
                WeakPtr (const WeakPtr &ptr) :
                        object (0),
                        references (0) {
                    Reset (ptr.GetSharedPtr ().Get ());
                }
                /// \brief
                /// dtor.
                ~WeakPtr () {
                    Reset ();
                }

                /// \brief
                /// Assignment operator.
                /// \param[in] object_ Raw pointer to reference counted object.
                /// \return *this.
                WeakPtr &operator = (T *object_) {
                    if (object != object_) {
                        Reset (object_);
                    }
                    return *this;
                }
                /// \brief
                /// Assignment operator.
                /// \param[in] ptr SharedPtr<T> to reference counted object.
                /// \return *this.
                WeakPtr &operator = (const SharedPtr<T> &ptr) {
                    if (object != ptr.Get ()) {
                        Reset (ptr.Get ());
                    }
                    return *this;
                }
                /// \brief
                /// Assignment operator.
                /// \param[in] ptr WeakPtr<T> to reference counted object.
                /// \return *this.
                WeakPtr &operator = (const WeakPtr<T> &ptr) {
                    if (object != ptr.Get ()) {
                        Reset (ptr.GetSharedPtr ().Get ());
                    }
                    return *this;
                }

                /// \brief
                /// Return a raw pointer to the reference counted object.
                /// \return A raw pointer to the reference counted object.
                /// IMPORTANT: You cannot pass on object from one WeakPtr::Get
                /// to another WeakPtr::ctor or WeakPtr::Reset. There's no
                /// guarantee that the pointer you get is still dereferencable.
                /// It's very important that you practice safe WeakPtr access
                /// method by first calling WeakPtr::GetSharedPtr below, checking
                /// if it's not null and only then using it.
                inline T *Get () const {
                    return object;
                }

                /// \brief
                /// Reset the object htis WeakPtr points to.
                /// \param[in] object_ New object to encapsulate.
                /// WARNING: You can't pass an object pointer from one WeakPtr::Get
                /// to another WeakPtr::Reset. There is no guarantee that there
                /// are any more shared references left on the object and that
                /// it's not dangling.
                void Reset (T *object_ = 0) {
                    if (object != object_) {
                        object = object_;
                        if (references != 0) {
                            references->ReleaseWeakRef ();
                        }
                        references = object != 0 ? object->references : 0;
                        if (references != 0) {
                            references->AddWeakRef ();
                        }
                    }
                }

                /// \brief
                /// Swap the contents of this WeakPtr with the one given.
                /// \param[in,out] ptr WeakPtr with which to swap contents.
                void Swap (WeakPtr<T> &ptr) {
                    std::swap (object, ptr.object);
                    std::swap (references, ptr.references);
                }

                /// \brief
                /// Return the count of shared references on the contained object.
                /// \return Count of shared references on the contained object.
                inline ui32 SharedCount () const {
                    return references != 0 ? references->GetSharedCount () : 0;
                }

                /// \brief
                /// Return true if the contained object has expired (deleted).
                /// \return true == the contained object has expired (deleted).
                inline bool IsExpired () const {
                    return references == 0 || references->GetSharedCount () == 0;
                }

                /// \brief
                /// Convert to SharedPtr<T>.
                /// \return SharedPtr<T>.
                inline SharedPtr<T> GetSharedPtr () const {
                    // We pass false to SharedPtr<T> (..., addRef) because References::LockObject
                    // takes out a reference on success and on failure we don't care since we're
                    // passing 0 as object pointer.
                    return SharedPtr<T> (
                        references != 0 && references->LockObject () ? object : 0,
                        false);
                }
            };

        protected:
            /// \brief
            /// Default method of suicide. Derived classes can
            /// override this method to provide whatever means
            /// are appropriate for them.
            /// IMPORTANT: If you're using SharedPtr (above) with stack
            /// or static RefCounted, overriding this method is
            /// compulsory, as they were never 'allocated' to
            /// begin with. In that case consider using \see{Singleton}
            /// with \see{RefCountedInstanceCreator} and \see{RefCountedInstanceDestroyer}.
            virtual void Harakiri () {
                delete this;
            }
        };

        /// \def THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS(type)
        /// Use this macro inside a \see{RefCounted} derived class to declare the pointers.
        #define THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS(type)\
            typedef thekogans::util::RefCounted::SharedPtr<type> SharedPtr;\
            typedef thekogans::util::RefCounted::WeakPtr<type> WeakPtr;

        /// \brief
        /// \see{RefCounted::SharedPtr} static cast operator.
        /// \param[in] from Type to cast from.
        /// \return \see{RefCounted::SharedPtr} to destination type.
        template<
            typename To,
            typename From>
        inline RefCounted::SharedPtr<To> _LIB_THEKOGANS_UTIL_API static_refcounted_sharedptr_cast (
                const RefCounted::SharedPtr<From> &from) throw () {
            return RefCounted::SharedPtr<To> (static_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{RefCounted::SharedPtr} dynamic cast operator.
        /// \param[in] from Type to cast from.
        /// \return \see{RefCounted::SharedPtr} to destination type.
        template<
            typename To,
            typename From>
        inline RefCounted::SharedPtr<To> _LIB_THEKOGANS_UTIL_API dynamic_refcounted_sharedptr_cast (
                const RefCounted::SharedPtr<From> &from) throw () {
            return RefCounted::SharedPtr<To> (dynamic_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{RefCounted::SharedPtr} const cast operator.
        /// \param[in] from Type to cast from.
        /// \return \see{RefCounted::SharedPtr} to destination type.
        template<
            typename To,
            typename From>
        inline RefCounted::SharedPtr<To> _LIB_THEKOGANS_UTIL_API const_refcounted_sharedptr_cast (
                RefCounted::SharedPtr<From> &from) throw () {
            return RefCounted::SharedPtr<To> (const_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{RefCounted::SharedPtr} reinterpret cast operator.
        /// \param[in] from Type to cast from.
        /// \return \see{RefCounted::SharedPtr} to destination type.
        template<
            typename To,
            typename From>
        inline RefCounted::SharedPtr<To> _LIB_THEKOGANS_UTIL_API reinterpret_refcounted_sharedptr_cast (
                RefCounted::SharedPtr<From> &from) throw () {
            return RefCounted::SharedPtr<To> (reinterpret_cast<To *> (from.Get ()));
        }

        /// \brief
        /// Compare two \see{RefCounted::SharedPtr}s for equality.
        /// \param[in] item1 First \see{RefCounted::SharedPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::SharedPtr} to compare.
        /// \return true == Same object, false == Different objects.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
                const RefCounted::SharedPtr<T> &item1,
                const RefCounted::SharedPtr<T> &item2) throw () {
            return item1.Get () == item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::SharedPtr}s for inequality.
        /// \param[in] item1 First \see{RefCounted::SharedPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::SharedPtr} to compare.
        /// \return true == Different objects, false == Same object.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
                const RefCounted::SharedPtr<T> &item1,
                const RefCounted::SharedPtr<T> &item2) throw () {
            return item1.Get () != item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::SharedPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::SharedPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::SharedPtr} to compare.
        /// \return true == item1 < item2.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator < (
                const RefCounted::SharedPtr<T> &item1,
                const RefCounted::SharedPtr<T> &item2) throw () {
            return item1.Get () < item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::SharedPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::SharedPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::SharedPtr} to compare.
        /// \return true == item1 <= item2.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator <= (
                const RefCounted::SharedPtr<T> &item1,
                const RefCounted::SharedPtr<T> &item2) throw () {
            return item1.Get () <= item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::SharedPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::SharedPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::SharedPtr} to compare.
        /// \return true == item1 > item2.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator > (
                const RefCounted::SharedPtr<T> &item1,
                const RefCounted::SharedPtr<T> &item2) throw () {
            return item1.Get () > item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::SharedPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::SharedPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::SharedPtr} to compare.
        /// \return true == item1 >= item2.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator >= (
                const RefCounted::SharedPtr<T> &item1,
                const RefCounted::SharedPtr<T> &item2) throw () {
            return item1.Get () >= item2.Get ();
        }

        /// \brief
        /// \see{RefCounted::WeakPtr} static cast operator.
        /// \param[in] from Type to cast from.
        /// \return \see{RefCounted::WeakPtr} to destination type.
        template<
            typename To,
            typename From>
        inline RefCounted::WeakPtr<To> _LIB_THEKOGANS_UTIL_API static_refcounted_weakptr_cast (
                const RefCounted::WeakPtr<From> &from) throw () {
            return RefCounted::WeakPtr<To> (static_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{RefCounted::WeakPtr} dynamic cast operator.
        /// \param[in] from Type to cast from.
        /// \return \see{RefCounted::WeakPtr} to destination type.
        template<
            typename To,
            typename From>
        inline RefCounted::WeakPtr<To> _LIB_THEKOGANS_UTIL_API dynamic_refcounted_weakptr_cast (
                const RefCounted::WeakPtr<From> &from) throw () {
            return RefCounted::WeakPtr<To> (dynamic_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{RefCounted::WeakPtr} const cast operator.
        /// \param[in] from Type to cast from.
        /// \return \see{RefCounted::WeakPtr} to destination type.
        template<
            typename To,
            typename From>
        inline RefCounted::WeakPtr<To> _LIB_THEKOGANS_UTIL_API const_refcounted_weakptr_cast (
                RefCounted::WeakPtr<From> &from) throw () {
            return RefCounted::WeakPtr<To> (const_cast<To *> (from.Get ()));
        }

        /// \brief
        /// \see{RefCounted::WeakPtr} reinterpret cast operator.
        /// \param[in] from Type to cast from.
        /// \return \see{RefCounted::WeakPtr} to destination type.
        template<
            typename To,
            typename From>
        inline RefCounted::WeakPtr<To> _LIB_THEKOGANS_UTIL_API reinterpret_refcounted_weakptr_cast (
                RefCounted::WeakPtr<From> &from) throw () {
            return RefCounted::WeakPtr<To> (reinterpret_cast<To *> (from.Get ()));
        }

        /// \brief
        /// Compare two \see{RefCounted::WeakPtr}s for equality.
        /// \param[in] item1 First \see{RefCounted::WeakPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::WeakPtr} to compare.
        /// \return true == Same object, false == Different objects.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
                const RefCounted::WeakPtr<T> &item1,
                const RefCounted::WeakPtr<T> &item2) throw () {
            return item1.Get () == item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::WeakPtr}s for inequality.
        /// \param[in] item1 First \see{RefCounted::WeakPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::WeakPtr} to compare.
        /// \return true == Different objects, false == Same object.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
                const RefCounted::WeakPtr<T> &item1,
                const RefCounted::WeakPtr<T> &item2) throw () {
            return item1.Get () != item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::WeakPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::WeakPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::WeakPtr} to compare.
        /// \return true == item1 < item2.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator < (
                const RefCounted::WeakPtr<T> &item1,
                const RefCounted::WeakPtr<T> &item2) throw () {
            return item1.Get () < item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::WeakPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::WeakPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::WeakPtr} to compare.
        /// \return true == item1 <= item2.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator <= (
                const RefCounted::WeakPtr<T> &item1,
                const RefCounted::WeakPtr<T> &item2) throw () {
            return item1.Get () <= item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::WeakPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::WeakPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::WeakPtr} to compare.
        /// \return true == item1 > item2.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator > (
                const RefCounted::WeakPtr<T> &item1,
                const RefCounted::WeakPtr<T> &item2) throw () {
            return item1.Get () > item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::WeakPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::WeakPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::WeakPtr} to compare.
        /// \return true == item1 >= item2.
        template<typename T>
        inline bool _LIB_THEKOGANS_UTIL_API operator >= (
                const RefCounted::WeakPtr<T> &item1,
                const RefCounted::WeakPtr<T> &item2) throw () {
            return item1.Get () >= item2.Get ();
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RefCounted_h)
