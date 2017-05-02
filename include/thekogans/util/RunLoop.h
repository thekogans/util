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

#if !defined (__thekogans_util_RunLoop_h)
#define __thekogans_util_RunLoop_h

#include "thekogans/util/Config.h"
#include "thekogans/util/JobQueue.h"

namespace thekogans {
    namespace util {

        /// \struct RunLoop RunLoop.h thekogans/util/RunLoop.h
        ///
        /// \brief
        /// RunLoop is an abstract base for \see{DefaultRunLoop} and \see{SystemRunLoop}.
        /// RunLoop allows you to schedule \see{JobQueue::Job} to be executed on the thread
        /// that's running the run loop.

        struct _LIB_THEKOGANS_UTIL_DECL RunLoop {
            /// \brief
            /// Convenient typedef for std::unique_ptr<RunLoop>.
            typedef std::unique_ptr<RunLoop> Ptr;

            /// \brief
            /// dtor.
            virtual ~RunLoop () {}

            /// \brief
            /// Start waiting for jobs.
            virtual void Start () = 0;
            /// \brief
            /// Stop the run loop and in all likelihood exit the thread hosting it.
            /// Obviously, this function needs to be called from a different thread
            /// than the one that called Start.
            virtual void Stop () = 0;

            /// \brief
            /// Enqueue a job to be performed on the run loop thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// NOTE: Same constraint applies to Enq as stop. Namely, you can't call Enq
            /// from the same thread that called Start.
            virtual void Enq (
                JobQueue::Job::UniquePtr job,
                bool wait = false) = 0;
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RunLoop_h)
