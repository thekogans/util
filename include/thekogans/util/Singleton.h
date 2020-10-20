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

#include <cassert>
#include "thekogans/util/Config.h"
#include "thekogans/util/NullLock.h"
#include "thekogans/util/LockGuard.h"

namespace thekogans {
    namespace util {

        /// \struct DefaultCreateInstance Singleton.h thekogans/util/Singleton.h
        ///
        /// \brief
        /// Implements the default singleton creation method. Used to parameterize singleton
        /// ctors. If your singleton ctor needs arguments, you can package them in a class
        /// like this and pass it as the third argument to Singleton (below). You can then
        /// call an appropriate ctor inside the class function call operator. All global
        /// singletons take a template parameter like this to allow you to customize their
        /// instance creation before using them.
        ///
        /// Example:
        ///
        /// \code{.cpp}
        /// struct GlobalJobQueueCreateInstance {
        ///     static std::string name;
        ///     static RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy;
        ///     static std::size_t workerCount;
        ///     static i32 workerPriority;
        ///     static ui32 workerAffinity;
        ///
        ///     static void Parameterize (
        ///             const std::string &name_ = std::string (),
        ///             RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy_ =
        ///                 RunLoop::JobExecutionPolicy::Ptr (new RunLoop::FIFOJobExecutionPolicy),
        ///             std::size_t workerCount_ = 1,
        ///             i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
        ///             ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY) {
        ///         name = name_;
        ///         jobExecutionPolicy = jobExecutionPolicy_;
        ///         workerCount = workerCount_;
        ///         workerPriority = workerPriority_;
        ///         workerAffinity = workerAffinity_;
        ///     }
        ///
        ///     JobQueue *operator () () {
        ///         return new JobQueue (
        ///             name,
        ///             jobExecutionPolicy,
        ///             workerCount,
        ///             workerPriority,
        ///             workerAffinity);
        ///     }
        /// };
        ///
        /// struct _LIB_THEKOGANS_UTIL_DECL GlobalJobQueue :
        ///     public Singleton<JobQueue, SpinLock, GlobalJobQueueCreateInstance> {};
        ///
        /// // Call GlobalJobQueueCreateInstance::Parameterize (...); before calling
        /// // GlobalJobQueue::Instance () and the GlobalJobQueue instance will be
        /// // created with your custom arguments.
        /// \endcode

        template<typename T>
        struct DefaultCreateInstance {
            /// \brief
            /// Create a single instance using the default ctor.
            /// \return Singleton instance.
            inline T *operator () () {
                return new T;
            }
        };

        /// \struct DefaultDestroyInstance Singleton.h thekogans/util/Singleton.h
        ///
        /// \brief
        /// Implements the default singleton destruction method.

        template<typename T>
        struct DefaultDestroyInstance {
            /// \brief
            /// Destroy the singleton instance.
            /// \param[in] instance Singleton instance to destroy.
            inline void operator () (T *instance) {
                if (instance != 0) {
                    delete instance;
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
            typename CreateInstance = DefaultCreateInstance<T>,
            typename DestroyInstance = DefaultDestroyInstance<T>>
        struct Singleton {
            /// \brief
            /// Alternative to using CreateInstance and DestroyInstance.
            /// Uses modern C++ template facilities to provide singleton
            /// ctor parameters.
            /// IMPORTANT: If you're going to use CreateSingleton, do not
            /// provide a CreateInstance and if you are going to provide
            /// DestroyInstance, make sure it uses delete.
            /// \param[in] args Variable length list of parameters to pass
            /// to singleton instance ctor.
            template<typename... arg_types>
            static void CreateSingleton (arg_types... args) {
                // We implement the double-checked locking pattern here
                // to allow our singleton instance method to be thread-safe
                // (i.e. thread-safe singleton construction).
                if (instance == 0) {
                    // Here we acquire the lock, check instance again,
                    // and if it's STILL null, we are the lucky ones,
                    // we get to create the actual instance!
                    LockGuard<Lock> guard (lock);
                    if (instance == 0) {
                        instance = new T (std::forward<arg_types> (args)...);
                    }
                }
                assert (instance != 0);
            }

            /// \brief
            /// Return the singleton instance. Create if first time accessed.
            /// \return The singleton instance.
            static T &Instance () {
                // We implement the double-checked locking pattern here
                // to allow our singleton instance method to be thread-safe
                // (i.e. thread-safe singleton construction).
                if (instance == 0) {
                    // Here we acquire the lock, check instance again,
                    // and if it's STILL null, we are the lucky ones,
                    // we get to create the actual instance!
                    LockGuard<Lock> guard (lock);
                    if (instance == 0) {
                        instance = CreateInstance () ();
                    }
                }
                assert (instance != 0);
                return *instance;
            }

            /// \brief
            /// Destroy the singleton instance.
            static void Destroy () {
                LockGuard<Lock> guard (lock);
                if (instance != 0) {
                    DestroyInstance () (instance);
                    instance = 0;
                }
            }

            /// \brief
            /// Return true if instance has been created.
            /// \return true if instance has been created.
            static bool IsInstantiated () {
                LockGuard<Lock> guard (lock);
                return instance != 0;
            }

        protected:
            /// \brief
            /// The one and only singleton instance.
            static T * volatile instance;
            /// \brief
            /// Lock protecting singleton construction.
            static Lock lock;

            /// \brief
            /// ctor.
            Singleton () {}
            /// \brief
            /// dtor.
            virtual ~Singleton () {}

            /// \brief
            /// Singleton is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Singleton)
        };

        /// \brief
        /// Definition of the Singleton<T, Lock, CreateInstance, DestroyInstance>::instance.
        template<
            typename T,
            typename Lock,
            typename CreateInstance,
            typename DestroyInstance>
        T * volatile Singleton<T, Lock, CreateInstance, DestroyInstance>::instance = 0;

        /// \brief
        /// Definition of the Singleton<T, Lock, CreateInstance, DestroyInstance>::lock.
        template<
            typename T,
            typename Lock,
            typename CreateInstance,
            typename DestroyInstance>
        Lock Singleton<T, Lock, CreateInstance, DestroyInstance>::lock;

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Singleton_h)
