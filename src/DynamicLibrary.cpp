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
    #include <dlfcn.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Exception.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/DynamicLibrary.h"

namespace thekogans {
    namespace util {

        void DynamicLibrary::Load (const std::string &path) {
            if (library != 0) {
                Unload ();
            }
        #if defined (TOOLCHAIN_OS_Windows)
            library = LoadLibraryExW (UTF8ToUTF16 (path).c_str (), 0, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (library == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            // FIXME: encapsulate RTLD_LAZY
            library = dlopen (path.c_str (), RTLD_LAZY);
            if (library == 0) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("%s", dlerror ());
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void DynamicLibrary::Unload () {
            if (library == 0) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("%s", "library == 0");
            }
        #if defined (TOOLCHAIN_OS_Windows)
            if (FreeLibrary ((HMODULE)library) != TRUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            if (dlclose (library) != 0) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("%s", dlerror ());
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
            library = 0;
        }

        void *DynamicLibrary::GetProc (const std::string &name) const {
            if (library == 0) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("%s", "library == 0");
            }
            void *proc;
        #if defined (TOOLCHAIN_OS_Windows)
            proc = (void *)GetProcAddress ((HMODULE)library, name.c_str ());
            if (proc == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            proc = dlsym (library, name.c_str ());
            if (proc == 0) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("%s", dlerror ());
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
            return proc;
        }

    #if !defined (TOOLCHAIN_OS_Windows)
        const int dladdrDummySymbol = 0;
    #endif // !defined (TOOLCHAIN_OS_Windows)

        std::string DynamicLibrary::GetPathName () const {
            if (library != 0) {
            #if defined (TOOLCHAIN_OS_Windows)
                wchar_t pathName[MAX_PATH];
                if (GetModuleFileNameW ((HMODULE)library, pathName, MAX_PATH) == 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                return UTF16ToUTF8 (pathName, wcslen (pathName));
            #else // defined (TOOLCHAIN_OS_Windows)
                Dl_info info;
                if (dladdr (&dladdrDummySymbol, &info) == 0) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("%s", dlerror ());
                }
                return info.dli_fname;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("%s", "library == 0");
            }
        }

    } // namespace util
} // namespace thekogans
