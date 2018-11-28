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

#if !defined (__thekogans_util_Timer_h)
#define __thekogans_util_Timer_h

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
#elif defined (TOOLCHAIN_OS_Linux)
    #include <ctime>
#elif defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/RunLoop.h"
#include "thekogans/util/JobQueuePool.h"

namespace thekogans {
    namespace util {

        /// \struct Timer Timer.h thekogans/util/Timer.h
        ///
        /// \brief
        /// A millisecond resolution, platform independent, asynchronous
        /// timer. It's use is suitable where accuracy is not paramount
        /// (idle processing). If high resolution timing is what you need,
        /// use HRTimer/HRTimerMgr instead. Here is a typical use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// struct IdleProcessor :
        ///         public util::Timer::Callback,
        ///         public util::Singleton<IdleProcessor, util::SpinLock> {
        /// private:
        ///     util::Timer timer;
        ///     util::JobQueue jobQueue;
        ///
        /// public:
        ///     IdleProcessor () :
        ///         timer ("IdleProcessor"),
        ///         jobQueue (
        ///             "IdleProcessor",
        ///             util::RunLoop::TYPE_FIFO,
        ///             util::UI32_MAX,
        ///             1,
        ///             THEKOGANS_UTIL_LOW_THREAD_PRIORITY) {}
        ///
        ///     inline void StartTimer (const util::TimeSpec &timeSpec) {
        ///         timer.Start (timeSpec);
        ///     }
        ///     inline void StopTimer () {
        ///         timer.Stop ();
        ///     }
        ///
        ///     inline void CancelPendingJobs (bool waitForIdle = true) {
        ///         jobQueue.CancelAllJobs ();
        ///         if (waitForIdle) {
        ///             jobQueue.WaitForIdle ();
        ///         }
        ///     }
        ///
        /// private:
        ///     // Timer::Callback
        ///     virtual void Alarm (util::Timer & /*timer*/) throw () {
        ///         // queue idle jobs.
        ///         jobQueue.EnqJob (...);
        ///     }
        /// };
        /// \endcode
        ///
        /// In your code you can now write:
        ///
        /// \code{.cpp}
        /// IdleProcessor::Instance ().StartTimer (TimeSpec::FromSeconds (30));
        /// \endcode
        ///
        /// This will arm the IdleProcessor timer to fire after 30 seconds.
        /// Call IdleProcessor::Instance ().StopTimer () to disarm the timer.
        ///
        /// NOTE: Timer is designed to have the same semantics on all supported
        /// platforms. To that end if you're using a periodic timer and it fires
        /// while Timer::Callback::Alarm is in progress, that event will be silently
        /// dropped and that cycle will be missed. There are no 'catchup' events.
        ///
        /// NOTE: IdleProcessor demonstrates the canonical way of using Timer.
        /// By coupling the lifetime of the timer to the lifetime of the callback
        /// (IdleProcessor), we avoid calling callback through a dangling pointer.

        struct _LIB_THEKOGANS_UTIL_DECL Timer {
            enum {
                /// \brief
                /// Default minimum number of \see{JobQueue}s in the pool.
                DEFAULT_MIN_JOB_QUEUE_POOL_JOB_QUEUES = 1,
                /// \brief
                /// Default maximum number of \see{JobQueue}s in the pool.
                DEFAULT_MAX_JOB_QUEUE_POOL_JOB_QUEUES = 100,
            };

            /// \struct JobQueuePoolCreateInstance Timer.h thekogans/util/Timer.h
            ///
            /// \brief
            /// Call Timer::JobQueuePoolCreateInstance::Parameterize before the first use of
            /// Timer to supply custom arguments to JobQueuePool ctor.
            struct _LIB_THEKOGANS_UTIL_DECL JobQueuePoolCreateInstance {
            private:
                /// \brief
                /// Minimum number of \see{JobQueue}s to keep in the pool.
                static ui32 minJobQueues;
                /// \brief
                /// Maximum number of \see{JobQueue}s allowed in the pool.
                static ui32 maxJobQueues;

            public:
                /// \brief
                /// Call before the first use of Timer.
                /// \param[in] minJobQueues_ Minimum \see{JobQueue}s to keep in the pool.
                /// \param[in] maxJobQueues_ Maximum \see{JobQueue}s to allow the pool to grow to.
                static void Parameterize (
                    ui32 minJobQueues_,
                    ui32 maxJobQueues_);

