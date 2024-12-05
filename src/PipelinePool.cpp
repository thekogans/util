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

#include "thekogans/util/Heap.h"
#include "thekogans/util/Timer.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/PipelinePool.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (PipelinePool::Pipeline)

        PipelinePool::PipelinePool (
                std::size_t minPipelines_,
                std::size_t maxPipelines_,
                const util::Pipeline::Stage *begin_,
                const util::Pipeline::Stage *end_,
                const std::string &name_,
                Pipeline::JobExecutionPolicy::SharedPtr jobExecutionPolicy_,
                std::size_t workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_) :
                minPipelines (minPipelines_),
                maxPipelines (maxPipelines_),
                stages (begin_, end_),
                name (name_),
                jobExecutionPolicy (jobExecutionPolicy_),
                workerCount (workerCount_),
                workerPriority (workerPriority_),
                workerAffinity (workerAffinity_),
                workerCallback (workerCallback_),
                idPool (0),
                idle (mutex) {
            // By requiring at least one Pipeline in reserve coupled
            // with the logic in ReleasePipeline below, we guarantee
            // that we avoid the deadlock associated with trying to
            // delete the Pipeline being released.
            if (minPipelines != 0 && maxPipelines >= minPipelines && !stages.empty ()) {
                for (std::size_t i = 0; i < minPipelines; ++i) {
                    std::string pipelineName;
                    if (!name.empty ()) {
                        pipelineName = FormatString (
                            "%s-%u",
                            name.c_str (),
                            ++idPool);
                    }
                    availablePipelines.push_back (
                        new Pipeline (
                            stages.data (),
                            stages.data () + stages.size (),
                            pipelineName,
                            jobExecutionPolicy,
                            workerCount,
                            workerPriority,
                            workerAffinity,
                            workerCallback,
                            *this));
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        PipelinePool::~PipelinePool () {
            // Wait for all borrowed queues to be returned.
            WaitForIdle ();
            assert (borrowedPipelines.empty ());
            availablePipelines.clear (
                [] (PipelineList::Callback::argument_type pipeline) -> PipelineList::Callback::result_type {
                    delete pipeline;
                    return true;
                }
            );
        }

        Pipeline::SharedPtr PipelinePool::GetPipeline (
                std::size_t retries,
                const TimeSpec &timeSpec) {
            Pipeline *pipeline = AcquirePipeline ();
            while (pipeline == nullptr && retries-- > 0) {
                Sleep (timeSpec);
                pipeline = AcquirePipeline ();
            }
            return util::Pipeline::SharedPtr (pipeline);
        }

        void PipelinePool::GetJobs (
                const RunLoop::EqualityTest &equalityTest,
                RunLoop::UserJobList &jobs) {
            LockGuard<Mutex> guard (mutex);
            borrowedPipelines.for_each (
                [&equalityTest, &jobs] (PipelineList::Callback::argument_type jobQueue) ->
                        PipelineList::Callback::result_type {
                    jobQueue->GetJobs (equalityTest, jobs);
                    return true;
                }
            );
        }

        bool PipelinePool::WaitForJobs (
                const RunLoop::EqualityTest &equalityTest,
                const TimeSpec &timeSpec) {
            RunLoop::UserJobList jobs;
            GetJobs (equalityTest, jobs);
            return RunLoop::WaitForJobs (jobs, timeSpec);
        }

        void PipelinePool::CancelJobs (const RunLoop::EqualityTest &equalityTest) {
            LockGuard<Mutex> guard (mutex);
            borrowedPipelines.for_each (
                [&equalityTest] (PipelineList::Callback::argument_type jobQueue) -> PipelineList::Callback::result_type {
                    jobQueue->CancelJobs (equalityTest);
                    return true;
                }
            );
        }

        bool PipelinePool::WaitForIdle (const TimeSpec &timeSpec) {
            LockGuard<Mutex> guard (mutex);
            if (timeSpec == TimeSpec::Infinite) {
                while (!borrowedPipelines.empty ()) {
                    idle.Wait ();
                }
            }
            else {
                TimeSpec now = GetCurrentTime ();
                TimeSpec deadline = now + timeSpec;
                while (!borrowedPipelines.empty () && deadline > now) {
                    idle.Wait (deadline - now);
                    now = GetCurrentTime ();
                }
            }
            return borrowedPipelines.empty ();
        }

        bool PipelinePool::IsIdle () {
            LockGuard<Mutex> guard (mutex);
            return borrowedPipelines.empty ();
        }

        PipelinePool::Pipeline *PipelinePool::AcquirePipeline () {
            Pipeline *pipeline = nullptr;
            {
                LockGuard<Mutex> guard (mutex);
                if (!availablePipelines.empty ()) {
                    // Borrow a pipeline from the front of the pool.
                    // This combined with ReleasePipeline putting
                    // returned pipeline at the front should
                    // guarantee the best cache utilization.
                    pipeline = availablePipelines.pop_front ();
                }
                else if (availablePipelines.size () + borrowedPipelines.size () < maxPipelines) {
                    std::string pipelineName;
                    if (!name.empty ()) {
                        pipelineName = FormatString (
                            "%s-%u",
                            name.c_str (),
                            ++idPool);
                    }
                    pipeline = new Pipeline (
                        stages.data (),
                        stages.data () + stages.size (),
                        pipelineName,
                        jobExecutionPolicy,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        workerCallback,
                        *this);
                }
                if (pipeline != nullptr) {
                    borrowedPipelines.push_back (pipeline);
                }
            }
            return pipeline;
        }

        void PipelinePool::ReleasePipeline (Pipeline *pipeline) {
            if (pipeline != nullptr) {
                LockGuard<Mutex> guard (mutex);
                borrowedPipelines.erase (pipeline);
                // Put the recently used pipeline at the front of
                // the list. With any luck the next time a pipeline
                // is borrowed from this pool, it will be the last
                // one used, and it's cache will be nice and warm.
                availablePipelines.push_front (pipeline);
                // If the pool is idle, see if we need to remove excess pipelines.
                if (borrowedPipelines.empty ()) {
                    while (availablePipelines.size () > minPipelines) {
                        // Delete the least recently used pipelines. This logic
                        // guarantees that we avoid the deadlock associated with
                        // deleating the passed in pipeline.
                        delete availablePipelines.pop_back ();
                    }
                    idle.SignalAll ();
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

    } // namespace util
} // namespace thekogans
