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

#if !defined (__thekogans_util_RefCountedRegistry_h)
#define __thekogans_util_RefCountedRegistry_h

#include <utility>
#include <memory>
#include <typeinfo>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/RefCounted.h"

namespace thekogans {
    namespace util {

        /// \struct RefCountedRegistry RefCountedRegistry.h thekogans/util/RefCountedRegistry.h
        ///
        /// \brief
        /// RefCountedRegistry acts as an interface between RefCounted objects in the C++
        /// world and raw pointers of the OS C API world. It is specifically
        /// designed to make life easier managing object lifetimes in the presence
        /// of async callback interfaces. Most OS APIs that take a function pointer
        /// to call back at a later (async) time also take a user parameter to pass
        /// to the callback. Passing in raw pointers is dangerous as object
        /// lifetime cannot be predicted and dereferencing raw pointers can lead
        /// to disasters. Instead, use the registry to 'register' a WeakPtr to
        /// the object that can later be used to create a SharedPtr (if the object
        /// still exists).

        template<typename T>
        struct RefCountedRegistry : public Singleton<RefCountedRegistry<T>> {
        public:
            /// \brief
            /// Default initial entry list size.
            static const std::size_t DEFAULT_ENTRIES_SIZE = 1024;
            /// \brief
            /// Bit pattern for an invalid token.
            static const ui64 INVALID_TOKEN = NIDX64;

            /// \struct RefCountedRegistry::Token RefCountedRegistry.h
            /// thekogans/util/RefCountedRegistry.h
            ///
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
            /// using fooRegistry = thekogans::util::RefCountedRegistry<foo>;
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
            ///
            /// NOTE: The limiting size of the token is 64 bits. That's the size of current
            /// register architectures. And that's usually the size of the user data that one
            /// can pass to OS APIs. That said, the layout of the index/counter pair does
            /// not have to be 32/32. If we find that in the future we want to keep track of
            /// more than 4G of objects all we need to do is allocate more of the 64 bits to
            /// the index part. Since all access to index/counter is done through a few API
            /// (\see{Token::MakeValue}, \see{Token::GetIndex}, \see{Token::GetCounter}), it
            /// should be a straight forward change (all user code using the registry will
            /// have to be recompiled).
            struct Token {
            #if defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI16)
                /// \brief
                /// Alias for ui64.
                using IndexType = ui64;
                /// \brief
                /// Alias for ui16.
                using CounterType = ui16;
            #elif defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI8)
                /// \brief
                /// Alias for ui64.
                using IndexType = ui64;
                /// \brief
                /// Alias for ui8.
                using CounterType = ui8;
            #else // defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI8)
                /// \brief
                /// Alias for ui32.
                using IndexType = ui32;
                /// \brief
                /// Alias for ui32.
                using CounterType = ui32;
            #endif // defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI16)
                /// \brief
                /// Alias for ui64.
                using ValueType = ui64;

            private:
                /// \brief
                /// {index, counter} pair.
                ValueType value;

            public:
                /// \brief
                /// ctor.
                /// \param[in] t Object to register.
                explicit Token (T *t) :
                    value (RefCountedRegistry<T>::Instance ()->Add (t)) {}
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
                    RefCountedRegistry<T>::Instance ()->Remove (value);
                }