                /// \brief
                /// Create a Timer \see{JobQueuePool} with custom ctor arguments.
                /// \return A Timer \see{JobQueuePool} with custom ctor arguments.
                util::JobQueuePool *operator () ();
            };

            /// \struct Timer::Callback Timer.h thekogans/util/Timer.h
            ///
            /// \brief
            /// An abstract base class used to notify timer listeners.
            struct _LIB_THEKOGANS_UTIL_DECL Callback {
                /// \brief
                /// dtor.
                virtual ~Callback () {}

                /// \brief
                /// Called every time the timer fires.
                /// \param[in] timer Timer that fired.
                virtual void Alarm (Timer & /*timer*/) throw () = 0;
            };

        private:
            /// \brief
            /// Receiver of the alarm notofocations.
            Callback &callback;
            /// \brief
            /// Timer name.
            std::string name;
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows native timer object.
            PTP_TIMER timer;
            /// \brief
            /// Windows timer callback.
            static VOID CALLBACK TimerCallback (
                PTP_CALLBACK_INSTANCE /*Instance*/,
                PVOID Context,
                PTP_TIMER /*Timer*/);
        #elif defined (TOOLCHAIN_OS_Linux)
            /// \brief
            /// Linux native timer object.
            timer_t timer;
            /// \brief
            /// Linux timer callback.
            static void TimerCallback (union sigval val);
        #elif defined (TOOLCHAIN_OS_OSX)
            /// \brief
            /// OS X native timer object.
            KQueueTimer *timer;
            /// \brief
            /// OS X timer callback.
            static void TimerCallback (void *userData);
        #endif // defined (TOOLCHAIN_OS_Windows)

        public:
            /// \brief
            /// ctor.
            /// \param[in] callback_ Callback to notify of timer events.
            /// \param[in] name_ Timer name. Use this parameter to help
            /// identify the timer that fired. This way a single callback
            /// can process multiple timers and be able to distinguish
            /// between them.
            Timer (
                Callback &callback_,
                const std::string &name_ = std::string ());
            /// \brief
            /// dtor.
            ~Timer ();

            /// \brief
            /// Return the timer name.
            /// \return Timer name.
            inline const std::string &GetName () const {
                return name;
            }

            /// \brief
            /// Start the timer. If the timer is already running
            /// it will be rearmed with the new parameters.
            /// \param[in] timeSpec How long before the timer fires.
            /// IMPORTANT: This is a relative value.
            /// \param[in] periodic true = timer is periodic, false = timer is one shot.
            /// It will fire with the period of timeSpec.
            void Start (
                const TimeSpec &timeSpec,
                bool periodic = false);
            /// \brief
            /// Stop the timer.
            void Stop ();

            /// \brief
            /// Rreturn true if timer is armed and running.
            /// \return true == timer is armed and running.
            bool IsRunning ();

            /// \brief
            /// Wait for all pending callbacks to complete.
            /// \param[in] timeSpec How long to wait for the callbacks to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \param[in] cancelCallbacks true == Cancel pending callbacks.
            /// \return true == All jobs completed, false == One or more jobs timed out.
            bool WaitForCallbacks (
                const TimeSpec &timeSpec = TimeSpec::Infinite,
                bool cancelCallbacks = true);

        private:
            /// \brief
            /// Convenient typedef for Singleton<util::JobQueuePool, SpinLock, JobQueuePoolCreateInstance>.
            typedef Singleton<util::JobQueuePool, SpinLock, JobQueuePoolCreateInstance> JobQueuePool;
            /// \brief
            /// Forward declaration of Job.
            struct Job;
            /// \brief
            /// Job list id.
            enum {
                /// \brief
                /// JobList ID.
                JOB_LIST_ID = RunLoop::LAST_JOB_LIST_ID,
                /// \brief
                /// Use this sentinel to create your own job lists.
                LAST_JOB_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<Job, JOB_LIST_ID>.
            typedef IntrusiveList<Job, JOB_LIST_ID> JobList;
            /// \brief
            /// Queue of pending jobs.
            JobList jobs;
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

            /// \brief
            /// Used internally to queue up a Job.
            void QueueJob ();

            /// \brief
            /// Timer is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Timer)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Timer_h)
