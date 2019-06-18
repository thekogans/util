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
#include <sstream>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/JobQueue.h"
#include "thekogans/util/Logger.h"
#include "thekogans/util/HRTimer.h"

namespace thekogans {
    namespace util {

        /// \struct LoggerMgr LoggerMgr.h thekogans/util/LoggerMgr.h
        ///
        /// \brief
        /// LoggerMgr is designed to provide uniform, cross-platform logging.
        /// It accepts pluggable Logger instances which can direct the log
        /// output to various locations. A number of  Logger instances have
        /// been defined: \see{ConsoleLogger}, \see{FileLogger}, \see{MemoryLogger},
        /// \see{NSLogLogger}, \see{OutputDebugStringLogger}. LoggerMgr
        /// supports two distinct use cases. The first, and simplest, is to
        /// log everything to a global namespace (subsystem). The second,
        /// and more powerful case, is to use namespaces (subsystem) to log
        /// output from various application subsystems to their respective
        /// Loggers.
        ///
        /// Here is a canonical simple use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// THEKOGANS_UTIL_LOG_INIT (log level, "| separated log level list");
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
        /// THEKOGANS_UTIL_LOG_INIT (log level, "| separated log level list");
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
        ///
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
        /// THEKOGANS_UTIL_LOG_INIT will initialize the LoggerMgr to run in
        /// the background with low priority. Use THEKOGANS_UTIL_LOG_INIT_EX
        /// to initialize LoggerMgr to either use different priority or log
        /// imediatelly (blocking).
        ///
        /// To create log entries in your code, use one of the macros
        /// provided below: THEKOGANS_UTIL_LOG_'LEVEL'[_EX]. These
        /// macros are designed to be a noop if the level at which they
        /// log is higher then the selected LoggerMgr level.
        ///
        /// *** IMPORTANT ***
        /// LoggerMgr queues entries to be logged. Before the process
        /// using LoggerMgr ends call THEKOGANS_UTIL_LOG_FLUSH to flush
        /// the queue or you will lose log entries not yet processed.
        ///
        /// *** VERY, VERY IMPORTANT ***
        /// You cannot use the LoggerMgr in JobQueue/SpinLock/Mutex!
        /// (circular dependency)

        struct _LIB_THEKOGANS_UTIL_DECL LoggerMgr {
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
                /// Log errors, warnings, info and debug.
                Debug,
                /// \brief
                /// Log errors, warnings, info, debug and development.
                Development,
                /// \brief
                /// Highest log level we support.
                MaxLevel = Development
            };

            /// \brief
            /// Log entry decorations.
            enum {
                /// \brief
                /// Log messages only.
                NoDecorations = 0,
                /// \brief
                /// Add a '*' separator between log entries.
                EntrySeparator = 1,
                /// \brief
                /// Add a sub-system to log entries.
                Subsystem = 2,
                /// \brief
                /// Add a log level to log entries.
                Level = 4,
                /// \brief
                /// Add a date to log entries.
                Date = 8,
                /// \brief
                /// Add a time to log entries.
                Time = 16,
                /// \brief
                /// Add a high resolution timer to log entries.
                HRTime = 32,
                /// \brief
                /// Add a high resolution timer since process start to log entries.
                HRElapsedTime = 64,
                /// \brief
                /// Add a host name to log entries.
                HostName = 128,
                /// \brief
                /// Add a process path to log entries.
                ProcessPath = 256,
                /// \brief
                /// Add a process id to log entries.
                ProcessId = 512,
                /// \brief
                /// Add a thread id to log entries.
                ThreadId = 1024,
                /// \brief
                /// Add a location to log entries.
                Location = 2048,
                /// \brief
                /// Format log entries accross multiple lines.
                Multiline = 4096,
                /// \brief
                /// Add every decoration to log entries.
                All = EntrySeparator |
                    Level |
                    Date |
                    Time |
                    HRTime |
                    HRElapsedTime |
                    HostName |
                    ProcessPath |
                    ProcessId |
                    ThreadId |
                    Location |
                    Multiline,
                /// \brief
                /// Add subsystem to all log entries.
                SubsystemAll = Subsystem | All
            };

