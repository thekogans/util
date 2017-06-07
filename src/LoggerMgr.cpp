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

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cassert>
#include <iostream>
#include "thekogans/util/internal.h"
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Console.h"
#include "thekogans/util/LoggerMgr.h"

namespace thekogans {
    namespace util {

        const char * const LoggerMgr::SUBSYSTEM_GLOBAL = "global";

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX (LoggerMgr::Entry, SpinLock, 5)

        void LoggerMgr::GetLogLevels (std::list<ui32> &levels) {
            levels.push_back (Error);
            levels.push_back (Warning);
            levels.push_back (Info);
            levels.push_back (Debug);
            levels.push_back (Development);
        }

        std::string LoggerMgr::levelTostring (ui32 level) {
            return level == Error ? "Error" :
                level == Warning ? "Warning" :
                level == Info ? "Info" :
                level == Debug ? "Debug" :
                level == Development ? "Development" : "Invalid";
        }

        ui32 LoggerMgr::stringTolevel (const std::string &level) {
            return level == "Error" ? Error :
                level == "Warning" ? Warning :
                level == "Info" ? Info :
                level == "Debug" ? Debug :
                level == "Development" ? Development : Invalid;
        }

        std::string LoggerMgr::decorationsTostring (ui32 decorations) {
            if (decorations == NoDecorations) {
                return "NoDecorations";
            }
            else if (decorations == All) {
                return "All";
            }
            else if (decorations == SubsystemAll) {
                return "SubsystemAll";
            }
            std::string value;
            if (Flags32 (decorations).Test (MessageSeparator)) {
                value = "MessageSeparator";
            }
            else if (Flags32 (decorations).Test (Subsystem)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "Subsystem";
            }
            else if (Flags32 (decorations).Test (Level)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "Level";
            }
            else if (Flags32 (decorations).Test (DateTime)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "DateTime";
            }
            else if (Flags32 (decorations).Test (HRTime)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "HRTime";
            }
            else if (Flags32 (decorations).Test (Host)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "Host";
            }
            else if (Flags32 (decorations).Test (ProcessName)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "ProcessName";
            }
            else if (Flags32 (decorations).Test (ProcessId)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "ProcessId";
            }
            else if (Flags32 (decorations).Test (ThreadId)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "ThreadId";
            }
            else if (Flags32 (decorations).Test (Location)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "Location";
            }
            else if (Flags32 (decorations).Test (Multiline)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "Multiline";
            }
            return value;
        }

        namespace {
            ui32 stringTodecoration (const std::string &decoration) {
                if (decoration == "MessageSeparator") {
                    return LoggerMgr::MessageSeparator;
                }
                else if (decoration == "Subsystem") {
                    return LoggerMgr::Subsystem;
                }
                else if (decoration == "Level") {
                    return LoggerMgr::Level;
                }
                else if (decoration == "DateTime") {
                    return LoggerMgr::DateTime;
                }
                else if (decoration == "HRTime") {
                    return LoggerMgr::HRTime;
                }
                else if (decoration == "Host") {
                    return LoggerMgr::Host;
                }
                else if (decoration == "ProcessName") {
                    return LoggerMgr::ProcessName;
                }
                else if (decoration == "ProcessId") {
                    return LoggerMgr::ProcessId;
                }
                else if (decoration == "ThreadId") {
                    return LoggerMgr::ThreadId;
                }
                else if (decoration == "Location") {
                    return LoggerMgr::Location;
                }
                else if (decoration == "Multiline") {
                    return LoggerMgr::Multiline;
                }
                return 0;
            }
        }

        ui32 LoggerMgr::stringTodecorations (const std::string &decorations) {
            if (decorations == "NoDecorations") {
                return NoDecorations;
            }
            else if (decorations == "All") {
                return All;
            }
            else if (decorations == "SubsystemAll") {
                return SubsystemAll;
            }
            ui32 value = 0;
            std::string::size_type lastPipe = 0;
            std::string::size_type currPipe = decorations.find_first_of ('|', 0);
            for (; currPipe == std::string::npos;
                    lastPipe = ++currPipe,
                    currPipe = decorations.find_first_of ('|', currPipe)) {
                value |= stringTodecoration (TrimSpaces (
                    decorations.substr (lastPipe, currPipe - lastPipe).c_str ()));
            }
            value |= stringTodecoration (TrimSpaces (
                decorations.substr (lastPipe).c_str ()));
            return value;
        }

