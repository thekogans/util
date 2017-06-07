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

#if !defined (__thekogans_util_Thread_h)
#define __thekogans_util_Thread_h

#include <memory>
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
    /// \def THEKOGANS_UTIL_IDLE_THREAD_PRIORITY
    /// Idle thread priority.
    #define THEKOGANS_UTIL_IDLE_THREAD_PRIORITY THREAD_PRIORITY_IDLE
    /// \def THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY
    /// Lowest thread priority.
    #define THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY THREAD_PRIORITY_LOWEST
    /// \def THEKOGANS_UTIL_LOW_THREAD_PRIORITY
    /// Low thread priority.
    #define THEKOGANS_UTIL_LOW_THREAD_PRIORITY THREAD_PRIORITY_BELOW_NORMAL
    /// \def THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY
    /// Normal thread priority.
    #define THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY THREAD_PRIORITY_NORMAL
    /// \def THEKOGANS_UTIL_HIGH_THREAD_PRIORITY
    /// High thread priority.
    #define THEKOGANS_UTIL_HIGH_THREAD_PRIORITY THREAD_PRIORITY_ABOVE_NORMAL
    /// \def THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY
    /// Highest thread priority.
    #define THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY THREAD_PRIORITY_HIGHEST
    /// \def THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY
    /// Real time thread priority.
    #define THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY THREAD_PRIORITY_TIME_CRITICAL
#elif defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
    #include <unistd.h>
    #include <pthread.h>
    #include <sched.h>
    /// \brief
    /// This is a virtual priority range. When you call
    /// Thread::SetThreadPriority, it gets adjusted to a
    /// relative value between policy::min and policy::max.
    /// \def THEKOGANS_UTIL_IDLE_THREAD_PRIORITY
    /// Idle thread priority.
    #define THEKOGANS_UTIL_IDLE_THREAD_PRIORITY 0
    /// \def THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY
    /// Lowest thread priority.
    #define THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY 5
    /// \def THEKOGANS_UTIL_LOW_THREAD_PRIORITY
    /// Low thread priority.
    #define THEKOGANS_UTIL_LOW_THREAD_PRIORITY 10
    /// \def THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY
    /// Normal thread priority.
    #define THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY 15
    /// \def THEKOGANS_UTIL_HIGH_THREAD_PRIORITY
    /// High thread priority.
    #define THEKOGANS_UTIL_HIGH_THREAD_PRIORITY 20
    /// \def THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY
    /// Highest thread priority.
    #define THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY 25
    /// \def THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY
    /// Real time thread priority.
    #define THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY 30
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/SpinLock.h"

/// \def THEKOGANS_UTIL_HANDLE
/// OS specific thread handle type.
#if defined (TOOLCHAIN_OS_Windows)
    typedef THEKOGANS_UTIL_HANDLE THEKOGANS_UTIL_THREAD_HANDLE;
#else // defined (TOOLCHAIN_OS_Windows)
    typedef pthread_t THEKOGANS_UTIL_THREAD_HANDLE;
#endif // defined (TOOLCHAIN_OS_Windows)

namespace thekogans {
    namespace util {

        /// \struct Thread Thread.h thekogans/util/Thread.h
        ///
        /// \brief
        /// Thread is a cross-platform wrapper around POSIX and Windows
        /// threads. It is an abstract base class meant to be derived
        /// from. All you need to do is reimplement the Run method.
        /// Platform specific features like priority and affinity are
        /// handled in a sensical and uniform way.
        ///
        /// NOTE: On POSIX systems, threads are created with signals disabled.

        struct _LIB_THEKOGANS_UTIL_DECL Thread {
            /// \brief
            /// Convenient typedef for std::unique_ptr<Thread>.
            typedef std::unique_ptr<Thread> UniquePtr;

