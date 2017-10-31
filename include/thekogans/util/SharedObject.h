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

#if !defined (__thekogans_util_SharedObject_h)
#define __thekogans_util_SharedObject_h

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
#else // defined (TOOLCHAIN_OS_Windows)
    #include <sys/stat.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <string>
#include "thekogans/util/Types.h"
#include "thekogans/util/TimeSpec.h"

namespace thekogans {
    namespace util {

    #if defined (TOOLCHAIN_OS_Windows)
        #if !defined (NAME_MAX)
            #define NAME_MAX 255
        #endif // !defined (NAME_MAX)
    #endif // defined (TOOLCHAIN_OS_Windows)

        /// \struct SharedObject SharedObject.h thekogans/util/SharedObject.h
        ///
        /// \brief
        /// SharedObject abstracts out the boilerplate CreateFileMapping and
        /// [Unm|M]apViewOfFile machinery on Windows and shm_* and m[un]map
        /// machinery on POSIX used to create or open shared memory regions.
        /// It's used by Event, Semaphore (POSIX) and SharedAllocator. Use it
        /// to create your own cross-process shared objects.
        /// NOTE: Shared objects, by their nature, cannot contain pointers as
        /// they would not be valid across process boundaries. So, while a concrete
        /// shared object derives from SharedObject, think of it more like containment
        /// or aggregation. This is why SharedObject does not define a virtual dtor.
        /// RTTI is also not available for SharedObject derivatives.

        struct _LIB_THEKOGANS_UTIL_DECL SharedObject {
        #if !defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Delete shared memory regions associated with a given name.
            /// \param[in] name Shared memory regions to delete.
            static void Cleanup (const char *name);
        #endif // !defined (TOOLCHAIN_OS_Windows)

            /// \struct SharedObject::Constructor SharedObject.h thekogans/util/SharedObject.h
            ///
            /// \brief
            /// Used by Create below to construct the shared object if the shared region
            /// was created. If the shared region was opened, it's refCount will be
            /// incremented.
            struct _LIB_THEKOGANS_UTIL_DECL Constructor {
                /// \brief
                /// dtor.
                virtual ~Constructor () {}

                /// \brief
                /// A concrete type of Constructor will use a placement new to construct
                /// the shared object and call an appropriate ctor.
                /// \param[in] ptr Pointer to call placement new on (new (ptr) SharedObject (...)).
                /// \return A constructed instance of SharedObject.
                virtual void *operator () (void *ptr) const {
                    return ptr;
                }
            };

            /// \brief
            /// Create or open a given shared memory region and construct the shared object.
            /// \param[in] name Name of shared memory region to create/open.
            /// \param[in] size Size of shared region
            /// (Usualy sizeof (SharedObject), but can be more. See \see{SharedAllocator}).
            /// \param[in] secure true = lock region to prevent swapping.
            /// \param[in] constructor A Constructor instance used to construct the shared object.
        #if defined (TOOLCHAIN_OS_Windows)
            /// \param[in] securityAttributes Security attributes used by the lock and shared memory region.
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] mode Protection mode used by the lock and shared memory region.
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] timeSpec Used by lock to put the process to sleep during lock contention.
            /// \return Created/Opened and constructed instance of SharedObject.
            static void *Create (
                const char *name,
                ui64 size,
                bool secure = false,
                const Constructor &constructor = Constructor (),
            #if defined (TOOLCHAIN_OS_Windows)
                LPSECURITY_ATTRIBUTES securityAttributes = 0,
            #else // defined (TOOLCHAIN_OS_Windows)
                mode_t mode = 0666,
            #endif // defined (TOOLCHAIN_OS_Windows)
                const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100));

            /// \struct SharedObject::Destructor SharedObject.h thekogans/util/SharedObject.h
            ///
            /// \brief
            /// Called by Destroy below to destruct an instance of SharedObject.
            struct _LIB_THEKOGANS_UTIL_DECL Destructor {
                /// \brief
                /// dtor.
                virtual ~Destructor () {}

                /// \brief
                /// Analog to Constructor above. More often then not call sharedObject->~SharedObject ().
                /// \param[in] ptr SharedObject instance to destruct.
                virtual void operator () (void * /*ptr*/) const {}
            };

            /// \brief
            /// Decrement the reference count and if 0, destroy the given instance of T.
            /// \param[in] ptr Instance of SharedObject to destroy.
            /// \param[in] destructor Analog to Constructor used to actually destroy t.
        #if defined (TOOLCHAIN_OS_Windows)
            /// \param[in] securityAttributes Security attributes used by the lock and shared memory region.
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] mode Protection mode used by the lock and shared memory region.
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] timeSpec Used by lock to put the process to sleep during lock contention.
            static void Destroy (
                void *ptr,
                const Destructor &destructor = Destructor (),
            #if defined (TOOLCHAIN_OS_Windows)
                LPSECURITY_ATTRIBUTES securityAttributes = 0,
            #else // defined (TOOLCHAIN_OS_Windows)
                mode_t mode = 0666,
            #endif // defined (TOOLCHAIN_OS_Windows)
                const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100));

        private:
            /// \struct SharedObject::Lock SharedObject.h thekogans/util/SharedObject.h
            ///
            /// \brief
            /// Lock used to serialize T construction/destruction.
            struct Lock {
            private:
            #if !defined (TOOLCHAIN_OS_Windows)
                /// \brief
                /// Lock name.
                std::string name;
            #endif // !defined (TOOLCHAIN_OS_Windows)
                /// \brief
                /// Shared memory region representing the lock.
                THEKOGANS_UTIL_HANDLE handle;

            public:
                /// \brief
                /// ctor.
                /// \param[in] name Shared object name.
            #if defined (TOOLCHAIN_OS_Windows)
                /// \param[in] securityAttributes Security attributes used by the lock and shared memory region.
            #else // defined (TOOLCHAIN_OS_Windows)
                /// \param[in] mode Protection mode used by the lock and shared memory region.
            #endif // defined (TOOLCHAIN_OS_Windows)
                /// \param[in] timeSpec Used to put the process to sleep during lock contention.
                Lock (
                    const char *name,
                #if defined (TOOLCHAIN_OS_Windows)
                    LPSECURITY_ATTRIBUTES securityAttributes = 0,
                #else // defined (TOOLCHAIN_OS_Windows)
                    mode_t mode = 0666,
                #endif // defined (TOOLCHAIN_OS_Windows)
                    const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100));
                /// \brief
                /// dtor.
                ~Lock ();

                /// \brief
                /// Synthesize a lock name from object name and "_lock";
                /// \param[in] name Object name.
                /// \return name + "_lock";
                static std::string GetName (const char *name);
            };
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SharedObject_h)
