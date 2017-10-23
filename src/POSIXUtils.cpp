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

#include "thekogans/util/Exception.h"
#include "thekogans/util/POSIXUtils.h"

namespace thekogans {
    namespace util {

        SharedLock::SharedLock (
                const std::string &name_,
                mode_t mode,
                const TimeSpec &timeSpec) :
                name (name_),
                handle (shm_open (name.c_str (), O_RDWR | O_CREAT | O_EXCL, mode)) {
            while (handle == -1) {
                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                if (errorCode != EEXIST) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
                Sleep (timeSpec);
                handle = shm_open (name.c_str (), O_RDWR | O_CREAT | O_EXCL, mode);
            }
        }

        SharedLock::~SharedLock () {
            close (handle);
            shm_unlink (name.c_str ());
        }

    } // namespace util
} // namespace thekogans
