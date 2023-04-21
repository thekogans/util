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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #if defined (__GNUC__)
        #define _WIN32_WINNT 0x0600
        #include <threadpoolapiset.h>
    #endif // defined (__GNUC__)
#elif defined (TOOLCHAIN_OS_Linux)
    #include <signal.h>
#elif defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Timer.h"

namespace thekogans {
    namespace util {

    #if defined (TOOLCHAIN_OS_Windows)
        VOID CALLBACK Timer::TimerCallback (
                PTP_CALLBACK_INSTANCE /*Instance*/,
                PVOID Context,
                PTP_TIMER Timer_) {
            Timer::SharedPtr timer = TimerRegistry::Instance ().Get (Context);
            if (timer.Get () != 0) {
                if (!timer->periodic) {
                    SetThreadpoolTimer (Timer_, 0, 0, 0);
                }
                timer->Produce (
                    std::bind (
                        &TimerEvents::OnTimerAlarm,
                        std::placeholders::_1,
                        timer));
            }
        }
    #elif defined (TOOLCHAIN_OS_Linux)
        void Timer::TimerCallback (union sigval val) {
            Timer::SharedPtr timer = TimerRegistry::Instance ().Get (val.sival_ptr);
            if (timer.Get () != 0) {
                if (!timer->periodic) {
                    timer->Stop ();
                }
                timer->Produce (
                    std::bind (
                        &TimerEvents::OnTimerAlarm,
                        std::placeholders::_1,
                        timer));
            }
        }
    #elif defined (TOOLCHAIN_OS_OSX)
        void Timer::TimerCallback (void *userData) {
            Timer::SharedPtr timer = TimerRegistry::Instance ().Get ((TimerRegistry::Token::ValueType)userData);
            if (timer != 0) {
                if (!timer->periodic) {
                    timer->Stop ();
                }
                timer->Produce (
                    std::bind (
                        &TimerEvents::OnTimerAlarm,
                        std::placeholders::_1,
                        timer));
            }
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        Timer::Timer (const std::string &name_) :
                name (name_),
                token (this),
                timer (0),
                periodic (false) {
        #if defined (TOOLCHAIN_OS_Windows)
            timer = CreateThreadpoolTimer (TimerCallback, (void *)token.GetValue (), 0);
            if (timer == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #elif defined (TOOLCHAIN_OS_Linux)
            sigevent sigEvent;
            memset (&sigEvent, 0, sizeof (sigEvent));
            sigEvent.sigev_notify = SIGEV_THREAD;
            sigEvent.sigev_value.sival_ptr = (void *)token.GetValue ();
            sigEvent.sigev_notify_function = TimerCallback;
            while (timer_create (CLOCK_REALTIME, &sigEvent, &timer) != 0) {
                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                if (errorCode != EAGAIN) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
                Sleep (TimeSpec::FromMilliseconds (50));
            }
        #elif defined (TOOLCHAIN_OS_OSX)
            timer = CreateKQueueTimer (TimerCallback, (void *)token.GetValue ());
            if (timer == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Timer::~Timer () {
            Stop ();
        #if defined (TOOLCHAIN_OS_Windows)
            CloseThreadpoolTimer (timer);
        #elif defined (TOOLCHAIN_OS_Linux)
            timer_delete (timer);
        #elif defined (TOOLCHAIN_OS_OSX)
            DestroyKQueueTimer (timer);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Timer::Start (
                const TimeSpec &timeSpec,
                bool periodic) {
            if (timeSpec != TimeSpec::Infinite) {
            #if defined (TOOLCHAIN_OS_Windows)
                ULARGE_INTEGER largeInteger;
                largeInteger.QuadPart = timeSpec.ToMilliseconds ();
                largeInteger.QuadPart *= -10000;
                FILETIME dueTime;
                dueTime.dwLowDateTime = largeInteger.LowPart;
                dueTime.dwHighDateTime = largeInteger.HighPart;
                SetThreadpoolTimer (timer, &dueTime,
                    periodic ? (DWORD)timeSpec.ToMilliseconds () : 0, 0);
            #elif defined (TOOLCHAIN_OS_Linux)
                itimerspec spec;
                memset (&spec, 0, sizeof (spec));
                if (periodic) {
                    spec.it_interval = timeSpec.Totimespec ();
                }
                spec.it_value = timeSpec.Totimespec ();
                if (timer_settime (timer, 0, &spec, 0) != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #elif defined (TOOLCHAIN_OS_OSX)
                StartKQueueTimer (timer, timeSpec, periodic);
            #endif // defined (TOOLCHAIN_OS_Windows)
                this->periodic = periodic;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Timer::Stop () {
        #if defined (TOOLCHAIN_OS_Windows)
            SetThreadpoolTimer (timer, 0, 0, 0);
            WaitForThreadpoolTimerCallbacks (timer, TRUE);
        #elif defined (TOOLCHAIN_OS_Linux)
            itimerspec spec;
            memset (&spec, 0, sizeof (spec));
            if (timer_settime (timer, 0, &spec, 0) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #elif defined (TOOLCHAIN_OS_OSX)
            StopKQueueTimer (timer);
        #endif // defined (TOOLCHAIN_OS_Windows)
            periodic = false;
        }

        bool Timer::IsRunning () {
        #if defined (TOOLCHAIN_OS_Windows)
            return IsThreadpoolTimerSet (timer) == TRUE;
        #elif defined (TOOLCHAIN_OS_Linux)
            itimerspec spec;
            memset (&spec, 0, sizeof (spec));
            if (timer_gettime (timer, &spec) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return TimeSpec (spec.it_value) != TimeSpec::Zero;
        #elif defined (TOOLCHAIN_OS_OSX)
            return IsKQueueTimerRunning (timer);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
