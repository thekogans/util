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

#if !defined (__thekogans_util_LoggerMgr_h)
#define __thekogans_util_LoggerMgr_h

#include <cstdlib>
#include <memory>
#include <string>
#include <list>
#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/Logger.h"
#include "thekogans/util/HRTimer.h"
#include "thekogans/util/SystemInfo.h"

namespace thekogans {
    namespace util {

        /// \struct LoggerMgr LoggerMgr.h thekogans/util/LoggerMgr.h
        ///
        /// \brief
        /// LoggerMgr is a singleton designed to provide uniform,
        /// cross-platform logging. It accepts pluggable Logger
        /// instances which can direct the log output to various
        /// locations. Three Logger instances have been defined:
        /// ConsoleLogger, FileLogger, and RemoteLogger. LoggerMgr
        /// supports two distinct use cases. The first, and simplest,
        /// is to log everything to a global namespace (subsystem).
        /// The second, and more powerful case, is to use namespaces
        /// (subsystem) to log output from various application
        /// subsystems to their respective Loggers.
        ///
        /// Here is a canonical use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// THEKOGANS_UTIL_LOG_INIT ("app name");
        /// THEKOGANS_UTIL_LOG_RESET ("| separated log level list");
        /// THEKOGANS_UTIL_LOG_ADD_LOGGER (
        ///     util::Logger::Ptr (new util::ConsoleLogger));
        /// THEKOGANS_UTIL_LOG_ADD_LOGGER (
        ///     util::Logger::Ptr (
        ///         new util::FileLogger ("log file path", true));
        /// \endcode
        ///
        /// Once initialized, you can now do something like this in your code:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_LOG_ERROR (
        ///     "a printf style format string",
        ///     variable argument list)
        /// \endcode
        ///
        /// Here is a canonical subsystem use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// THEKOGANS_UTIL_LOG_INIT ("app name");
        /// THEKOGANS_UTIL_LOG_SUBSYSTEM_RESET ("| separated log level list");
        /// THEKOGANS_UTIL_LOG_SUBSYSTEM_ADD_LOGGER (
        ///     "subsystem name",
        ///     util::Logger::Ptr (new util::ConsoleLogger));
        /// THEKOGANS_UTIL_LOG_SUBSYSTEM_ADD_LOGGER (
        ///     "subsystem name",
        ///     util::Logger::Ptr (
        ///         new util::FileLogger ("log file path", true)));
        /// ...
        /// Continue adding loggers for various application subsystems.
        /// \endcode
        ///
        /// Once initialized, you can now do something like this in your code:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR (
        ///     "subsystem name",
        ///     "a printf style format string",
        ///     variable argument list)
        /// \endcode
        ///
        /// The power of the subsystem logging technique comes from the fact
        /// that, at run time, you can omit loggers for those subsystems that
        /// you want to exclude from the log!
        ///
        /// Each successive log level adds to all the previous ones. Be
        /// careful using 'Debug' and 'Development' as they are basically
        /// a fire hose, and will log a ton of info.
        ///
        /// THEKOGANS_UTIL_LOG_INIT will initialize the LoggerMgr to
        /// use all decorations. If you want to limit them, use
        /// THEKOGANS_UTIL_LOG_INIT_EX instead. You can than pass in
        /// the '|' separated list of decorations each log entry will have.
        /// Also, LoggerMgr thread runs in the background with low
        /// priority. THEKOGANS_UTIL_LOG_INIT_EX is used to set the
        /// thread's priority as well.
        ///
        /// To create log entries in your code, use one of the macros
        /// provided below: THEKOGANS_UTIL_LOG_'LEVEL'[_EX]. These
        /// macros are designed to be a noop if the level at which they
        /// log is higher then the selected LoggerMgr level.
        ///
        /// *** IMPORTANT ***
        /// LoggerMgr queues entries to be logged. Before the process
        /// using LoggerMgr ends call THEKOGANS_UTIL_LOG_FLUSH to flush
        /// the queue or you will lose messages not yet processed.
        ///
        /// *** VERY, VERY IMPORTANT ***
        /// You cannot use the LoggerMgr in Thread/Mutex/Condition/Singleton!
        /// (circular dependency)

