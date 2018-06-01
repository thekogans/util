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

#include "thekogans/util/Config.h"
#include "thekogans/util/Timer.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/PipelinePool.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (PipelinePool::Pipeline, SpinLock)

        PipelinePool::PipelinePool (
                ui32 minPipelines_,
                ui32 maxPipelines_,
                const util::Pipeline::Stage *begin_,
                const util::Pipeline::Stage *end_,
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_) :
                minPipelines (minPipelines_),
                maxPipelines (maxPipelines_),
                begin (begin_),
                end (end_),
                name (name_),
                type (type_),
                maxPendingJobs (maxPendingJobs_),
                workerCount (workerCount_),
                workerPriority (workerPriority_),
                workerAffinity (workerAffinity_),
                workerCallback (workerCallback_),
                idPool (0),
                idle (mutex) {
            // By keeping at least one Pipeline in reserve coupled
            // with the logic in ReleasePipeline below, we guarantee
            // that we avoid the deadlock associated with trying to
            // delete the Pipeline being released.
            if (0 < minPipelines && minPipelines <= maxPipelines) {
                for (ui32 i = 0; i < minPipelines; ++i) {
                    std::string pipelineName;
                    if (!name.empty ()) {
                        pipelineName = FormatString (
                            "%s-%u",
                            name.c_str (),
                            ++idPool);
                    }
                    availablePipelines.push_back (
                        new Pipeline (
                            begin,
                            end,
                            pipelineName,
                            type,
                            maxPendingJobs,
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
            LockGuard<Mutex> guard (mutex);
            struct DeleteCallback : public PipelineList::Callback {
                typedef PipelineList::Callback::result_type result_type;
                typedef PipelineList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type pipeline) {
                    delete pipeline;
                    return true;
                }
            } deleteCallback;
            availablePipelines.clear (deleteCallback);
            borrowedPipelines.clear (deleteCallback);
        }

        Pipeline::Ptr PipelinePool::GetPipeline (
                ui32 retries,
                const TimeSpec &timeSpec) {
            Pipeline *pipeline = AcquirePipeline ();
            while (pipeline == 0 && retries-- > 0) {
                Sleep (timeSpec);
                pipeline = AcquirePipeline ();
            }
            return util::Pipeline::Ptr (pipeline);
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
            Pipeline *pipeline = 0;
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
                        begin,
                        end,
                        pipelineName,
                        type,
                        maxPendingJobs,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        workerCallback,
                        *this);
                }
                if (pipeline != 0) {
                    borrowedPipelines.push_back (pipeline);
                }
            }
            return pipeline;
        }

        void PipelinePool::ReleasePipeline (Pipeline *pipeline) {
            if (pipeline != 0) {
                LockGuard<Mutex> guard (mutex);
                borrowedPipelines.erase (pipeline);
                // Put the recently used pipeline at the front of
                // the list. With any luck the next time a pipeline
                // is borrowed from this pool, it will be the last
                // one used, and it's cache will be nice and warm.
                availablePipelines.push_front (pipeline);
                // If the pool is idle, see if we need to remove excess pipelines.
                if (borrowedPipelines.empty ()) {
                    if (availablePipelines.size () > minPipelines) {
                        struct DeleteCallback : public PipelineList::Callback {
                            typedef PipelineList::Callback::result_type result_type;
                            typedef PipelineList::Callback::argument_type argument_type;
                            std::size_t deleteCount;
                            explicit DeleteCallback (std::size_t deleteCount_) :
                                deleteCount (deleteCount_) {}
                            virtual result_type operator () (argument_type pipeline) {
                                delete pipeline;
                                return --deleteCount > 0;
                            }
                        } deleteCallback (availablePipelines.size () - minPipelines);
                        // Walk the pool in reverse to delete the least recently used pipelines.
                        // This logic guarantees that we avoid the deadlock associated with
                        // deleating the passed in pipeline.
                        availablePipelines.for_each (deleteCallback, true);
                    }
                    idle.SignalAll ();
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        ui32 GlobalPipelinePoolCreateInstance::minPipelines = 0;
        ui32 GlobalPipelinePoolCreateInstance::maxPipelines = 0;
        const Pipeline::Stage *GlobalPipelinePoolCreateInstance::begin = 0;
        const Pipeline::Stage *GlobalPipelinePoolCreateInstance::end = 0;
        std::string GlobalPipelinePoolCreateInstance::name = std::string ();
        RunLoop::Type GlobalPipelinePoolCreateInstance::type = RunLoop::TYPE_FIFO;
        ui32 GlobalPipelinePoolCreateInstance::maxPendingJobs = UI32_MAX;
        ui32 GlobalPipelinePoolCreateInstance::workerCount = 1;
        i32 GlobalPipelinePoolCreateInstance::workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY;
        ui32 GlobalPipelinePoolCreateInstance::workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY;
        RunLoop::WorkerCallback *GlobalPipelinePoolCreateInstance::workerCallback = 0;

        void GlobalPipelinePoolCreateInstance::Parameterize (
                ui32 minPipelines_,
                ui32 maxPipelines_,
                const Pipeline::Stage *begin_,
                const Pipeline::Stage *end_,
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_) {
            if (minPipelines_ != 0 && maxPipelines_ != 0 &&
                    begin_ != 0 && end_ != 0) {
                minPipelines = minPipelines_;
                maxPipelines = maxPipelines_;
                begin = begin_;
                end = end_;
                name = name_;
                type = type_;
                maxPendingJobs = maxPendingJobs_;
                workerCount = workerCount_;
                workerPriority = workerPriority_;
                workerAffinity = workerAffinity_;
                workerCallback = workerCallback_;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        PipelinePool *GlobalPipelinePoolCreateInstance::operator () () {
            if (minPipelines != 0 && maxPipelines != 0 &&
                    begin != 0 && end != 0) {
                return new PipelinePool (
                    minPipelines,
                    maxPipelines,
                    begin,
                    end,
                    name,
                    type,
                    maxPendingJobs,
                    workerCount,
                    workerPriority,
                    workerAffinity,
                    workerCallback);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION ("%s",
                    "Must provide GlobalPipelinePool minPipelines, maxPipelines, begin and end. "
                    "Call GlobalPipelinePoolCreateInstance::Parameterize.");
            }
        }

    } // namespace util
} // namespace thekogans