        protected:
            /// \brief
            /// Thread name.
            const std::string name;
            /// \brief
            /// OS specific thread handle.
            THEKOGANS_UTIL_THREAD_HANDLE thread;
        #if !defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// true = thread is joinable (waitable).
            const bool joinable;
            /// \brief
            /// true = Wait was called.
            volatile bool joined;
            /// \brief
            /// This global lock is used to synchronize
            /// access to thread creation. On POSIX
            /// thread creation needs to be done with
            /// signals disabled. Since threads can
            /// be created from other threads we need
            /// to serialize access to pthread_sigmask.
            static SpinLock spinLock;
        #endif // !defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// true = thread function has exited.
            volatile bool exited;
            /// \brief
            /// Convenient typedef for void (*) (THEKOGANS_UTIL_THREAD_HANDLE thread).
            typedef void (*ExitFunc) (THEKOGANS_UTIL_THREAD_HANDLE thread);
            /// \brief
            /// Convenient typedef for std::vector<ExitFunc>.
            typedef std::vector<ExitFunc> ExitFuncList;
            /// \brief
            /// List of at exit functions.
            static ExitFuncList exitFuncList;

        public:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor.
            /// \param[in] name_ Thread name.
            Thread (
                const std::string &name_ = std::string (),
                bool /*joinable_*/ = true) :
                name (name_),
                thread (THEKOGANS_UTIL_INVALID_HANDLE_VALUE),
                exited (true) {}
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor.
            /// \param[in] name_ Thread name.
            /// \param[in] joinable_ true = thread is joinable, false = thread is detached.
            Thread (
                const std::string &name_ = std::string (),
                bool joinable_ = true) :
                name (name_),
                joinable (joinable_),
                joined (true),
                exited (true) {}
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Virtual dtor.
            virtual ~Thread ();

            /// \brief
            /// Register an at exit function to be called when each thread terminates.
            /// \param[in] exitFunc At exit function to register.
            static void AddExitFunc (ExitFunc exitFunc);

            /// \brief
            /// Get scheduler policy.
            /// \return scheduling policy in effect for this thread.
            static i32 GetPolicy ();
            /// \brief
            /// Priority range.
            /// first = lowest
            /// second = highest
            typedef std::pair<i32, i32> PriorityRange;
            /// \brief
            /// Get policy priority range.
            /// \param[in] policy Policy for which to return priority range.
            /// \return Priority range for a given policy.
            static PriorityRange GetPriorityRange (i32 policy = GetPolicy ());

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

            /// \brief
            /// Return thread name.
            /// \return Thread name.
            inline const std::string &GetName () const {
                return name;
            }

            /// \brief
            /// Create the thread.
            /// \param[in] priority Thread priority.
            /// \param[in] affinity Thread processor affinity.
            void Create (
                i32 priority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 affinity = UI32_MAX);

