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

#if !defined (__thekogans_util_PipelinePool_h)
#define __thekogans_util_PipelinePool_h

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/Pipeline.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/SystemInfo.h"
#include "thekogans/util/Singleton.h"

namespace thekogans {
    namespace util {

        /// \struct PipelinePool PipelinePool.h thekogans/util/PipelinePool.h
        ///
        /// \brief
        /// PipelinePool implements a very convenient pool of
        /// \see{Pipeline}s. Here is a canonical use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// util::PipelinePool pipelinePool;
        ///
        /// void foo (...) {
        ///     struct Job : public util::Pipeline::Job {
        ///         util::Pipeline::Ptr pipeline;
        ///         ...
        ///         Job (
        ///             util::Pipeline::Ptr pipeline_,
        ///             ...) :
        ///             pipeline (pipeline_),
        ///             ... {}
        ///
        ///         // util::Pipeline::Job
        ///         virtual void Execute (const std::atomic<bool> &) throw () {
        ///             ...
        ///         }
        ///     };
        ///     util::Pipeline::Ptr pipeline = pipelinePool.GetPipeline ();
        ///     if (pipeline.Get () != 0) {
        ///         pipeline->EnqJob (Pipeline::Job::Ptr (new Job (pipeline, ...)));
        ///     }
        /// }
        /// \endcode
        ///
        /// Note how the Job controls the lifetime of the \see{Pipeline}.
        /// By passing util::Pipeline::Ptr in to the Job's ctor we guarantee
        /// that the \see{Pipeline} will be returned back to the pool as soon
        /// as the Job goes out of scope (as Job will be the last reference).

        struct _LIB_THEKOGANS_UTIL_DECL PipelinePool {
        private:
            /// \brief
            /// Minimum number of pipelines to keep in the pool.
            const std::size_t minPipelines;
            /// \brief
            /// Maximum number of pipelines allowed in the pool.
            const std::size_t maxPipelines;
            /// \brief
            /// \see{Pipeline::Stage} array.
            const std::vector<util::Pipeline::Stage> stages;
            /// \brief
            /// \see{Pipeline} name.
            const std::string name;
            /// \brief
            /// \see{Pipeline} \see{Pipeline::JobExecutionPolicy}.
            Pipeline::JobExecutionPolicy::Ptr jobExecutionPolicy;
            /// \brief
            /// Number of worker threads servicing the \see{Pipeline}.
            const std::size_t workerCount;
            /// \brief
            /// \see{Pipeline} worker thread priority.
            const i32 workerPriority;
            /// \brief
            /// \see{Pipeline} worker thread processor affinity.
            const ui32 workerAffinity;
            /// \brief
            /// Called to initialize/uninitialize the \see{Pipeline} worker thread.
            RunLoop::WorkerCallback *workerCallback;
            /// \brief
            /// Forward declaration of Pipeline.
            struct Pipeline;
            enum {
                /// \brief
                /// PipelineList list id.
                PIPELINE_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<Pipeline, PIPELINE_LIST_ID>.
            typedef IntrusiveList<Pipeline, PIPELINE_LIST_ID> PipelineList;
        #if defined (_MSC_VER)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (_MSC_VER)
            /// \struct PipelinePool::Pipeline PipelinePool.h thekogans/util/PipelinePool.h
            ///
            /// \brief
            /// Extends \see{Pipeline} to enable returning self to the pool after use.
            struct Pipeline :
                    public PipelineList::Node,
                    public util::Pipeline {
                /// \brief
                /// Pipeline has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Pipeline, SpinLock)

            private:
                /// \brief
                /// PipelinePool from which this \see{Pipeline} came.
                PipelinePool &pipelinePool;

            public:
                /// \brief
                /// ctor.
                /// \param[in] begin Pointer to the beginning of the \see{Pipeline::Stage} array.
                /// \param[in] end Pointer to the end of the \see{Pipeline::Stage} array.
                /// \param[in] name \see{Pipeline} name.
                /// \param[in] jobExecutionPolicy \see{Pipeline} \see{Pipeline::JobExecutionPolicy}.
                /// \param[in] workerCount Number of worker threads servicing the \see{Pipeline}.
                /// \param[in] workerPriority \see{Pipeline} worker thread priority.
                /// \param[in] workerAffinity \see{Pipeline} worker thread processor affinity.
                /// \param[in] workerCallback Called to initialize/uninitialize the
                /// \see{Pipeline} worker thread(s).
                /// \param[in] pipelinePool_ PipelinePool to which this pipeline belongs.
                Pipeline (
                    const util::Pipeline::Stage *begin,
                    const util::Pipeline::Stage *end,
                    const std::string &name,
                    util::Pipeline::JobExecutionPolicy::Ptr jobExecutionPolicy,
                    std::size_t workerCount,
                    i32 workerPriority,
                    ui32 workerAffinity,
                    RunLoop::WorkerCallback *workerCallback,
                    PipelinePool &pipelinePool_) :
                    util::Pipeline (
                        begin,
                        end,
                        name,
                        jobExecutionPolicy,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        workerCallback),
                    pipelinePool (pipelinePool_) {}

            protected:
                // RefCounted
                /// \brief
                /// If there are no more references to this pipeline,
                /// release it back to the pool.
                virtual void Harakiri () {
                    pipelinePool.ReleasePipeline (this);
                }

                /// \brief
                /// Pipeline is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Pipeline)
            };
        #if defined (_MSC_VER)
            #pragma warning (pop)
        #endif // defined (_MSC_VER)
            /// \brief
            /// List of available \see{Pipeline}s.
            PipelineList availablePipelines;
            /// \brief
            /// List of borrowed \see{Pipeline}s.
            PipelineList borrowedPipelines;
            /// \brief
            /// \see{Pipeline} id pool. If !name.empty (),
            /// each \see{Pipeline} created by this pool
            /// will have the following name:
            /// FormatString ("%s-%u", name.c_str (), ++idPool);
            std::atomic<std::size_t> idPool;
            /// \brief
            /// Synchronization mutex.
            Mutex mutex;
            /// \brief
            /// Synchronization condition variable.
            Condition idle;

