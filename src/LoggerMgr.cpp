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
#include "thekogans/util/Exception.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Console.h"
#include "thekogans/util/SystemInfo.h"
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
            else if (Flags32 (decorations).Test (HRElapsedTime)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "HRElapsedTime";
            }
            else if (Flags32 (decorations).Test (HostName)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "HostName";
            }
            else if (Flags32 (decorations).Test (ProcessPath)) {
                if (!value.empty ()) {
                    value += " | ";
                }
                value += "ProcessPath";
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
                if (decoration == "EntrySeparator") {
                    return LoggerMgr::EntrySeparator;
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
                else if (decoration == "HRElapsedTime") {
                    return LoggerMgr::HRElapsedTime;
                }
                else if (decoration == "HostName") {
                    return LoggerMgr::HostName;
                }
                else if (decoration == "ProcessPath") {
                    return LoggerMgr::ProcessPath;
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

        void LoggerMgr::Reset (
                ui32 level_,
                ui32 decorations_,
                ui32 flags,
                bool blocking,
                const std::string &name,
                i32 priority,
                ui32 affinity) {
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
            jobQueue.reset (!blocking ?
                new JobQueue (name, JobQueue::TYPE_FIFO, 1, priority, affinity) : 0);
        }

        void LoggerMgr::AddLogger (
                const char *subsystem,
                Logger::Ptr logger) {
            if (subsystem != 0 && logger.Get () != 0) {
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
            if (subsystem != 0 && !loggerList.empty ()) {
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

        void LoggerMgr::AddDefaultLogger (Logger::Ptr logger) {
            if (logger.Get () != 0) {
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
                if (decorations.Test (EntrySeparator) && decorations.Test (Multiline)) {
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
                    header += FormatString ("%011.4f ",
                            HRTimer::ToSeconds (HRTimer::Click ()));
                }
                if (decorations.Test (HRElapsedTime)) {
                    header += FormatString ("%011.4f ",
                            HRTimer::ToSeconds (HRTimer::ComputeEllapsedTime (startTime, HRTimer::Click ())));
                }
                if (decorations.Test (HostName)) {
                    header += SystemInfo::Instance ().GetHostName ();
                    header += " ";
                }
                if (decorations.Test (ProcessPath)) {
                    header += SystemInfo::Instance ().GetProcessPath ();
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

            struct LogSubsystemJob : public JobQueue::Job {
                LoggerMgr::Entry::UniquePtr entry;
                LoggerMgr::LoggerList loggerList;

                LogSubsystemJob (
                    LoggerMgr::Entry::UniquePtr entry_,
                    const LoggerMgr::LoggerList &loggerList_) :
                    entry (std::move (entry_)),
                    loggerList (loggerList_) {}

                virtual void Execute (volatile const bool & /*done*/) throw () {
                    LogSubsystem (*entry, loggerList);
                }
            };
        }

        void LoggerMgr::Log (
                const char *subsystem,
                ui32 level,
                const std::string &header,
                const std::string &message) {
            Entry::UniquePtr entry (new Entry (subsystem, level, header, message));
            if (FilterEntry (*entry)) {
                LockGuard<Mutex> guard (mutex);
                LoggerMap::iterator it = loggerMap.find (subsystem);
                if (it != loggerMap.end () || !defaultLoggers.empty ()) {
                    if (jobQueue.get () != 0) {
                        jobQueue->Enq (
                            *JobQueue::Job::Ptr (
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

        void LoggerMgr::AddFilter (Filter::UniquePtr filter) {
            if (filter.get () != 0) {
                LockGuard<Mutex> guard (mutex);
                filterList.push_back (std::move (filter));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void LoggerMgr::Flush () {
            LockGuard<Mutex> guard (mutex);
            if (jobQueue.get () != 0) {
                jobQueue->WaitForIdle ();
            }
            for (LoggerMap::iterator
                    it = loggerMap.begin (),
                    end = loggerMap.end (); it != end; ++it) {
                for (LoggerList::iterator
                        jt = it->second.begin (),
                        end = it->second.end (); jt != end; ++jt) {
                    (*jt)->Flush ();
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

        ui32 GlobalLoggerMgrCreateInstance::level = LoggerMgr::Info;
        ui32 GlobalLoggerMgrCreateInstance::decorations = LoggerMgr::All;
        bool GlobalLoggerMgrCreateInstance::blocking = false;
        std::string GlobalLoggerMgrCreateInstance::name = "GlobalLoggerMgr";
        i32 GlobalLoggerMgrCreateInstance::priority = THEKOGANS_UTIL_LOW_THREAD_PRIORITY;
        ui32 GlobalLoggerMgrCreateInstance::affinity = UI32_MAX;

        void GlobalLoggerMgrCreateInstance::Parameterize (
                ui32 level_,
                ui32 decorations_,
                bool blocking_,
                const std::string &name_,
                i32 priority_,
                ui32 affinity_) {
            level = level_;
            decorations = decorations_;
            blocking = blocking_;
            name = name_;
            priority = priority_;
            affinity = affinity_;
        }

        LoggerMgr *GlobalLoggerMgrCreateInstance::operator () () {
            return new LoggerMgr (
                level,
                decorations,
                blocking,
                name,
                priority,
                affinity);
        }

    } // namespace util
} // namespace thekogans