        void LoggerMgr::Init (
                const std::string &processName_,
                bool blocking_,
                i32 priority,
                ui32 affinity) {
            processName = processName_;
            blocking = blocking_;
            if (!blocking) {
                Create (priority, affinity);
            }
        }

        void LoggerMgr::Reset (
                ui32 level_,
                ui32 decorations_) {
            Flush ();
            level = level_;
            decorations = decorations_;
            {
                LockGuard<Mutex> guard (loggerMapMutex);
                loggerMap.clear ();
            }
        }

        void LoggerMgr::AddLogger (
                const char *subsystem,
                Logger::Ptr logger) {
            if (subsystem != 0 && logger.Get () != 0) {
                LockGuard<Mutex> guard (loggerMapMutex);
                loggerMap[subsystem].push_back (logger);
            }
        }

        void LoggerMgr::AddLoggerList (
                const char *subsystem,
                const LoggerList &loggerList) {
            if (subsystem != 0) {
                LockGuard<Mutex> guard (loggerMapMutex);
                loggerMap[subsystem] = loggerList;
            }
        }

        namespace {
            void LogSubsystem (
                    const LoggerMgr::Entry &entry,
                    const LoggerMgr::LoggerList &loggerList) {
                for (LoggerMgr::LoggerList::const_iterator
                        it = loggerList.begin (),
                        end = loggerList.end (); it != end; ++it) {
                    THEKOGANS_UTIL_TRY {
                        if ((*it)->level >= entry.level) {
                            (*it)->Log (entry.subsystem, entry.level, entry.header, entry.message);
                        }
                    }
                    THEKOGANS_UTIL_CATCH (std::exception) {
                        // There is very little we can do here.
                    #if defined (TOOLCHAIN_CONFIG_Debug)
                        Console::Instance ().PrintString (
                            FormatString (
                                "LoggerMgr::Log: %s\n",
                                exception.what ()),
                            Console::PRINT_CERR,
                            Console::TEXT_COLOR_RED);
                    #else // defined (TOOLCHAIN_CONFIG_Debug)
                        (void)exception;
                    #endif // defined (TOOLCHAIN_CONFIG_Debug)
                    }
                }
            }
        }

        void LoggerMgr::Log (
                const char *subsystem,
                ui32 level,
                const char *file,
                const char *function,
                ui32 line,
                const char *buildTime,
                const char *format,
                ...) {
            std::string header;
            if (decorations != NoDecorations) {
                if (decorations.Test (MessageSeparator) && decorations.Test (Multiline)) {
                    header += std::string (80, '*');
                    header += "\n";
                }
                if (decorations.Test (Subsystem)) {
                    header += subsystem;
                    header += " ";
                }
                if (decorations.Test (Level)) {
                    header += levelTostring (level);
                    header += " ";
                }
                if (decorations.Test (DateTime)) {
                    char tmp[101] = {0};
                    time_t rawTime;
                    time (&rawTime);
                #if defined (TOOLCHAIN_OS_Windows)
                    struct tm tm;
                    localtime_s (&tm, &rawTime);
                    strftime (tmp, 100, "%b %d %X", &tm);
                #else // defined (TOOLCHAIN_OS_Windows)
                    strftime (tmp, 100, "%b %d %T", localtime (&rawTime));
                #endif // defined (TOOLCHAIN_OS_Windows)
                    header += tmp;
                    header += " ";
                }
                if (decorations.Test (HRTime)) {
                    header += FormatString (
                        "%011.4f ",
                        HRTimer::ToSeconds (HRTimer::ComputeEllapsedTime (startTime, HRTimer::Click ())));
                }
                if (decorations.Test (Host)) {
                    header += hostName;
                    header += " ";
                }
                if (decorations.Test (ProcessName)) {
                    header += processName;
                    header += " ";
                }
                if (decorations.Test (ProcessId | ThreadId)) {
                #if defined (TOOLCHAIN_OS_Windows)
                    header += FormatString ("[%u:%05u]",
                        GetCurrentProcessId (), GetCurrentThreadId ());
                #else // defined (TOOLCHAIN_OS_Windows)
                    header += FormatString ("[%u:%p]",
                        getpid (), (void *)pthread_self ());
                #endif // defined (TOOLCHAIN_OS_Windows)
                    header += " ";
                }
                else {
                    if (decorations.Test (ProcessId)) {
                    #if defined (TOOLCHAIN_OS_Windows)
                        header += FormatString ("[%u]", GetCurrentProcessId ());
                    #else // defined (TOOLCHAIN_OS_Windows)
                        header += FormatString ("[%u]", getpid ());
                    #endif // defined (TOOLCHAIN_OS_Windows)
                        header += " ";
                    }
                    else if (decorations.Test (ThreadId)) {
                    #if defined (TOOLCHAIN_OS_Windows)
                        header += FormatString ("[%05u]", GetCurrentThreadId ());
                    #else // defined (TOOLCHAIN_OS_Windows)
                        header += FormatString ("[%p]", (void *)pthread_self ());
                    #endif // defined (TOOLCHAIN_OS_Windows)
                        header += " ";
                    }
                }
                if (!header.empty () && decorations.Test (Multiline)) {
                    header += "\n";
                }
                if (decorations.Test (Location)) {
                    header += FormatString (
                        "%s:%s:%d (%s)", file, function, line, buildTime);
                    if (decorations.Test (Multiline)) {
                        header += "\n";
                    }
                }
            }
            std::string message;
            {
                va_list argptr;
                va_start (argptr, format);
                message = FormatStringHelper (format, argptr);
                va_end (argptr);
            }
            Log (subsystem, level, header, message);
        }

