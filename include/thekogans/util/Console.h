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

#if !defined (__thekogans_util_Console_h)
#define __thekogans_util_Console_h

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/Event.h"
#include "thekogans/util/JobQueue.h"
#include "thekogans/util/TimeSpec.h"

namespace thekogans {
    namespace util {

        /// \struct Console Console.h thekogans/util/Console.h
        ///
        /// \brief
        /// Console provides platform independent CTRL+BREAK handling and
        /// colored text output. On Linux and OS X it also turns on core
        /// dumping, and ignores the SIGPIPE.

        struct _LIB_THEKOGANS_UTIL_DECL Console : public Singleton<Console> {
        private:
            /// \brief
            /// Used to synchronize access to std::cout and std::cerr in PrintString.
            JobQueue::SharedPtr jobQueue;

        public:
            /// \brief
            /// ctor.
            /// NOTE: If you want the Console singleton to be created with custom ctor arguments, call
            /// Console::CreateInstance (...) with your custom arguments before the first call to
            /// Console::Instance ().
            /// \param[in] threadSafePrintString true == Serialize access to std::cout and std::cerr.
            /// \param[in] hookCtrlBreak true == Hook CTRL-C to call MainRunLoop::Instance ()->Stop ().
            /// \param[in] hookChild Linux and OS X only. true == Hook SIGCHLD to avoid zombie children.
            /// NOTE: You should only pass in true for hookChild if you're calling \see{ChildProcess::Spawn}
            /// (instead of \see{ChildProcess::Exec}, and you don't want to reap the zombie children yourself.
            /// \param[in] coreDump Linux only. true == Turn on core dump.
            Console (
                bool threadSafePrintString = true,
                bool hookCtrlBreak = true,
                bool hookChild = false,
                bool coreDump = true);

            enum StdStream {
                /// \brief
                /// Print to std::cout.
                StdOut,
                /// \brief
                /// Print to std::cerr.
                StdErr
            };

        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Color type.
            typedef WORD ColorType;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Color type.
            typedef char *ColorType;
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Red.
            static const ColorType TEXT_COLOR_RED;
            /// \brief
            /// Greeen.
            static const ColorType TEXT_COLOR_GREEN;
            /// \brief
            /// Yellow.
            static const ColorType TEXT_COLOR_YELLOW;
            /// \brief
            /// Blue.
            static const ColorType TEXT_COLOR_BLUE;
            /// \brief
            /// Magenta.
            static const ColorType TEXT_COLOR_MAGENTA;
            /// \brief
            /// Cyan.
            static const ColorType TEXT_COLOR_CYAN;
            /// \brief
            /// White.
            static const ColorType TEXT_COLOR_WHITE;

            /// \brief
            /// Print a string to std::cout or std::cerr.
            /// \param[in] str String to print.
            /// \param[in] where StdOut or StdErr.
            /// \param[in] color Text color.
            void PrintString (
                const std::string &str,
                StdStream where = StdOut,
                const ColorType color = 0);

            /// \brief
            /// if threadSafePrintString == true, wait for jobQueue to become idle.
            ///
            /// VERY IMPORTANT: If threadSafePrintString == true, there exists a race
            /// between application close and the print jobQueue flushing it's queue.
            /// If you use \see{LoggerMgr} and setup your main properly:
            ///
            /// \code{.cpp}
            /// int main (
            ///         int argc,
            ///         const char *argv[]) {
            ///     THEKOGANS_UTIL_LOG_INIT (
            ///         thekogans::util::LoggerMgr::Debug,
            ///         thekogans::util::LoggerMgr::All);
            ///     THEKOGANS_UTIL_LOG_ADD_LOGGER (new thekogans::util::ConsoleLogger);
            ///     THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER;
            ///     THEKOGANS_UTIL_TRY {
            ///         ...
            ///         thekogans::util::MainRunLoop::Instance ()->Start ();
            ///         THEKOGANS_UTIL_LOG_DEBUG ("%s exiting.\n", argv[0]);
            ///     }
            ///     THEKOGANS_UTIL_CATCH_AND_LOG
            ///     return 0;
            /// }
            /// \endcode
            ///
            /// there's nothing for you to do as THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER will
            /// take care of calling thekogans::util::Console::Instance ()->FlushPrintQueue ().
            ///
            /// If you do something else, you need to make sure to call FlushPrintQueue
            /// yourself or you risk having your application deadlock on exit.
            /// \param[in] timeSpec How long to wait for FlushPrintQueue to complete.
            /// IMPORTANT: timeSpec is a relative value.
            void FlushPrintQueue (const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Console is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Console)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Console_h)
