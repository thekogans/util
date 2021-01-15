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

#if !defined (__thekogans_util_Config_h)
#define __thekogans_util_Config_h

#if !defined (__cplusplus)
    #error libthekogans_util requires C++ compilation (use a .cpp suffix).
#endif // !defined (__cplusplus)

/// \brief
/// NOTE: Headers should be included in the following order:
///
/// - system/os specific
/// - std c
/// - std c++
/// - third party dependencies
/// - thekogans dependencies
/// - project
///
/// The reason for this order is to prevent global namespace
/// collisions as much as posible.

#include <cstdlib>
#include <iostream>

#if defined (TOOLCHAIN_OS_Windows)
    #define _LIB_THEKOGANS_UTIL_API __stdcall
    #if defined (THEKOGANS_UTIL_TYPE_Shared)
        #if defined (_LIB_THEKOGANS_UTIL_BUILD)
            #define _LIB_THEKOGANS_UTIL_DECL __declspec (dllexport)
        #else // defined (_LIB_THEKOGANS_UTIL_BUILD)
            #define _LIB_THEKOGANS_UTIL_DECL __declspec (dllimport)
        #endif // defined (_LIB_THEKOGANS_UTIL_BUILD)
        #define THEKOGANS_UTIL_EXPORT __declspec (dllexport)
    #else // defined (THEKOGANS_UTIL_TYPE_Shared)
        #define _LIB_THEKOGANS_UTIL_DECL
        #define THEKOGANS_UTIL_EXPORT
    #endif // defined (THEKOGANS_UTIL_TYPE_Shared)
    #if defined (_MSC_VER) && (_MSC_VER <= 1200)
        #define THEKOGANS_UTIL_TYPENAME
    #else // defined (_MSC_VER) && (_MSC_VER <= 1200)
        #define THEKOGANS_UTIL_TYPENAME typename
    #endif // defined (_MSC_VER) && (_MSC_VER <= 1200)
    #define THEKOGANS_UTIL_PACKED(x) __pragma (pack (push, 1)) x __pragma (pack (pop))
    #if defined (_MSC_VER)
        #pragma warning (disable: 4251)  // using non-exported as public in exported
        #pragma warning (disable: 4786)
    #endif // defined (_MSC_VER)
#else // defined (TOOLCHAIN_OS_Windows)
    #define _LIB_THEKOGANS_UTIL_API
    #define _LIB_THEKOGANS_UTIL_DECL
    #define THEKOGANS_UTIL_EXPORT
    #define THEKOGANS_UTIL_TYPENAME typename
    #define THEKOGANS_UTIL_PACKED(x) x __attribute__ ((packed))
#endif // defined (TOOLCHAIN_OS_Windows)

#if defined (THEKOGANS_UTIL_CONFIG_Debug)
    /// \def THEKOGANS_UTIL_ASSERT(condition, message)
    /// A more capable replacement for assert.
    #define THEKOGANS_UTIL_ASSERT(condition, message)\
        do {\
            if (!(condition)) {\
                std::cerr << "Assertion `" #condition "` failed in " << __FILE__ <<\
                    " line " << __LINE__ << ": " << message << std::endl;\
                std::exit (EXIT_FAILURE);\
            }\
        } while (false)
    /// \def THEKOGANS_UTIL_DEBUG_BREAK
    /// Trigger a debug break.
    #if defined (THEKOGANS_UTIL_DEBUG_BREAK_ON_THROW)
        #if defined (TOOLCHAIN_OS_Windows)
            #define THEKOGANS_UTIL_DEBUG_BREAK __debugbreak ();
        #else // defined (TOOLCHAIN_OS_Windows)
            #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                #define THEKOGANS_UTIL_DEBUG_BREAK __asm__ ("int $3");
            #else // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                #define THEKOGANS_UTIL_DEBUG_BREAK
            #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
        #endif // defined (TOOLCHAIN_OS_Windows)
    #else // defined (THEKOGANS_UTIL_BREAK_ON_THROW)
        #define THEKOGANS_UTIL_DEBUG_BREAK
    #endif // defined (THEKOGANS_UTIL_BREAK_ON_THROW)
