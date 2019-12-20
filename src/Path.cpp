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
    #include <direct.h>
#else // defined (TOOLCHAIN_OS_Windows)
#if defined (TOOLCHAIN_OS_Linux)
    #include <pwd.h>
#endif // defined (TOOLCHAIN_OS_Linux)
    #include <sys/stat.h>
    #include <climits>
    #include <cstdio>
    #include <cstdlib>
    #include <unistd.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <sys/types.h>
#include <cctype>
#include <cassert>
#include <algorithm>
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/StringUtils.h"
    #include "thekogans/util/WindowsUtils.h"
#elif defined (TOOLCHAIN_OS_Linux)
    #include "thekogans/util/LinuxUtils.h"
#elif defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Directory.h"
#include "thekogans/util/File.h"
#include "thekogans/util/Path.h"

namespace thekogans {
    namespace util {

        namespace {
            const char WINDOWS_PATH_SEPARATOR = '\\';
            const char UNIX_PATH_SEPARATOR = '/';

        #if defined (TOOLCHAIN_OS_Windows)
            const char NATIVE_PATH_SEPARATOR = WINDOWS_PATH_SEPARATOR;
            const char OTHER_PATH_SEPARATOR = UNIX_PATH_SEPARATOR;
            const char DRIVE_LETTER_DELIMITER = ':';
        #else // defined (TOOLCHAIN_OS_Windows)
            const char NATIVE_PATH_SEPARATOR = UNIX_PATH_SEPARATOR;
            const char OTHER_PATH_SEPARATOR = WINDOWS_PATH_SEPARATOR;
        #endif // defined (TOOLCHAIN_OS_Windows)
            const char PATH_SEPARATORS[3] = {
                NATIVE_PATH_SEPARATOR,
                OTHER_PATH_SEPARATOR,
                0
            };

            const char FILE_EXTENSION_DELIMITER = '.';
        }

        std::string Path::GetCurrDirectory () {
            std::string path;
        #if defined (TOOLCHAIN_OS_Windows)
            char *buffer = _getcwd (0, 0);
        #else // defined (TOOLCHAIN_OS_Windows)
            char *buffer = getcwd (0, 0);
        #endif // defined (TOOLCHAIN_OS_Windows)
            if (buffer != 0) {
                path = buffer;
                free (buffer);
            }
            else {
                THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_POSIX_OS_ERROR_CODE);
            }
            return path;
        }

