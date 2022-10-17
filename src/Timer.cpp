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
            Timer *timer = static_cast<Timer *> (Context);
            if (timer != 0) {
                if (!timer->periodic) {
                    SetThreadpoolTimer (Timer_, 0, 0, 0);
                }
                timer->QueueJob ();
            }
        }
    #elif defined (TOOLCHAIN_OS_Linux)
        void Timer::TimerCallback (union sigval val) {
            Timer *timer = static_cast<Timer *> (val.sival_ptr);
            if (timer != 0) {
                if (!timer->periodic) {
                    timer->Stop ();
                }
                timer->QueueJob ();
            }
        }
    #elif defined (TOOLCHAIN_OS_OSX)
        void Timer::TimerCallback (void *userData) {
            Timer *timer = static_cast<Timer *> (userData);
            if (timer != 0) {
                if (!timer->periodic) {
                    timer->Stop ();
                }
                timer->QueueJob ();
            }
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        Timer::Timer (
                Callback &callback_,
                const std::string &name_,
                bool reentrantAlarm_) :
                callback (callback_),
                name (name_),
                reentrantAlarm (reentrantAlarm_),
                timer (0),
                periodic (false) {
        #if defined (TOOLCHAIN_OS_Windows)
            timer = CreateThreadpoolTimer (TimerCallback, this, 0);
            if (timer == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #elif defined (TOOLCHAIN_OS_Linux)
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
        #elif defined (TOOLCHAIN_OS_OSX)
            timer = CreateKQueueTimer (TimerCallback, this);
            if (timer == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Timer::~Timer () {
            Stop ();
            WaitForCallbacks ();
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

        struct Timer::Job :
                public RunLoop::Job,
                public Timer::JobList::Node {
            THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Job, SpinLock)

        private:
            JobQueue::SharedPtr jobQueue;
            Timer &timer;

        public:
            Job (
                JobQueue::SharedPtr jobQueue_,
                Timer &timer_) :
                jobQueue (jobQueue_),
                timer (timer_) {}

        protected:
            // RunLoop::Job
            virtual void SetState (State state) {
                if (state == Pending) {
                    // Don't grab the lock here as SetState (Pending)
                    // will be called from JobQueue::EnqJob which is
                    // protected in QueueJob below.
                    timer.jobs.push_back (this);
                }
                else if (state == Completed) {
                    LockGuard<SpinLock> guard (timer.spinLock);
                    timer.jobs.erase (this);
                }
                RunLoop::Job::SetState (state);
            }

            virtual void Execute (const std::atomic<bool> &done) throw () {
                if (!ShouldStop (done)) {
                    timer.callback.Alarm (timer);
                }
            }
        };

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (Timer::Job, SpinLock)

        bool Timer::WaitForCallbacks (
                const TimeSpec &timeSpec,
                bool cancelCallbacks) {
            RunLoop::UserJobList jobs;
            {
                LockGuard<SpinLock> guard (spinLock);
                this->jobs.for_each (
                    [&jobs, cancelCallbacks] (JobList::Callback::argument_type job) -> JobList::Callback::result_type {
                        if (cancelCallbacks) {
                            job->Cancel ();
                        }
                        jobs.push_back (Job::SharedPtr (job));
                        return true;
                    }
                );
            }
            return RunLoop::WaitForJobs (jobs, timeSpec);
        }

        void Timer::QueueJob () {
            LockGuard<SpinLock> guard (spinLock);
            if (reentrantAlarm || jobs.empty ()) {
                // Try to acquire a job queue from the pool. Note the
                // retry count == 0. Delivering timer alarms on time
                // is more important then delivering them at all. It's
                // better to log a warning to let the developer adjust
                // available/max queue count (JobQueuePoolCreateInstance).
                JobQueue::SharedPtr jobQueue = JobQueuePool::Instance ().GetJobQueue (0);
                if (jobQueue.Get () != 0) {
                    THEKOGANS_UTIL_TRY {
                        jobQueue->EnqJob (Job::SharedPtr (new Job (jobQueue, *this)));
                    }
                    THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
                }
                else {
                    THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING (
                        THEKOGANS_UTIL,
                        "Unable to acquire a '%s' worker, skipping Alarm call.\n",
                        name.c_str ());
                }
            }
            else {
                THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING (
                    THEKOGANS_UTIL,
                    "Supressing non re-entrant Alarm call in '%s'.\n",
                    name.c_str ());
            }
        }

    } // namespace util
} // namespace thekogans
