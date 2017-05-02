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

#if !defined (__thekogans_util_DynamicLibrary_h)
#define __thekogans_util_DynamicLibrary_h

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
#endif // defined (TOOLCHAIN_OS_Windows)
#include <string>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct DynamicLibrary DynamicLibrary.h thekogans/util/DynamicLibrary.h
        ///
        /// \brief
        /// Implements loading dynamic/shared libraries (*.so, *.dylib, *dll).

        struct _LIB_THEKOGANS_UTIL_DECL DynamicLibrary {
        private:
            /// \brief
            /// Opaque OS specific library pointer.
            void *library;

        public:
            /// \brief
            /// ctor.
            /// \param[in] library_ Wrap an OS library handle.
            DynamicLibrary (void *library_ = 0) :
                library (library_) {}
            /// \brief
            /// dtor.
            /// NOTE: dtor does not call Unload. This is by design.
            ~DynamicLibrary () {}

            /// \brief
            /// FIXME: In the best of all possible worlds this api would be
            /// unnecessary. But because legacy code is written around void *
            /// This accessor is implemented here.
            /// \return Handle to the library.
            inline void *GetHandle () const {
                return library;
            }
            /// \brief
            /// Load a library given a path. If unsuccessful, throw an Exception.
            /// \param[in] path Library to load.
            void Load (const std::string &path);
            /// \brief
            /// Free/Unload a library previously loaded (passed in to ctor).
            /// If unsuccessful, throw an Exception.
            void Unload ();
            /// \brief
            /// Look up a function address given a name.
            /// If unsuccessful, throw an Exception.
            /// \param[in] name Name of symbol to look up.
            /// \return Pointer to symbol.
            void *GetProc (const std::string &name) const;
            /// \brief
            /// Return the library pathname.
            /// IMPORTANT: When using dl* library, this function uses
            /// dladdr against a local symbol. This will not work if util
            /// is compiled as a standalone so.
            /// \return Library path.
            std::string GetPathName () const;
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_DynamicLibrary_h)
