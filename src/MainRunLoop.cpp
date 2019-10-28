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

#include "thekogans/util/Thread.h"
#include "thekogans/util/ThreadRunLoop.h"
#include "thekogans/util/MainRunLoop.h"

namespace thekogans {
    namespace util {

        const char * MainRunLoopCreateInstance::MAIN_RUN_LOOP_NAME = "MainRunLoop";

        std::string MainRunLoopCreateInstance::name = MainRunLoopCreateInstance::MAIN_RUN_LOOP_NAME;
        RunLoop::JobExecutionPolicy::Ptr MainRunLoopCreateInstance::jobExecutionPolicy =
            RunLoop::JobExecutionPolicy::Ptr (new RunLoop::FIFOJobExecutionPolicy);

        void MainRunLoopCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy_) {
            name = name_;
            jobExecutionPolicy = jobExecutionPolicy_;
            Thread::SetMainThread ();
        }

    #if defined (TOOLCHAIN_OS_Windows)
        SystemRunLoop::EventProcessor MainRunLoopCreateInstance::eventProcessor = 0;
        void *MainRunLoopCreateInstance::userData = 0;
        Window::Ptr MainRunLoopCreateInstance::window;

        void MainRunLoopCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy_,
                SystemRunLoop::EventProcessor eventProcessor_,
                void *userData_,
                Window::Ptr window_) {
            name = name_;
            jobExecutionPolicy = jobExecutionPolicy_;
            eventProcessor = eventProcessor_;
            userData = userData_;
            window = std::move (window_);
            Thread::SetMainThread ();
        }

        RunLoop *MainRunLoopCreateInstance::operator () () {
            return window.get () != 0 ?
                (RunLoop *)new SystemRunLoop (
                    name,
                    jobExecutionPolicy,
                    eventProcessor,
                    userData,
                    std::move (window)) :
                (RunLoop *)new ThreadRunLoop (
                    name,
                    jobExecutionPolicy);
        }
    #elif defined (TOOLCHAIN_OS_Linux)
    #if defined (THEKOGANS_UTIL_HAVE_XLIB)
        SystemRunLoop::EventProcessor MainRunLoopCreateInstance::eventProcessor = 0;
        void *MainRunLoopCreateInstance::userData = 0;
        SystemRunLoop::XlibWindow::Ptr MainRunLoopCreateInstance::window;
        std::vector<Display *> MainRunLoopCreateInstance::displays;

        void MainRunLoopCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy_,
                SystemRunLoop::EventProcessor eventProcessor_,
                void *userData_,
                SystemRunLoop::XlibWindow::Ptr window_,
                const std::vector<Display *> &displays_) {
            name = name_;
            jobExecutionPolicy = jobExecutionPolicy_;
            eventProcessor = eventProcessor_;
            userData = userData_;
            window = std::move (window_);
            displays = displays_;
            Thread::SetMainThread ();
        }

        RunLoop *MainRunLoopCreateInstance::operator () () {
            return eventProcessor != 0 ?
                (RunLoop *)new SystemRunLoop (
                    name,
                    jobExecutionPolicy,
                    eventProcessor,
                    userData,
                    std::move (window),
                    displays) :
                (RunLoop *)new ThreadRunLoop (
                    name,
                    jobExecutionPolicy);
        }
    #else // defined (THEKOGANS_UTIL_HAVE_XLIB)
        RunLoop *MainRunLoopCreateInstance::operator () () {
            return new ThreadRunLoop (
                name,
                jobExecutionPolicy);
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_XLIB)
    #elif defined (TOOLCHAIN_OS_OSX)
        SystemRunLoop::OSXRunLoop::Ptr MainRunLoopCreateInstance::runLoop;

        void MainRunLoopCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy_,
                SystemRunLoop::OSXRunLoop::Ptr runLoop_) {
            name = name_;
            jobExecutionPolicy = jobExecutionPolicy_;
            runLoop = std::move (runLoop_);
            Thread::SetMainThread ();
        }

        RunLoop *MainRunLoopCreateInstance::operator () () {
            return runLoop.get () != 0 ?
                (RunLoop *)new SystemRunLoop (
                    name,
                    jobExecutionPolicy,
                    std::move (runLoop)) :
                (RunLoop *)new ThreadRunLoop (
                    name,
                    jobExecutionPolicy);
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

    } // namespace util
} // namespace thekogans
