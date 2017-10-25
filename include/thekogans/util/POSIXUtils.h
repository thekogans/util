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
        /// It's used by Event, Semaphore and SharedAllocator. Use it to create your
        /// own cross-process shared objects.

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

            /// \brief
            /// Delete shared memory regions associated with a name.
            /// \param[in] name Shared memory regions to delete.
            static void Cleanup (const char *name) {
                if (name != 0) {
                    shm_unlink (name);
                    shm_unlink (Lock::GetName (name).c_str ());
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }

            /// \struct SharedObject::Constructor POSIXUtils.h thekogans/util/POSIXUtils.h
            ///
            /// \brief
            /// Used by Create below to construct the shared object if the shared region
            /// was created. If the shared region was opened, it's refCount will be
            /// incremented.
            struct Constructor {
                /// \brief
                /// dtor.
                virtual ~Constructor () {}

                /// \brief
                /// A concrete type of Constructor will use a placement new to construct
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
            /// \param[in] constructor A Constructor instance used to construct the shared object.
            /// \param[in] secure true = lock region to prevent swapping.
            /// \param[in] mode Protection mode used by the lock and shared memory region.
            /// \param[in] timeSpec Used by lock to put the process to sleep during lock contention.
            static T *Create (
                    const char *name,
                    const Constructor &constructor,
                    bool secure = false,
                    mode_t mode = 0666,
                    const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100)) {
                if (name != 0) {
                    Lock lock (name, mode, timeSpec);
                    struct SharedMemory {
                        const char *name;
                        THEKOGANS_UTIL_HANDLE handle;
                        bool created;
                        SharedMemory (
                                const char *name_,
                                mode_t mode) :
                                name (name_),
                                handle (shm_open (name, O_RDWR | O_CREAT | O_EXCL, mode)),
                                created (handle != -1) {
                            if (created) {
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
                                        created = false;
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

                        void Destroy () {
                            if (created) {
                                shm_unlink (name);
                            }
                        }
                    } sharedMemory (name, mode);
                    void *ptr = mmap (0, sizeof (T), PROT_READ | PROT_WRITE,
                        MAP_SHARED, sharedMemory.handle, 0);
                    if (ptr != MAP_FAILED) {
                        if (!secure || LockRegion (ptr)) {
                            T *t;
                            if (sharedMemory.created) {
                                THEKOGANS_UTIL_TRY {
                                    t = constructor (ptr, name);
                                }
                                THEKOGANS_UTIL_CATCH_ANY {
                                    munmap (ptr, sizeof (T));
                                    sharedMemory.Destroy ();
                                    throw;
                                }
                            }
                            else {
                                t = (T *)ptr;
                                ++t->refCount;
                            }
                            return t;
                        }
                        else {
                            THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                            munmap (ptr, sizeof (T));
                            sharedMemory.Destroy ();
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                    }
                    else {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        sharedMemory.Destroy ();
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
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
                /// Analog to Constructor above. More often then not call t->~T ().
                /// \param[in] t T instance to destruct.
                virtual void operator () (T * /*t*/) const = 0;
            };

            /// \brief
            /// Decrement the reference count and if 0, destroy the given instance of T.
            /// \param[in] t Instance of T to destroy.
            /// \param[in] destructor Analog to Constructor used to actually destroy t.
            /// \param[in] secure true = unlock previously locked region.
            /// \param[in] mode Protection mode used by the lock.
            /// \param[in] timeSpec Used by lock to put the process to sleep during lock contention.
            static void Destroy (
                    T *t,
                    const Destructor &destructor,
                    bool secure = false,
                    mode_t mode = 0666,
                    const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100)) {
                Lock lock (t->name, mode, timeSpec);
                if (--t->refCount == 0) {
                    destructor (t);
                    shm_unlink (t->name);
                }
                if (secure) {
                    munlock (t, sizeof (T));
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
                        const char *name_,
                        mode_t mode = 0666,
                        const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100)) :
                        name (GetName (name_)),
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

                /// \brief
                /// Synthesize a lock name from object name and "_lock";
                /// \param[in] name Object name.
                /// \return name + "_lock";
                static std::string GetName (const char *name) {
                    return std::string (name) + "_lock";
                }
            };

            /// \brief
            /// Lock shared memory region to prevent swapping.
            /// \param[in] ptr Shared memory region to lock.
            /// \return true = Locked the given memory region. false = failed to lock.
            static bool LockRegion (void *ptr) {
                if (mlock (ptr, sizeof (T)) == 0) {
                #if defined (MADV_DONTDUMP)
                    if (madvise (ptr, sizeof (T), MADV_DONTDUMP) == 0) {
                #endif // defined (MADV_DONTDUMP)
                        return true;
                #if defined (MADV_DONTDUMP)
                    }
                    else {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        munlock (ptr, sizeof (T));
                        THEKOGANS_UTIL_OS_ERROR_CODE = errorCode;
                    }
                #endif // defined (MADV_DONTDUMP)
                }
                return false;
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_POSIXUtils_h)
