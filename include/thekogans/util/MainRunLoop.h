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