#else // defined (THEKOGANS_UTIL_CONFIG_Debug)
    #define THEKOGANS_UTIL_ASSERT(condition, message) do {} while (false)
    #define THEKOGANS_UTIL_DEBUG_BREAK
#endif // defined (THEKOGANS_UTIL_CONFIG_Debug)

/// \def THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN(type)
/// A convenient macro to suppress copy construction and assignment.
#define THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN(type)\
private:\
    type (const type &) = delete;\
    type &operator = (const type &) = delete;

#if defined (THEKOGANS_UTIL_TYPE_Static)
/// \def THEKOGANS_UTIL_STATIC_INIT(type)
/// Common dynamic discovery static initializer macro.
#define THEKOGANS_UTIL_STATIC_INIT(type)\
    public:\
    static void StaticInit () {\
        static volatile bool registered = false;\
        static thekogans::util::SpinLock spinLock;\
        if (!registered) {\
            thekogans::util::LockGuard<thekogans::util::SpinLock> guard (spinLock);\
            if (!registered) {\
                std::pair<Map::iterator, bool> result =\
                    GetMap ().insert (Map::value_type (#type, type::Create));\
                if (!result.second) {\
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                        "'%s' is already registered.", #type);\
                }\
                registered = true;\
            }\
        }\
    }
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

/// \def THEKOGANS_UTIL
/// Logging subsystem name.
#define THEKOGANS_UTIL "thekogans_util"

namespace thekogans {
    namespace util {

        enum {
            /// \brief
            /// Log only errors.
            Error = 1,
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
            /// Add a host name to log entries.
            HostName = 32,
            /// \brief
            /// Add a process id to log entries.
            ProcessId = 64,
            /// \brief
            /// Add a process path to log entries.
            ProcessPath = 128,
            /// \brief
            /// Add a high resolution timer since process start to log entries.
            ProcessStartTime = 256,
            /// \brief
            /// Add a high resolution timer since process start to log entries.
            ProcessUpTime = 512,
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
                HostName |
                ProcessId |
                ProcessPath |
                ProcessStartTime |
                ProcessUpTime |
                ThreadId |
                Location |
                Multiline,
            /// \brief
            /// Add subsystem to all log entries.
            SubsystemAll = Subsystem | All
        };

        /// \brief
        /// Helper functions to get around circular headers. They mirror the
        /// two primary log functions in \see{LoggerMgr}. There exist a
        /// handful of low level classes (\see{Heap}, \see{RefCounted}) that
        /// greatly benefit from being able to log extended error messages.
        /// Since \see{LoggerMgr} is higher level (it depends on \see{Heap}
        /// and \see{RefCounted}) they cannot use it directly.

        /// \brief
        /// Force log a message to the \see{GlobalLoggerMgr} irrespective of it's level.
        /// \param[in] decorations Decorations touse to format the entry header.
        /// \param[in] subsystem Subsystem to log to.
        /// \param[in] level Level at which to log.
        /// \param[in] file Translation unit of this entry.
        /// \param[in] function Function of the translation unit of this entry.
        /// \param[in] line Translation unit line number of this entry.
        /// \param[in] buildTime Translation unit build time of this entry.
        /// \param[in] format A printf format string followed by a
        /// variable number of arguments.
        _LIB_THEKOGANS_UTIL_DECL void Log (
            unsigned int decorations,
            const char *subsystem,
            unsigned int level,
            const char *file,
            const char *function,
            unsigned int line,
            const char *buildTime,
            const char *format,
            ...);
        /// \brief
        /// Force log a message to the \see{GlobalLoggerMgr} irrespective of it's level.
        /// \param[in] subsystem Subsystem to log to.
        /// \param[in] level Level at which to log.
        /// \param[in] header Entry header.
        /// \param[in] message Entry message.
        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API Log (
            const char *subsystem,
            unsigned int level,
            const std::string &header,
            const std::string &message);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Config_h)