            /// \brief
            /// Convenient typedef for std::list<Logger::Ptr>.
            typedef std::list<Logger::Ptr> LoggerList;

            /// \struct LoggerMgr::Entry LoggerMgr.h thekogans/util/LoggerMgr.h
            ///
            /// \brief
            /// Formated log entry.
            struct Entry {
                /// \brief
                /// Convenient typedef for std::unique_ptr<Entry>.
                typedef std::unique_ptr<Entry> UniquePtr;

                /// \brief
                /// Entry has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Entry, SpinLock)

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
                /// \param[in, out] entry Entry to filter.
                /// \return true = log the entry. false = skip the entry.
                virtual bool FilterEntry (Entry &entry) throw () = 0;
            };

        private:
            /// \brief
            /// Level at which to log.
            ui32 level;
            /// \brief
            /// Decorations currently in effect.
            Flags32 decorations;
            /// \brief
            /// Birth time of LoggerMgr.
            ui64 startTime;
            /// \brief
            /// Convenient typedef for std::map<std::string, LoggerList>.
            typedef std::map<std::string, LoggerList> LoggerMap;
            /// \brief
            /// Map of all loggers.
            LoggerMap loggerMap;
            /// \brief
            /// List of default loggers.
            /// NOTE: All unknown subsystems will be serviced by the
            /// loggers in this list.
            LoggerList defaultLoggers;
            /// \brief
            /// Convenient typedef for std::list<Filter::UniquePtr>.
            typedef std::list<Filter::UniquePtr> FilterList;
            /// \brief
            /// List of registered filters.
            FilterList filterList;
            /// \brief
            /// Queue to excecute LogSubsystemJob jobs.
            JobQueue::Ptr jobQueue;
            /// \brief
            /// Synchronization mutex.
            Mutex mutex;

        public:
            /// \brief
            /// ctor.
            /// \param[in] level_ Level at which to log.
            /// \param[in] decorations_ Decorations currently in effect.
            /// \param[in] blocking true = Log entries immediately without the use of a JobQueue.
            /// \param[in] name \see{JobQueue} name.
            /// \param[in] priority \see{JobQueue} worker priority.
            /// \param[in] affinity \see{JobQueue} worker affinity.
            LoggerMgr (
                ui32 level_ = Info,
                ui32 decorations_ = All,
                bool blocking = false,
                const std::string &name = "LoggerMgr",
                i32 priority = THEKOGANS_UTIL_LOW_THREAD_PRIORITY,
                ui32 affinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY) :
                level (level_),
                decorations (decorations_),
                startTime (HRTimer::Click ()),
                jobQueue (!blocking ?
                    new JobQueue (
                        name,
                        RunLoop::TYPE_FIFO,
                        UI32_MAX,
                        1,
                        priority,
                        affinity) : 0) {}
            /// \brief
            /// dtor.
            ~LoggerMgr () {
                Flush ();
            }

            /// \brief
            /// Global sub-system name.
            static const char * const SUBSYSTEM_GLOBAL;

            /// \brief
            /// Get the list of logging levels.
            static void GetLevels (std::list<ui32> &levels);
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
            /// Get the list of logging decorations.
            static void GetDecorations (std::list<ui32> &decorations);
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
            /// Return the current log level.
            /// \return Current log level.
            inline ui32 GetLevel () const {
                return level;
            }

