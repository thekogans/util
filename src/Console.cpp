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

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
    #include "thekogans/util/Exception.h"
#else // defined (TOOLCHAIN_OS_Windows)
    #if defined (TOOLCHAIN_OS_Linux)
        #include <sys/resource.h>
        #include <sys/wait.h>
    #endif // defined (TOOLCHAIN_OS_Linux)
    #include <signal.h>
#endif // defined (TOOLCHAIN_OS_Windows
#include "thekogans/util/MainRunLoop.h"
#include "thekogans/util/Console.h"

namespace thekogans {
    namespace util {

        bool ConsoleCreateInstance::threadSafePrintString = true;
        bool ConsoleCreateInstance::hookCtrlBreak = true;
        bool ConsoleCreateInstance::hookChild = false;
        bool ConsoleCreateInstance::coreDump = true;

        void ConsoleCreateInstance::Parameterize (
                bool threadSafePrintString_,
                bool hookCtrlBreak_,
                bool hookChild_,
                bool coreDump_) {
            threadSafePrintString = threadSafePrintString_;
            hookCtrlBreak = hookCtrlBreak_;
            hookChild = hookChild_;
            coreDump = coreDump_;
        }

        Console *ConsoleCreateInstance::operator () () {
            return new Console (
                threadSafePrintString,
                hookCtrlBreak,
                hookChild,
                coreDump);
        }

