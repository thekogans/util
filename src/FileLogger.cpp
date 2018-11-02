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

#include <cassert>
#include <cstdio>
#include <iostream>
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Path.h"
#include "thekogans/util/Directory.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Console.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/FileLogger.h"

namespace thekogans {
    namespace util {

        void FileLogger::Log (
                const std::string & /*subsystem*/,
                ui32 /*level*/,
                const std::string &header,
                const std::string &message) throw () {
            if (!header.empty () || !message.empty ()) {
                THEKOGANS_UTIL_TRY {
                    ArchiveLog ();
                    if (!header.empty ()) {
                        file.Write (header.c_str (), header.size ());
                    }
                    if (!message.empty ()) {
                        file.Write (message.c_str (), message.size ());
                    }
                }
                THEKOGANS_UTIL_CATCH (std::exception) {
                    // There is very little we can do here.
                #if defined (TOOLCHAIN_CONFIG_Debug)
                    Console::Instance ().PrintString (
                        FormatString (
                            "FileLogger::Log: %s\n",
                            exception.what ()),
                        Console::StdErr,
                        Console::TEXT_COLOR_RED);
                #else // defined (TOOLCHAIN_CONFIG_Debug)
                    (void)exception;
                #endif // defined (TOOLCHAIN_CONFIG_Debug)
                }
            }
        }

        void FileLogger::ArchiveLog () {
            if (archive && archiveCount > 0 && Path (path).Exists ()) {
                Directory::Entry entry (path);
                if (entry.size > maxLogFileSize) {
                    file.Close ();
                    std::size_t archiveNumber = archiveCount;
                    std::string archivePath =
                        FormatString (
                            "%s." THEKOGANS_UTIL_SIZE_T_FORMAT,
                            path.c_str (),
                            archiveNumber--);
                    if (Path (archivePath).Exists ()) {
                        if (unlink (archivePath.c_str ()) < 0) {
                            THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_POSIX_OS_ERROR_CODE);
                        }
                    }
                    while (archiveNumber > 0) {
                        std::string archivePath =
                            FormatString (
                                "%s." THEKOGANS_UTIL_SIZE_T_FORMAT,
                                path.c_str (),
                                archiveNumber);
                        if (Path (archivePath).Exists ()) {
                            std::string nextArchivePath =
                                FormatString (
                                    "%s." THEKOGANS_UTIL_SIZE_T_FORMAT,
                                    path.c_str (),
                                    archiveNumber + 1);
                            if (rename (archivePath.c_str (), nextArchivePath.c_str ()) < 0) {
                                THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_POSIX_OS_ERROR_CODE);
                            }
                        }
                        --archiveNumber;
                    }
                    if (rename (path.c_str (), std::string (path + ".1").c_str ()) < 0) {
                        THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_POSIX_OS_ERROR_CODE);
                    }
                    file.Open (path, SimpleFile::ReadWrite | SimpleFile::Create | SimpleFile::Append);
                }
            }
        }

    } // namespace util
} // namespace thekogans