        public:
            /// \brief
            /// ctor.
            /// \param[in] minPipelines_ Minimum \see{Pipeline}s to keep in the pool.
            /// \param[in] maxPipelines_ Maximum \see{Pipeline}s to allow the pool to grow to.
            /// \param[in] begin_ Pointer to the beginning of the \see{Pipeline::Stage} array.
            /// \param[in] end_ Pointer to the end of the \see{Pipeline::Stage} array.
            /// \param[in] name_ \see{Pipeline} name.
            /// \param[in] jobExecutionPolicy_ \see{Pipeline} \see{Pipeline::JobExecutionPolicy}.
            /// \param[in] workerCount_ Number of worker threads servicing the \see{Pipeline}.
            /// \param[in] workerPriority_ \see{Pipeline} worker thread priority.
            /// \param[in] workerAffinity_ \see{Pipeline} worker thread processor affinity.
            /// \param[in] workerCallback_ Called to initialize/uninitialize the \see{Pipeline}
            /// worker thread.
            PipelinePool (
                std::size_t minPipelines_,
                std::size_t maxPipelines_,
                const util::Pipeline::Stage *begin_,
                const util::Pipeline::Stage *end_,
                const std::string &name_ = std::string (),
                Pipeline::JobExecutionPolicy::Ptr jobExecutionPolicy_ =
                    Pipeline::JobExecutionPolicy::Ptr (new Pipeline::FIFOJobExecutionPolicy),
                std::size_t workerCount_ = 1,
                i32 workerPriority_ = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                ui32 workerAffinity_ = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                RunLoop::WorkerCallback *workerCallback_ = 0);
            /// \brief
            /// dtor.
            ~PipelinePool ();

            /// \brief
            /// Acquire a \see{Pipeline} from the pool.
            /// \param[in] retries Number of times to retry if a \see{Pipeline} is not
            /// immediately available.
            /// \param[in] timeSpec How long to wait between retries.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return A \see{Pipeline} from the pool (Pipeline::Ptr () if pool is exhausted).
            util::Pipeline::Ptr GetPipeline (
                std::size_t retries = 1,
                const TimeSpec &timeSpec = TimeSpec::FromMilliseconds (100));

