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
    #include "thekogans/util/Thread.h"
    #include "thekogans/util/WorkerPool.h"
    #include "thekogans/util/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LockGuard.h"
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
                if (timer->spinLock.TryAcquire ()) {
                    LockGuard<SpinLock> guard (timer->spinLock, false);
                    timer->callback.Alarm (*timer);
                }
                else {
                    THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING (
                        THEKOGANS_UTIL,
                        "Skipping overlapping '%s' Alarm call.\n",
                        timer->GetName ().c_str ());
                }
            }
        }
    #elif defined (TOOLCHAIN_OS_Linux)
        void TimerCallback (union sigval val) {
            Timer *timer = static_cast<Timer *> (val.sival_ptr);
            if (timer != 0) {
                if (!timer->periodic) {
                    timer->Stop ();
                }
                if (timer->spinLock.TryAcquire ()) {
                    LockGuard<SpinLock> guard (timer->spinLock, false);
                    timer->callback.Alarm (*timer);
                }
                else {
                    THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING (
                        THEKOGANS_UTIL,
                        "Skipping overlapping '%s' Alarm call.\n",
                        timer->GetName ().c_str ());
                }
            }
        }
    #elif defined (TOOLCHAIN_OS_OSX)
        struct Timer::AlarmJob : public RunLoop::Job {
            WorkerPool::WorkerPtr::Ptr workerPtr;
            Timer *timer;

            AlarmJob (
                    WorkerPool::WorkerPtr::Ptr workerPtr_,
                    Timer *timer_) :
                    workerPtr (workerPtr_),
                    timer (timer_) {
                // NOTE: No need to grab the lock here as it's
                // already acquired by Run below.
                timer->alarmJob = this;
            }
            ~AlarmJob () {
                LockGuard<SpinLock> guard (timer->spinLock);
                timer->alarmJob = 0;
            }

            void Wait () {
                (*workerPtr)->WaitForJob (GetId ());
            }

            // RunLoop::Job
            virtual void Execute (const THEKOGANS_UTIL_ATOMIC<bool> &done) throw () {
                if (!ShouldStop (done)) {
                    // Alarm must be called unprotected if we're to
                    // follow the same semantics as Windows and Linux
                    // and allow it to call Start/Stop.
                    timer->callback.Alarm (*timer);
                }
            }
        };

        struct TimerQueue :
                public Singleton<TimerQueue, SpinLock>,
                public Thread {
            THEKOGANS_UTIL_HANDLE handle;
            THEKOGANS_UTIL_ATOMIC<ui64> idPool;
            WorkerPool workerPool;

            TimerQueue () :
                    Thread ("TimerQueue"),
                    handle (kqueue ()),
                    idPool (0),
                    workerPool (
                        Timer::minWorkers,
                        Timer::maxWorkers,
                        "TimerQueue-WorkerPool") {
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
                LockGuard<SpinLock> guard (timer.spinLock);
                // If we're changing the timers period, cancel
                // previously scheduled Alarm job.
                if (timer.alarmJob != 0) {
                    timer.alarmJob->Cancel ();
                    timer.alarmJob->Wait ();
                }
                // StartTimer can be called repeatedly without calling StopTimer.
                // This behavior is desirable when adjusting an already existing
                // timer. In this case, reuse the existing timer id and EV_ADD will
                // modify the existing kqueue entry.
                ui64 id = timer.id == NIDX64 ? idPool++ : timer.id;
                ui16 flags = EV_ADD;
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
                LockGuard<SpinLock> guard (timer.spinLock);
                if (timer.id != NIDX64) {
                    if (timer.alarmJob != 0) {
                        timer.alarmJob->Cancel ();
                        timer.alarmJob->Wait ();
                    }
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
            void Run () throw () {
                const ui32 MaxEventsBatch = 32;
                keventStruct kqueueEvents[MaxEventsBatch];
                while (1) {
                    int count = keventFunc (handle, 0, 0, kqueueEvents, MaxEventsBatch, 0);
                    for (int i = 0; i < count; ++i) {
                        Timer *timer = (Timer *)kqueueEvents[i].udata;
                        if (timer != 0) {
                            LockGuard<SpinLock> guard (timer->spinLock);
                            if (timer->id != NIDX64) {
                                if (timer->alarmJob == 0) {
                                    if (!timer->periodic) {
                                        timer->id = NIDX64;
                                    }
                                    THEKOGANS_UTIL_TRY {
                                        WorkerPool::WorkerPtr::Ptr workerPtr (
                                            new WorkerPool::WorkerPtr (workerPool));
                                        (*workerPtr)->EnqJob (
                                            RunLoop::Job::Ptr (
                                                new Timer::AlarmJob (workerPtr, timer)));
                                    }
                                    THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
                                }
                                else {
                                    THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING (
                                        THEKOGANS_UTIL,
                                        "Skipping overlapping '%s' Alarm call.\n",
                                        timer->GetName ().c_str ());
                                }
                            }
                        }
                    }
                }
            }
        };

        ui32 Timer::minWorkers = DEFAULT_MIN_WORKER_POOL_WORKERS;
        ui32 Timer::maxWorkers = DEFAULT_MAX_WORKER_POOL_WORKERS;

        void Timer::SetWorkerPoolMinMaxWorkers (
                ui32 minWorkers_,
                ui32 maxWorkers_) {
            minWorkers = minWorkers_;
            maxWorkers = maxWorkers_;
        }
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
            while (timer_create (CLOCK_REALTIME, &sigEvent, &timer) != 0) {
                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                if (errorCode != EAGAIN) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
                Sleep (TimeSpec::FromMilliseconds (50));
            }
        }

        Timer::~Timer () {
            Stop ();
            timer_delete (timer);
        }
    #elif defined (TOOLCHAIN_OS_OSX)
        Timer::Timer (
            Callback &callback_,
            const std::string &name_) :
            callback (callback_),
            name (name_),
            id (NIDX64),
            alarmJob (0),
            periodic (false) {}

        Timer::~Timer () {
            Stop ();
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
                itimerspec spec;
                memset (&spec, 0, sizeof (spec));
                if (periodic_) {
                    spec.it_interval = timeSpec.Totimespec ();
                }
                spec.it_value = timeSpec.Totimespec ();
                if (timer_settime (timer, 0, &spec, 0) != 0) {
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
            memset (&spec, 0, sizeof (spec));
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
            memset (&spec, 0, sizeof (spec));
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
