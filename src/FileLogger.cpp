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
#include "thekogans/util/internal.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/Directory.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Console.h"
#include "thekogans/util/FileLogger.h"

namespace thekogans {
    namespace util {

        void FileLogger::Log (
                const std::string & /*subsystem*/,
                ui32 /*level*/,
                const std::string &header,
                const std::string &message) throw () {
            if (!header.empty () || !message.empty ()) {
                ArchiveLog ();
                THEKOGANS_UTIL_TRY {
                    if (!header.empty ()) {
                        file.Write (&header[0], (ui32)header.size ());
                    }
                    if (!message.empty ()) {
                        file.Write (&message[0], (ui32)message.size ());
                    }
                }
                THEKOGANS_UTIL_CATCH (std::exception) {
                    // There is very little we can do here.
                #if defined (TOOLCHAIN_CONFIG_Debug)
                    Console::Instance ().PrintString (
                        FormatString (
                            "FileLogger::Log: %s\n",
                            exception.what ()),
                        Console::PRINT_CERR,
                        Console::TEXT_COLOR_RED);
                #else // defined (TOOLCHAIN_CONFIG_Debug)
                    (void)exception;
                #endif // defined (TOOLCHAIN_CONFIG_Debug)
                }
            }
        }

        void FileLogger::ArchiveLog () {
            if (archive && Path (path).Exists ()) {
                Directory::Entry entry (path);
                if (entry.size > maxLogFileSize) {
                    file.Close ();
                    if (Path (path + ".2").Exists ()) {
                        if (unlink (std::string (path + ".2").c_str ()) < 0) {
                            THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_POSIX_OS_ERROR_CODE);
                        }
                    }
                    if (Path (path + ".1").Exists ()) {
                        if (rename (std::string (path + ".1").c_str (),
                                std::string (path + ".2").c_str ()) < 0) {
                            THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_POSIX_OS_ERROR_CODE);
                        }
                    }
                    if (rename (path.c_str (), std::string (path + ".1").c_str ()) < 0) {
                        THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_POSIX_OS_ERROR_CODE);
                    }
                    file.Open (path, SimpleFile::WriteOnly | SimpleFile::Create | SimpleFile::Append);
                }
            }
        }

    } // namespace util
} // namespace thekogans
