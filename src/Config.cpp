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

#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/DynamicCreatable.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/ConsoleLogger.h"
#include "thekogans/util/Console.h"

namespace thekogans {
    namespace util {

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void StaticInit () {
            DynamicCreatable::StaticInit ();
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        _LIB_THEKOGANS_UTIL_DECL void Log (
                unsigned int decorations,
                const char *subsystem,
                unsigned int level,
                const char *file,
                const char *function,
                unsigned int line,
                const char *buildTime,
                const char *format,
                ...) {
            std::string header = LoggerMgr::FormatHeader (
                decorations,
                subsystem,
                level,
                file,
                function,
                line,
                buildTime);
            va_list argptr;
            va_start (argptr, format);
            std::string message = FormatStringHelper (format, argptr);
            va_end (argptr);
            Log (subsystem, level, header, message);
        }

        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API Log (
                const char *subsystem,
                unsigned int level,
                const std::string &header,
                const std::string &message) {
            if (GlobalLoggerMgr::IsInstanceCreated ()) {
                GlobalLoggerMgr::Instance ()->Log (subsystem, level, header, message);
                // This function is usually called right before the
                // process exits. In case the user forgot to call flush,
                // we do it here so that this important error message is
                // written to the log.
                GlobalLoggerMgr::Instance ()->Flush ();
            }
            else if (Console::IsInstanceCreated ()) {
                Console::Instance ()->PrintString (
                    header + message,
                    Console::StdErr,
                    ConsoleLogger::DefaultColorScheme::GetColorForLevel (level));
            }
            else {
                std::cerr << header << message;
            }
        }

    } // namespace util
} // namespace thekogans
