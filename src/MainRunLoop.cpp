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
#include "thekogans/util/DefaultRunLoop.h"
#if defined (THEKOGANS_OS_OSX)
    #include "thekogans/util/Exception.h"
    #include "thekogans/util/OSXUtils.h"
#endif // defined (THEKOGANS_OS_OSX)
#include "thekogans/util/MainRunLoop.h"

namespace thekogans {
    namespace util {

        std::string MainRunLoopCreateInstance::name = "Main Thread";
        RunLoop::Type MainRunLoopCreateInstance::type = RunLoop::TYPE_FIFO;
        ui32 MainRunLoopCreateInstance::maxPendingJobs = UI32_MAX;
        bool MainRunLoopCreateInstance::willCallStart = true;
    #if defined (TOOLCHAIN_OS_Windows)
        SystemRunLoop::EventProcessor MainRunLoopCreateInstance::eventProcessor = 0;
        void *MainRunLoopCreateInstance::userData = 0;
        Window::Ptr MainRunLoopCreateInstance::window;

        void MainRunLoopCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                bool willCallStart_,
                SystemRunLoop::EventProcessor eventProcessor_,
                void *userData_,
                Window::Ptr window_) {
            name = name_;
            type = type_;
            maxPendingJobs = maxPendingJobs_;
            willCallStart = willCallStart_;
            eventProcessor = eventProcessor_;
            userData = userData_;
            window = std::move (window_);
            Thread::SetMainThread ();
        }

        RunLoop *MainRunLoopCreateInstance::operator () () {
            return window.get () != 0 ?
                (RunLoop *)new SystemRunLoop (
                    name,
                    type,
                    maxPendingJobs,
                    willCallStart,
                    eventProcessor,
                    userData,
                    std::move (window)) :
                (RunLoop *)new DefaultRunLoop (
                    name,
                    type,
                    maxPendingJobs);
        }
    #elif defined (TOOLCHAIN_OS_Linux)
    #if defined (THEKOGANS_UTIL_HAVE_XLIB)
        SystemRunLoop::EventProcessor MainRunLoopCreateInstance::eventProcessor = 0;
        void *MainRunLoopCreateInstance::userData = 0;
        SystemRunLoop::XlibWindow::Ptr MainRunLoopCreateInstance::window;
        std::vector<Display *> MainRunLoopCreateInstance::displays;

        void MainRunLoopCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                bool willCallStart_,
                SystemRunLoop::EventProcessor eventProcessor_,
                void *userData_,
                SystemRunLoop::XlibWindow::Ptr window_,
                const std::vector<Display *> &displays_) {
            name = name_;
            type = type_;
            maxPendingJobs = maxPendingJobs_;
            willCallStart = willCallStart_;
            eventProcessor = eventProcessor_;
            userData = userData_;
            window = std::move (window_);
            displays = displays_;
            Thread::SetMainThread ();
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_XLIB)
        RunLoop *MainRunLoopCreateInstance::operator () () {
        #if defined (THEKOGANS_UTIL_HAVE_XLIB)
            return eventProcessor != 0 ?
                (RunLoop *)new SystemRunLoop (
                    name,
                    type,
                    maxPendingJobs,
                    willCallStart,
                    eventProcessor,
                    userData,
                    std::move (window),
                    displays) :
                (RunLoop *)new DefaultRunLoop (
                    name,
                    type,
                    maxPendingJobs);

        #else // defined (THEKOGANS_UTIL_HAVE_XLIB)
            return new DefaultRunLoop (
                name,
                type,
                maxPendingJobs);
        #endif // defined (THEKOGANS_UTIL_HAVE_XLIB)
        }
    #elif defined (TOOLCHAIN_OS_OSX)
        SystemRunLoop::OSXRunLoop::Ptr MainRunLoopCreateInstance::runLoop;

        void MainRunLoopCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                bool willCallStart_,
                SystemRunLoop::OSXRunLoop::Ptr runLoop_) {
            name = name_;
            type = type_;
            maxPendingJobs = maxPendingJobs_;
            willCallStart = willCallStart_;
            runLoop = std::move (runLoop_);
            Thread::SetMainThread ();
        }

        RunLoop *MainRunLoopCreateInstance::operator () () {
            return runLoop.get () != 0 ?
                (RunLoop *)new SystemRunLoop (
                    name,
                    type,
                    maxPendingJobs,
                    willCallStart,
                    std::move (runLoop)) :
                (RunLoop *)new DefaultRunLoop (
                    name,
                    type,
                    maxPendingJobs);
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

    } // namespace util
} // namespace thekogans
