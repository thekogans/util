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

#if !defined (__thekogans_util_Singleton_h)
#define __thekogans_util_Singleton_h

#include <typeinfo>
#include "thekogans/util/Config.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/NullLock.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"

namespace thekogans {
    namespace util {

        /// \struct DefaultInstanceCreator Singleton.h thekogans/util/Singleton.h
        ///
        /// \brief
        /// Implements the default singleton creation method. Used to parameterize singleton
        /// ctors. If your singleton ctor needs custom creation, you can package it in a class
        /// like this and pass it as the third argument to Singleton (below). You can then
        /// call an appropriate ctor inside the class function call operator. All global
        /// singletons take a template parameter like this to allow you to customize their
        /// instance creation before using them.

        template<typename T>
        struct DefaultInstanceCreator {
            /// \brief
            /// Create a single instance using the default ctor.
            /// \param[in] args List of arguments to instance ctor.
            /// \return Singleton instance.
            template<typename... Args>
            inline T *operator () (Args... args) {
                return new T (std::forward<Args> (args)...);
            }
        };

        /// \struct DefaultInstanceDestroyer Singleton.h thekogans/util/Singleton.h
        ///
        /// \brief
        /// Implements the default singleton destruction method.

        template<typename T>
        struct DefaultInstanceDestroyer {
            /// \brief
            /// Destroy the singleton instance.
            /// \param[in] instance Singleton instance to destroy.
            inline void operator () (T *instance) {
                if (instance != nullptr) {
                    delete instance;
                }
            }
        };

        /// \struct RefCountedInstanceCreator Singleton.h thekogans/util/Singleton.h
        ///
        /// \brief
        /// Implements the \see{RefCounted} singleton creation method. Used to parameterize singleton
        /// ctors. If your singleton ctor needs custom creation, you can package it in a class
        /// like this and pass it as the third argument to Singleton (below). You can then
        /// call an appropriate ctor inside the class function call operator. All global
        /// singletons take a template parameter like this to allow you to customize their
        /// instance creation before using them.

        template<typename T>
        struct RefCountedInstanceCreator {
            /// \brief
            /// Create a single instance using the default ctor.
            /// \param[in] args List of arguments to instance ctor.
            /// \return Singleton instance.
            template<typename... Args>
            inline T *operator () (Args... args) {
                T *instance = new T (std::forward<Args> (args)...);
                // The singleton is the owner of the instance.
                instance->AddRef ();
                return instance;
            }
        };

        /// \struct RefCountedInstanceDestroyer Singleton.h thekogans/util/Singleton.h
        ///
        /// \brief
        /// Implements a \see{RefCounted} singleton destruction method.
        ///
        /// \code{.cpp}
        /// // AsyncIoEventQueue is a derived from \see{RefCounted}.
        /// struct _LIB_THEKOGANS_STREAM_DECL AsyncIoEventQueue :
        ///         public AsyncIoEventQueue,
        ///         public util::Singleton<
        ///             AsyncIoEventQueue,
        ///             util::SpinLock,
        ///             util::RefCountedInstanceCreator<AsyncIoEventQueue>,
        ///             util::RefCountedInstanceDestroyer<AsyncIoEventQueue>>,
        ///         ... {
        ///     ...
        /// };
        /// \endcode

        template<typename T>
        struct RefCountedInstanceDestroyer {
            /// \brief
            /// Destroy the singleton instance.
            /// \param[in] instance Singleton instance to destroy.
            inline void operator () (T *instance) {
                if (instance != nullptr) {
                    instance->Release ();
                }
            }
        };

