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
    #include "thekogans/util/Thread.h"
    #include "thekogans/util/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Timer.h"

namespace thekogans {
    namespace util {

        ui32 Timer::JobQueuePoolCreateInstance::minJobQueues = Timer::DEFAULT_MIN_JOB_QUEUE_POOL_JOB_QUEUES;
        ui32 Timer::JobQueuePoolCreateInstance::maxJobQueues = Timer::DEFAULT_MAX_JOB_QUEUE_POOL_JOB_QUEUES;

        void Timer::JobQueuePoolCreateInstance::Parameterize (
                ui32 minJobQueues_,
                ui32 maxJobQueues_) {
            if (minJobQueues_ != 0 && maxJobQueues_ != 0 && minJobQueues_ <= maxJobQueues_) {
                minJobQueues = minJobQueues_;
                maxJobQueues = maxJobQueues_;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        util::JobQueuePool *Timer::JobQueuePoolCreateInstance::operator () () {
            return new util::JobQueuePool (minJobQueues, maxJobQueues, "TimerJobQueuePool");
        }

    #if defined (TOOLCHAIN_OS_Windows)
        VOID CALLBACK Timer::TimerCallback (
                PTP_CALLBACK_INSTANCE /*Instance*/,
                PVOID Context,
                PTP_TIMER /*Timer*/) {
            Timer *timer = static_cast<Timer *> (Context);
            if (timer != 0) {
                timer->QueueAlarmJob ();
            }
        }
    #elif defined (TOOLCHAIN_OS_Linux)
        void Timer::TimerCallback (union sigval val) {
            Timer *timer = static_cast<Timer *> (val.sival_ptr);
            if (timer != 0) {
                timer->QueueAlarmJob ();
            }
        }
    #elif defined (TOOLCHAIN_OS_OSX)
        THEKOGANS_UTIL_ATOMIC<ui64> Timer::idPool (0);

        struct Timer::KQueue :
                public Singleton<KQueue, SpinLock>,
                public Thread {
            THEKOGANS_UTIL_HANDLE handle;

            KQueue () :
                    Thread ("TimerKQueue"),
                    handle (kqueue ()) {
                if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                Create (THEKOGANS_UTIL_HIGH_THREAD_PRIORITY);
            }
            // Considering this is a singleton, if this dtor ever gets
            // called, we have a big problem. It's here only for show and
            // completeness.
            ~KQueue () {
                close (handle);
            }

            void StartTimer (
                    Timer &timer,
                    const TimeSpec &timeSpec,
                    bool periodic) {
                ui16 flags = EV_ADD;
                if (!periodic) {
                    flags |= EV_ONESHOT;
                }
                keventStruct event;
                keventSet (&event, timer.id, EVFILT_TIMER, flags, 0,
                    timeSpec.ToMilliseconds (), &timer);
                if (keventFunc (handle, &event, 1, 0, 0, 0) != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }

            void StopTimer (Timer &timer) {
                keventStruct event;
                keventSet (&event, timer.id, EVFILT_TIMER, EV_DELETE, 0, 0, 0);
                // No error checking here as non-periodic filters are
                // removed from the queue after an event has been delivered.
                keventFunc (handle, &event, 1, 0, 0, 0);
            }

            bool IsTimerRunning (const Timer &timer) {
                keventStruct event;
                keventSet (&event, timer.id, EVFILT_TIMER, EV_ENABLE, 0, 0, 0);
                return keventFunc (handle, &event, 1, 0, 0, 0) == 0;
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
                            timer->QueueAlarmJob ();
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
                timer (0) {
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
                timer (0) {
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
            id (idPool++) {}

        Timer::~Timer () {
            Stop ();
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        void Timer::Start (
                const TimeSpec &timeSpec,
                bool periodic) {
            if (timeSpec != TimeSpec::Zero && timeSpec != TimeSpec::Infinite) {
                LockGuard<SpinLock> guard (spinLock);
                StopHelper ();
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
                spec.it_value = timeSpec_.Totimespec ();
                if (timer_settime (timer, 0, &spec, 0) != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #elif defined (TOOLCHAIN_OS_OSX)
                KQueue::Instance ().StartTimer (*this, timeSpec, periodic);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Timer::Stop () {
            LockGuard<SpinLock> guard (spinLock);
            StopHelper ();
        }

        bool Timer::IsRunning () {
            LockGuard<SpinLock> guard (spinLock);
        #if defined (TOOLCHAIN_OS_Windows)
            return IsThreadpoolTimerSet (timer) == TRUE;
        #elif defined (TOOLCHAIN_OS_Linux)
            itimerspec spec;
            memset (&spec, 0, sizeof (spec));
            if (timer_gettime (timer, &spec) != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return spec.it_value != TimeSpec::Zero;
        #elif defined (TOOLCHAIN_OS_OSX)
            return KQueue::Instance ().IsTimerRunning (*this);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        struct Timer::AlarmJob : public RunLoop::Job {
            THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (AlarmJob, SpinLock)

            JobQueue::Ptr jobQueue;
            Timer &timer;

            AlarmJob (
                JobQueue::Ptr jobQueue_,
                Timer &timer_) :
                jobQueue (jobQueue_),
                timer (timer_) {}

        protected:
            // RunLoop::Job
            virtual void Execute (const THEKOGANS_UTIL_ATOMIC<bool> &done) throw () {
                if (!ShouldStop (done)) {
                    if (timer.inAlarmSpinLock.TryAcquire ()) {
                        LockGuard<SpinLock> guard (timer.inAlarmSpinLock, false);
                        timer.callback.Alarm (timer);
                    }
                    else {
                        THEKOGANS_UTIL_LOG_SUBSYSTEM_WARNING (
                            THEKOGANS_UTIL,
                            "Skipping overlapping '%s' Alarm call.\n",
                            timer.name.c_str ());
                    }
                }
            }
        };

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (Timer::AlarmJob, SpinLock)

        void Timer::StopHelper () {
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
            KQueue::Instance ().StopTimer (*this);
        #endif // defined (TOOLCHAIN_OS_Windows)
            RunLoop::UserJobList jobs;
            struct GetJobsEqualityTest : public RunLoop::EqualityTest {
                Timer &timer;
                explicit GetJobsEqualityTest (Timer &timer_) :
                    timer (timer_) {}
                virtual bool operator () (RunLoop::Job &job) const throw () {
                    AlarmJob *alarmJob = dynamic_cast<AlarmJob *> (&job);
                    return alarmJob != 0 && &alarmJob->timer == &timer;
                }
            } getJobsEqualityTest (*this);
            JobQueuePool::Instance ().GetJobs (getJobsEqualityTest, jobs);
            RunLoop::CancelJobs (jobs);
            RunLoop::WaitForJobs (jobs);
        }

        void Timer::QueueAlarmJob () {
            LockGuard<SpinLock> guard (spinLock);
            // Try to acquire a job queue from the pool. Note the
            // retry count == 0. Delivering timer alarms on time
            // is more important then delivering them at all. It's
            // better to log a warning to let the developer adjust
            // available/max queue count (JobQueuePoolCreateInstance).
            JobQueue::Ptr jobQueue = JobQueuePool::Instance ().GetJobQueue (0);
            if (jobQueue.Get () != 0) {
                THEKOGANS_UTIL_TRY {
                    jobQueue->EnqJob (RunLoop::Job::Ptr (new AlarmJob (jobQueue, *this)));
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

    } // namespace util
} // namespace thekogans