        std::string Path::GetTempDirectory () {
        #if defined (TOOLCHAIN_OS_Windows)
            wchar_t tempDirectory[MAX_PATH + 1];
            if (GetTempPathW (MAX_PATH, tempDirectory) != 0) {
                return UTF16ToUTF8 (tempDirectory, wcslen (tempDirectory));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            std::string tempDirectory = GetEnvironmentVariable ("TMPDIR");
        #if defined (P_tmpdir) || defined (_PATH_TMP)
            if (tempDirectory.empty ()) {
            #if defined (P_tmpdir)
                tempDirectory = P_tmpdir;
                if (tempDirectory.empty ()) {
            #endif // defined (P_tmpdir)
            #if defined (_PATH_TMP)
                    tempDirectory = _PATH_TMP;
            #endif // defined (_PATH_TMP)
            #if defined (P_tmpdir)
                }
            #endif // defined (P_tmpdir)
            }
        #endif // defined (P_tmpdir) || defined (_PATH_TMP)
            return tempDirectory;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        std::string Path::GetHomeDirectory () {
        #if defined (TOOLCHAIN_OS_Windows)
            return GetEnvironmentVariable ("USERPROFILE");
        #elif defined (TOOLCHAIN_OS_Linux)
            std::string homeDirectory = GetEnvironmentVariable ("HOME");
            if (homeDirectory.empty ()) {
                const char *dir = getpwuid (getuid ())->pw_dir;
                if (dir != 0) {
                    homeDirectory = dir;
                }
            }
            return homeDirectory;
        #elif defined (TOOLCHAIN_OS_OSX)
            return util::GetHomeDirectory ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        char Path::GetNativePathSeparator () {
            return NATIVE_PATH_SEPARATOR;
        }

        std::string Path::ToNativePathSeparator () const {
            std::string tmpPath = path;
            std::replace (tmpPath.begin (), tmpPath.end (),
                OTHER_PATH_SEPARATOR, NATIVE_PATH_SEPARATOR);
            return tmpPath;
        }

        std::string Path::ToWindowsPathSeparator () const {
            std::string tmpPath = path;
            std::replace (tmpPath.begin (), tmpPath.end (),
                UNIX_PATH_SEPARATOR, WINDOWS_PATH_SEPARATOR);
            return tmpPath;
        }

        std::string Path::ToUnixPathSeparator () const {
            std::string tmpPath = path;
            std::replace (tmpPath.begin (), tmpPath.end (),
                WINDOWS_PATH_SEPARATOR, UNIX_PATH_SEPARATOR);
            return tmpPath;
        }

    #if defined (TOOLCHAIN_OS_Windows)
        char Path::GetDrive () const {
            return path.size () > 1 && path[1] == DRIVE_LETTER_DELIMITER ? path[0] : -1;
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        bool Path::IsAbsolute () const {
            std::string::size_type start = 0;
        #if defined (TOOLCHAIN_OS_Windows)
            char drive = GetDrive ();
            if (drive != -1) {
                start = 2;
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
            return start < path.size () &&
                (path[start] == NATIVE_PATH_SEPARATOR ||
                    path[start] == OTHER_PATH_SEPARATOR);
        }

        std::string Path::MakeAbsolute () const {
        #if defined (TOOLCHAIN_OS_Windows)
            wchar_t fullPath[32767];
            std::size_t length = GetFullPathNameW (UTF8ToUTF16 (path).c_str (), fullPath, 32767, 0);
            if (length > 0) {
                return UTF16ToUTF8 (fullPath, length);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            struct FullPath {
                char *fullPath;
                FullPath (const std::string &path) :
                        fullPath (realpath (path.c_str (), 0)) {
                    if (fullPath == 0) {
                        THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_POSIX_OS_ERROR_CODE);
                    }
                }
                ~FullPath () {
                    free (fullPath);
                }
            } fullPath (path);
            return fullPath.fullPath;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        bool Path::GetComponents (std::list<std::string> &components) const {
            std::string::size_type start = 0;
        #if defined (TOOLCHAIN_OS_Windows)
            char drive = GetDrive ();
            if (drive != -1) {
                const char driveString[3] = {drive, DRIVE_LETTER_DELIMITER, '\0'};
                components.push_back (driveString);
                start = 2;
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
            std::string::size_type end = path.find_first_of (PATH_SEPARATORS, start);
            bool absolute = end == start;
            do {
                std::string component;
                if (end == std::string::npos) {
                    component = path.substr (start);
                }
                else {
                    component = path.substr (start, end - start);
                }
                if (!component.empty ()) {
                    components.push_back (component);
                }
                start = end + 1;
                end = path.find_first_of (PATH_SEPARATORS, start);
            } while (start != 0);
            return absolute;
        }

        std::string Path::GetDirectory (bool includePathSeparator) const {
            std::string::size_type pathSeparator =
                path.find_last_of (PATH_SEPARATORS);
            return pathSeparator == std::string::npos ?
                std::string () : path.substr (0, pathSeparator + (includePathSeparator ? 1 : 0));
        }

        std::string Path::GetDirectoryName () const {
            std::string::size_type pathSeparator1 =
                path.find_last_of (PATH_SEPARATORS);
            if (pathSeparator1 == std::string::npos) {
                return path;
            }
            if (pathSeparator1 < path.size () - 1) {
                return path.substr (pathSeparator1 + 1);
            }
            --pathSeparator1;
            std::string::size_type pathSeparator2 =
                path.find_last_of (PATH_SEPARATORS, pathSeparator1);
            if (pathSeparator2 == std::string::npos) {
                pathSeparator2 = 0;
            }
            else {
                ++pathSeparator2;
            }
            return path.substr (pathSeparator2, pathSeparator1 - pathSeparator2 + 1);
        }

        std::string Path::GetFullFileName () const {
            std::string::size_type pathSeparator =
                path.find_last_of (PATH_SEPARATORS);
            return pathSeparator == std::string::npos ?
                path : path.substr (pathSeparator + 1);
        }

        std::string Path::GetFileName () const {
            std::string::size_type pathSeparator =
                path.find_last_of (PATH_SEPARATORS);
            std::string file = pathSeparator == std::string::npos ?
                path : path.substr (pathSeparator + 1);
            return file.substr (0, file.find_last_of (FILE_EXTENSION_DELIMITER));
        }

        std::string Path::GetExtension (bool includeDot) const {
            std::string::size_type dot = path.find_last_of (FILE_EXTENSION_DELIMITER);
            return dot == std::string::npos ? std::string () : path.substr (dot + (!includeDot ? 1 : 0));
        }

        bool Path::Exists () const {
        #if defined (TOOLCHAIN_OS_Windows)
            WIN32_FILE_ATTRIBUTE_DATA attributeData;
            return GetFileAttributesExW (UTF8ToUTF16 (path).c_str (),
                GetFileExInfoStandard, &attributeData) == TRUE;
        #else // defined (TOOLCHAIN_OS_Windows)
            STAT_STRUCT buf;
            return STAT_FUNC (path.c_str (), &buf) == 0;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Path::Move (const std::string &to) const {
            if (path != to) {
                Path toPath (to);
                if (toPath.Exists ()) {
                    toPath.Delete ();
                }
                if (rename (path.c_str (), to.c_str ()) < 0) {
                    THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_POSIX_OS_ERROR_CODE);
                }
            }
        }

        void Path::Delete (bool recursive) const {
            Directory::Entry entry (path);
            if (entry.type == Directory::Entry::Folder) {
                Directory::Delete (path, recursive);
            }
            else if (entry.type == Directory::Entry::File ||
                    entry.type == Directory::Entry::Link) {
                File::Delete (path);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API MakePath (
                const std::string &directory,
                const std::string &name) {
            std::string path;
        #if defined (TOOLCHAIN_OS_Windows)
            {
                char directoryDrive = Path (directory).GetDrive ();
                char nameDrive = Path (name).GetDrive ();
                // (-1, -1) - valid
                // (-1, ?) - valid
                // (?, -1) - valid FIXME: it's harmless, but just doesn't smell right.
                // (?, ?) - valid iff ? == ?
                if ((directoryDrive == -1 && nameDrive == -1) ||
                        (directoryDrive != -1 && nameDrive == -1)) {
                    path = directory;
                }
                else if ((directoryDrive == -1 && nameDrive != -1) ||
                         directoryDrive == nameDrive) {
                    return MakePath (directory, &name[2]);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Directory and name are from different drives: %s (%s)",
                        name.c_str (),
                        directory.c_str ());
                }
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            path = directory;
        #endif // defined (TOOLCHAIN_OS_Windows)
            if (!directory.empty () &&
                    directory[directory.size () - 1] != NATIVE_PATH_SEPARATOR &&
                    directory[directory.size () - 1] != OTHER_PATH_SEPARATOR &&
                    !name.empty () &&
                    name[0] != NATIVE_PATH_SEPARATOR &&
                    name[0] != OTHER_PATH_SEPARATOR) {
                path += NATIVE_PATH_SEPARATOR;
            }
            path += name;
            return path;
        }

    #if defined (TOOLCHAIN_OS_Windows)
        namespace {
            inline bool IsDrive (const std::string &path) {
                return path.size () == 2 && isalpha (path[0]) && path[1] == DRIVE_LETTER_DELIMITER;
            }
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API MakePath (
                const std::list<std::string> &components,
                bool absolute) {
            std::string path;
            std::list<std::string>::const_iterator it = components.begin ();
            std::list<std::string>::const_iterator end = components.end ();
        #if defined (TOOLCHAIN_OS_Windows)
            if (it != end && IsDrive (*it)) {
                path = *it++;
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
            if (absolute) {
                path += NATIVE_PATH_SEPARATOR;
            }
            for (; it != end; ++it) {
                path = MakePath (path, *it);
            }
            return path;
        }

    } // namespace util
} // namespace thekogans
