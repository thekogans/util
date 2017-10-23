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
#include <string>
#include "thekogans/util/Types.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Exception.h"

namespace thekogans {
    namespace util {

        struct SharedLock {
            std::string name;
            THEKOGANS_UTIL_HANDLE handle;

            SharedLock (
                const std::string &name_,
                mode_t mode = 0666,
                const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100));
            ~SharedLock ();
        };

        template<typename T>
        struct SharedObject {
            char name[NAME_MAX];
            ui32 refCount;

            SharedObject (const char *name_) :
                    refCount (1) {
                strncpy (name, name_, NAME_MAX);
            }

            struct Creator {
                virtual ~Creator () {}

                virtual T *operator () (
                    void * /*ptr*/,
                    const char * /*name*/) const = 0;
            };

            static T *Create (
                    const char *name,
                    const Creator &creator,
                    mode_t mode = 0666) {
                if (name != 0) {
                    SharedLock lock (name + std::string ("_lock"));
                    struct File {
                        THEKOGANS_UTIL_HANDLE handle;
                        bool owner;
                        File (
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
                        ~File () {
                            close (handle);
                        }
                    } file (name, mode);
                    void *ptr = mmap (0, sizeof (T), PROT_READ | PROT_WRITE,
                        MAP_SHARED, file.handle, 0);
                    if (ptr != 0) {
                        T *t;
                        if (file.owner) {
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

            static void Destroy (T *t) {
                SharedLock lock (t->name + std::string ("_lock"));
                if (--t->refCount == 0) {
                    t->~T ();
                    shm_unlink (t->name);
                }
                munmap (t, sizeof (T));
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_POSIXUtils_h)
