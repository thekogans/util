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

#if !defined (__thekogans_util_MainRunLoop_h)
#define __thekogans_util_MainRunLoop_h

#include "thekogans/util/Environment.h"
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
#elif defined (TOOLCHAIN_OS_OSX)
    #include <CoreFoundation/CFRunLoop.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include <list>
#include "thekogans/util/Config.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/RunLoop.h"
#include "thekogans/util/SystemRunLoop.h"
#include "thekogans/util/ThreadRunLoop.h"

namespace thekogans {
    namespace util {

        /// \struct MainRunLoopCreateInstance MainRunLoop.h thekogans/util/MainRunLoop.h
        ///
        /// \brief
        /// Call MainRunLoop::CreateInstance before the first use of
        /// MainRunLoop::Instance to supply custom arguments to SystemRunLoop ctor.
        /// If you don't call MainRunLoop::CreateInstance, MainRunLoop
        /// will create a \see{ThreadRunLoop} on it's first invocation of Instance.
        ///
        /// VERY IMPORTANT: MainRunLoop::CreateInstance performs initialization
        /// (calls Thread::SetMainThread ()) that only makes sense when called from the
        /// main thread (main).
        ///
        /// Follow these templates in order to create the \see{SystemRunLoop} on the
        /// right thread:
        ///
        /// On Windows:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// int CALLBACK WinMain (
        ///         HINSTANCE /*hInstance*/,
        ///         HINSTANCE /*hPrevInstance*/,
        ///         LPSTR /*lpCmdLine*/,
        ///         int /*nCmdShow*/) {
        ///     ...
        ///     util::MainRunLoop::CreateInstance (
        ///         "MainRunLoop",
        ///         util::RunLoop::JobExecutionPolicy::SharedPtr (
        ///             new util::RunLoop::FIFOJobExecutionPolicy),
        ///         0,
        ///         0,
        ///         util::SystemRunLoop::CreateThreadWindow ());
        ///     ...
        ///     BOOL result;
        ///     MSG msg;
        ///     while ((result = GetMessage (&msg, 0, 0, 0)) != 0) {
        ///         if (result != -1) {
        ///             TranslateMessage (&msg);
        ///             DispatchMessage (&msg);
        ///         }
        ///         else {
        ///             THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
        ///                 THEKOGANS_UTIL_OS_ERROR_CODE);
        ///         }
        ///     }
        ///     or
        ///     util::MainRunLoop::Instance ().Start ();
        ///     ...
        ///     return 0;
        /// }
        /// \endcode
        ///
        /// On Linux:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// int main (
        ///         int /*argc*/,
        ///         const char * /*argv*/ []) {
        ///     ...
        ///     util::MainRunLoop::CreateInstance (
        ///         "MainRunLoop",
        ///         util::RunLoop::JobExecutionPolicy::SharedPtr (
        ///             new util::RunLoop::FIFOJobExecutionPolicy),
        ///         0,
        ///         0,
        ///         util::SystemRunLoop::CreateThreadWindow ());
        ///     ...
        ///     Display *display = XOpenDisplay (0);
        ///     while (1) {
        ///         XEvent event;
        ///         XNextEvent (display, &event);
        ///         if (!util::MainRunLoop::Instance ().DispatchEvent (display, event)) {
        ///             break;
        ///         }
        ///     }
        ///     or
        ///     util::MainRunLoop::Instance ().Start ();
        ///     ...
        ///     return 0;
        /// }
        /// \endcode
        ///
        /// On OS X:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// int main (
        ///         int /*argc*/,
        ///         const char * /*argv*/ []) {
        ///     ...
        ///     // If using Cocoa, uncomment the following two lines.
        ///     //util::RunLoop::CocoaInitializer cocoaInitializer;
        ///     //util::RunLoop::WorkerInitializer workerInitializer (&cocoaInitializer);
        ///     ...
        ///     util::MainRunLoop::CreateInstance (
        ///         "MainRunLoop",
        ///         util::RunLoop::JobExecutionPolicy::SharedPtr (
        ///             new util::RunLoop::FIFOJobExecutionPolicy),
        ///         SystemRunLoop::OSXRunLoop::SharedPtr (
        ///             new SystemRunLoop::CocoaOSXRunLoop
        ///             or
        ///             new SystemRunLoop::CFOSXRunLoop (CFRunLoopGetMain ())));
        ///     ...
        ///     util::MainRunLoop::Instance ().Start ();
        ///     ...
        ///     return 0;
        /// }
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL MainRunLoopInstanceCreator {
            /// \brief
            /// Create a main thread run loop with custom ctor arguments.
            /// \param[in] name RunLoop name.
            /// \param[in] jobExecutionPolicy JobQueue \see{RunLoop::JobExecutionPolicy}.
            /// \return A main thread run loop with custom ctor arguments.
            RunLoop *operator () (
                    const std::string &name = "MainRunLoop",
                    RunLoop::JobExecutionPolicy::SharedPtr jobExecutionPolicy =
                        RunLoop::JobExecutionPolicy::SharedPtr (new RunLoop::FIFOJobExecutionPolicy)) {
                Thread::SetMainThread ();
                RunLoop *runLoop =
                #if defined (TOOLCHAIN_OS_Windows)
                    new SystemRunLoop<os::windows::RunLoop> (name, jobExecutionPolicy);
                #elif defined (TOOLCHAIN_OS_Linux)
                    new SystemRunLoop<os::linux::XlibRunLoop> (name, jobExecutionPolicy);
                #elif defined (TOOLCHAIN_OS_OSX)
                    new SystemRunLoop<os::osx::NSAppRunLoop> (name, jobExecutionPolicy);
                #endif // defined (TOOLCHAIN_OS_Windows)
                runLoop->AddRef ();
                return runLoop;
            }
        };

        /// \struct MainRunLoop MainRunLoop.h thekogans/util/MainRunLoop.h
        ///
        /// \brief
        /// Main thread run loop.
        struct _LIB_THEKOGANS_UTIL_DECL MainRunLoop :
            public Singleton<
                RunLoop,
                SpinLock,
                MainRunLoopInstanceCreator,
                RefCountedInstanceDestroyer<RunLoop>> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_MainRunLoop_h)
