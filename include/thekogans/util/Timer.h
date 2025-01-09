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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#elif defined (TOOLCHAIN_OS_Linux)
    #include <ctime>
#elif defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/os/osx/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/RefCountedRegistry.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/Producer.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Forward declaration of Timer.
        struct Timer;

        /// \struct TimerEvents Timer.h thekogans/util/Timer.h
        ///
        /// \brief

        struct _LIB_THEKOGANS_UTIL_DECL TimerEvents {
            /// \brief
            /// dtor.
            virtual ~TimerEvents () {}

            /// \brief
            /// Called every time the timer fires.
            /// \param[in] timer Timer that fired.
            virtual void OnTimerAlarm (RefCounted::SharedPtr<Timer> /*timer*/) throw () {}
        };

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
        ///         public util::Subscriber<util::TimerEvents>,
        ///         public util::Singleton<IdleProcessor> {
        /// private:
        ///     util::Timer::SharedPtr timer;
        ///     util::JobQueue jobQueue;
        ///
        /// public:
        ///     IdleProcessor () :
        ///             timer (util::Timer::Create ("IdleProcessor")),
        ///             jobQueue (
        ///                 "IdleProcessor",
        ///                 new util::RunLoop::FIFOJobExecutionPolicy,
        ///                 1,
        ///                 THEKOGANS_UTIL_LOW_THREAD_PRIORITY) {
        ///         Subscribe (
        ///             *timer,
        ///             new util::Producer<util::TimerEvents>::ImmediateEventDeliveryPolicy);
        ///     }
        ///
        ///     inline void StartTimer (const util::TimeSpec &timeSpec) {
        ///         timer->Start (timeSpec);
        ///     }
        ///     inline void StopTimer () {
        ///         timer->Stop ();
        ///     }
        ///
        ///     inline void CancelPendingJobs (
        ///             bool waitForIdle = true,
        ///             const util::TimeSpec &timeSpec = util::TimeSpec::Infinite) {
        ///         jobQueue.CancelAllJobs ();
        ///         if (waitForIdle) {
        ///             jobQueue.WaitForIdle (timeSpec);
        ///         }
        ///     }
        ///
        /// private:
        ///     // TimerEvents
        ///     virtual void OnTimerAlarm (util::Timer::SharedPtr /*timer*/) throw () {
        ///         // queue idle jobs.
        ///         jobQueue.EnqJob (...);
        ///     }
        /// };
        /// \endcode
        ///
        /// In your code you can now write:
        ///
        /// \code{.cpp}
        /// IdleProcessor::Instance ()->StartTimer (util::TimeSpec::FromSeconds (30));
        /// \endcode
        ///
        /// This will arm the IdleProcessor timer to fire after 30 seconds.
        /// Call IdleProcessor::Instance ()->StopTimer () to disarm the timer.
        ///
        /// NOTE: IdleProcessor demonstrates the canonical way of using Timer.

        struct _LIB_THEKOGANS_UTIL_DECL Timer : public Producer<TimerEvents> {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Timer)

            /// \brief
            /// Timer has a private heap to help with memory
            /// management, performance, and global heap fragmentation.
            THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

        private:
            /// \brief
            /// Timer name.
            const std::string name;
            /// \brief
            /// Alias for RefCountedRegistry<Timer>.
            using Registry = RefCountedRegistry<Timer>;
            /// \brief
            /// This token is the key between the c++ and the c async io worlds (os).
            /// This token is registered with os specific apis (io completion port on
            /// windows, epoll on linux and kqueue on os x). On callback the token
            /// is used to get a Timer::SharedPtr from the Timer::WeakPtr found in
            /// the \see{util::RefCountedRegistry<Timer>}.
            const Registry::Token token;
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows native timer object.
            PTP_TIMER timer;
            /// \brief
            /// Windows timer callback.
            static VOID CALLBACK TimerCallback (
                PTP_CALLBACK_INSTANCE /*Instance*/,
                PVOID Context,
                PTP_TIMER Timer_);
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
            os::osx::KQueueTimer *timer;
            /// \brief
            /// OS X timer callback.
            static void TimerCallback (void *userData);
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// ctor.
            /// \param[in] name_ Timer name. Use this parameter to help
            /// identify the timer that fired. This way a single callback
            /// can process multiple timers and be able to distinguish
            /// between them.
            Timer (const std::string &name_ = std::string ());
            /// \brief
            /// dtor.
            ~Timer ();

        public:
            /// \brief
            /// Timer factory method. Timers must be created on the
            /// heap and this method insures that.
            /// \param[in] name Timer name. Use this parameter to help
            /// identify the timer that fired. This way a single callback
            /// can process multiple timers and be able to distinguish
            /// between them.
            /// \return A newly created timer.
            static SharedPtr Create (const std::string &name = std::string ()) {
                return SharedPtr (new Timer (name));
            }

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
            /// Return true if timer is armed and running.
            /// \return true == timer is armed and running.
            bool IsRunning ();

            /// \brief
            /// Timer is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Timer)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Timer_h)