            /// \brief
            /// Wait for thread to finish.
            /// \param[in] timeSpec How long to wait for.
            /// IMPORTANT: timeSpec is a relative value.
            /// On POSIX (pthreads) systems Wait will add
            /// the current time to the value provided
            /// before calling pthread_timedwait_np.
            /// VERY IMPORTANT: POSIX dictates that if you call
            /// pthread_join/pthread_timedjoin_np more than once,
            /// the second calls behavior is undefined. This is
            /// why Thread uses bool joinable. Wait sets it to
            /// false after a successful join and returns false
            /// thereafter.
            bool Wait (const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Busy spin a thread for specified count.
            /// \param[in] count Number of iterations to busy spin for.
            static void Pause (ui32 count);
            /// \brief
            /// Yield the current thread's time slice.
            static void YieldSlice ();

            /// \brief
            /// Set thread priority.
            /// \param[in] priority Thread priority.
            static void SetThreadPriority (
                THEKOGANS_UTIL_THREAD_HANDLE thread,
                i32 priority);

            /// \brief
            /// Set thread affinity. This will bind the
            /// thread to a particular processor.
            /// \param[in] affinity Thread processor affinity.
            static void SetThreadAffinity (
                THEKOGANS_UTIL_THREAD_HANDLE thread,
                ui32 affinity);

            /// \brief
            /// Return current thread handle.
            /// \return Current thread handle.
            static THEKOGANS_UTIL_THREAD_HANDLE GetCurrThreadHandle () {
            #if defined (TOOLCHAIN_OS_Windows)
                return GetCurrentThread ();
            #else // defined (TOOLCHAIN_OS_Windows)
                return pthread_self ();
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            /// \brief
            /// Return this thread handle.
            /// \return this thread handle.
            inline THEKOGANS_UTIL_THREAD_HANDLE GetThreadHandle () const {
                return thread;
            }

        protected:
            /// \brief
            /// Derivative classes must override this method
            /// to provide their own thread implementation.
            virtual void Run () throw () = 0;

        private:
            /// \brief
            /// Thread function.
        #if defined (TOOLCHAIN_OS_Windows)
            static DWORD WINAPI ThreadProc (LPVOID data);
        #else // defined (TOOLCHAIN_OS_Windows)
            static void *ThreadProc (void *data);
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// Call registered at exit functions.
            /// \param[in] thread Terminating thread.
            static void AtExit (THEKOGANS_UTIL_THREAD_HANDLE thread);

            /// \brief
            /// Thread is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Thread)
        };

    #if defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// Increament thread priority.
        /// \param[in] priority Priority to increment.
        /// \return Incremented priority.
        inline i32 IncPriority (i32 priority) {
            return priority == THEKOGANS_UTIL_IDLE_THREAD_PRIORITY ? THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY ? THEKOGANS_UTIL_LOW_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_LOW_THREAD_PRIORITY ? THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY ? THEKOGANS_UTIL_HIGH_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_HIGH_THREAD_PRIORITY ? THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY ? THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY :
                THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY;
        }
        /// \brief
        /// Add delta to the given thread priority.
        /// \param[in] priority Priority to increment.
        /// \param[in] delta Amount to add to the priority.
        /// \return Incremented priority.
        inline i32 AddPriority (
                i32 priority,
                i32 delta) {
            while (delta-- > 0) {
                priority = IncPriority (priority);
            }
            return priority;
        }
        /// \brief
        /// Decreament thread priority.
        /// \param[in] priority Priority to decrement.
        /// \return Decremented priority.
        inline i32 DecPriority (i32 priority) {
            return priority == THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY ? THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY ? THEKOGANS_UTIL_HIGH_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_HIGH_THREAD_PRIORITY ? THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY ? THEKOGANS_UTIL_LOW_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_LOW_THREAD_PRIORITY ? THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY ? THEKOGANS_UTIL_IDLE_THREAD_PRIORITY :
                THEKOGANS_UTIL_IDLE_THREAD_PRIORITY;
        }
        /// \brief
        /// Subtract delta from the given thread priority.
        /// \param[in] priority Priority to decrement.
        /// \param[in] delta Amount to subtract from the priority.
        /// \return Decremented priority.
        inline i32 SubPriority (
                i32 priority,
                i32 delta) {
            while (delta-- > 0) {
                priority = DecPriority (priority);
            }
            return priority;
        }
    #else // defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// Increament thread priority.
        /// \param[in] priority Priority to increment.
        /// \return Incremented priority.
        inline i32 IncPriority (i32 priority) {
            return CLAMP (++priority,
                THEKOGANS_UTIL_IDLE_THREAD_PRIORITY,
                THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY);
        }
        /// \brief
        /// Add delta to the given thread priority.
        /// \param[in] priority Priority to increment.
        /// \param[in] delta Amount to add to the priority.
        /// \return Incremented priority.
        inline i32 AddPriority (
                i32 priority,
                i32 delta) {
            return CLAMP (priority + delta,
                THEKOGANS_UTIL_IDLE_THREAD_PRIORITY,
                THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY);
        }
        /// \brief
        /// Decreament thread priority.
        /// \param[in] priority Priority to decrement.
        /// \return Decremented priority.
        inline i32 DecPriority (i32 priority) {
            return CLAMP (--priority,
                THEKOGANS_UTIL_IDLE_THREAD_PRIORITY,
                THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY);
        }
        /// \brief
        /// Subtract delta to the given thread priority.
        /// \param[in] priority Priority to decrement.
        /// \param[in] delta Amount to subtract from the priority.
        /// \return Decremented priority.
        inline i32 SubPriority (
                i32 priority,
                i32 delta) {
            return CLAMP (priority - delta,
                THEKOGANS_UTIL_IDLE_THREAD_PRIORITY,
                THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY);
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        /// \brief
        /// Convert thread handle to string representation.
        /// \param[in] thread Handle to convert.
        /// \return String representation of the thread handle.
        inline std::string FormatThreadHandle (THEKOGANS_UTIL_THREAD_HANDLE thread) {
            return HexFormatBuffer (&thread, sizeof (thread));
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Thread_h)