        /// \struct Singleton Singleton.h thekogans/util/Singleton.h
        ///
        /// \brief
        /// Implements a Singleton pattern. It's design allows for two types
        /// of use cases:
        ///
        /// 1) Derived from by classes that need to be singletons.
        ///    Here is an example:
        ///
        /// \code{.cpp}
        /// struct foo : public thekogans::util::Singleton<foo, thekogans::util::SpinLock> {
        ///     void bar ();
        ///     ...
        /// };
        /// \endcode
        ///
        /// foo will now be a singleton, and it's one and only instance
        /// can be accessed like this:
        ///
        /// \code{.cpp}
        /// foo::Instance ().bar ();
        /// \endcode
        ///
        /// 2) Used directly to make a singleton out of an exisiting class.
        ///    Here is an example:
        ///
        /// \code{.cpp}
        /// struct foo {
        ///     void bar ();
        ///     ...
        /// };
        ///
        /// typedef thekogans::util::Singleton<foo, thekogans::util::SpinLock> FooSingleton;
        /// \endcode
        ///
        /// FooSingleton will now be a singleton of type foo, and it's one and only instance
        /// can be accessed like this:
        ///
        /// \code{.cpp}
        /// FooSingleton::Instance ().bar ();
        /// \endcode
        ///
        /// NOTE: That thekogans::util::SpinLock used as the second template
        /// parameter means that foo's creation will be thread safe (not
        /// that foo itself is thread safe). If you don't care about thread
        /// safety when creating the singleton, use thekogans::util::NullLock (default).

        template<
            typename T,
            typename Lock = NullLock,
            typename InstanceCreator = DefaultInstanceCreator<T>,
            typename InstanceDestroyer = DefaultInstanceDestroyer<T>>
        struct Singleton {
            /// \brief
            /// Uses modern C++ template facilities to provide singleton
            /// ctor parameters.
            /// NOTE: In order to supply custom ctor arguments you need
            /// to call this method before the first call to Instance below.
            /// If you don't, CreateInstance will be a noop as instance will
            /// have already be created.
            /// \param[in] args Variable length list of parameters to pass
            /// to singleton instance ctor.
            /// \return The singleton instance.
            template<typename... Args>
            static T &CreateInstance (Args... args) {
                // We implement the double-checked locking pattern here
                // to allow our singleton instance method to be thread-safe
                // (i.e. thread-safe singleton construction).
                if (instance () == nullptr) {
                    // Here we acquire the lock, check instance again,
                    // and if it's STILL null, we are the lucky ones,
                    // we get to create the actual instance!
                    LockGuard<Lock> guard (lock ());
                    if (instance () == nullptr) {
                        instance () = InstanceCreator () (std::forward<Args> (args)...);
                    }
                }
                return *instance ();
            }

            /// \brief
            /// Destroy the singleton instance.
            static void DestroyInstance () {
                // Note that while DestroyInstance will protect
                // against two threads calling at the same time it
                // cannot and will not protect against one thread
                // calling DestroyInstance while another is still
                // using the Instance. That kind of synchronization is
                // outside the scope of Singleton and needs to be
                // handled by the application.
                LockGuard<Lock> guard (lock ());
                if (instance () != nullptr) {
                    T *instance_ = nullptr;
                    InstanceDestroyer () (EXCHANGE (instance (), instance_));
                }
            }

            /// \brief
            /// Return true if instance has been created.
            /// \return true if instance has been created.
            static bool IsInstanceCreated () {
                return instance () != nullptr;
            }

            /// \brief
            /// Return the singleton instance. Create if first time accessed.
            /// \return The singleton instance.
            static T &Instance () {
                // We implement the double-checked locking pattern here
                // to allow our singleton instance method to be thread-safe
                // (i.e. thread-safe singleton construction).
                if (instance () == nullptr) {
                    // Here we acquire the lock, check instance again,
                    // and if it's STILL null, we are the lucky ones,
                    // we get to create the actual instance!
                    LockGuard<Lock> guard (lock ());
                    if (instance () == nullptr) {
                        instance () = InstanceCreator () ();
                    }
                }
                return *instance ();
            }

        protected:
            /// \brief
            /// By wrapping the Singleton static members in accessor
            /// functions we guarantee that the ctors are called in
            /// the right order.
            /// \return A reference to the one and only instance pointer.
            static T *&instance () {
                // The one and only singleton instance.
                static T *_instance = nullptr;
                return _instance;
            };
            /// \brief
            /// By wrapping the Singleton static members in accessor
            /// functions we guarantee that the ctors are called in
            /// the right order.
            /// \return A reference to the lock protecting singleton construction.
            static Lock &lock () {
                // Lock protecting singleton construction.
                static Lock _lock;
                return _lock;
            }

            /// \brief
            /// ctor.
            Singleton () {}
            /// \brief
            /// dtor.
            ~Singleton () {}

            /// \brief
            /// Singleton is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Singleton)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Singleton_h)
