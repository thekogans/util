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

#if !defined (__thekogans_util_ThreadRunLoop_h)
#define __thekogans_util_ThreadRunLoop_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RunLoop.h"

namespace thekogans {
    namespace util {

        /// \struct ThreadRunLoop ThreadRunLoop.h thekogans/util/ThreadRunLoop.h
        ///
        /// \brief
        /// ThreadRunLoop implements a very simple thread run loop.
        ///
        /// To turn any thread in to a run loop, use the following template:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// struct MyRunLoopThread :
        ///        public util::ThreadRunLoop,
        ///        public util::Thread {
        ///
        /// public:
        ///     MyThread (
        ///             const std::string &name = std::string (),
        ///             util::RunLoop::JobExecutionPolicy::SharedPtr jobExecutionPolicy =
        ///                 new util::RunLoop::FIFOJobExecutionPolicy,
        ///             util::i32 priority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
        ///             util::ui32 affinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY) :
        ///             ThreadRunLoop (name, jobExecutionPolicy),
        ///             Thread (name) {
        ///         Create (priority, affinity);
        ///     }
        ///
        /// private:
        ///     // util::Thread
        ///     virtual void Run () noexcept override {
        ///         THEKOGANS_UTIL_TRY {
        ///             Start ();
        ///         }
        ///         THEKOGANS_UTIL_CATCH_AND_LOG
        ///     }
        /// }
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL ThreadRunLoop : public RunLoop {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (ThreadRunLoop)

        public:
            /// \brief
            /// ctor.
            /// \param[in] name RunLoop name.
            /// \param[in] jobExecutionPolicy \see{RunLoop::JobExecutionPolicy}.
            ThreadRunLoop (
                const std::string &name = std::string (),
                JobExecutionPolicy::SharedPtr jobExecutionPolicy = new FIFOJobExecutionPolicy) :
                RunLoop (name, jobExecutionPolicy) {}

            /// \brief
            /// Start the run loop. This is a blocking call and will
            /// only complete when Stop is called.
            virtual void Start () override;
            /// \brief
            /// Stop the run loop. Calling this function will cause the Start call
            /// to return.
            /// \param[in] cancelRunningJobs true = Cancel all running jobs.
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            virtual void Stop (
                bool cancelRunningJobs = true,
                bool cancelPendingJobs = true) override;
            /// \brief
            /// Return true is the run loop is running (Start was called).
            /// \return true is the run loop is running (Start was called).
            virtual bool IsRunning () override;

        protected:
            /// \brief
            /// ctor.
            /// \param[in] state Shared RunLoop state.
            /// NOTE: This ctor is meant to be used by ThreadRunLoop derivatives
            /// that extend the RunLoop::State.
            explicit ThreadRunLoop (State::SharedPtr state) :
                RunLoop (state) {}

            /// \brief
            /// ThreadRunLoop is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (ThreadRunLoop)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_ThreadRunLoop_h)
