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

#include "thekogans/util/DefaultRunLoop.h"
#include "thekogans/util/MainRunLoop.h"

namespace thekogans {
    namespace util {

        std::string MainRunLoopCreateInstance::name;
        RunLoop::Type MainRunLoopCreateInstance::type = RunLoop::TYPE_FIFO;
        ui32 MainRunLoopCreateInstance::maxPendingJobs = UI32_MAX;
        RunLoop::WorkerCallback *MainRunLoopCreateInstance::workerCallback = 0;
        bool MainRunLoopCreateInstance::willCallStart = true;
    #if defined (TOOLCHAIN_OS_Windows)
        SystemRunLoop::EventProcessor MainRunLoopCreateInstance::eventProcessor = 0;
        void *MainRunLoopCreateInstance::userData = 0;
        Window::Ptr MainRunLoopCreateInstance::window;

        void MainRunLoopCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                RunLoop::WorkerCallback *workerCallback_,
                bool willCallStart_,
                SystemRunLoop::EventProcessor eventProcessor_,
                void *userData_,
                Window::Ptr window_) {
            name = name_;
            type = type_;
            maxPendingJobs = maxPendingJobs_;
            workerCallback = workerCallback_;
            willCallStart = willCallStart_;
            eventProcessor = eventProcessor_;
            userData = userData_;
            window = std::move (window_);
        }

        RunLoop *MainRunLoopCreateInstance::operator () () {
            return window.get () != 0 ?
                (RunLoop *)new SystemRunLoop (
                    name,
                    type,
                    maxPendingJobs,
                    workerCallback,
                    willCallStart,
                    eventProcessor,
                    userData,
                    std::move (window)) :
                (RunLoop *)new DefaultRunLoop (
                    name,
                    type,
                    maxPendingJobs,
                    workerCallback);
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
                RunLoop::WorkerCallback *workerCallback_,
                bool willCallStart_,
                SystemRunLoop::EventProcessor eventProcessor_,
                void *userData_,
                SystemRunLoop::XlibWindow::Ptr window_,
                const std::vector<Display *> &displays_) {
            name = name_;
            type = type_;
            maxPendingJobs = maxPendingJobs_;
            willCallStart = willCallStart_;
            workerCallback = workerCallback_;
            eventProcessor = eventProcessor_;
            userData = userData_;
            window = std::move (window_);
            displays = displays_;
        }
    #endif // defined (THEKOGANS_UTIL_HAVE_XLIB)
        RunLoop *MainRunLoopCreateInstance::operator () () {
        #if defined (THEKOGANS_UTIL_HAVE_XLIB)
            return eventProcessor != 0 ?
                (RunLoop *)new SystemRunLoop (
                    name,
                    type,
                    maxPendingJobs,
                    workerCallback,
                    willCallStart,
                    eventProcessor,
                    userData,
                    std::move (window),
                    displays) :
                (RunLoop *)new DefaultRunLoop (
                    name,
                    type,
                    maxPendingJobs,
                    workerCallback);

        #else // defined (THEKOGANS_UTIL_HAVE_XLIB)
            return new DefaultRunLoop (
                name,
                type,
                maxPendingJobs,
                workerCallback);
        #endif // defined (THEKOGANS_UTIL_HAVE_XLIB)
        }
    #elif defined (TOOLCHAIN_OS_OSX)
        CFRunLoopRef MainRunLoopCreateInstance::runLoop = 0;

        void MainRunLoopCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                RunLoop::WorkerCallback *workerCallback_,
                bool willCallStart_,
                CFRunLoopRef runLoop_) {
            name = name_;
            type = type_;
            maxPendingJobs = maxPendingJobs_;
            workerCallback = workerCallback_;
            willCallStart = willCallStart_;
            runLoop = runLoop_;
        }

        RunLoop *MainRunLoopCreateInstance::operator () () {
            return runLoop != 0 ?
                (RunLoop *)new SystemRunLoop (
                    name,
                    type,
                    maxPendingJobs,
                    workerCallback,
                    willCallStart,
                    runLoop) :
                (RunLoop *)new DefaultRunLoop (
                    name,
                    type,
                    maxPendingJobs,
                    workerCallback);
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

    } // namespace util
} // namespace thekogans
