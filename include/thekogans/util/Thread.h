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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
#elif defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
    #include <unistd.h>
    #include <pthread.h>
    #include <sched.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/ThreadRunLoop.h"

#if defined (TOOLCHAIN_OS_Windows)
    /// \brief
    /// Window specific thread handle type.
    typedef THEKOGANS_UTIL_HANDLE THEKOGANS_UTIL_THREAD_HANDLE;
    /// \brief
    /// Window specific thread id type.
    typedef DWORD THEKOGANS_UTIL_THREAD_ID;
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
#else // defined (TOOLCHAIN_OS_Windows)
    /// \brief
    /// POSIX specific thread id type.
    typedef pthread_t THEKOGANS_UTIL_THREAD_HANDLE;
    /// \brief
    /// Window specific thread id type.
    typedef uint64_t THEKOGANS_UTIL_THREAD_ID;
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
/// \def THEKOGANS_UTIL_INVALID_THREAD_HANDLE_VALUE
/// \see{THEKOGANS_UTIL_THREAD_HANDLE} initialization sentinal.
#define THEKOGANS_UTIL_INVALID_THREAD_HANDLE_VALUE 0

/// \def THEKOGANS_UTIL_MAX_THREAD_AFFINITY
/// Thread can run on any core.
#define THEKOGANS_UTIL_MAX_THREAD_AFFINITY thekogans::util::UI32_MAX

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
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows thread id.
            THEKOGANS_UTIL_THREAD_ID id;
        #else // defined (TOOLCHAIN_OS_Windows)
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
            /// \brief
            /// Main thread handle. Call \see{Thread::SetMainThread} from
            /// any thread you want to set as main (usually main ()).
            static THEKOGANS_UTIL_THREAD_HANDLE mainThread;

        public:
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// ctor.
            /// \param[in] name_ Thread name.
            /// \param[in] joinable_ A dummy param meant to make Windows and POSIX implementations identical.
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
                ui32 affinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY);

            /// \brief
            /// Wait for thread to finish.
            /// \param[in] timeSpec How long to wait for.
            /// \return true = succeeded, false = timed out
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

            /// \struct Thread::Backoff Thread.h thekogans/util/Thread.h
            ///
            /// \brief
            /// Implements an exponential thread contention management scheme.
            /// Used by Spin[RW]Lock. Use it in your threading and resource
            /// sharing algorithms to implement simple thread management to
            /// avoid the 'Thundering Herd' problem.
            struct _LIB_THEKOGANS_UTIL_DECL Backoff {
                enum : ui32 {
                    /// \brief
                    /// Default max pause iterations before giving up the time slice.
                    DEFAULT_MAX_PAUSE_BEFORE_YIELD = 16
                };
                /// \brief
                /// Max pause iterations before giving up the time slice.
                ui32 maxPauseBeforeYield;
                /// \briwf
                /// Current pause count.
                ui32 count;

                /// \brief
                /// ctor.
                /// \param[in] maxPauseBeforeYield_ Max pause iterations before giving up the time slice.
                Backoff (ui32 maxPauseBeforeYield_ = DEFAULT_MAX_PAUSE_BEFORE_YIELD) :
                    maxPauseBeforeYield (maxPauseBeforeYield_),
                    count (1) {}

                /// \brief
                /// Pause the cpu or yield the time slice if we've been spinning too much.
                void Pause () {
                    if (count <= maxPauseBeforeYield) {
                        for (ui32 i = 0; i < count; ++i) {
                            Thread::Pause ();
                        }
                        // Pause twice as long the next time.
                        count *= 2;
                    }
                    else {
                        // Pause is so long that we might as well
                        // yield CPU to scheduler.
                        Thread::YieldSlice ();
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

            /// \brief
            /// Return current thread id.
            /// \return Current thread id.
            static THEKOGANS_UTIL_THREAD_ID GetCurrThreadId () {
            #if defined (TOOLCHAIN_OS_Windows)
                return GetCurrentThreadId ();
            #else // defined (TOOLCHAIN_OS_Windows)
                THEKOGANS_UTIL_THREAD_ID id;
                pthread_threadid_np (0, &id);
                return id;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Return this thread id.
            /// \return this thread id.
            inline THEKOGANS_UTIL_THREAD_ID GetThreadId () const {
                return id;
            }
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// Call this function from main () or any other thread you
            /// want to set as main.
            /// NOTE: mainThread is initialized by GetCurrThreadHandle ()
            /// and if static initialization is done correctly by the runtime
            /// should, by default, be initialized with main threads handle.
            /// \param[in] mainThread_ Thread handle to set as main.
            static void SetMainThread (
                    THEKOGANS_UTIL_THREAD_HANDLE mainThread_ = GetCurrThreadHandle ()) {
                mainThread = mainThread_;
            }
            /// \brief
            /// Return true if the given thread is the main thread.
            /// \param[in] thread Thread handle o test if main.
            /// \return true == thread is the main thread.
            static bool IsMainThread (
                    THEKOGANS_UTIL_THREAD_HANDLE thread = GetCurrThreadHandle ()) {
            #if defined (TOOLCHAIN_OS_Windows)
                return thread == mainThread;
            #else // defined (TOOLCHAIN_OS_Windows)
                return pthread_equal (thread, mainThread) != 0;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

        protected:
            /// \brief
            /// Derivative classes must override this method
            /// to provide their own thread implementation.
            /// IMPORTANT: Note the throw (). It's there to remind
            /// you that if your Run implementation leaks exceptions,
            /// your application will crash.
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
        /// \param[in] format Conversion format.
        /// \return String representation of the thread handle.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API FormatThreadHandle (
            THEKOGANS_UTIL_THREAD_HANDLE thread,
        #if defined (TOOLCHAIN_OS_Windows)
            const char *format = "%05u");
        #else // defined (TOOLCHAIN_OS_Windows)
            const char *format = "%p");
        #endif // defined (TOOLCHAIN_OS_Windows)

        /// \brief
        /// Convert thread id to string representation.
        /// \param[in] id Id to convert.
        /// \param[in] format Conversion format.
        /// \return String representation of the thread id.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API FormatThreadId (
            THEKOGANS_UTIL_THREAD_ID id,
        #if defined (TOOLCHAIN_OS_Windows)
            const char *format = "%05u");
        #else // defined (TOOLCHAIN_OS_Windows)
            const char *format = THEKOGANS_UTIL_UI64_FORMAT);
        #endif // defined (TOOLCHAIN_OS_Windows)

        /// \struct ThreadReaper Thread.h thekogans/util/Thread.h
        ///
        /// \brief
        /// ThreadReaper is a \see{Singleton} whose job is to wait for given
        /// threads to exit, join with them to release the system resources and
        /// delete the \see{Thread} wrapper. It should be used by \see{Thread}
        /// derivatives at the end of their \see{Thread::Run} method like this:
        ///
        /// \code{.cpp}
        /// void SomeThread::Run () {
        ///     ...
        ///     ThreadReaper::Instance ().ReapThread (this);
        /// }
        /// \endcode
        ///
        /// This mechanism allows the thread to control it's own lifetime and
        /// cleanup after itself avoiding leaks.

        struct _LIB_THEKOGANS_UTIL_DECL ThreadReaper :
                public Thread,
                public Singleton<ThreadReaper, SpinLock> {
            /// \brief
            /// Convenient typedef for std::function<void (Thread * /*thread*/)>.
            typedef std::function<void (Thread * /*thread*/)> Deleter;

        private:
            /// \brief
            /// \see{ThreadRunLoop} that will be executed in this thread.
            ThreadRunLoop runLoop;

        public:
            /// \brief
            /// ctor.
            ThreadReaper ();

            /// \brief
            /// Given a \see{Thread} to reap, create a lambda job that will wait for it to exit,
            /// join with it, and then delete the \see{Thread} wrapper.
            /// \param[in] thread \see{Thread} to reap.
            /// \param[in] timeSpec How long to wait for \see{Thread} to exit.
            /// \param[in] deleter Deleter used to deallocate the thread pointer.
            void ReapThread (
                Thread *thread,
                const TimeSpec &timeSpec = TimeSpec::Infinite,
                const Deleter &deleter = [] (Thread *thread) {delete thread;});

        private:
            // Thread
            /// \brief
            /// Worker thread.
            virtual void Run () throw () override;
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Thread_h)
