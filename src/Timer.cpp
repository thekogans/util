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
    #if defined (__GNUC__)
        #define _WIN32_WINNT 0x0600
        #include <threadpoolapiset.h>
    #endif // defined (__GNUC__)
#elif defined (TOOLCHAIN_OS_Linux)
    #include <signal.h>
#elif defined (TOOLCHAIN_OS_OSX)
    #include <sys/types.h>
    #include <sys/event.h>
    #include <sys/time.h>
    #include "thekogans/util/Constants.h"
    #include "thekogans/util/Singleton.h"
    #include "thekogans/util/SpinLock.h"
    #include "thekogans/util/Thread.h"
    #include "thekogans/util/internal.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Timer.h"

namespace thekogans {
    namespace util {

    #if defined (TOOLCHAIN_OS_Windows)
        VOID CALLBACK TimerCallback (
                PTP_CALLBACK_INSTANCE /*Instance*/,
                PVOID Context,
                PTP_TIMER /*Timer*/) {
            Timer *timer = static_cast<Timer *> (Context);
            if (timer != 0) {
                if (!timer->periodic) {
                    timer->Stop ();
                }
                timer->callback.Alarm (*timer);
            }
        }
    #elif defined (TOOLCHAIN_OS_Linux)
        void TimerCallback (union sigval val) {
            Timer *timer = static_cast<Timer *> (val.sival_ptr);
            if (timer != 0) {
                if (!timer->periodic) {
                    timer->Stop ();
                }
                timer->callback.Alarm (*timer);
            }
        }
    #elif defined (TOOLCHAIN_OS_OSX)
        namespace {
        #if defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
            typedef std::atomic<ui64> IdPool;
        #else // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
            typedef boost::atomic<ui64> IdPool;
        #endif // defined (THEKOGANS_UTIL_HAVE_STD_ATOMIC)
        }

