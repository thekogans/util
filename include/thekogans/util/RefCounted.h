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
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"

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
        /// VERY, VERY IMPORTANT: The flexibility of RefCounted comes with a
        /// price. If you create a RefCounted object on the stack, you are NOT
        /// allowed to use RefCounted::Ptr without either, overriding Harakiri
        /// or calling AddRef in it's ctor. This way there will be an imbalance
        /// between AddRef and Release calls and Harakiri will never be called.

        template<typename Count>
        struct _LIB_THEKOGANS_UTIL_DECL RefCounted {
        private:
            /// \brief
            /// Object to delete when count == 0.
            bool doDelete;
            /// \brief
            /// Reference count.
            Count count;

        public:
            /// \brief
            /// ctor.
            /// \param[in] Object to delete when count == 0.
            RefCounted (bool doDelete_ = true) :
                doDelete (doDelete_),
                count (0) {}
            /// \brief
            /// dtor.
            virtual ~RefCounted () {}

            /// \brief
            /// Increment reference count.
            /// \return Incremented reference count.
            ui32 AddRef () {
                return ++count;
            }
            /// \brief
            /// Decrement reference count, and if 0, delete the object.
            /// \return Decremented reference count.
            ui32 Release () {
                assert (count > 0);
                ui32 newCount = --count;
                if (newCount == 0) {
                    Harakiri ();
                }
                return newCount;
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
            template<typename T>
            struct Ptr {
                /// \brief
                /// Reference counted object.
                T *object;

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
                /// Replace reference counted object.
                /// \param[in] object_ New object to point to.
                void Reset (T *object_ = 0) {
                    if (object != 0) {
                        object->Release ();
                    }
                    object = object_;
                    if (object != 0) {
                        object->AddRef ();
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
            virtual void Harakiri () {
                if (doDelete) {
                    delete this;
                }
            }
        };

        /// \brief
        /// Convenient typedef for RefCounted<THEKOGANS_UTIL_ATOMIC<ui32>>.
        typedef RefCounted<THEKOGANS_UTIL_ATOMIC<ui32>> ThreadSafeRefCounted;
        /// \brief
        /// Convenient typedef for RefCounted<ui32>.
        typedef RefCounted<ui32> SingleThreadedRefCounted;

        /// \brief
        /// Compare two pointers for equality.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == Same object, false == Different objects.
        template<typename T>
        inline bool operator == (
                const ThreadSafeRefCounted::Ptr<T> &item1,
                const ThreadSafeRefCounted::Ptr<T> &item2) {
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
                const ThreadSafeRefCounted::Ptr<T> &item2) {
            return item1.Get () != item2.Get ();
        }

        /// \brief
        /// Compare two pointers for equality.
        /// \param[in] item1 First pointer to compare.
        /// \param[in] item2 Second pointer to compare.
        /// \return true == Same object, false == Different objects.
        template<typename T>
        inline bool operator == (
                const SingleThreadedRefCounted::Ptr<T> &item1,
                const SingleThreadedRefCounted::Ptr<T> &item2) {
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
                const SingleThreadedRefCounted::Ptr<T> &item2) {
            return item1.Get () != item2.Get ();
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RefCounted_h)