            /// \brief
            /// Return all borrowedPipelines jobs matching the given equality test.
            /// \param[in] equalityTest EqualityTest to query to determine the matching jobs.
            /// \param[out] jobs \see{RunLoop::UserJobList} (\see{IntrusiveList}) containing the matching jobs.
            /// NOTE: This method will take a reference on all matching jobs.
            void GetJobs (
                const RunLoop::EqualityTest &equalityTest,
                RunLoop::UserJobList &jobs);
            /// \brief
            /// Wait for all borrowedPipelines jobs matching the given equality test to complete.
            /// \param[in] equalityTest EqualityTest to query to determine which jobs to wait on.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == All jobs satisfying the equalityTest completed,
            /// false == One or more matching jobs timed out.
            bool WaitForJobs (
                const RunLoop::EqualityTest &equalityTest,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Cancel all borrowedPipelines jobs matching the given equality test.
            /// \param[in] equalityTest EqualityTest to query to determine the matching jobs.
            void CancelJobs (RunLoop::EqualityTest &equalityTest);

            /// \brief
            /// Blocks until all borrowed \see{Pipeline}s have been returned to the pool.
            /// \param[in] timeSpec How long to wait for \see{Pipeline}s to return.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == PipelinePool is idle, false == Timed out.
            bool WaitForIdle (
                const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Return true if this pool has no outstanding \see{Pipeline}s.
            /// \return true == this pool has no outstanding \see{Pipeline}s.
            bool IsIdle ();

        private:
            /// \brief
            /// Used by \see{GetPipeline} to acquire a \see{Pipeline} from the pool.
            /// \return \see{Pipeline} pointer.
            Pipeline *AcquirePipeline ();
            /// \brief
            /// Used by \see{Pipeline} to release itself to the pool.
            /// \param[in] pipeline \see{Pipeline} to release.
            void ReleasePipeline (Pipeline *pipeline);

            /// \brief
            /// PipelinePool is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (PipelinePool)
        };

        /// \struct GlobalPipelinePoolCreateInstance PipelinePool.h thekogans/util/PipelinePool.h
        ///
        /// \brief
        /// Call GlobalPipelinePool::CreateInstance before the first use of
        /// GlobalPipelinePool::Instance to supply custom arguments to GlobalPipelinePool ctor.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalPipelinePoolCreateInstance {
            /// \brief
            /// Create a global \see{PipelinePool} with custom ctor arguments.
            /// \param[in] minPipelines Minimum \see{Pipeline}s to keep in the pool.
            /// \param[in] maxPipelines Maximum \see{Pipeline}s to allow the pool to grow to.
            /// \param[in] begin Pointer to the beginning of the \see{Pipeline::Stage} array.
            /// \param[in] end Pointer to the end of the \see{Pipeline::Stage} array.
            /// \param[in] name \see{Pipeline} name.
            /// \param[in] jobExecutionPolicy \see{Pipeline} \see{Pipeline::JobExecutionPolicy}.
            /// \param[in] workerCount Number of worker threads servicing the \see{Pipeline}.
            /// \param[in] workerPriority \see{Pipeline} worker thread priority.
            /// \param[in] workerAffinity \see{Pipeline} worker thread processor affinity.
            /// \param[in] workerCallback Called to initialize/uninitialize the \see{Pipeline}
            /// thread.
            /// \return A global \see{PipelinePool} with custom ctor arguments.
            PipelinePool *operator () (
                    std::size_t minPipelines,
                    std::size_t maxPipelines,
                    const Pipeline::Stage *begin,
                    const Pipeline::Stage *end,
                    const std::string &name = "GlobalPipelinePoolCreateInstance",
                    Pipeline::JobExecutionPolicy::Ptr jobExecutionPolicy =
                        Pipeline::JobExecutionPolicy::Ptr (new Pipeline::FIFOJobExecutionPolicy),
                    std::size_t workerCount = 1,
                    i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                    ui32 workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                    RunLoop::WorkerCallback *workerCallback = 0) {
                if (minPipelines != 0 && maxPipelines >= minPipelines && begin != 0 && end != 0) {
                    return new PipelinePool (
                        minPipelines,
                        maxPipelines,
                        begin,
                        end,
                        name,
                        jobExecutionPolicy,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        workerCallback);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("%s",
                        "Must provide GlobalPipelinePool minPipelines, maxPipelines, begin and end. "
                        "Call GlobalPipelinePool::CreateInstance.");
                }
            }
        };

        /// \struct GlobalPipelinePool PipelinePool.h thekogans/util/PipelinePool.h
        ///
        /// \brief
        /// A global \see{PipelinePool} instance. The \see{PipelinePool} is
        /// designed to be as flexible as possible. To be useful in different
        /// situations the \see{PipelinePool}'s min/max worker count needs to
        /// be parametrized as we might need different pools running different
        /// counts at different worker priorities. That said, the most basic
        /// (and the most useful) use case will have a single pool using the
        /// defaults. This struct exists to aid in that. If all you need is a
        /// global \see{PipelinePool} then GlobalPipelinePool::Instance () will
        /// do the trick.
        /// IMPORTANT: Unlike some other global objects, you cannot use this one
        /// without first calling GlobalPipelinePool::CreateInstance first. This
        /// is because, at the very least, you need to provide the stages that
        /// will be implemented by the pipelines in this pool.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalPipelinePool :
            public Singleton<PipelinePool, SpinLock, GlobalPipelinePoolCreateInstance> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_PipelinePool_h)