        struct TimerQueue :
                public Singleton<TimerQueue, SpinLock>,
                public Thread {
            THEKOGANS_UTIL_HANDLE handle;
            IdPool idPool;

            TimerQueue () :
                    Thread ("TimerQueue"),
                    handle (kqueue ()),
                    idPool (0) {
                if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                Create (THEKOGANS_UTIL_HIGH_THREAD_PRIORITY);
            }
            // Considering this is a singleton, if this dtor ever gets
            // called, we have a big problem. It's here only for show and
            // completeness.
            ~TimerQueue () {
                close (handle);
            }

            void StartTimer (
                    Timer &timer,
                    const TimeSpec &timeSpec,
                    bool periodic) {
                ui64 id = timer.id == NIDX64 ? idPool++ : timer.id;
                uint16_t flags = EV_ADD;
                if (!periodic) {
                    flags |= EV_ONESHOT;
                }
                keventStruct event;
                keventSet (&event, id, EVFILT_TIMER, flags, 0,
                    timeSpec.ToMilliseconds (), &timer);
                if (keventFunc (handle, &event, 1, 0, 0, 0) != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                timer.id = id;
                timer.periodic = periodic;
            }

            void StopTimer (Timer &timer) {
                if (timer.id != NIDX64) {
                    keventStruct event;
                    keventSet (&event, timer.id, EVFILT_TIMER, EV_DELETE, 0, 0, 0);
                    if (keventFunc (handle, &event, 1, 0, 0, 0) != 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                    timer.id = NIDX64;
                    timer.periodic = false;
                }
            }

            bool IsTimerRunning (const Timer &timer) const {
                return timer.id != NIDX64;
            }

        private:
            void Run () {
                const ui32 MaxEventsBatch = 32;
                keventStruct kqueueEvents[MaxEventsBatch];
                while (1) {
                    int count = keventFunc (handle, 0, 0, kqueueEvents, MaxEventsBatch, 0);
                    for (int i = 0; i < count; ++i) {
                        Timer *timer = (Timer *)kqueueEvents[i].udata;
                        if (timer != 0) {
                            if (!timer->periodic) {
                                timer->id = NIDX64;
                            }
                            timer->callback.Alarm (*timer);
                        }
                    }
                }
            }
        };
    #endif // defined (TOOLCHAIN_OS_Windows)

    #if defined (TOOLCHAIN_OS_Windows)
        Timer::Timer (
                Callback &callback_,
                const std::string &name_) :
                callback (callback_),
                name (name_),
                timer (0),
                periodic (false) {
            timer = CreateThreadpoolTimer (TimerCallback, this, 0);
            if (timer == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        }

        Timer::~Timer () {
            Stop ();
            WaitForThreadpoolTimerCallbacks (timer, TRUE);
            CloseThreadpoolTimer (timer);
        }
    #elif defined (TOOLCHAIN_OS_Linux)
        Timer::Timer (
                Callback &callback_,
                const std::string &name_) :
                callback (callback_),
                name (name_),
                timer (0),
                periodic (false) {
            sigevent sigEvent;
            memset (&sigEvent, 0, sizeof (sigEvent));
            sigEvent.sigev_notify = SIGEV_THREAD;
            sigEvent.sigev_value.sival_ptr = this;
            sigEvent.sigev_notify_function = TimerCallback;
            if (timer_create (CLOCK_REALTIME, &sigEvent, &timer) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        }

        Timer::~Timer () {
            timer_delete (timer);
        }
    #elif defined (TOOLCHAIN_OS_OSX)
        Timer::Timer (
            Callback &callback_,
            const std::string &name_) :
            callback (callback_),
            name (name_),
            id (NIDX64),
            periodic (false) {}

        Timer::~Timer () {
            TimerQueue::Instance ().StopTimer (*this);
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        void Timer::Start (
                const TimeSpec &timeSpec,
                bool periodic_) {
            if (timeSpec != TimeSpec::Infinite) {
            #if defined (TOOLCHAIN_OS_Windows)
                ULARGE_INTEGER largeInteger;
                largeInteger.QuadPart = timeSpec.ToMilliseconds ();
                largeInteger.QuadPart *= -10000;
                FILETIME dueTime;
                dueTime.dwLowDateTime = largeInteger.LowPart;
                dueTime.dwHighDateTime = largeInteger.HighPart;
                SetThreadpoolTimer (timer, &dueTime,
                    periodic_ ? (DWORD)timeSpec.ToMilliseconds () : 0, 0);
            #elif defined (TOOLCHAIN_OS_Linux)
                itimerspec timerSpec;
                memset (&timerSpec, 0, sizeof (timerSpec));
                if (periodic_) {
                    timerSpec.it_interval = timeSpec;
                }
                timerSpec.it_value = timeSpec;
                if (timer_settime (timer, 0, &timerSpec, 0) != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #elif defined (TOOLCHAIN_OS_OSX)
                TimerQueue::Instance ().StartTimer (*this, timeSpec, periodic_);
            #endif // defined (TOOLCHAIN_OS_Windows)
                periodic = periodic_;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Timer::Stop () {
        #if defined (TOOLCHAIN_OS_Windows)
            SetThreadpoolTimer (timer, 0, 0, 0);
        #elif defined (TOOLCHAIN_OS_Linux)
            itimerspec spec;
            memset (&spec, 0, sizeof (itimerspec));
            if (timer_settime (timer, 0, &spec, 0) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #elif defined (TOOLCHAIN_OS_OSX)
            TimerQueue::Instance ().StopTimer (*this);
        #endif // defined (TOOLCHAIN_OS_Windows)
            periodic = false;
        }

        bool Timer::IsRunning () const {
        #if defined (TOOLCHAIN_OS_Windows)
            return IsThreadpoolTimerSet (timer) == TRUE;
        #elif defined (TOOLCHAIN_OS_Linux)
            itimerspec spec;
            memset (&spec, 0, sizeof (itimerspec));
            if (timer_gettime (timer, &spec) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return
                spec.it_interval != TimeSpec::Zero ||
                spec.it_value != TimeSpec::Zero;
        #elif defined (TOOLCHAIN_OS_OSX)
            return TimerQueue::Instance ().IsTimerRunning (*this);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
