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
#if defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
    #include <signal.h>
#if defined (TOOLCHAIN_OS_OSX)
    #include <mach/thread_policy.h>
    #include <mach/thread_act.h>
#endif // defined (TOOLCHAIN_OS_OSX)
#endif // defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
#include <cassert>
#include "thekogans/util/Thread2.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/CPU.h"
#if defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/LockGuard.h"
#endif // defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)

namespace thekogans {
    namespace util {

        void Thread2::Impl::Create () {
            Detach ();
            {
                LockGuard<Mutex> guard (mutex);
                AddRef ();
                joinable = true;
            #if defined (TOOLCHAIN_OS_Windows)
                thread = CreateThread2 (0, 0, ThreadProc, this, 0, 0);
                if (thread == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
            #else // defined (TOOLCHAIN_OS_Windows)
                THEKOGANS_UTIL_ERROR_CODE errorCode =
                    pthread_create (&thread, 0, ThreadProc, this);
                if (errorCode != 0) {
            #endif // defined (TOOLCHAIN_OS_Windows)
                    Release ();
                    joinable = false;
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            }
        }

        void Thread2::Impl::SetName (const std::string &name) {
            LockGuard<Mutex> guard (mutex);
            if (joinable) {
            #if defined (TOOLCHAIN_OS_Windows)
                // This code came from: https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
                const DWORD MS_VC_EXCEPTION = 0x406D1388;
            #pragma pack (push, 8)
                struct THREADNAME_INFO {
                    DWORD dwType; // Must be 0x1000.
                    LPCSTR szName; // Pointer to name (in user addr space).
                    DWORD dwThreadID; // Thread ID (-1 = caller thread).
                    DWORD dwFlags; // Reserved for future use, must be zero.

                    THREADNAME_INFO (
                        const std::string &name,
                        THEKOGANS_UTIL_THREAD_HANDLE thread) :
                        dwType (0x1000),
                        szName (name.c_str ()),
                        dwThreadID (::GetThreadId (impl->thread)),
                        dwFlags (0) {}
                } info (name, thread);
            #pragma pack (pop)
            #pragma warning (push)
            #pragma warning (disable: 6320 6322)
                __try {
                    RaiseException (
                        MS_VC_EXCEPTION,
                        0,
                        sizeof (info) / sizeof (ULONG_PTR),
                        (ULONG_PTR *)&info);
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                }
            #pragma warning (pop)
            #elif defined (TOOLCHAIN_OS_OSX)
                pthread_setname_np (name.c_str ());
            #else // defined (TOOLCHAIN_OS_OSX)
                const std::size_t LINUX_MAX_THREAD_NAME = 15;
                char name_[LINUX_MAX_THREAD_NAME + 1];
                std::size_t length =
                    std::min (name.size (), LINUX_MAX_THREAD_NAME);
                strncpy (name_, name.c_str (), length);
                name_[length] = '\0';
                pthread_setname_np (thread, name_);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        }

        void Thread2::Impl::SetPriority (i32 priority) {
            LockGuard<Mutex> guard (mutex);
            if (joinable) {
            #if defined (TOOLCHAIN_OS_Windows)
                if (!::SetThreadPriority (thread, priority)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                if (priority >= THEKOGANS_UTIL_IDLE_THREAD_PRIORITY &&
                        priority <= THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY) {
                    int policy;
                    sched_param param;
                    THEKOGANS_UTIL_ERROR_CODE errorCode =
                        pthread_getschedparam (thread, &policy, &param);
                    if (errorCode != 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                    // Our priorities come from a virtual range:
                    // (THEKOGANS_UTIL_IDLE_THREAD_PRIORITY <= priority <= THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY).
                    // We need to map them to the range supported
                    // by the particular scheduler policy.
                    int minPriority = sched_get_priority_min (policy);
                    int maxPriority = sched_get_priority_max (policy);
                    f32 scale = ((f32)maxPriority - (f32)minPriority) /
                        ((f32)THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY - (f32)THEKOGANS_UTIL_IDLE_THREAD_PRIORITY);
                    param.sched_priority = minPriority + scale * priority;
                    errorCode = pthread_setschedparam (thread, policy, &param);
                    if (errorCode != 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Priority %d is out of range (%d, %d).",
                        priority,
                        THEKOGANS_UTIL_IDLE_THREAD_PRIORITY,
                        THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        }

        void Thread2::Impl::SetAffinity (ui32 affinity) {
            LockGuard<Mutex> guard (mutex);
            if (joinable) {
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD affinityMask = 1 << affinity;
                if (SetThreadAffinityMask (thread, affinityMask) == 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            #elif defined (TOOLCHAIN_OS_Linux)
                cpu_set_t affinityMask;
                CPU_ZERO (&affinityMask);
                CPU_SET (affinity, &affinityMask);
                THEKOGANS_UTIL_ERROR_CODE errorCode =
                    pthread_setaffinity_np (thread, sizeof (affinityMask), &affinityMask);
                if (errorCode != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            #elif defined (TOOLCHAIN_OS_OSX)
                thread_affinity_policy_data_t policy;
                policy.affinity_tag = affinity;
                kern_return_t result = thread_policy_set (
                    pthread_mach_thread_np (thread),
                    THREAD_AFFINITY_POLICY,
                    reinterpret_cast<thread_policy_t> (&policy),
                    THREAD_AFFINITY_POLICY_COUNT);
                if (result != KERN_SUCCESS && result != KERN_NOT_SUPPORTED) {
                    THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION (result);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        }


        void Thread2::Impl::Detach () {
            LockGuard<Mutex> guard (mutex);
            if (joinable) {
            #if defined (TOOLCHAIN_OS_Windows)
                CloseHandle (thread);
                thread = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
            #else // defined (TOOLCHAIN_OS_Windows)
                pthread_detach (thread);
                exitCondition.Signal ();
            #endif // defined (TOOLCHAIN_OS_Windows)
                joinable = false;
            }
        }

        bool Thread2::Impl::Join (const TimeSpec &timeSpec) {
            LockGuard<Mutex> guard (mutex);
            if (joinable) {
            #if defined (TOOLCHAIN_OS_Windows)
                mutex.Release ();
                DWORD result = WaitForSingleObject (thread, (DWORD)timeSpec.ToMilliseconds ());
                mutex.Acquire ();
                if (result == WAIT_TIMEOUT || result == WAIT_ABANDONED) {
                    return false;
                }
            #else // defined (TOOLCHAIN_OS_Windows)
                if (timeSpec == TimeSpec::Infinite) {
                    while (joinable) {
                        exitCondition.Wait (mutex);
                    }
                }
                else {
                    const TimeSpec deadline = GetMonotonicTime () + timeSpec;
                    while (joinable) {
                        TimeSpec now = GetMonotonicTime ();
                        if (now >= deadline) {
                            return false;
                        }
                        exitCondition.Wait (mutex, deadline - now);
                    }
                }
                if (!joinable) {
                    pthread_join (thread, 0);
                }
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            return !joinable;
        }

    #if defined (TOOLCHAIN_OS_Windows)
        DWORD WINAPI Thread2::Impl::ThreadProc (LPVOID data) {
    #else // defined (TOOLCHAIN_OS_Windows)
        void *Thread2::Impl::ThreadProc (void *data) {
    #endif // defined (TOOLCHAIN_OS_Windows)
            assert (data != nullptr);
            {
                Thread2::Impl::SharedPtr impl (static_cast<Thread2::Impl *> (data), false);
                impl->callable ();
                Thread2::AtExit (impl->thread);
                {
                    LockGuard<Mutex> guard (impl->mutex);
                    impl->joinable = false;
                #if !defined (TOOLCHAIN_OS_Windows)
                    impl->exitCondition.Signal ();
                #endif // !defined (TOOLCHAIN_OS_Windows)
                }
            }
            return 0;
        }

        Thread2::ExitFuncList Thread2::exitFuncList;

        Thread2 &Thread2::operator = (Thread2 &&other) noexcept {
            if (this != &other) {
                impl = std::move (other.impl);
            }
            return *this;
        }


        void Thread2::SetName (const std::string &name) {
            THEKOGANS_UTIL_ASSERT (impl != nullptr, "Calling Thread2::SetName on a moved object.");
            if (impl != nullptr) {
                impl->SetName (name);
            }
        }

        void Thread2::SetPriority (i32 priority) {
            THEKOGANS_UTIL_ASSERT (impl != nullptr, "Calling Thread2::SetPriority on a moved object.");
            if (impl != nullptr) {
                impl->SetPriority (priority);
            }
        }

        void Thread2::SetAffinity (ui32 affinity) {
            THEKOGANS_UTIL_ASSERT (impl != nullptr, "Calling Thread2::SetAffinity on a moved object.");
            if (impl != nullptr) {
                impl->SetAffinity (affinity);
            }
        }

        void Thread2::Detach () {
            THEKOGANS_UTIL_ASSERT (impl != nullptr, "Calling Thread2::Detach on a moved object.");
            if (impl != nullptr) {
                impl->Detach ();
            }
        }

        bool Thread2::Join (const TimeSpec &timeSpec) {
            THEKOGANS_UTIL_ASSERT (impl != nullptr, "Calling Thread2::Join on a moved object.");
            return impl != nullptr && impl->Join (timeSpec);
        }

        void Thread2::AddExitFunc (ExitFunc exitFunc) {
            if (exitFunc != 0) {
                exitFuncList.push_back (exitFunc);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        i32 Thread2::GetSchedulerPolicy () {
        #if defined (TOOLCHAIN_OS_Windows)
            return 0;
        #elif defined (TOOLCHAIN_OS_Linux)
            return sched_getscheduler (getpid ());
        #elif defined (TOOLCHAIN_OS_OSX)
            return 0;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Thread2::PriorityRange Thread2::GetPriorityRange (i32 policy) {
        #if defined (TOOLCHAIN_OS_Windows)
            return PriorityRange (
                THEKOGANS_UTIL_IDLE_THREAD_PRIORITY,
                THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY);
        #elif defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
            return PriorityRange (
                sched_get_priority_min (policy),
                sched_get_priority_max (policy));
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        const char * const Thread2::IDLE_THREAD_PRIORITY = "idle";
        const char * const Thread2::LOWEST_THREAD_PRIORITY = "lowest";
        const char * const Thread2::LOW_THREAD_PRIORITY = "low";
        const char * const Thread2::NORMAL_THREAD_PRIORITY = "normal";
        const char * const Thread2::HIGH_THREAD_PRIORITY = "high";
        const char * const Thread2::HIGHEST_THREAD_PRIORITY = "highest";
        const char * const Thread2::REAL_TIME_THREAD_PRIORITY = "real_time";

        std::string Thread2::priorityTostring (i32 priority) {
            return
                priority == THEKOGANS_UTIL_IDLE_THREAD_PRIORITY ? IDLE_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY ? LOWEST_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_LOW_THREAD_PRIORITY ? LOW_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY ? NORMAL_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_HIGH_THREAD_PRIORITY ? HIGH_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY ? HIGHEST_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY ? REAL_TIME_THREAD_PRIORITY :
                i32Tostring (priority);
        }

        i32 Thread2::stringToPriority (const std::string &priority) {
            return
                priority == IDLE_THREAD_PRIORITY ? THEKOGANS_UTIL_IDLE_THREAD_PRIORITY :
                priority == LOWEST_THREAD_PRIORITY ? THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY :
                priority == LOW_THREAD_PRIORITY ? THEKOGANS_UTIL_LOW_THREAD_PRIORITY :
                priority == NORMAL_THREAD_PRIORITY ? THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY :
                priority == HIGH_THREAD_PRIORITY ? THEKOGANS_UTIL_HIGH_THREAD_PRIORITY :
                priority == HIGHEST_THREAD_PRIORITY ? THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY :
                priority == REAL_TIME_THREAD_PRIORITY ? THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY :
                stringToi32 (priority.c_str ());
        }

        void Thread2::Pause () {
            CPU::Pause ();
        }

        void Thread2::YieldSlice () {
            CPU::YieldSlice ();
        }

        void Thread2::AtExit (THEKOGANS_UTIL_THREAD_HANDLE thread) {
            for (auto &exitFunc : exitFuncList) {
                exitFunc (thread);
            }
        }

        ThreadReaper2::ThreadReaper2 () :
                thread (
                    [this] () {
                        while (true) {
                            bool empty;
                            {
                                LockGuard<Mutex> guard (mutex);
                                // 1. Only sleep if there is absolutely zero work to track
                                while (threads.empty ()) {
                                    notEmpty.Wait (mutex);
                                }
                                // 2. Clear out everything that has finished executing naturally
                                for (std::size_t i = threads.size (); i-- != 0;) {
                                    if (threads[i].Join (TimeSpec::Zero)) {
                                        threads.erase (threads.begin () + i);
                                    }
                                }
                                empty = threads.empty ();
                            }
                            // 3. If the list is NOT empty, it means only stuck/running threads remain.
                            // Temporarily yield the CPU so we don't spin at 100%.
                            if (!empty) {
                                Sleep (TimeSpec::FromMilliseconds (50));
                            }
                        }
                    }) {
            thread.SetPriority (THEKOGANS_UTIL_LOW_THREAD_PRIORITY);
        }

        void ThreadReaper2::ReapThreads (std::vector<Thread2> &&threads_) {
            LockGuard<Mutex> guard (mutex);
            threads.reserve (threads.size () + threads_.size ());
            threads.insert (
                threads.end (),
                std::make_move_iterator (threads_.begin ()),
                std::make_move_iterator (threads_.end ()));
            notEmpty.Signal ();
        }

        void ThreadReaper2::ReapThread (Thread2 &&thread) {
            LockGuard<Mutex> guard (mutex);
            threads.push_back (std::move (thread));
            notEmpty.Signal ();
        }

    } // namespace util
} // namespace thekogans