        void LoggerMgr::Log (
                const char *subsystem,
                ui32 level,
                const std::string &header,
                const std::string &message) {
            if (blocking) {
                Entry entry (subsystem, level, header, message);
                if (FilterEntry (entry)) {
                    LockGuard<Mutex> guard (loggerMapMutex);
                    LoggerMap::iterator it = loggerMap.find (subsystem);
                    if (it != loggerMap.end ()) {
                        LogSubsystem (entry, it->second);
                    }
                }
            }
            else {
                LockGuard<Mutex> guard (entryListMutex);
                entryList.push_back (new Entry (subsystem, level, header, message));
                entryListNotEmpty.Signal ();
            }
        }

        void LoggerMgr::AddFilter (Filter::UniquePtr filter) {
            if (filter.get () != 0) {
                LockGuard<Mutex> guard (filterListMutex);
                filterList.push_back (std::move (filter));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void LoggerMgr::Flush () {
            while (1) {
                LockGuard<Mutex> guard (entryListMutex);
                if (entryList.empty ()) {
                    break;
                }
                entryListNotEmpty.Wait (TimeSpec::FromMilliseconds (200));
            }
            // Flush is sometimes called right before the application
            // exits. In that case waiting for entryList to be empty
            // is not enough as there is a race between the
            // application closing and the LoggerMgr thread finishing
            // processing the entries. We grab this lock here to make
            // sure we finished processing all the entries before
            // exiting.
            volatile LockGuard<Mutex> guard (loggerMapMutex);
            Console::Instance ().FlushPrintQueue ();
        }

        void LoggerMgr::Run () throw () {
            while (1) {
                Entry::UniquePtr entry = Deq ();
                if (entry.get () != 0 && !entry->message.empty () && FilterEntry (*entry)) {
                    LockGuard<Mutex> guard (loggerMapMutex);
                    LoggerMap::iterator it = loggerMap.find (entry->subsystem);
                    if (it != loggerMap.end ()) {
                        LogSubsystem (*entry, it->second);
                    }
                }
            }
        }

        bool LoggerMgr::FilterEntry (Entry &entry) {
            LockGuard<Mutex> guard (filterListMutex);
            for (FilterList::iterator
                    it = filterList.begin (),
                    end = filterList.end (); it != end; ++it) {
                if (!(*it)->FilterEntry (entry)) {
                    return false;
                }
            }
            return true;
        }

        LoggerMgr::Entry::UniquePtr LoggerMgr::Deq () {
            LockGuard<Mutex> guard (entryListMutex);
            while (entryList.empty ()) {
                entryListNotEmpty.Wait ();
            }
            Entry::UniquePtr entry;
            assert (!entryList.empty ());
            if (!entryList.empty ()) {
                entry.reset (entryList.pop_front ());
            }
            return entry;
        }

    } // namespace util
} // namespace thekogans
