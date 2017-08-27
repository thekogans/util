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
#if defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_TYPE_Shared)
    #include <typeinfo>
    #include "thekogans/util/Mutex.h"
#endif // defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_TYPE_Shared)
#include "thekogans/util/Config.h"
#include "thekogans/util/NullLock.h"
#include "thekogans/util/LockGuard.h"

namespace thekogans {
    namespace util {

    #if defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_TYPE_Shared)
        namespace detail {
            /// \brief
            /// Used by Singleton::Instance to implement a global T * map.
            /// IMPORTANT: The lock must be a Mutex (critical section) as
            /// it's recursive nature allows chaining of singleton construction.
            /// \return Global T * map lock.
            _LIB_THEKOGANS_UTIL_DECL Mutex & _LIB_THEKOGANS_UTIL_API GetInstanceLock ();
            /// \brief
            /// Used by Singleton::Instance to implement a global T * map.
            /// \param[in] name Class instance name.
            /// \return T * associated with the given name.
            _LIB_THEKOGANS_UTIL_DECL void * _LIB_THEKOGANS_UTIL_API GetInstance (const char *name);
            /// \brief
            /// Used by Singleton::Instance to implement a global T * map.
            /// \param[in] name Class instance name.
            /// \param[in] instance T * associated with the name,
            _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API SetInstance (
                const char *name,
                void *instance);
        }
    #endif // defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_TYPE_Shared)

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
        ///     static JobQueue::Type type;
        ///     static ui32 workerCount;
        ///     static i32 workerPriority;
        ///     static ui32 workerAffinity;
        ///     static ui32 maxPendingJobsl
        ///
        ///     static void Parameterize (
        ///             const std::string &name_ = std::string (),
        ///             JobQueue::Type type_ = JobQueue::TYPE_FIFO,
        ///             ui32 workerCount_ = 1,
        ///             i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
        ///             ui32 workerAffinity_ = UI32_MAX,
        ///             ui32 maxPendingJobs_ = UI32_MAX) {
        ///         name = name_;
        ///         type = type_;
        ///         workerCount = workerCount_;
        ///         workerPriority = workerPriority_;
        ///         workerAffinity = workerAffinity_;
        ///         maxPendingJobs = maxPendingJobs_;
        ///     }
        ///
        ///     JobQueue *operator () () {
        ///         return JobQueue (
        ///             name,
        ///             type,
        ///             workerCount,
        ///             workerPriority,
        ///             workerAffinity,
        ///             maxPendingJobs);
        ///     }
        /// };
        ///
        /// typedef Singleton<JobQueue, SpinLock, GlobalJobQueueCreateInstance> GlobalJobQueue;
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
        /// FooSingleton will now be a singleton, and it's one and only instance
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
            typename CreateInstance = DefaultCreateInstance<T>>
        struct Singleton {
            /// \brief
            /// Return the singleton instance. Create if first time accessed.
            /// \return The singleton instance.
            static T &Instance () {
                static T * volatile instance = 0;
                // We implement the double-checked locking pattern here
                // to allow our singleton instance method to be thread-safe
                // (i.e. thread-safe singleton construction).
                if (instance == 0) {
                    // Here we acquire the lock, check instance again,
                    // and if it's STILL null, we are the lucky ones,
                    // we get to create the actual instance!
                #if defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_TYPE_Shared)
                    // Windows shared libraries (DLL) are very different from their
                    // Linux (so)/OS X (dylib) counterparts. On Linux and OS X shared
                    // libraries are very similar to static libraries and the linker
                    // is able to resolve symbols very easily. In fact you have to go
                    // through some pains to actually hide symbol names from the linker.
                    // As a result templates instantiated in one so/dylib are available
                    // to other shared libraries and executables that link against them.
                    // On Windows, DLLs have their own data segments and symbol visibility
                    // rules. As a result, on Windows, It's practically impossible to share
                    // template instances across DLLs. To remedy this, when building for
                    // Windows we use a global instance map to hold the T *. So, while the
                    // Singleton template instances will be different across DLL and process
                    // boundaries, they will all access the same T *.
                    LockGuard<Mutex> guard (detail::GetInstanceLock ());
                    instance = reinterpret_cast<T *> (detail::GetInstance (typeid (T).name ()));
                #else // defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_TYPE_Shared)
                    static Lock lock;
                    LockGuard<Lock> guard (lock);
                #endif // defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_TYPE_Shared)
                    if (instance == 0) {
                        instance = CreateInstance () ();
                    #if defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_TYPE_Shared)
                        // Think of SetInstance as an extension of CreateInstance. Inserting
                        // the created instance in to the map is part of constructing it. If
                        // something goes wrong and it's not able to insert the new T * in to
                        // the map SetInstance will throw an exception. Treat that exception
                        // as you would any other thrown by the T ctor.
                        detail::SetInstance (typeid (T).name (), instance);
                    #endif // defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_TYPE_Shared)
                    }
                }
                assert (instance != 0);
                return *instance;
            }

        protected:
            /// \brief
            /// ctor.
            Singleton () {}
            /// \brief
            /// dtor.
            virtual ~Singleton () {
                // Whoa!, be on to you, who thinks it's a good idea to
                // call delete instance here. Singletons are like std's ;),
                // once acquired, persist forever.
                //delete instance;
                // Not even this is allowed (static initialization).
                //instance = 0;
            }

            /// \brief
            /// Singleton is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Singleton)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Singleton_h)
