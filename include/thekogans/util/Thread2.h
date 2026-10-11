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

#if !defined (__thekogans_util_Thread2_h)
#define __thekogans_util_Thread2_h

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#elif defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
    #include <unistd.h>
    #include <pthread.h>
    #include <sched.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include <functional>
#include <utility>
#include <tuple>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/RefCounted.h"
#if defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/NormalMutex.h"
    #include "thekogans/util/Condition2.h"
#endif // defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
#include "thekogans/util/Thread.h"

namespace thekogans {
    namespace util {

        /// \struct Thread2 Thread2.h thekogans/util/Thread2.h
        ///
        /// \brief
        /// Thread2 is a cross-platform wrapper around POSIX and Windows threads.
        /// Platform specific features like priority and affinity are handled in
        /// a sensical and uniform way.
        ///
        /// NOTE: On POSIX systems, threads are created with signals disabled.
        struct _LIB_THEKOGANS_UTIL_DECL Thread2 {
            /// \brief
            /// Alias for void (*) (THEKOGANS_UTIL_THREAD_HANDLE thread).
            using ExitFunc = std::function<void (THEKOGANS_UTIL_THREAD_HANDLE thread)>;
            /// \brief
            /// Alias for std::vector<ExitFunc>.
            using ExitFuncList = std::vector<ExitFunc>;
            /// \brief
            /// List of at exit functions.
            static ExitFuncList exitFuncList;

        protected:
            struct Impl : public RefCounted {
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Impl)

                /// \brief
                /// Callable constructed from the parameters passed to the
                /// perfect forwarding ctor.
                std::function<void ()> callable;
                /// \brief
                /// OS specific thread handle.
                THEKOGANS_UTIL_THREAD_HANDLE thread;
                /// \brief
                /// true = thread is joinable (waitable).
                bool joinable;
            #if !defined (TOOLCHAIN_OS_Windows)
                /// \brief
                NormalMutex mutex;
                Condition2 exitCondition;
            #endif // !defined (TOOLCHAIN_OS_Windows)

                template<
                    typename F,
                    typename... Args>
                explicit Impl (
                    F &&f_,
                    Args &&... args_) :
                    callable ([args = std::make_tuple (std::forward<F> (f_), std::forward<Args> (args_)...)] () mutable {
                        std::apply (
                            [] (auto &&func, auto &&... params) {
                                // 3. Invoke directly. This cleanly bypasses the strict noexcept
                                // template resolution constraint of std::invoke.
                                std::forward<decltype (func)> (func)(std::forward<decltype (params)> (params)...);
                            },
                            std::move (args));
                    }),
                #if defined (TOOLCHAIN_OS_Windows)
                    thread (THEKOGANS_UTIL_INVALID_HANDLE_VALUE),
                #endif // defined (TOOLCHAIN_OS_Windows)
                    joinable (false) {}
                ~Impl () {
                    Detach ();
                }

                void Create ();

                /// \brief
                /// Set thread name.
                /// \param[in] name Thread2 name.
                void SetName (const std::string &name);

                /// \brief
                /// Set thread priority.
                /// \param[in] priority Thread2 priority.
                void SetPriority (i32 priority);

                /// \brief
                /// Set thread affinity. This will bind the
                /// thread to a particular processor.
                /// \param[in] affinity Thread2 processor affinity.
                void SetAffinity (ui32 affinity);

                /// \brief
                /// Detach from the running thread.
                void Detach ();

                /// \brief
                /// Wait for thread to finish.
                /// \param[in] timeSpec How long to wait for.
                /// \return true = succeeded, false = timed out
                /// IMPORTANT: timeSpec is a relative value.
                bool Join (const TimeSpec &timeSpec = TimeSpec::Infinite);

            private:
                /// \brief
                /// Thread2 function.
            #if defined (TOOLCHAIN_OS_Windows)
                static DWORD WINAPI ThreadProc (LPVOID data);
            #else // defined (TOOLCHAIN_OS_Windows)
                static void *ThreadProc (void *data);
            #endif // defined (TOOLCHAIN_OS_Windows)
            };
            Impl::SharedPtr impl;

        public:
            /// \brief
            /// Move ctor.
            /// \param[in] other Thread2 to move.
            Thread2 (Thread2 &&other) noexcept :
                impl (std::move (other.impl)) {}
            /// \brief
            /// Perfect forwarding ctor.
            /// \param[in] f_ Function object.
            /// \param[in] args_ Variable list of arguments passed to f_.
            template<
                typename F,
                typename... Args>
            explicit Thread2 (
                    F &&f,
                    Args &&... args) :
                    impl (new Impl (std::forward<F> (f), std::forward<Args> (args)...)) {
                impl->Create ();
            }

            /// \brief
            /// Move assignment operator.
            /// \param[in] other Thread2 to move assign.
            Thread2 &operator = (Thread2 &&other) noexcept;