                /// \brief
                /// Make token from the given index and counter.
                /// \param[in] index Index part of the token.
                /// \param[in] counter Counter part of the token.
                /// \return A freshly minted token value.
                static inline ValueType MakeValue (
                        IndexType index,
                        CounterType counter) {
                #if defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI16)
                    return THEKOGANS_UTIL_MK_UI64_FROM_UI64_UI16 (index, counter);
                #elif defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI8)
                    return THEKOGANS_UTIL_MK_UI64_FROM_UI64_UI8 (index, counter);
                #else // defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI8)
                    return THEKOGANS_UTIL_MK_UI64 (index, counter);
                #endif // defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI16)
                }
                /// \brief
                /// Extract the index.
                /// \param[in] value Token value to extract the index from.
                /// \return index;
                static inline IndexType GetIndex (ValueType value) {
                #if defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI16)
                    return THEKOGANS_UTIL_UI64_FROM_UI64_UI16_GET_UI64 (value);
                #elif defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI8)
                    return THEKOGANS_UTIL_UI64_FROM_UI64_UI8_GET_UI64 (value);
                #else // defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI8)
                    return THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (value, 0);
                #endif // defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI16)
                }
                /// \brief
                /// Extract the counter.
                /// \param[in] value Token value to extract the counter from.
                /// \return counter;
                static inline CounterType GetCounter (ValueType value) {
                #if defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI16)
                    return THEKOGANS_UTIL_UI64_GET_UI16_AT_INDEX (value, 3);
                #elif defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI8)
                    return THEKOGANS_UTIL_UI64_GET_UI8_AT_INDEX (value, 7);
                #else // defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI8)
                    return THEKOGANS_UTIL_UI64_GET_UI32_AT_INDEX (value, 1);
                #endif // defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI16)
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
        #if defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI16)
            /// \brief
            /// Bad index value.
            static const typename Token::IndexType BAD_INDEX = (NIDX64 >> 16) & 0x0000ffffffffffff;
        #elif defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI8)
            /// \brief
            /// Bad index value.
            static const typename Token::IndexType BAD_INDEX = (NIDX64 >> 8) & 0x00ffffffffffffff;
        #else // defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI8)
            /// \brief
            /// Bad index value.
            static const typename Token::IndexType BAD_INDEX = NIDX32;
        #endif // defined (THEKOGANS_UTIL_REF_COUNTED_REGISTRY_TOKEN_COUNTER_UI16)

            /// \struct RefCountedRegistry::Entry RefCounted.h thekogans/util/RefCounted.h
            ///
            /// \brief
            /// RefCountedRegistry entry keeps track of registered objects.
            struct Entry {
                /// \brief
                /// Weak pointer to registered object.
                typename RefCounted::WeakPtr<T> object;
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
                    typename RefCounted::WeakPtr<T> object_ = nullptr,
                    typename Token::CounterType counter_ = 0,
                    typename Token::IndexType next_ = BAD_INDEX) :
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
            RefCountedRegistry (std::size_t entriesSize = DEFAULT_ENTRIES_SIZE) :
                entries (entriesSize == 0 ? 1 : entriesSize),
                count (0),
                freeList (BAD_INDEX) {}

            /// \brief
            /// Add an object to the registry.
            /// \param[in] t Object to add.
            /// \return Token describing the object entry.
            /// NOTE: This method is meant to be used by
            /// \see{Token::Token} which will be called
            /// by the containing (T) object's ctor. In this
            /// case passing a raw pointer is appropriate.
            typename Token::ValueType Add (T *t) {
                typename Token::ValueType value = INVALID_TOKEN;
                if (t != nullptr) {
                    typename Token::IndexType index;
                    typename Token::CounterType counter;
                    {
                        LockGuard<SpinLock> guard (spinLock);
                        if (freeList != BAD_INDEX) {
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
                                // need to grow the array, we double its
                                // size. This scheme has two advantages:
                                // 1. Keep the registry small for types with few instances.
                                // 2. Less grow copy overhead for types with many instances.
                                // A byproduct of this approach is that every
                                // index allocated and returned by the registry
                                // is valid in perpetuity. This is exactly the
                                // reason counter is used in the token to disambiguate
                                // object lifetimes. When an object leaves the registry,
                                // \see{Remove} will bump up the counter so that any
                                // leftover token copies will now point to an earlier,
                                // possibly deleted object.
                                entries.resize (count * 2);
                            }
                        }
                        entries[index] = Entry (
                            typename RefCounted::WeakPtr<T> (t), counter, BAD_INDEX);
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
                    entries[index] = Entry (nullptr, ++counter, freeList);
                    freeList = index;
                    --count;
                }
            }

            /// \brief
            /// Given a token, retrieve the object at the entry.
            /// \param[in] value Token describing the object entry to retrieve.
            /// \return SharedPtr<T>.
            typename RefCounted::SharedPtr<T> Get (typename Token::ValueType value) {
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
                    nullptr;
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RefCountedRegistry_h)
