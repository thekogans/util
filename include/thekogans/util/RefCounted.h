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

#include <utility>
#include <memory>
#include <typeinfo>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"

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
        ///
        /// NOTE: When inheriting from RefCounted, consider making it 'public virtual'.
        /// This will go a long way towards resolving multiple inheritance ambiguities.
        ///
        /// VERY IMPORTANT: If you're going to create RefCounted::SharedPtrs on
        /// stack for static (see \see{Singleton}) objects, it's important that the
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
                ui32 weak;
                /// \brief
                /// Count of shared references.
                ui32 shared;

            public:
                /// \brief
                /// ctor.
                References () :
                    weak (1),
                    shared (0) {}

                /// \brief
                /// Increment the weak reference count.
                /// \return Incremented weak reference count.
                ui32 AddWeakRef ();
                /// \brief
                /// Decrement the weak reference count, and if 0, call delete.
                /// \return Decremented weak reference count.
                ui32 ReleaseWeakRef ();
                /// \brief
                /// Return the count of weak references held.
                /// \return Count of weak references held.
                ui32 GetWeakCount () const;

                /// \brief
                /// Increment the shared reference count.
                /// \return Incremented shared reference count.
                ui32 AddSharedRef ();
                /// \brief
                /// Decrement the shared reference count, and if 0, call object->Harakiri ().
                /// \return Decremented shared reference count.
                ui32 ReleaseSharedRef (RefCounted *object);
                /// \brief
                /// Return the count of shared references held.
                /// \return Count of shared references held.
                ui32 GetSharedCount () const;
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
            ///
            /// NOTE: Unlike other ref counted pointers, SharedPtr is symmetric in the
            /// way it deals with an object's shared reference count. It incrementes it
            /// in it's ctor, and decrements it in it's dtor. In other words, SharedPtr
            /// manages it's own reference, not one taken out by someone else. This behavior
            /// can be modified by passing addRef = false to the ctor. This way SharedPtr
            /// can take ownership of an object Released by another.
            template<typename T>
            struct SharedPtr {
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
                SharedPtr (const SharedPtr<T> &ptr) :
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
                SharedPtr<T> &operator = (const SharedPtr<T> &ptr) {
                    if (&ptr != this) {
                        Reset (ptr.object);
                    }
                    return *this;
                }
                /// \brief
                /// Assignemnet operator.
                /// \param[in] object_ Object to assign.
                /// \return *this.
                SharedPtr<T> &operator = (T *object_) {
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
                void Swap (SharedPtr<T> &ptr) {
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
            ///
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
            /// to races and crashes. If you need to dereference the object pointed to
            /// by the raw pointer, again, you must first call GetSharedPtr and check it's
            /// return value for nullness before using it.
            template<typename T>
            struct WeakPtr {
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
                WeakPtr (const WeakPtr<T> &ptr) :
                        object (ptr.object),
                        references (ptr.references) {
                    if (references != 0) {
                        references->AddWeakRef ();
                    }
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
                WeakPtr<T> &operator = (T *object_) {
                    if (object != object_) {
                        Reset (object_);
                    }
                    return *this;
                }
                /// \brief
                /// Assignment operator.
                /// \param[in] ptr SharedPtr<T> to reference counted object.
                /// \return *this.
                WeakPtr<T> &operator = (const SharedPtr<T> &ptr) {
                    if (object != ptr.Get ()) {
                        Reset (ptr.Get ());
                    }
                    return *this;
                }
                /// \brief
                /// Assignment operator.
                /// \param[in] ptr WeakPtr<T> to reference counted object.
                /// \return *this.
                WeakPtr<T> &operator = (const WeakPtr<T> &ptr) {
                    if (object != ptr.Get ()) {
                        if (references != 0) {
                            references->ReleaseWeakRef ();
                        }
                        object = ptr.object;
                        references = ptr.references;
                        if (references != 0) {
                            references->AddWeakRef ();
                        }
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
                /// Reset the object this WeakPtr points to.
                /// \param[in] object_ New object to encapsulate.
                /// WARNING: You can't pass an object pointer from one WeakPtr::Get
                /// to another WeakPtr::Reset. There is no guarantee that there
                /// are any more shared references left on the object and that
                /// it's not dangling.
                void Reset (T *object_ = 0) {
                    if (object != object_) {
                        if (references != 0) {
                            references->ReleaseWeakRef ();
                        }
                        object = object_;
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

            /// \struct RefCounted::Registry RefCounted.h thekogans/util/RefCounted.h
            ///
            /// \brief
            /// Registry acts as an interface between RefCounted objects in the C++
            /// world and raw pointers of the OS C APIs. It is specifically designed to
            /// make life easier managing object lifetimes in the presence of async
            /// callback interfaces. Most OS APIs that take a function pointer to
            /// call back at a later (async) time also take a user parameter to pass
            /// to the callback. Passing in raw pointers is dangerous as object
            /// lifetime cannot be predicted and dereferencing raw pointers can lead
            /// to disasters. Instead, use the registry to 'register' a WeakPtr to
            /// the object that can later be used to create a SharedPtr if the object
            /// still exists.
            template<typename T>
            struct Registry : public Singleton<Registry<T>, SpinLock> {
            public:
                enum {
                    /// \brief
                    /// Default initial entry list size.
                    DEFAULT_ENTRIES_SIZE = 1024,
                    /// \brief
                    /// Bit pattern for an invalid token.
                    INVALID_TOKEN = NIDX64
                };

                /// \brief
                /// Encapsulates the ui64 token consisting of {index, counter} pair. The
                /// index part is the index used to access the registration in the entries
                /// vector. The counter acts to disembiguate the various entry lifetimes.
                /// Entries are recycled and have to be protected from eronious latent
                /// access that will result in serious damage (major security risks).
                /// Therefore as soon as an entry is released it's counter is bumped up to
                /// notify the old token that it is no longer valid.
                ///
                /// Provides convenient access as well as registration lifetime management.
                ///
                /// Ex:
                /// \code{.cpp}
                /// typedef thekogans::util::RefCounted::Registry<foo> fooRegistry;
                ///
                /// struct foo : public thekogans::util::RefCounted {
                ///     THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (foo)
                ///
                /// private:
                ///     const fooRegistry::Token token;
                ///
                /// public:
                ///     foo () :
                ///         token (this) {}
                ///
                ///     inline fooRegistry::Token::ValueType GetToken () const {
                ///         return token.GetValue ();
                ///     }
                /// };
                /// \endcode
                struct Token {
                    /// \brief
                    /// Convenient typedef for ui32.
                    typedef ui32 IndexType;
                    /// \brief
                    /// Convenient typedef for ui32.
                    typedef ui32 CounterType;
                    /// \brief
                    /// Convenient typedef for ui64.
                    typedef ui64 ValueType;

                private:
                    /// \brief
                    /// {index, counter} pair.
                    ValueType value;

                public:
                    /// \brief
                    /// ctor.
                    /// \param[in] t Object to register.
                    explicit Token (T *t) :
                        value (Registry<T>::Instance ().Add (t)) {}
                    /// \brief
                    /// dtor.
                    ~Token () {
                        // NOTE: There's no race here. Even though a
                        // registry entry still exists when the dtor
                        // is called there are no more shared
                        // references (hence the call to dtor).
                        // Therefore even if we're interrupted and a
                        // call is made to the registry to get a
                        // shared pointer, null will be returned as
                        // the object is in the middle of cleaning
                        // up after itself.
                        Registry<T>::Instance ().Remove (value);
                    }

                    /// \brief
                    /// Make token from the given index and counter.
                    /// \param[in] index Index part of the token.
                    /// \param[in] counter Counter part of the token.
                    /// \return A freshly minted token value.
                    static inline ValueType MakeValue (
                            IndexType index,
                            CounterType counter) {
                        return THEKOGANS_UTIL_MK_UI64 (index, counter);
                    }
                    /// \brief
                    /// Extract the index.
                    /// \return THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (value, 0);
                    static inline IndexType GetIndex (ValueType value) {
                        return THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (value, 0);
                    }
                    /// \brief
                    /// Extract the counter.
                    /// \return THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (value, 1);
                    static inline CounterType GetCounter (ValueType value) {
                        return THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (value, 1);
                    }

                    /// \brief
                    /// Retutn the token value.
                    /// \return value.
                    inline ValueType GetValue () const {
                        return value;
                    }

                    /// \brief
                    /// Token is neither copy constructable, nor assignable.
                    THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Token)
                };

            private:
                /// \struct RefCounted::Registry::Entry RefCounted.h thekogans/util/RefCounted.h
                ///
                /// \brief
                /// Registry entry keeps track of registered objects.
                struct Entry {
                    /// \brief
                    /// Weak pointer to registered object.
                    WeakPtr<T> object;
                    /// \brief
                    /// Counter to disambiguate entry objects.
                    typename Token::CounterType counter;
                    /// \brief
                    /// Next index in free list.
                    typename Token::IndexType next;

                    /// \brief
                    /// ctor.
                    /// \param[in] object_ Entry object.
                    /// \param[in] counter_ Object counter.
                    /// \param[in] next_ Next index in free list.
                    Entry (
                        WeakPtr<T> object_ = WeakPtr<T> (),
                        typename Token::CounterType counter_ = 0,
                        typename Token::IndexType next_ = NIDX32) :
                        object (object_),
                        counter (counter_),
                        next (next_) {}
                };
                /// \brief
                /// The list of registered objects.
                std::vector<Entry> entries;
                /// \brief
                /// Count of registered objects.
                typename Token::IndexType count;
                /// \brief
                /// Index of last freed object.
                typename Token::IndexType freeList;
                /// \brief
                /// Synchronization lock.
                SpinLock spinLock;

            public:
                /// \brief
                /// ctor.
                /// \param[in] entriesSize Initial entry list size.
                /// NOTE: The resizing algorithm used below doubles this
                /// starting seed every time it needs to add room for a
                /// new entry. This is why the check for 0 is done below.
                Registry (std::size_t entriesSize = DEFAULT_ENTRIES_SIZE) :
                    entries (entriesSize == 0 ? 1 : entriesSize),
                    count (0),
                    freeList (NIDX32) {}

                /// \brief
                /// Add an object to the registry.
                /// \param[in] t Object to add.
                /// \return Token describing the object entry.
                /// NOTE: This method is meant to be used by the
                /// \see{Token} ctor which will be initialized
                /// by the containing (T) object's ctor. In this
                /// case passing a raw pointer is appropriate.
                typename Token::ValueType Add (T *t) {
                    typename Token::ValueType value = INVALID_TOKEN;
                    if (t != 0) {
                        typename Token::IndexType index;
                        typename Token::CounterType counter;
                        {
                            LockGuard<SpinLock> guard (spinLock);
                            if (freeList != NIDX32) {
                                // Reuse a free entry.
                                index = freeList;
                                counter = entries[index].counter;
                                freeList = entries[index].next;
                            }
                            else {
                                index = count;
                                // This is the first time we're using this slot.
                                counter = 0;
                                if (count == entries.size ()) {
                                    // Here we implement a simple exponential
                                    // array grow algorithm. Every time we
                                    // need to grow the array, we double it's
                                    // size. This scheme has two advantages:
                                    // 1. Keep the registry small for types with few instances.
                                    // 2. Less grow copy overhead for types with many instances.
                                    // A byproduct of this approach is that every
                                    // index allocated and returned by the registry
                                    // is valid in perpetuity.
                                    entries.resize (count * 2);
                                }
                            }
                            entries[index] = Entry (WeakPtr<T> (t), counter, NIDX32);
                            ++count;
                        }
                        // Pack index and counter describing this entry in to a token value.
                        value = Token::MakeValue (index, counter);
                    }
                    return value;
                }

                /// \brief
                /// Given a token, remove the object entry.
                /// \param[in] value Token describing the object entry to remove.
                void Remove (typename Token::ValueType value) {
                    // Unpack the token to get index and counter.
                    typename Token::IndexType index = Token::GetIndex (value);
                    typename Token::CounterType counter = Token::GetCounter (value);
                    LockGuard<SpinLock> guard (spinLock);
                    // Check the counter to make sure this token
                    // still has access to this slot.
                    if (index < entries.size () && entries[index].counter == counter) {
                        // Incrementing the counter prevents double Remove and Get after Remove.
                        // NOTE: Because of WeakPtr<T> () in Entry below, Get after Remove is
                        // harmless, though it's still semantically wrong. Double Remove on the
                        // other hand would lead to freeList corruption.
                        entries[index] = Entry (WeakPtr<T> (), ++counter, freeList);
                        freeList = index;
                        --count;
                    }
                }

                /// \brief
                /// Given a token, retrieve the object at the entry.
                /// \param[in] value Token describing the object entry to retrieve.
                /// \return SharedPtr<T>.
                SharedPtr<T> Get (typename Token::ValueType value) {
                    // Unpack the token to get index and counter.
                    typename Token::IndexType index = Token::GetIndex (value);
                    typename Token::CounterType counter = Token::GetCounter (value);
                    LockGuard<SpinLock> guard (spinLock);
                    // Check the counter to make sure this token
                    // still references the current object.
                    return index < entries.size () && entries[index].counter == counter ?
                        // Yes? Ask the WeakPtr<T> registered to return a SharedPtr<T>.
                        entries[index].object.GetSharedPtr () :
                        // No? Return NULL pointer.
                        SharedPtr<T> ();
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
        public:\
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
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
                const RefCounted::SharedPtr<T1> &item1,
                const RefCounted::SharedPtr<T2> &item2) throw () {
            return item1.Get () == item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::SharedPtr}s for inequality.
        /// \param[in] item1 First \see{RefCounted::SharedPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::SharedPtr} to compare.
        /// \return true == Different objects, false == Same object.
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
                const RefCounted::SharedPtr<T1> &item1,
                const RefCounted::SharedPtr<T2> &item2) throw () {
            return item1.Get () != item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::SharedPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::SharedPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::SharedPtr} to compare.
        /// \return true == item1 < item2.
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator < (
                const RefCounted::SharedPtr<T1> &item1,
                const RefCounted::SharedPtr<T2> &item2) throw () {
            return item1.Get () < item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::SharedPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::SharedPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::SharedPtr} to compare.
        /// \return true == item1 <= item2.
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator <= (
                const RefCounted::SharedPtr<T1> &item1,
                const RefCounted::SharedPtr<T2> &item2) throw () {
            return item1.Get () <= item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::SharedPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::SharedPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::SharedPtr} to compare.
        /// \return true == item1 > item2.
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator > (
                const RefCounted::SharedPtr<T1> &item1,
                const RefCounted::SharedPtr<T2> &item2) throw () {
            return item1.Get () > item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::SharedPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::SharedPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::SharedPtr} to compare.
        /// \return true == item1 >= item2.
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator >= (
                const RefCounted::SharedPtr<T1> &item1,
                const RefCounted::SharedPtr<T2> &item2) throw () {
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
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator == (
                const RefCounted::WeakPtr<T1> &item1,
                const RefCounted::WeakPtr<T2> &item2) throw () {
            return item1.Get () == item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::WeakPtr}s for inequality.
        /// \param[in] item1 First \see{RefCounted::WeakPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::WeakPtr} to compare.
        /// \return true == Different objects, false == Same object.
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator != (
                const RefCounted::WeakPtr<T1> &item1,
                const RefCounted::WeakPtr<T2> &item2) throw () {
            return item1.Get () != item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::WeakPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::WeakPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::WeakPtr} to compare.
        /// \return true == item1 < item2.
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator < (
                const RefCounted::WeakPtr<T1> &item1,
                const RefCounted::WeakPtr<T2> &item2) throw () {
            return item1.Get () < item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::WeakPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::WeakPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::WeakPtr} to compare.
        /// \return true == item1 <= item2.
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator <= (
                const RefCounted::WeakPtr<T1> &item1,
                const RefCounted::WeakPtr<T2> &item2) throw () {
            return item1.Get () <= item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::WeakPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::WeakPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::WeakPtr} to compare.
        /// \return true == item1 > item2.
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator > (
                const RefCounted::WeakPtr<T1> &item1,
                const RefCounted::WeakPtr<T2> &item2) throw () {
            return item1.Get () > item2.Get ();
        }

        /// \brief
        /// Compare two \see{RefCounted::WeakPtr}s for order.
        /// \param[in] item1 First \see{RefCounted::WeakPtr} to compare.
        /// \param[in] item2 Second \see{RefCounted::WeakPtr} to compare.
        /// \return true == item1 >= item2.
        template<
            typename T1,
            typename T2>
        inline bool _LIB_THEKOGANS_UTIL_API operator >= (
                const RefCounted::WeakPtr<T1> &item1,
                const RefCounted::WeakPtr<T2> &item2) throw () {
            return item1.Get () >= item2.Get ();
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RefCounted_h)