            enum {
                /// \brief
                /// Clear the loggerMap.
                ClearLoggers = 1,
                /// \brief
                /// Clear the filterList.
                ClearFilters = 2
            };
            /// \param[in] level_ Level at which to log.
            /// \param[in] decorations_ Decorations currently in effect.
            /// \param[in] flags 0 or more of the Clear... flags.
            /// \param[in] blocking true = Log entries immediately without the use of a JobQueue.
            /// \param[in] name \see{JobQueue} name.
            /// \param[in] priority \see{JobQueue} worker priority.
            /// \param[in] affinity \see{JobQueue} worker affinity.
            void Reset (
                ui32 level_ = Info,
                ui32 decorations_ = All,
                ui32 flags = ClearLoggers | ClearFilters,
                bool blocking = false,
                const std::string &name = "LoggerMgr",
                i32 priority = THEKOGANS_UTIL_LOW_THREAD_PRIORITY,
                ui32 affinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY);

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
            /// Add a default logger. All entries for unknown subsystems
            /// will be handled by the loggers in the defaultLoggers list.
            /// \param[in] logger Logger to add.
            void AddDefaultLogger (Logger::Ptr logger);
            /// \brief
            /// Add a default logger list. All entries for unknown subsystems
            /// will be handled by the loggers in this list.
            /// \param[in] loggerList Logger list to add.
            void AddDefaultLoggerList (const LoggerList &loggerList);

            /// \brief
            /// Add a filter to the LoggerMgr.
            /// \param[in] filter Filter to add.
            void AddFilter (Filter::UniquePtr filter);

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
            /// Wait until all queued log entries have
            /// been processed, and the queue is empty.
            /// \param[in] timeSpec How long to wait for Flush to complete.
            /// IMPORTANT: timeSpec is a relative value.
            void Flush (const TimeSpec &timeSpec = TimeSpec::Infinite);

        protected:
            /// \brief
            /// Return true if entry passed all registered filters.
            /// \param[in] entry Entry to filter.
            /// \return true if entry passed all registered filters.
            bool FilterEntry (Entry &entry);

            /// \brief
            /// LoggerMgr is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (LoggerMgr)
        };

        /// \struct GlobalLoggerMgrCreateInstance LoggerMgr.h thekogans/util/LoggerMgr.h
        ///
        /// \brief
        /// Call GlobalLoggerMgrCreateInstance::Parameterize before the first use of
        /// GlobalLoggerMgr::Instance to supply custom arguments to GlobalLoggerMgr ctor.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalLoggerMgrCreateInstance {
        private:
            /// \brief
            /// Level at which to log.
            static ui32 level;
            /// \brief
            /// Decorations currently in effect.
            static ui32 decorations;
            /// \brief
            /// true = Log entries immediately without the use of a JobQueue.
            static bool blocking;
            /// \brief
            /// \see{JobQueue} name.
            static std::string name;
            /// \brief
            /// \see{JobQueue} worker priority.
            static i32 priority;
            /// \brief
            /// \see{JobQueue} worker affinity.
            static ui32 affinity;

        public:
            /// \brief
            /// Call before the first use of GlobalLoggerMgr::Instance.
            /// \param[in] level_ Level at which to log.
            /// \param[in] decorations_ Decorations currently in effect.
            /// \param[in] blocking_ true = Log entries immediately without the use of a JobQueue.
            /// \param[in] name_ \see{JobQueue} name.
            /// \param[in] priority_ \see{JobQueue} worker priority.
            /// \param[in] affinity_ \see{JobQueue} worker affinity.
            static void Parameterize (
                ui32 level_ = LoggerMgr::Info,
                ui32 decorations_ = LoggerMgr::All,
                bool blocking_ = false,
                const std::string &name_ = "GlobalLoggerMgr",
                i32 priority_ = THEKOGANS_UTIL_LOW_THREAD_PRIORITY,
                ui32 affinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY);

