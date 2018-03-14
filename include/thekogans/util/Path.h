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

#if !defined (__thekogans_util_Path_h)
#define __thekogans_util_Path_h

#include <string>
#include <list>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \struct Path Path.h thekogans/util/Path.h
        ///
        /// \brief
        /// Path provides a platform independent way of manipulating
        /// file system paths. Most apis are textual in nature
        /// (manning they only work on strings, and do no validity
        /// checking).
        /// IMPORTANT: UNC paths are not supported.
        /// VERY IMPORTANT: While great pains were taken to hide
        /// Windows drive garbage, it is by no means perfect.
        /// Specifically, GetComponents might return a drive letter
        /// as the first component (if one existed in the path to
        /// begin with). You can test for that by checking the
        /// following in the first component:
        /// - length == 2
        /// - [0] = [a-z]|[A-Z]
        /// - [1] = ':'

        struct _LIB_THEKOGANS_UTIL_DECL Path {
            /// \brief
            /// The path.
            std::string path;

            /// \brief
            /// ctor.
            /// \param[in] path_ Path to initialize to.
            explicit Path (const std::string &path_) :
                path (path_) {}

            /// \brief
            /// Return the current directory path.
            /// \return Current directory path.
            static std::string GetCurrDirectory ();
            /// \brief
            /// Return the temporary directory path.
            /// \return Temporary directory path.
            static std::string GetTempDirectory ();
            /// \brief
            /// Return the current user home directory path.
            /// \return Current user home directory path.
            static std::string GetHomeDirectory ();

            /// \brief
            /// Return system native path seperator.
            /// Windows = '\', POSIX = '/'
            static char GetNativePathSeparator ();
            /// \brief
            /// Convert all path seperators to native form.
            std::string ToNativePathSeparator () const;
            /// \brief
            /// Convert all '/' seperators to '\'.
            std::string ToWindowsPathSeparator () const;
            /// \brief
            /// Convert all '\' seperators to '/'.
            std::string ToUnixPathSeparator () const;

            /// \brief
            /// Return true if path is empty.
            /// \return true = path is empty, false = path is not empty.
            inline bool IsEmpty () const {
                return path.empty ();
            }

        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// On windows, if the path contains a drive,
            /// return the letter, -1 otherwise.
            /// \return If path contains a drive, return the letter,
            /// -1 otherwise.
            char GetDrive () const;
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// Return true if path is absolute, false if relative.
            /// NOTE: On Windows, this function will take the drive
            /// designation in to account.
            /// \return true = path is absolute, false = path is relative.
            bool IsAbsolute () const;
            /// \brief
            /// Convert a relative path to it's canonical form.
            /// \return Canonical (absolute) form of the path.
            std::string MakeAbsolute () const;

            /// \brief
            /// Return a list of path components. If on Windows, and
            /// the path contains a drive letter, the first component
            /// will be that drive letter. Followed by all directory
            /// names, followed by file name.
            /// \param[out] components List to store path components.
            /// \return true = Original path was absolute,
            /// false = Original path was relative.
            bool GetComponents (std::list<std::string> &components) const;

            /// \brief
            /// Return path leading up to the last component.
            /// \return Path leading up to the last component.
            std::string GetDirectory () const;
            /// \brief
            /// Return directoy name.
            /// \return Directory name.
            std::string GetDirectoryName () const;
            /// \brief
            /// Return file name + extension.
            /// \return File name + extension.
            std::string GetFullFileName () const;
            /// \brief
            /// Return file name.
            /// \return File name.
            std::string GetFileName () const;
            /// \brief
            /// Return extension.
            /// \return Extension.
            std::string GetExtension () const;

            /// \brief
            /// Check for path existance.
            /// \return true = exists, false = does not exist.
            bool Exists () const;
            /// \brief
            /// Move the underlying file/direcory pointed by path.
            /// \param[in] to Destination to move the path to.
            void Move (const std::string &to) const;
            /// \brief
            /// Delete the path. If the path points to a
            /// directory, will delete the whole branch if
            /// recursive == true.
            /// \param[in] recursive true = delete branch,
            /// false = delete file or empty directory.
            void Delete (bool recursive = true) const;
        };

        /// \brief
        /// Create a path from directory and name.
        /// \param[in] directory Directory part of the path.
        /// \param[in] name File/Folder part of the path.
        /// \return Path from directory and name.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API MakePath (
            const std::string &directory,
            const std::string &name);
        /// \brief
        /// Create a path from a list of components.
        /// \param[in] components List of path components.
        /// \param[in] absolute true = start with path seperator,
        /// false = start with empty path.
        /// \return Path from a list of components.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API MakePath (
            const std::list<std::string> &components,
            bool absolute = false);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Path_h)
