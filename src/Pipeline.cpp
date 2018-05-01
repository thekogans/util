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

#include <cassert>
#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Pipeline.h"

namespace thekogans {
    namespace util {

        void Pipeline::Job::Prologue (volatile const bool &done) throw () {
            if (stage == 0) {
                Begin ();
            }
        }

        void Pipeline::Job::Epilogue (volatile const bool &done) throw () {
            if (!done) {
                Job::Ptr next = Clone ();
                if (next.Get () != 0) {
                    std::size_t nextStage = next->stage;
                    assert (nextStage < pipeline.stages.size ());
                    pipeline.stages[nextStage]->EnqJob (
                        dynamic_refcounted_pointer_cast<RunLoop::Job> (next));
                }
                else {
                    End ();
                }
            }
            else {
                End ();
            }
        }

        Pipeline::Pipeline (
                const StageInfo *begin,
                const StageInfo *end) {
            for (; begin != end; ++begin) {
                JobQueue::UniquePtr stage (
                    new JobQueue (
                        begin->name,
                        begin->type,
                        begin->workerCount,
                        begin->workerPriority,
                        begin->workerAffinity,
                        begin->maxPendingJobs,
                        begin->workerCallback));
                assert (stage.get () != 0);
                if (stage.get () != 0) {
                    stages.push_back (stage.get ());
                    stage.release ();
                }
            }
        }

        Pipeline::Pipeline (
                ui32 stageCount,
                StagePriorityAdjustment stagePriorityAdjustment,
                i32 stagePriorityAdjustmentDelta,
                const std::string &name,
                RunLoop::Type type,
                ui32 workerCount,
                i32 workerPriority,
                ui32 workerAffinity,
                ui32 maxPendingJobs,
                RunLoop::WorkerCallback *workerCallback) {
            assert (stageCount > 0);
            for (ui32 i = 0; i < stageCount; ++i) {
                std::string stageName;
                if (!name.empty ()) {
                    stageName = FormatString ("%s-%u", name.c_str (), i);
                }
                JobQueue::UniquePtr stage (
                    new JobQueue (
                        stageName,
                        type,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        maxPendingJobs,
                        workerCallback));
            #if !defined (TOOLCHAIN_OS_Windows)
                switch (stagePriorityAdjustment) {
                    case NoAdjustment:
                        break;
                    case Inc:
                        workerPriority = IncPriority (workerPriority);
                        break;
                    case Add:
                        workerPriority =
                            AddPriority (workerPriority, stagePriorityAdjustmentDelta);
                        break;
                    case Dec:
                        workerPriority = DecPriority (workerPriority);
                        break;
                    case Sub:
                        workerPriority =
                            SubPriority (workerPriority, stagePriorityAdjustmentDelta);
                        break;
                }
            #endif // !defined (TOOLCHAIN_OS_Windows)
                assert (stage.get () != 0);
                if (stage.get () != 0) {
                    stages.push_back (stage.get ());
                    stage.release ();
                }
            }
        }

        void Pipeline::Start () {
            for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                stages[i]->Start ();
            }
        }

        void Pipeline::Stop () {
            for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                stages[i]->Stop ();
            }
        }

        void Pipeline::EnqJob (
                Job::Ptr job,
                std::size_t stage) {
            if (job.Get () != 0 && stage < stages.size ()) {
                stages[stage]->EnqJob (
                    dynamic_refcounted_pointer_cast<RunLoop::Job> (job));
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Pipeline::Flush () {
            for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                stages[i]->CancelAllJobs ();
            }
        }

        void Pipeline::GetStats (std::vector<RunLoop::Stats> &stats) {
            stats.resize (stages.size ());
            for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                stats[i] = stages[i]->GetStats ();
            }
        }

        void Pipeline::WaitForIdle (const TimeSpec &timeSpec) {
            for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                stages[i]->WaitForIdle (timeSpec);
            }
        }

    } // namespace util
} // namespace thekogans