            /// \brief
            /// Create a global job queue with custom ctor arguments.
            /// \return A global job queue with custom ctor arguments.
            LoggerMgr *operator () ();
        };

        /// \struct GlobalLoggerMgr LoggerMgr.h thekogans/util/LoggerMgr.h
        ///
        /// \brief
        /// A global logger manager instance.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalLoggerMgr :
            public Singleton<LoggerMgr, SpinLock, GlobalLoggerMgrCreateInstance> {};

        /// \def THEKOGANS_UTIL_LOG_INIT_EX(
        ///          level, decorations, blocking, name, priority, affinity)
        /// Parameterize the GlobalLoggerMgr ctor.
        #define THEKOGANS_UTIL_LOG_INIT_EX(\
                level, decorations, blocking, name, priority, affinity)\
            thekogans::util::GlobalLoggerMgrCreateInstance::Parameterize (\
                level, decorations, blocking, name, priority, affinity)
        /// \def THEKOGANS_UTIL_LOG_INIT(level, decorations)
        /// Parameterize the GlobalLoggerMgr ctor.
        #define THEKOGANS_UTIL_LOG_INIT(level, decorations)\
            thekogans::util::GlobalLoggerMgrCreateInstance::Parameterize (level, decorations)

        /// \def THEKOGANS_UTIL_LOG_RESET_EX(
        ///          level, decorations, flags, blocking, name, priority, affinity)
        /// Reset the GlobalLoggerMgr.
        #define THEKOGANS_UTIL_LOG_RESET_EX(\
                level, decorations, flags, blocking, name, priority, affinity)\
            thekogans::util::GlobalLoggerMgr::Instance ().Reset (\
                level, decorations, flags, blocking, name, priority, affinity)
        /// \def THEKOGANS_UTIL_LOG_RESET(level, decorations)
        /// Reset the GlobalLoggerMgr.
        #define THEKOGANS_UTIL_LOG_RESET(level, decorations)\
            thekogans::util::GlobalLoggerMgr::Instance ().Reset (level, decorations)

        /// \def THEKOGANS_UTIL_LOG_ADD_LOGGER(logger)
        /// After calling THEKOGANS_UTIL_LOG_[INIT | RESET][_EX] use
        /// this macro to add new global loggers to the GlobalLoggerMgr.
        #define THEKOGANS_UTIL_LOG_ADD_LOGGER(logger)\
            thekogans::util::GlobalLoggerMgr::Instance ().AddLogger (\
                thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL, logger)
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_ADD_LOGGER(subsystem, logger)
        /// After calling THEKOGANS_UTIL_LOG_[INIT | RESET][_EX] use
        /// this macro to add new subsystem loggers to the GlobalLoggerMgr.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_ADD_LOGGER(subsystem, logger)\
            thekogans::util::GlobalLoggerMgr::Instance ().AddLogger (subsystem, logger)
        /// \def THEKOGANS_UTIL_LOG_ADD_LOGGER_LIST(loggerList)
        /// After calling THEKOGANS_UTIL_LOG_[INIT | RESET][_EX] use
        /// this macro to add new global logger list to the GlobalLoggerMgr.
        #define THEKOGANS_UTIL_LOG_ADD_LOGGER_LIST(loggerList)\
            thekogans::util::GlobalLoggerMgr::Instance ().AddLoggerList (\
                thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL, loggerList)
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_ADD_LOGGER_LIST(subsystem, loggerList)
        /// After calling THEKOGANS_UTIL_LOG_[INIT | RESET][_EX] use
        /// this macro to add new subsystem logger list to the GlobalLoggerMgr.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_ADD_LOGGER_LIST(subsystem, loggerList)\
            thekogans::util::GlobalLoggerMgr::Instance ().AddLoggerList (subsystem, loggerList)

        /// \def THEKOGANS_UTIL_LOG_ADD_DEFAULT_LOGGER(logger)
        /// After calling THEKOGANS_UTIL_LOG_[INIT | RESET][_EX] use
        /// this macro to add new default loggers to the GlobalLoggerMgr.
        #define THEKOGANS_UTIL_LOG_ADD_DEFAULT_LOGGER(logger)\
            thekogans::util::GlobalLoggerMgr::Instance ().AddDefaultLogger (logger)
        /// \def THEKOGANS_UTIL_LOG_ADD_DEFAULT_LOGGER_LIST(loggerList)
        /// After calling THEKOGANS_UTIL_LOG_[INIT | RESET][_EX] use
        /// this macro to add new default logger list to the GlobalLoggerMgr.
        #define THEKOGANS_UTIL_LOG_ADD_DEFAULT_LOGGER_LIST(loggerList)\
            thekogans::util::GlobalLoggerMgr::Instance ().AddLoggerList (loggerList)

        /// \def THEKOGANS_UTIL_LOG_ADD_FILTER(filter)
        /// After calling THEKOGANS_UTIL_LOG_[INIT | RESET][_EX] use
        /// this macro to add a filter to the GlobalLoggerMgr.
        #define THEKOGANS_UTIL_LOG_ADD_FILTER(filter)\
            thekogans::util::GlobalLoggerMgr::Instance ().AddFilter (filter)

        /// \def THEKOGANS_UTIL_LOG_EX(level, file, line, buildTime, format, ...)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG_EX(level, file, function, line, buildTime, format, ...)\
            thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL, level,\
                file, function, line, buildTime, format, __VA_ARGS__);
        /// \def THEKOGANS_UTIL_LOG(level, format, ...)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG(level, format, ...)\
            thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL, level,\
                __FILE__, __FUNCTION__, __LINE__,\
                __DATE__ " " __TIME__, format, __VA_ARGS__);
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_EX(
        ///          subsystem, level, file, function, line, buildTime, format, ...)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_EX(\
                subsystem, level, file, function, line, buildTime, format, ...)\
            thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                subsystem, level, file, function, line, buildTime, format, __VA_ARGS__);
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM(subsystem, level, format, ...)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM(subsystem, level, format, ...)\
            thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                subsystem, level,\
                __FILE__, __FUNCTION__, __LINE__,\
                __DATE__ " " __TIME__, format, __VA_ARGS__);

        /// \def THEKOGANS_UTIL_LOG_ERROR_EX(file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Error.
        #define THEKOGANS_UTIL_LOG_ERROR_EX(file, function, line, buildTime, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Error) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Error,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_ERROR(format, ...)
        /// Use this macro to log at level Error.
        #define THEKOGANS_UTIL_LOG_ERROR(format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Error) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Error,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR_EX(
        ///          subsystem, file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Error.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR_EX(\
                subsystem, file, function, line, buildTime, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Error) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Error,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR(subsystem, format, ...)
        /// Use this macro to log at level Error.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_ERROR(subsystem, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Error) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Error,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }

        /// \def THEKOGANS_UTIL_LOG_WARNING_EX(file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Warning.
        #define THEKOGANS_UTIL_LOG_WARNING_EX(file, function, line, buildTime, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Warning) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Warning,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_WARNING(format, ...)
        /// Use this macro to log at level Warning.
        #define THEKOGANS_UTIL_LOG_WARNING(format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Warning) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Warning,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING_EX(
        ///          subsystem, file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Warning.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING_EX(\
                subsystem, file, function, line, buildTime, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Warning) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Warning,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING(subsystem, format, ...)
        /// Use this macro to log at level Warning.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING(subsystem, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Warning) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Warning,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }

        /// \def THEKOGANS_UTIL_LOG_INFO_EX(file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Info.
        #define THEKOGANS_UTIL_LOG_INFO_EX(file, function, line, buildTime, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Info) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Info,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_INFO(format, ...)
        /// Use this macro to log at level Info.
        #define THEKOGANS_UTIL_LOG_INFO(format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Info) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Info,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_INFO_EX(
        ///          subsystem, file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Info.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_INFO_EX(\
                subsystem, file, function, line, buildTime, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Info) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Info,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_INFO(subsystem, format, ...)
        /// Use this macro to log at level Info.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_INFO(subsystem, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Info) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Info,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }

        /// \def THEKOGANS_UTIL_LOG_DEBUG_EX(file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Debug.
        #define THEKOGANS_UTIL_LOG_DEBUG_EX(file, function, line, buildTime, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Debug) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Debug,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_DEBUG(format, ...)
        /// Use this macro to log at level Debug.
        #define THEKOGANS_UTIL_LOG_DEBUG(format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Debug) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Debug,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_DEBUG_EX(
        ///          subsystem, file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Debug.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_DEBUG_EX(\
                subsystem, file, function, line, buildTime, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Debug) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Debug,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_DEBUG(subsystem, format, ...)
        /// Use this macro to log at level Debug.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_DEBUG(subsystem, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Debug) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Debug,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }

        /// \def THEKOGANS_UTIL_LOG_DEVELOPMENT_EX(file, function, line, buildTime, format, ...)
        /// Use this macro to log at level Development.
        #define THEKOGANS_UTIL_LOG_DEVELOPMENT_EX(file, function, line, buildTime, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Development) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                    thekogans::util::LoggerMgr::Development,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_DEVELOPMENT(format, ...)
        /// Use this macro to log at level Development.
        #define THEKOGANS_UTIL_LOG_DEVELOPMENT(format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Development) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
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
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Development) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Development,\
                    file, function, line, buildTime, format, __VA_ARGS__);\
            }
        /// \def THEKOGANS_UTIL_LOG_SUBSYSTEM_DEVELOPMENT(subsystem, format, ...)
        /// Use this macro to log at level Development.
        #define THEKOGANS_UTIL_LOG_SUBSYSTEM_DEVELOPMENT(subsystem, format, ...)\
            if (thekogans::util::GlobalLoggerMgr::Instance ().GetLevel () >=\
                    thekogans::util::LoggerMgr::Development) {\
                thekogans::util::GlobalLoggerMgr::Instance ().Log (\
                    subsystem, thekogans::util::LoggerMgr::Development,\
                    __FILE__, __FUNCTION__, __LINE__,\
                    __DATE__ " " __TIME__, format, __VA_ARGS__);\
            }

        /// \def THEKOGANS_UTIL_LOG_FLUSH
        /// Use this macro to wait for GlobalLoggerMgr entry queue to drain.
        #define THEKOGANS_UTIL_LOG_FLUSH\
            thekogans::util::GlobalLoggerMgr::Instance ().Flush ();

        /// \def THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER
        /// Use this macro at the top of your main to flush the log on exit.
        #define THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER\
            struct LogFlusher {\
                ~LogFlusher () {\
                    THEKOGANS_UTIL_LOG_FLUSH\
                }\
            } logFlusher

        /// \struct LogStream LoggerMgr.h thekogans/util/LoggerMgr.h
        ///
        /// \brief
        /// LogStream is an adapter which converts LoggerMgr::Log from c printf
        /// style interface in to a c++ stream style interface. Use the macros
        /// below instead of the ones above to achieve this effect.

        struct _LIB_THEKOGANS_UTIL_DECL LogStream : public std::stringstream {
            /// \brief
            /// Subsystem to log to.
            const char *subsystem;
            /// \brief
            /// Level at which to log.
            ui32 level;
            /// \brief
            /// Translation unit of this entry.
            const char *file;
            /// \brief
            /// Function of the translation unit of this entry.
            const char *function;
            /// \brief
            /// Translation unit line number of this entry.
            ui32 line;
            /// \brief
            /// Translation unit build time of this entry.
            const char *buildTime;
            /// \brief
            /// \see{LoggerMgr} to log too.
            LoggerMgr &loggerMgr;

            /// \brief
            /// ctor.
            /// \param[in] subsystem_ Subsystem to log to.
            /// \param[in] level_ Level at which to log.
            /// If LoggerMgr::level < level_, entry is not logged.
            /// \param[in] file_ Translation unit of this entry.
            /// \param[in] function_ Function of the translation unit of this entry.
            /// \param[in] line_ Translation unit line number of this entry.
            /// \param[in] buildTime_ Translation unit build time of this entry.
            /// \param[in] loggerMgr_ \see{LoggerMgr} to log too.
            LogStream (
                const char *subsystem_,
                ui32 level_,
                const char *file_,
                const char *function_,
                ui32 line_,
                const char *buildTime_,
                LoggerMgr &loggerMgr_ = GlobalLoggerMgr::Instance ()) :
                subsystem (subsystem_),
                level (level_),
                file (file_),
                function (function_),
                line (line_),
                buildTime (buildTime_),
                loggerMgr (loggerMgr_) {}
            /// \brief
            /// dtor.
            /// This is where the magic happens. Our dtor will be called
            /// after all insertions have been performed. Grab the resulting
            /// string and pass it to LoggerMgr.
            ~LogStream () {
                if (loggerMgr.GetLevel () >= level) {
                    loggerMgr.Log (
                        subsystem,
                        level,
                        file,
                        function,
                        line,
                        buildTime,
                        str ().c_str ());
                }
            }
        };

        /// \def THEKOGANS_UTIL_LOG_STREAM_EX(level, file, line, buildTime, format)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG_STREAM_EX(level, file, function, line, buildTime)\
            thekogans::util::LogStream (\
                thekogans::util::LoggerMgr::SUBSYSTEM_GLOBAL,\
                level,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM(level)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG_STREAM(level)\
            THEKOGANS_UTIL_LOG_STREAM_EX(\
                level,\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)
        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_EX(
        ///          subsystem, level, file, function, line, buildTime)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_EX(\
                subsystem, level, file, function, line, buildTime)\
            thekogans::util::LogStream (\
                subsystem,\
                level,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM(subsystem, level)
        /// Use this macro to bypass the level checking machinery.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM(subsystem, level)\
            THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_EX(\
                subsystem,\
                level,\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

        /// \def THEKOGANS_UTIL_LOG_STREAM_ERROR_EX(file, function, line, buildTime)
        /// Use this macro to log at level Error.
        #define THEKOGANS_UTIL_LOG_STREAM_ERROR_EX(file, function, line, buildTime)\
            THEKOGANS_UTIL_LOG_STREAM_EX(\
                thekogans::util::LoggerMgr::Error,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM_ERROR
        /// Use this macro to log at level Error.
        #define THEKOGANS_UTIL_LOG_STREAM_ERROR\
            THEKOGANS_UTIL_LOG_STREAM_ERROR_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_ERROR_EX(
        ///          subsystem, file, function, line, buildTime)
        /// Use this macro to log at level Error.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_ERROR_EX(\
                subsystem, file, function, line, buildTime)\
            THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_EX(\
                subsystem,\
                thekogans::util::LoggerMgr::Error,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_ERROR(subsystem)
        /// Use this macro to log at level Error.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_ERROR(subsystem)\
            THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_ERROR_EX(\
                subsystem,\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

        /// \def THEKOGANS_UTIL_LOG_STREAM_WARNING_EX(file, function, line, buildTime)
        /// Use this macro to log at level Warning.
        #define THEKOGANS_UTIL_LOG_STREAM_WARNING_EX(file, function, line, buildTime)\
            THEKOGANS_UTIL_LOG_STREAM_EX(\
                thekogans::util::LoggerMgr::Warning,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM_WARNING
        /// Use this macro to log at level Warning.
        #define THEKOGANS_UTIL_LOG_STREAM_WARNING\
            THEKOGANS_UTIL_LOG_STREAM_WARNING_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_WARNING_EX(
        ///          subsystem, file, function, line, buildTime)
        /// Use this macro to log at level Warning.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_WARNING_EX(\
                subsystem, file, function, line, buildTime)\
            THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_EX(\
                subsystem,\
                thekogans::util::LoggerMgr::Warning,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_WARNING(subsystem)
        /// Use this macro to log at level Warning.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_WARNING(subsystem)\
            THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_WARNING_EX(\
                subsystem,\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

        /// \def THEKOGANS_UTIL_LOG_STREAM_INFO_EX(file, function, line, buildTime)
        /// Use this macro to log at level Info.
        #define THEKOGANS_UTIL_LOG_STREAM_INFO_EX(file, function, line, buildTime)\
            THEKOGANS_UTIL_LOG_STREAM_EX(\
                thekogans::util::LoggerMgr::Info,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM_INFO
        /// Use this macro to log at level Info.
        #define THEKOGANS_UTIL_LOG_STREAM_INFO\
            THEKOGANS_UTIL_LOG_STREAM_INFO_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_INFO_EX(
        ///          subsystem, file, function, line, buildTime)
        /// Use this macro to log at level Info.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_INFO_EX(\
                subsystem, file, function, line, buildTime)\
            THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_EX(\
                subsystem,\
                thekogans::util::LoggerMgr::Info,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_INFO(subsystem)
        /// Use this macro to log at level Info.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_INFO(subsystem)\
            THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_INFO_EX(\
                subsystem,\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

        /// \def THEKOGANS_UTIL_LOG_STREAM_DEBUG_EX(file, function, line, buildTime)
        /// Use this macro to log at level Debug.
        #define THEKOGANS_UTIL_LOG_STREAM_DEBUG_EX(file, function, line, buildTime)\
            THEKOGANS_UTIL_LOG_STREAM_EX(\
                thekogans::util::LoggerMgr::Debug,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM_DEBUG
        /// Use this macro to log at level Debug.
        #define THEKOGANS_UTIL_LOG_STREAM_DEBUG\
            THEKOGANS_UTIL_LOG_STREAM_DEBUG_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_DEBUG_EX(
        ///          subsystem, file, function, line, buildTime)
        /// Use this macro to log at level Debug.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_DEBUG_EX(\
                subsystem, file, function, line, buildTime)\
            THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_EX(\
                subsystem,\
                thekogans::util::LoggerMgr::Debug,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_DEBUG(subsystem)
        /// Use this macro to log at level Debug.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_DEBUG(subsystem)\
            THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_DEBUG_EX(\
                subsystem,\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

        /// \def THEKOGANS_UTIL_LOG_STREAM_DEVELOPMENT_EX(file, function, line, buildTime)
        /// Use this macro to log at level Development.
        #define THEKOGANS_UTIL_LOG_STREAM_DEVELOPMENT_EX(file, function, line, buildTime)\
            THEKOGANS_UTIL_LOG_STREAM_EX(\
                thekogans::util::LoggerMgr::Development,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM_DEVELOPMENT
        /// Use this macro to log at level Development.
        #define THEKOGANS_UTIL_LOG_STREAM_DEVELOPMENT\
            THEKOGANS_UTIL_LOG_STREAM_DEVELOPMENT_EX (\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_DEVELOPMENT_EX(
        ///          subsystem, file, function, line, buildTime)
        /// Use this macro to log at level Development.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_DEVELOPMENT_EX(\
                subsystem, file, function, line, buildTime)\
            THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_EX(\
                subsystem,\
                thekogans::util::LoggerMgr::Development,\
                file, function, line, buildTime)
        /// \def THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_DEVELOPMENT(subsystem)
        /// Use this macro to log at level Development.
        #define THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_DEVELOPMENT(subsystem)\
            THEKOGANS_UTIL_LOG_STREAM_SUBSYSTEM_DEVELOPMENT_EX(\
                subsystem,\
                __FILE__, __FUNCTION__, __LINE__, __DATE__ " " __TIME__)

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_LoggerMgr_h)
