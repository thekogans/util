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
#include "thekogans/util/Heap.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Console.h"
#include "thekogans/util/SystemInfo.h"
#include "thekogans/util/LoggerMgr.h"

namespace thekogans {
    namespace util {

        const char * const LoggerMgr::SUBSYSTEM_GLOBAL = "global";

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (LoggerMgr::Entry)

        LoggerMgr::~LoggerMgr () {
            THEKOGANS_UTIL_TRY {
                const i64 TIMEOUT_SECONDS = 2;
                Flush (TimeSpec::FromSeconds (TIMEOUT_SECONDS));
            }
            THEKOGANS_UTIL_CATCH_ANY {
                // Not much we can do here. Try a last-ditch effort to
                // let the engineer know that the log was not flushed.
                std::cerr << "~LoggerMgr could not flush the log." << std::endl;
            }
        }

        void LoggerMgr::GetLevels (std::list<ui32> &levels) {
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

        void LoggerMgr::GetDecorations (std::list<ui32> &decorations) {
            decorations.push_back (NoDecorations);
            decorations.push_back (EntrySeparator);
            decorations.push_back (Subsystem);
            decorations.push_back (Level);
            decorations.push_back (Date);
            decorations.push_back (Time);
            decorations.push_back (HostName);
            decorations.push_back (ProcessId);
            decorations.push_back (ProcessPath);
            decorations.push_back (ProcessStartTime);
            decorations.push_back (ProcessUpTime);
            decorations.push_back (ThreadId);
            decorations.push_back (Location);
            decorations.push_back (Multiline);
            decorations.push_back (All);
            decorations.push_back (SubsystemAll);
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
            if (Flags32 (decorations).Test (EntrySeparator)) {
                value = "EntrySeparator";
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
            else if (Flags32 (decorations).Test (Date)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "Date";
            }
            else if (Flags32 (decorations).Test (Time)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "Time";
            }
            else if (Flags32 (decorations).Test (HostName)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "HostName";
            }
            else if (Flags32 (decorations).Test (ProcessId)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "ProcessId";
            }
            else if (Flags32 (decorations).Test (ProcessPath)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "ProcessPath";
            }
            else if (Flags32 (decorations).Test (ProcessStartTime)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "ProcessStartTime";
            }
            else if (Flags32 (decorations).Test (ProcessUpTime)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "ProcessUpTime";
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
                if (decoration == "EntrySeparator") {
                    return LoggerMgr::EntrySeparator;
                }
                else if (decoration == "Subsystem") {
                    return LoggerMgr::Subsystem;
                }
                else if (decoration == "Level") {
                    return LoggerMgr::Level;
                }
                else if (decoration == "Date") {
                    return LoggerMgr::Date;
                }
                else if (decoration == "Time") {
                    return LoggerMgr::Time;
                }
                else if (decoration == "HostName") {
                    return LoggerMgr::HostName;
                }
                else if (decoration == "ProcessId") {
                    return LoggerMgr::ProcessId;
                }
                else if (decoration == "ProcessPath") {
                    return LoggerMgr::ProcessPath;
                }
                else if (decoration == "ProcessStartTime") {
                    return LoggerMgr::ProcessStartTime;
                }
                else if (decoration == "ProcessUpTime") {
                    return LoggerMgr::ProcessUpTime;
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
            for (; currPipe != std::string::npos;
                    lastPipe = ++currPipe,
                    currPipe = decorations.find_first_of ('|', currPipe)) {
                value |= stringTodecoration (
                    TrimSpaces (
                        decorations.substr (lastPipe, currPipe - lastPipe).c_str ()));
            }
            value |= stringTodecoration (
                TrimSpaces (
                    decorations.substr (lastPipe).c_str ()));
            return value;
        }

        void LoggerMgr::Reset (
                ui32 level_,
                ui32 decorations_,
                ui32 flags,
                bool blocking,
                const std::string &name,
                i32 priority,
                ui32 affinity) {
            if (level_ >= Invalid && level_ <= MaxLevel && decorations_ <= SubsystemAll) {
                Flush ();
                LockGuard<Mutex> guard (mutex);
                level = level_;
                decorations = decorations_;
                if (Flags32 (flags).Test (ClearLoggers)) {
                    loggerMap.clear ();
                }
                if (Flags32 (flags).Test (ClearFilters)) {
                    filterList.clear ();
                }
                jobQueue.Reset (!blocking ?
                    new JobQueue (
                        name,
                        new RunLoop::FIFOJobExecutionPolicy,
                        1,
                        priority,
                        affinity) : 0);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void LoggerMgr::AddLogger (
                const char *subsystem,
                Logger::SharedPtr logger) {
            if (subsystem != nullptr && logger != nullptr) {
                LockGuard<Mutex> guard (mutex);
                loggerMap[subsystem].push_back (logger);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void LoggerMgr::AddLoggerList (
                const char *subsystem,
                const LoggerList &loggerList) {
            if (subsystem != nullptr && !loggerList.empty ()) {
                LockGuard<Mutex> guard (mutex);
                loggerMap[subsystem].insert (
                    loggerMap[subsystem].end (),
                    loggerList.begin (),
                    loggerList.end ());
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void LoggerMgr::AddDefaultLogger (Logger::SharedPtr logger) {
            if (logger != nullptr) {
                LockGuard<Mutex> guard (mutex);
                defaultLoggers.push_back (logger);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void LoggerMgr::AddDefaultLoggerList (const LoggerList &loggerList) {
            if (!loggerList.empty ()) {
                LockGuard<Mutex> guard (mutex);
                defaultLoggers.insert (
                    defaultLoggers.end (),
                    loggerList.begin (),
                    loggerList.end ());
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        std::string LoggerMgr::FormatHeader (
                ui32 decorations,
                const char *subsystem,
                ui32 level,
                const char *file,
                const char *function,
                ui32 line,
                const char *buildTime) {
            if (decorations <= SubsystemAll &&
                    subsystem != nullptr &&
                    level > Invalid && level <= MaxLevel &&
                    file != nullptr &&
                    function != nullptr &&
                    buildTime != nullptr) {
                std::string header;
                if (decorations != NoDecorations) {
                    Flags32 flags (decorations);
                    if (flags.Test (EntrySeparator) && flags.Test (Multiline)) {
                        header += std::string (80, '*');
                        header += "\n";
                    }
                    if (flags.Test (Subsystem)) {
                        header += subsystem;
                        header += " ";
                    }
                    if (flags.Test (Level)) {
                        header += levelTostring (level);
                        header += " ";
                    }
                    TimeSpec timeSpec = GetCurrentTime ();
                    if (flags.Test (Date)) {
                        header += FormatTimeSpec (timeSpec, "%a %b %d %Y ");
                    }
                    if (flags.Test (Time)) {
                        header += FormatTimeSpec (timeSpec, "%X ");
                    }
                    if (flags.Test (HostName)) {
                        header += SystemInfo::Instance ()->GetHostName ();
                        header += " ";
                    }
                    if (flags.Test (ProcessId | ThreadId)) {
                        header += FormatString ("[%u:%s] ",
                            SystemInfo::Instance ()->GetProcessId (),
                            FormatThreadId (Thread::GetCurrThreadId ()).c_str ());
                    }
                    else if (flags.Test (ProcessId)) {
                        header += FormatString ("[%u] ",
                            SystemInfo::Instance ()->GetProcessId ());
                    }
                    else if (flags.Test (ThreadId)) {
                        header += FormatString ("[%s] ",
                            FormatThreadId (Thread::GetCurrThreadId ()).c_str ());
                    }
                    if (flags.Test (ProcessPath)) {
                        header += SystemInfo::Instance ()->GetProcessPath ();
                        header += " ";
                    }
                    if (flags.Test (ProcessStartTime)) {
                        header += FormatTimeSpec (SystemInfo::Instance ()->GetProcessStartTime ());
                        header += " ";
                    }
                    if (flags.Test (ProcessUpTime)) {
                        TimeSpec upTime = timeSpec - SystemInfo::Instance ()->GetProcessStartTime ();
                        i64 days = upTime.seconds / (3600 * 24);
                        i64 hours = upTime.seconds % (3600 * 24) / 3600;
                        i64 minutes = upTime.seconds % 3600 / 60;
                        i64 seconds = upTime.seconds % 60;
                        i64 milliseconds = upTime.nanoseconds / 1000000;
                        if (days > 0) {
                            header += FormatString ("[%dd %dh %dm %ds %dms] ",
                                days, hours, minutes, seconds, milliseconds);
                        }
                        else if (hours > 0) {
                            header += FormatString ("[%dh %dm %ds %dms] ",
                                hours, minutes, seconds, milliseconds);
                        }
                        else if (minutes > 0) {
                            header += FormatString ("[%dm %ds %dms] ", minutes, seconds, milliseconds);
                        }
                        else if (seconds > 0) {
                            header += FormatString ("[%ds %dms] ", seconds, milliseconds);
                        }
                        else if (milliseconds > 0) {
                            header += FormatString ("[%dms] ", milliseconds);
                        }
                    }
                    if (!header.empty () && flags.Test (Multiline)) {
                        header += "\n";
                    }
                    if (flags.Test (Location)) {
                        header += FormatString (
                            "%s:%s:%d (%s)", file, function, line, buildTime);
                        if (flags.Test (Multiline)) {
                            header += "\n";
                        }
                    }
                }
                return header;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
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
            std::string header =
                FormatHeader (decorations, subsystem, level, file, function, line, buildTime);
            std::string message;
            {
                va_list argptr;
                va_start (argptr, format);
                message = FormatStringHelper (format, argptr);
                va_end (argptr);
            }
            Log (subsystem, level, header, message);
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
                    #if defined (THEKOGANS_UTIL_CONFIG_Debug)
                        Console::Instance ()->PrintString (
                            FormatString (
                                "LoggerMgr::Log: %s\n",
                                exception.what ()),
                            Console::StdErr,
                            Console::TEXT_COLOR_RED);
                    #else // defined (THEKOGANS_UTIL_CONFIG_Debug)
                        (void)exception;
                    #endif // defined (THEKOGANS_UTIL_CONFIG_Debug)
                    }
                }
            }

            struct LogSubsystemJob : public RunLoop::Job {
                LoggerMgr::Entry::UniquePtr entry;
                LoggerMgr::LoggerList loggerList;

                LogSubsystemJob (
                    LoggerMgr::Entry::UniquePtr entry_,
                    const LoggerMgr::LoggerList &loggerList_) :
                    entry (std::move (entry_)),
                    loggerList (loggerList_) {}

                virtual void Execute (const std::atomic<bool> & /*done*/) noexcept {
                    LogSubsystem (*entry, loggerList);
                }
            };
        }

        void LoggerMgr::Log (
                const char *subsystem,
                ui32 level,
                const std::string &header,
                const std::string &message) {
            if (subsystem != nullptr && level > Invalid && level <= MaxLevel) {
                Entry::UniquePtr entry (new Entry (subsystem, level, header, message));
                if (FilterEntry (*entry)) {
                    LockGuard<Mutex> guard (mutex);
                    LoggerMap::iterator it = loggerMap.find (subsystem);
                    if (it != loggerMap.end () || !defaultLoggers.empty ()) {
                        if (jobQueue != nullptr) {
                            jobQueue->EnqJob (
                                RunLoop::Job::SharedPtr (
                                    new LogSubsystemJob (
                                        std::move (entry),
                                        it != loggerMap.end () ? it->second : defaultLoggers)));
                        }
                        else {
                            LogSubsystem (
                                *entry,
                                it != loggerMap.end () ? it->second : defaultLoggers);
                        }
                    }
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void LoggerMgr::AddFilter (Filter::UniquePtr filter) {
            if (filter != nullptr) {
                LockGuard<Mutex> guard (mutex);
                filterList.push_back (std::move (filter));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void LoggerMgr::Flush (const TimeSpec &timeSpec) {
            LockGuard<Mutex> guard (mutex);
            if (jobQueue != nullptr) {
                jobQueue->WaitForIdle (timeSpec);
            }
            for (LoggerMap::iterator
                    it = loggerMap.begin (),
                    end = loggerMap.end (); it != end; ++it) {
                for (LoggerList::iterator
                        jt = it->second.begin (),
                        end = it->second.end (); jt != end; ++jt) {
                    (*jt)->Flush (timeSpec);
                }
            }
        }

        bool LoggerMgr::FilterEntry (Entry &entry) {
            LockGuard<Mutex> guard (mutex);
            for (FilterList::iterator
                    it = filterList.begin (),
                    end = filterList.end (); it != end; ++it) {
                if (!(*it)->FilterEntry (entry)) {
                    return false;
                }
            }
            return true;
        }

    } // namespace util
} // namespace thekogans
