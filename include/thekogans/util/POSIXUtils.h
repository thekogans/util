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

#if !defined (__thekogans_util_POSIXUtils_h)
#define __thekogans_util_POSIXUtils_h

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <cstring>
#include <string>
#include "thekogans/util/Types.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Exception.h"
#if defined (TOOLCHAIN_OS_Linux)
    #include "thekogans/util/LinuxUtils.h"
#elif defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_Linux)

namespace thekogans {
    namespace util {

        /// \struct SharedObject POSIXUtils.h thekogans/util/POSIXUtils.h
        ///
        /// \brief
        /// SharedObject template abstracts out the boilerplate shm_* and m[un]map
        /// machinery used to create or open shared memory regions on POSIX systems.
        /// It's used by SharedEvent, SharedMuted, SharedSemaphore and SharedAllocator.
        /// Use it to create your own cross-process shared objects.

        template<typename T>
        struct SharedObject {
            /// \brief
            /// Name of the shared memory region.
            char name[NAME_MAX];
            /// \brief
            /// Reference count.
            /// NOTE: SharedObject is responsible for it's own lifetime.
            /// refCount is used to keep track of object references. The
            /// last reference is responsible for calling shm_unlink.
            ui32 refCount;

            /// \brief
            /// ctor.
            /// \param[in] name_ Name of the shared memory region.
            SharedObject (const char *name_) :
                    refCount (1) {
                if (name_ != 0) {
                    strncpy (name, name_, NAME_MAX);
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }

            /// \struct SharedObject::Creator POSIXUtils.h thekogans/util/POSIXUtils.h
            ///
            /// \brief
            /// Used by Create below to construct the shared object if the shared region
            /// was created. If the shared region was opened, it's refCount will be
            /// incremented.
            struct Creator {
                /// \brief
                /// dtor.
                virtual ~Creator () {}

                /// \brief
                /// A concrete type of Creator will use a placement new to construct
                /// the shared object and call an appropriate ctor.
                /// \param[in] ptr Pointer to call placement new on (new (ptr) T (name, ...)).
                /// \param[in] name Name of shared object.
                /// \return An instance of T.
                virtual T *operator () (
                    void * /*ptr*/,
                    const char * /*name*/) const = 0;
            };

            /// \brief
            /// Create or open a given shared memory region and construct the shared object.
            /// \param[in] name Name of shared memory region to create/open.
            /// \param[in] creator A Creator instance used to construct the shared object.
            /// \param[in] mode Protection mode used by the lock and shared memory region.
            /// \param[in] timeSpec Used by lock to put the process to sleep during lock contention.
            static T *Create (
                    const char *name,
                    const Creator &creator,
                    mode_t mode = 0666,
                    const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100)) {
                if (name != 0) {
                    Lock lock (name, mode, timeSpec);
                    struct SharedMemory {
                        THEKOGANS_UTIL_HANDLE handle;
                        bool owner;
                        SharedMemory (
                                const char *name,
                                mode_t mode) :
                                handle (shm_open (name, O_RDWR | O_CREAT | O_EXCL, mode)),
                                owner (handle != -1) {
                            if (handle != -1) {
                                if (FTRUNCATE_FUNC (handle, sizeof (T)) == -1) {
                                    THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                                    close (handle);
                                    shm_unlink (name);
                                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                                }
                            }
                            else {
                                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                                if (errorCode == EEXIST) {
                                    handle = shm_open (name, O_RDWR);
                                    if (handle != -1) {
                                        owner = false;
                                    }
                                    else {
                                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                            THEKOGANS_UTIL_OS_ERROR_CODE);
                                    }
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                                }
                            }
                        }
                        ~SharedMemory () {
                            close (handle);
                        }
                    } sharedMemory (name, mode);
                    void *ptr = mmap (0, sizeof (T), PROT_READ | PROT_WRITE,
                        MAP_SHARED, sharedMemory.handle, 0);
                    if (ptr != 0) {
                        T *t;
                        if (sharedMemory.owner) {
                            t = creator (ptr, name);
                        }
                        else {
                            t = (T *)ptr;
                            ++t->refCount;
                        }
                        return t;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }

            /// \struct SharedObject::Destructor POSIXUtils.h thekogans/util/POSIXUtils.h
            ///
            /// \brief
            /// Called by Destroy below to destruct an instance of T.
            struct Destructor {
                /// \brief
                /// dtor.
                virtual ~Destructor () {}

                /// \brief
                /// Analog to Creator above. More often then not call t->~T ().
                /// \param[in] t T instance to destruct.
                virtual void operator () (T * /*t*/) const = 0;
            };

            /// \brief
            /// Decrement the reference count and if 0, destroy the given instance of T.
            /// \param[in] t Instance of T to destroy.
            /// \param[in] destructor Analog to Creator used to actually destroy t.
            /// \param[in] mode Protection mode used by the lock.
            /// \param[in] timeSpec Used by lock to put the process to sleep during lock contention.
            static void Destroy (
                    T *t,
                    const Destructor &destructor,
                    mode_t mode = 0666,
                    const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100)) {
                Lock lock (t->name, mode, timeSpec);
                if (--t->refCount == 0) {
                    destructor (t);
                    shm_unlink (t->name);
                }
                munmap (t, sizeof (T));
            }

        private:
            /// \struct SharedObject::Lock POSIXUtils.h thekogans/util/POSIXUtils.h
            ///
            /// \brief
            /// Lock used to serialize T construction/destruction.
            struct Lock {
                /// \brief
                /// Lock name (shared object name + "_lock").
                std::string name;
                /// \brief
                /// Shared memory region representing the lock.
                THEKOGANS_UTIL_HANDLE handle;

                /// \brief
                /// ctor.
                /// \param[in] name_ Shared object name.
                /// \param[in] mode Lock protection mode.
                /// \param[in] timeSpec Used to put the process to sleep during lock contention.
                Lock (
                        const std::string &name_,
                        mode_t mode = 0666,
                        const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100)) :
                        name (name_ + "_lock"),
                        handle (shm_open (name.c_str (), O_RDWR | O_CREAT | O_EXCL, mode)) {
                    while (handle == -1) {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        if (errorCode == EEXIST) {
                            Sleep (timeSpec);
                            handle = shm_open (name.c_str (), O_RDWR | O_CREAT | O_EXCL, mode);
                        }
                        else {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                    }
                }
                /// \brief
                /// dtor.
                ~Lock () {
                    close (handle);
                    shm_unlink (name.c_str ());
                }
            };
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_POSIXUtils_h)