            /// \brief
            /// Set thread name.
            /// \param[in] name Thread2 name.
            void SetName (const std::string &name);

            /// \brief
            /// Set thread priority.
            /// \param[in] priority Thread2 priority.
            void SetPriority (i32 priority);

            /// \brief
            /// Set thread affinity. This will bind the
            /// thread to a particular processor.
            /// \param[in] affinity Thread2 processor affinity.
            void SetAffinity (ui32 affinity);

            /// \brief
            /// Detach from the running thread.
            void Detach ();

            /// \brief
            /// Wait for thread to finish.
            /// \param[in] timeSpec How long to wait for.
            /// \return true = succeeded, false = timed out
            /// IMPORTANT: timeSpec is a relative value.
            bool Join (const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Register an at exit function to be called when each thread terminates.
            /// \param[in] exitFunc At exit function to register.
            static void AddExitFunc (ExitFunc exitFunc);

            /// \brief
            /// Get scheduler policy.
            /// \return scheduling policy in effect for this thread.
            static i32 GetSchedulerPolicy ();
            /// \brief
            /// Priority range.
            /// first = lowest
            /// second = highest
            using PriorityRange = std::pair<i32, i32>;
            /// \brief
            /// Get policy priority range.
            /// \param[in] policy Policy for which to return priority range.
            /// \return Priority range for a given policy.
            static PriorityRange GetPriorityRange (i32 policy = GetSchedulerPolicy ());

            /// \brief
            /// "idle"
            static const char * const IDLE_THREAD_PRIORITY;
            /// \brief
            /// "lowest"
            static const char * const LOWEST_THREAD_PRIORITY;
            /// \brief
            /// "low"
            static const char * const LOW_THREAD_PRIORITY;
            /// \brief
            /// "normal"
            static const char * const NORMAL_THREAD_PRIORITY;
            /// \brief
            /// "high"
            static const char * const HIGH_THREAD_PRIORITY;
            /// \brief
            /// "highest"
            static const char * const HIGHEST_THREAD_PRIORITY;
            /// \brief
            /// "real_time"
            static const char * const REAL_TIME_THREAD_PRIORITY;
            /// \brief
            /// Convert integral priority to it's string equivalent.
            /// \param[in] priority Integral priority value.
            /// \return Priority string representation.
            static std::string priorityTostring (i32 priority);
            /// \brief
            /// Convert string priority to it's integral equivalent.
            /// \param[in] priority String priority representation.
            /// \return Priority integral value.
            static i32 stringToPriority (const std::string &priority);

            /// \struct Thread2::Backoff Thread2.h thekogans/util/Thread2.h
            ///
            /// \brief
            /// Implements an exponential thread contention management scheme.
            /// Used by Spin[RW]Lock. Use it in your threading and resource
            /// sharing algorithms to implement simple thread management to
            /// avoid the 'Thundering Herd' problem.
            struct _LIB_THEKOGANS_UTIL_DECL Backoff {
                /// \brief
                /// Default pause iterations before giving up the time slice.
                static const std::size_t DEFAULT_PAUSE_BEFORE_YIELD = 16;
                /// \brief
                /// Max pause before yield.
                static const std::size_t MAX_PAUSE_BEFORE_YIELD = SIZE_T_MAX / 2;

                /// \brief
                /// Max pause iterations before giving up the time slice.
                std::size_t maxPauseBeforeYield;
                /// \briwf
                /// Current pause count.
                std::size_t count;

                /// \brief
                /// ctor.
                /// \param[in] maxPauseBeforeYield_ Max pause iterations before giving up the time slice.
                Backoff (std::size_t maxPauseBeforeYield_ = DEFAULT_PAUSE_BEFORE_YIELD) :
                    maxPauseBeforeYield (MIN (maxPauseBeforeYield_, MAX_PAUSE_BEFORE_YIELD)),
                    count (1) {}

                /// \brief
                /// Pause the cpu or yield the time slice if we've been spinning too much.
                void Pause () {
                    if (count < maxPauseBeforeYield) {
                        Thread2::Pause ();
                        count *= 2;
                    }
                    else {
                        // Pause is so long that we might as well
                        // yield CPU to scheduler.
                        Thread2::YieldSlice ();
                    }
                }
                /// \brief
                /// Reset the current pause count to 1.
                inline void Reset () {
                    count = 1;
                }
            };

            /// \brief
            /// Put the thread to sleep without giving up the time slice.
            static void Pause ();
            /// \brief
            /// Yield the current thread's time slice.
            static void YieldSlice ();

            /// \brief
            /// Call registered at exit functions.
            /// \param[in] thread Terminating thread.
            static void AtExit (THEKOGANS_UTIL_THREAD_HANDLE thread);

            /// \brief
            /// Thread2 is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Thread2)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Thread2_h)