        namespace {
        #if defined (TOOLCHAIN_OS_Windows)
            BOOL WINAPI CtrlBreakHandler (DWORD fdwCtrlType) {
                switch (fdwCtrlType) {
                    case CTRL_C_EVENT:
                    case CTRL_BREAK_EVENT:
                    case CTRL_LOGOFF_EVENT:
                    case CTRL_SHUTDOWN_EVENT:
                        MainRunLoop::Instance ().Stop ();
                        return TRUE;
                    case CTRL_CLOSE_EVENT:
                        MainRunLoop::Instance ().Stop ();
                        return FALSE;
                }
                return FALSE;
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            void CtrlBreakHandler (int /*signal*/) {
                MainRunLoop::Instance ().Stop ();
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Console::Console (
                bool threadSafePrintString,
                bool hookCtrlBreak,
                bool hookChild,
                bool coreDump) :
                jobQueue (threadSafePrintString ? new JobQueue ("Console") : 0) {
            if (hookCtrlBreak) {
            #if defined (TOOLCHAIN_OS_Windows)
                if (!SetConsoleCtrlHandler ((PHANDLER_ROUTINE)CtrlBreakHandler, TRUE)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                signal (SIGTERM, CtrlBreakHandler);
                signal (SIGINT, CtrlBreakHandler);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        #if defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
            signal (SIGPIPE, SIG_IGN);
            if (hookChild) {
                struct sigaction action;
                memset (&action, 0, sizeof (action));
                action.sa_handler = SIG_IGN;
                // Never wait for termination of a child process.
                action.sa_flags = SA_NOCLDWAIT;
                sigaction (SIGCHLD, &action, 0);
            }
        #endif // defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
        #if defined (TOOLCHAIN_OS_Linux)
            if (coreDump) {
                rlimit limit;
                limit.rlim_cur = limit.rlim_max = RLIM_INFINITY;
                setrlimit (RLIMIT_CORE, &limit);
            }
        #endif // defined (TOOLCHAIN_OS_Linux)
        }

    #if defined (TOOLCHAIN_OS_Windows)
        const Console::ColorType Console::TEXT_COLOR_RED = FOREGROUND_RED;
        const Console::ColorType Console::TEXT_COLOR_GREEN = FOREGROUND_GREEN;
        const Console::ColorType Console::TEXT_COLOR_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN;
        const Console::ColorType Console::TEXT_COLOR_BLUE = FOREGROUND_BLUE;
        const Console::ColorType Console::TEXT_COLOR_MAGENTA = FOREGROUND_RED | FOREGROUND_BLUE;
        const Console::ColorType Console::TEXT_COLOR_CYAN = FOREGROUND_GREEN | FOREGROUND_BLUE;
        const Console::ColorType Console::TEXT_COLOR_WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    #else // defined (TOOLCHAIN_OS_Windows)
        const Console::ColorType Console::TEXT_COLOR_RED = (const Console::ColorType)"\033[0;31m";
        const Console::ColorType Console::TEXT_COLOR_GREEN = (const Console::ColorType)"\033[0;32m";
        const Console::ColorType Console::TEXT_COLOR_YELLOW = (const Console::ColorType)"\033[0;33m";
        const Console::ColorType Console::TEXT_COLOR_BLUE = (const Console::ColorType)"\033[0;34m";
        const Console::ColorType Console::TEXT_COLOR_MAGENTA = (const Console::ColorType)"\033[0;35m";
        const Console::ColorType Console::TEXT_COLOR_CYAN = (const Console::ColorType)"\033[0;36m";
        const Console::ColorType Console::TEXT_COLOR_WHITE = (const Console::ColorType)"\033[0;37m";
    #endif // defined (TOOLCHAIN_OS_Windows)

        namespace {
            void PrintStringWithColor (
                    const std::string &str,
                    ui32 where,
                    const Console::ColorType color) {
                std::ostream &stream = where == Console::PRINT_COUT ? std::cout : std::cerr;
                if (color != 0) {
                #if defined (TOOLCHAIN_OS_Windows)
                    struct ColorSetter {
                        DWORD stdHandle;
                        CONSOLE_SCREEN_BUFFER_INFO consoleScreenBufferInfo;
                        ColorSetter (
                                DWORD stdHandle_,
                                const Console::ColorType color) :
                                stdHandle (stdHandle_) {
                            HANDLE handle = GetStdHandle (stdHandle);
                            GetConsoleScreenBufferInfo (handle, &consoleScreenBufferInfo);
                            SetConsoleTextAttribute (handle, color | FOREGROUND_INTENSITY);
                        }
                        ~ColorSetter () {
                            SetConsoleTextAttribute (GetStdHandle (stdHandle),
                                consoleScreenBufferInfo.wAttributes);
                        }
                    } colorSetter (
                        where == Console::PRINT_COUT ?
                            STD_OUTPUT_HANDLE :
                            STD_ERROR_HANDLE, color);
                #else // defined (TOOLCHAIN_OS_Windows)
                    struct ColorSetter {
                        std::ostream &stream;
                        ColorSetter (
                                std::ostream &stream_,
                                const Console::ColorType color) :
                                stream (stream_) {
                            stream << color;
                        }
                        ~ColorSetter () {
                            stream << "\033[0m";
                        }
                    } colorSetter (stream, color);
                #endif // defined (TOOLCHAIN_OS_Windows)
                    stream << str;
                    stream.flush ();
                }
                else {
                    stream << str;
                    stream.flush ();
                }
            }
        }

        void Console::PrintString (
                const std::string &str,
                ui32 where,
                const ColorType color) {
            if (jobQueue.Get () != 0) {
                struct PrintJob : public RunLoop::Job {
                    std::string str;
                    ui32 where;
                    const ColorType color;

                    PrintJob (
                        const std::string &str_,
                        ui32 where_,
                        const ColorType color_) :
                        str (str_),
                        where (where_),
                        color (color_) {}

                    virtual void Execute (const THEKOGANS_UTIL_ATOMIC<bool> &done) throw () {
                        if (!ShouldStop (done)) {
                            PrintStringWithColor (str, where, color);
                        }
                    }
                };
                jobQueue->EnqJob (
                    RunLoop::Job::Ptr (
                        new PrintJob (str, where, color)));
            }
            else {
                PrintStringWithColor (str, where, color);
            }
        }

        void Console::FlushPrintQueue (const TimeSpec &timeSpec) {
            if (jobQueue.Get () != 0) {
                jobQueue->WaitForIdle (timeSpec);
            }
        }

    } // namespace util
} // namespace thekogans