        struct _LIB_THEKOGANS_UTIL_DECL LoggerMgr :
                public Thread,
                public Singleton<LoggerMgr, SpinLock> {
            /// \brief
            /// Name of proceess.
            std::string processName;
            /// \brief
            /// true = Log entries immediately without the use of a background thread.
            bool blocking;
            /// \brief
            /// Birth time of LoggerMgr.
            ui64 startTime;
            /// \brief
            /// Host name.
            const std::string hostName;
            /// \brief
            /// Log levels. Each successive level builds on the previous ones.
            enum {
                /// \brief
                /// Log nothing.
                Invalid,
                /// \brief
                /// Log only errors.
                Error,
                /// \brief
                /// Log errors and warnings.
                Warning,
                /// \brief
                /// Log errors, warnings and info.
                Info,
                /// \brief
                /// Log errors, warnings, info and dubug.
                Debug,
                /// \brief
                /// Log errors, warnings, info, dubug and development.
                Development,
                /// \brief
                /// Highest log level we support.
                MaxLevel = Development
            };
            /// \brief
            /// Level at which to log.
            ui32 level;
            /// \brief
            /// Log entry decorations.
            enum {
                /// \brief
                /// Log messages only.
                NoDecorations = 0,
                /// \brief
                /// Add a '*' separator between messages.
                MessageSeparator = 1,
                /// \brief
                /// Add a sub-system to log entries.
                Subsystem = 2,
                /// \brief
                /// Add a log level to log entries.
                Level = 4,
                /// \brief
                /// Add a date and time to log entries.
                DateTime = 8,
                /// \brief
                /// Add a high resulution timer to log entries.
                HRTime = 16,
                /// \brief
                /// Add a host nemae to log entries.
                Host = 32,
                /// \brief
                /// Add a process name to log entries.
                ProcessName = 64,
                /// \brief
                /// Add a process id to log entries.
                ProcessId = 128,
                /// \brief
                /// Add a thread id to log entries.
                ThreadId = 256,
                /// \brief
                /// Add a location to log entries.
                Location = 512,
                /// \brief
                /// Add a location to log entries.
                Multiline = 1024,
                /// \brief
                /// Add every decoration to log entries.
                All = MessageSeparator | Level | DateTime | HRTime |
                    Host | ProcessName | ProcessId | ThreadId | Location | Multiline,
                /// \brief
                /// Add subsystem to all log entries.
                SubsystemAll = Subsystem | All
            };
            /// \brief
            /// Decorations currently in effect.
            Flags32 decorations;
            /// \brief
            /// Convenient typedef for AbstractOwnerList<Logger>.
            typedef std::list<Logger::Ptr> LoggerList;
            /// \brief
            /// Convenient typedef for std::map<std::string, LoggerList>.
            typedef std::map<std::string, LoggerList> LoggerMap;
            /// \brief
            /// Map of all loggers.
            LoggerMap loggerMap;
            /// \brief
            /// Synchronization mutex.
            Mutex loggerMapMutex;
            /// \brief
            /// Forward declaration of Entry.
            struct Entry;
            enum {
                /// \brief
                /// EntryList list id.
                ENTRY_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<Page, ENTRY_LIST_ID>.
            typedef IntrusiveList<Entry, ENTRY_LIST_ID> EntryList;
            /// \struct LoggerMgr::Entry LoggerMgr.h thekogans/util/LoggerMgr.h
            ///
            /// \brief
            /// Internal class representing a log entry.
            struct Entry : public EntryList::Node {
                /// \brief
                /// Entry has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Entry, SpinLock)

                /// \brief
                /// Convenient typedef for std::unique_ptr<Entry>.
                typedef std::unique_ptr<Entry> UniquePtr;

                /// \brief
                /// Subsystem that generated this log entry.
                std::string subsystem;
                /// \brief
                /// Entry log level.
                ui32 level;
                /// \brief
                /// Entry header.
                std::string header;
                /// \brief
                /// Entry message.
                std::string message;

                /// \brief
                /// ctor.
                /// \param[in] subsystem_ Subsystem that generated this log entry.
                /// \param[in] level_ Entry log level.
                Entry (
                    const char *subsystem_,
                    ui32 level_) :
                    subsystem (subsystem_),
                    level (level_) {}
                /// \brief
                /// ctor.
                /// \param[in] subsystem_ Subsystem that generated this log entry.
                /// \param[in] level_ Entry log level.
                /// \param[in] header_ Entry header.
                /// \param[in] message_ Entry message.
                Entry (
                    const std::string &subsystem_,
                    ui32 level_,
                    const std::string &header_,
                    const std::string &message_) :
                    subsystem (subsystem_),
                    level (level_),
                    header (header_),
                    message (message_) {}

                /// \brief
                /// Entry is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Entry)
            };
            /// \brief
            /// Log entries waiting to be processed.
            EntryList entryList;
            /// \brief
            /// Synchronization mutex.
            Mutex entryListMutex;
            /// \brief
            /// Synchronization condition variable.
            Condition entryListNotEmpty;
            /// \struct LoggerMgr::Filter LoggerMgr.h thekogans/util/LoggerMgr.h
            ///
            /// \brief
            /// Base class for LoggerMgr filters. Filters serve two purposes;
            /// 1) They provide a hook to allow an application to determine which
            /// entries get logged and which get dropped and 2) Allow the filter
            /// to transform an entry before logging it.
            struct Filter {
                /// \brief
                /// Convenient typedef for std::unique_ptr<Filter>.
                typedef std::unique_ptr<Filter> UniquePtr;

                /// \brief
                /// dtor.
                virtual ~Filter () {}

                /// \brief
                /// Called by LoggerMgr before logging an entry.
                /// \param[in] entry Entry to filter.
                /// \return true = log the entry. false = skip the entry.
                virtual bool FilterEntry (Entry &entry) throw () = 0;
            };
            /// \brief
            /// Convenient typedef for std::list<Filter::UniquePtr>.
            typedef std::list<Filter::UniquePtr> FilterList;
            /// \brief
            /// List of registered filters.
            FilterList filterList;
            /// \brief
            /// Synchronization mutex.
            Mutex filterListMutex;

            /// \brief
            /// Default ctor.
            LoggerMgr () :
                Thread ("LoggerMgr", false),
                blocking (false),
                startTime (HRTimer::Click ()),
                hostName (SystemInfo::Instance ().GetHostName ()),
                level (Invalid),
                decorations (NoDecorations),
                entryListNotEmpty (entryListMutex) {}

            /// \brief
            /// Global sub-system name.
            static const char * const SUBSYSTEM_GLOBAL;

            /// \brief
            /// Get the list of logging levels.
            static void GetLogLevels (std::list<ui32> &levels);
            /// \brief
            /// Convert an integral level to it's string equivalent.
            /// \param[in] level Invalid, Error, Warning, Info, Debug, Development.
            /// \return "Invalid", "Error", "Warning", "Info", "Debug", "Development".
            static std::string levelTostring (ui32 level);
            /// \brief
            /// Convert a string representation of the log level to it's integral form.
            /// \param[in] level "Invalid", "Error", "Warning", "Info", "Debug", "Development".
            /// \return Invalid, Error, Warning, Info, Debug, Development.
            static ui32 stringTolevel (const std::string &level);

            /// \brief
            /// Convert an integral form of decorations
            /// flags in to '|' separated list of strings.
            /// \param[in] decorations Integral form of decorations flags.
            /// \return '|' separated list of string equivalents.
            static std::string decorationsTostring (ui32 decorations);
            /// \brief
            /// Convert a '|' separated list of decorations in
            /// to it's integral form.
            /// \param[in] decorations '|' separated list of decorations.
            /// \return Integral form of decoration flags.
            static ui32 stringTodecorations (const std::string &decorations);

            /// \brief
            /// Save the process name. Create the logger manager thread.
            /// \param[in] processName_ Name of process.
            /// \param[in] blocking_ true = Log entries immediately without the use of a background thread.
            /// \param[in] priority Logger manager thread priority.
            /// \param[in] affinity Logger manager thread processor affinity.
            void Init (
                const std::string &processName_,
                bool blocking_ = false,
                i32 priority = THEKOGANS_UTIL_LOW_THREAD_PRIORITY,
                ui32 affinity = UI32_MAX);
            /// \brief
            /// Flush the entry queue, reset the level,
            /// and decorations, and delete all loggers.
            /// \param[in] level_ Level at with to log.
            /// \param[in] decorations_ Decorations to use with each log entry.
            void Reset (
                ui32 level_,
                ui32 decorations_ = LoggerMgr::All);
            /// \brief
            /// Add a subsystem logger. It will be called for each subsystem log entry.
            /// Multiple loggers can be added.
            /// \param[in] subsystem Subsystem to add the logger to.
            /// \param[in] logger Logger to add.
            void AddLogger (
                const char *subsystem,
                Logger::Ptr logger);
            /// \brief
            /// Add a subsystem logger list. It will be called for each subsystem log entry.
            /// \param[in] subsystem Subsystem to add the logger to.
            /// \param[in] loggerList Logger list to add.
            void AddLoggerList (
                const char *subsystem,
                const LoggerList &loggerList);
            /// \brief
            /// Log an event.
            /// \param[in] subsystem Subsystem to log to.
            /// \param[in] level Level at which to log.
            /// If LoggerMgr::level < level, entry is not logged.
            /// \param[in] file Translation unit of this entry.
            /// \param[in] function Function of the translation unit of this entry.
            /// \param[in] line Translation unit line number of this entry.
            /// \param[in] buildTime Translation unit build time of this entry.
            /// \param[in] format A printf format string followed by a
            /// variable number of arguments.
            void Log (
                const char *subsystem,
                ui32 level,
                const char *file,
                const char *function,
                ui32 line,
                const char *buildTime,
                const char *format,
                ...);
            /// \brief
            /// Log an event.
            /// \param[in] subsystem Subsystem to log to.
            /// \param[in] level Level at which to log.
            /// \param[in] header Entry header.
            /// \param[in] message Entry message.
            void Log (
                const char *subsystem,
                ui32 level,
                const std::string &header,
                const std::string &message);

            /// \brief
            /// Add a filter to the LoggerMgr.
            /// \param[in] filter Filter to add.
            void AddFilter (Filter::UniquePtr filter);

            /// \brief
            /// Wait until all queued log entries have
            /// been processed, and the queue is empty.
            void Flush ();

        protected:
            // Thread
            /// \brief
            /// Thread that processes the log entries.
            virtual void Run () throw ();

            /// \brief
            /// Return true if entry passed all registered filters.
            /// \param[in] entry Entry to filter.
            /// \return true if entry passed all registered filters.
            bool FilterEntry (Entry &entry);

            /// \brief
            /// Used by the thread to dequeue log entries.
            std::unique_ptr<Entry> Deq ();

            /// \brief
            /// LoggerMgr is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (LoggerMgr)
        };

        /// \def THEKOGANS_UTIL_LOG_INIT_EX(processName, blocking, priority, affinity)
        /// Initialize the LoggerMgr. Set process name and, if not blocking,
        /// create the processing thread with given priority and affinity.
        /// This macro should be the first one called,
        /// and should only be called once.
        #define THEKOGANS_UTIL_LOG_INIT_EX(processName, blocking, priority, affinity)\
            thekogans::util::LoggerMgr::Instance ().Init (\
                processName, blocking, priority, affinity)
        /// \def THEKOGANS_UTIL_LOG_INIT(processName)
        /// Initialize the LoggerMgr. Set process name, and
        /// create the processing thread with THEKOGANS_UTIL_LOW_THREAD_PRIORITY.
        /// This macro (or the one above it) should be the first one called,
        /// and should only be called once.
        #define THEKOGANS_UTIL_LOG_INIT(processName)\
            thekogans::util::LoggerMgr::Instance ().Init (processName)
        /// \def THEKOGANS_UTIL_LOG_RESET_EX(level, decorations)
        /// Reset the LoggerMgr. Flush the events queue, reset
        /// the level and decorations, and delete all loggers. This
        /// macro can be called as often as you like. It's canonical
        /// use case is to reset the LoggerMgr while servicing a
        /// changed options file.
        #define THEKOGANS_UTIL_LOG_RESET_EX(level, decorations)\
            thekogans::util::LoggerMgr::Instance ().Reset (level, decorations)
        /// \def THEKOGANS_UTIL_LOG_RESET(level)
        /// Reset the LoggerMgr. Flush the events queue, reset
        /// the level, set decorations = All, and delete all loggers.
        /// This macro can be called as often as you like. It's
        /// canonical use case is to reset the LoggerMgr while
        /// servicing a changed options file.
        #define THEKOGANS_UTIL_LOG_RESET(level)\
            thekogans::util::LoggerMgr::Instance ().Reset (level)
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_RESET_EX(level, decorations)
        /// Reset the LoggerMgr. Flush the events queue, reset
        /// the level and decorations, and delete all loggers. This
        /// macro can be called as often as you like. It's canonical
        /// use case is to reset the LoggerMgr while servicing a
        /// changed options file.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_RESET_EX(level, decorations)\
            thekogans::util::LoggerMgr::Instance ().Reset (\
                level, thekogans::util::LoggerMgr::Subsystem | decorations)
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_RESET(level)
        /// Reset the LoggerMgr. Flush the events queue, reset
        /// the level, set decorations = All, and delete all loggers.
        /// This macro can be called as often as you like. It's
        /// canonical use case is to reset the LoggerMgr while
        /// servicing a changed options file.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_RESET(level)\
            thekogans::util::LoggerMgr::Instance ().Reset (\
                level, thekogans::util::LoggerMgr::SubsystemAll)
        /// \def THEKOGANS_UTIL_LOG_ADD_LOGGER(logger)
        /// After calling THEKOGANS_UTIL_LOG_RESET[_EX] use
        /// this macro to add new global loggers to the LoggerMgr.
        #define THEKOGANS_UTIL_LOG_ADD_LOGGER(logger)\
            thekogans::util::LoggerMgr::Instance ().AddLogger (\
                thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL, logger)
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_ADD_LOGGER(subsystem, logger)
        /// After calling THEKOGANS_UTIL_LOG_RESET[_EX] use
        /// this macro to add new subsystem loggers to the LoggerMgr.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_ADD_LOGGER(subsystem, logger)\
            thekogans::util::LoggerMgr::Instance ().AddLogger (subsystem, logger)
        /// \def THEKOGANS_UTIL_LOG_ADD_LOGGER_LIST(loggerList)
        /// After calling THEKOGANS_UTIL_LOG_RESET[_EX] use
        /// this macro to add new global logger list to the LoggerMgr.
        #define THEKOGANS_UTIL_LOG_ADD_LOGGER_LIST(loggerList)\
            thekogans::util::LoggerMgr::Instance ().AddLoggerList (\
                thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL, loggerList)
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_ADD_LOGGER_LIST(subsystem, loggerList)
        /// After calling THEKOGANS_UTIL_LOG_RESET[_EX] use
        /// this macro to add new subsystem logger list to the LoggerMgr.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_ADD_LOGGER_LIST(subsystem, loggerList)\
            thekogans::util::LoggerMgr::Instance ().AddLoggerList (subsystem, loggerList)

        /// \def THEKOGANS_UTIL_LOG_EX(level, file, line, buildTime, format, ...)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG_EX(level, file, function, line, buildTime, format, ...)\
            thekogans::util::LoggerMgr::Instance ().Log (\
                thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL, level,\
                file, function, line, buildTime, format, __VA_ARGS__);
        /// \def THEKOGANS_UTIL_LOG(level, format, ...)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG(level, format, ...)\
            thekogans::util::LoggerMgr::Instance ().Log (\
                thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL, level,\
                __FILE__, __FUNCTION__, __LINE__,\
                __DATE__ " " __TIME__, format, __VA_ARGS__);
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_EX(
        ///          subsystem, level, file, function, line, buildTime, format, ...)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_EX(\
                subsystem, level, file, function, line, buildTime, format, ...)\
            thekogans::util::LoggerMgr::Instance ().Log (\
                subsystem, level, file, function, line, buildTime, format, __VA_ARGS__);
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM(subsystem, level, format, ...)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM(subsystem, level, format, ...)\
            thekogans::util::LoggerMgr::Instance ().Log (subsystem, level,\
            __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__, format, __VA_ARGS__);

        /// \def THEKOGANS_UTIL_LOG_ERROR_EX(file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Error or higher.
        #define THEKOGANS_UTIL_LOG_ERROR_EX(file, function, line, buildTime, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Error) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Error,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_ERROR(format, ...)
        /// Use this macro to log at level Error or higher.
        #define THEKOGANS_UTIL_LOG_ERROR(format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Error) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Error,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR_EX(
        ///          subsystem, file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Error or higher.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR_EX(\
                subsystem, file, function, line, buildTime, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Error) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Error,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR(subsystem, format, ...)
        /// Use this macro to log at level Error or higher.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR(subsystem, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Error) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Error,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }

        /// \def THEKOGANS_UTIL_LOG_WARNING_EX(file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Warning or higher.
        #define THEKOGANS_UTIL_LOG_WARNING_EX(file, function, line, buildTime, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Warning) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Warning,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_WARNING(format, ...)
        /// Use this macro to log at level Warning or higher.
        #define THEKOGANS_UTIL_LOG_WARNING(format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Warning) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Warning,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING_EX(
        ///          subsystem, file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Warning or higher.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING_EX(\
                subsystem, file, function, line, buildTime, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Warning) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Warning,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING(subsystem, format, ...)
        /// Use this macro to log at level Warning or higher.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING(subsystem, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Warning) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Warning,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }

        /// \def THEKOGANS_UTIL_LOG_INFO_EX(file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Info or higher.
        #define THEKOGANS_UTIL_LOG_INFO_EX(file, function, line, buildTime, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Info) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Info,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_INFO(format, ...)
        /// Use this macro to log at level Info or higher.
        #define THEKOGANS_UTIL_LOG_INFO(format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Info) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Info,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_INFO_EX(
        ///          subsystem, file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Info or higher.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_INFO_EX(\
                subsystem, file, function, line, buildTime, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Info) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Info,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_INFO(subsystem, format, ...)
        /// Use this macro to log at level Info or higher.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_INFO(subsystem, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Info) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Info,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }

        /// \def THEKOGANS_UTIL_LOG_DEBUG_EX(file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Debug or higher.
        #define THEKOGANS_UTIL_LOG_DEBUG_EX(file, function, line, buildTime, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Debug) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Debug,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_DEBUG(format, ...)
        /// Use this macro to log at level Debug or higher.
        #define THEKOGANS_UTIL_LOG_DEBUG(format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Debug) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Debug,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_DEBUG_EX(
        ///          subsystem, file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Debug or higher.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_DEBUG_EX(\
                subsystem, file, function, line, buildTime, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Debug) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Debug,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_DEBUG(subsystem, format, ...)
        /// Use this macro to log at level Debug or higher.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_DEBUG(subsystem, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Debug) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Debug,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }

        /// \def THEKOGANS_UTIL_LOG_DEVELOPMENT_EX(file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Development.
        #define THEKOGANS_UTIL_LOG_DEVELOPMENT_EX(file, function, line, buildTime, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Development) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Development,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_DEVELOPMENT(format, ...)
        /// Use this macro to log at level Development.
        #define THEKOGANS_UTIL_LOG_DEVELOPMENT(format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Development) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Development,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_DEVELOPMENT_EX(
        ///          subsystem, file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Development.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_DEVELOPMENT_EX(\
                subsystem, file, function, line, buildTime, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Development) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Development,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_DEVELOPMENT(subsystem, format, ...)
        /// Use this macro to log at level Development.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_DEVELOPMENT(subsystem, format, ...)\
            if (thekogans::util::LoggerMgr::Instance ().level >=\
                    thekogans::util::LoggerMgr::Development) {\
                thekogans::util::LoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Development,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }

        /// \def THEKOGANS_UTIL_LOG_FLUSH
        /// Use this macro to wait for LoggerMgr entry queue to drain.
        #define THEKOGANS_UTIL_LOG_FLUSH\
            thekogans::util::LoggerMgr::Instance ().Flush ();

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_LoggerMgr_h)
