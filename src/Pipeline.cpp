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
                Job::UniquePtr next = Clone ();
                if (next.get () != 0) {
                    std::size_t nextStage = next->stage;
                    assert (nextStage < pipeline.stages.size ());
                    pipeline.stages[nextStage]->Enq (
                        JobQueue::Job::UniquePtr (next.release ()));
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
                        begin->workerAffinity));
                assert (stage.get () != 0);
                if (stage.get () != 0) {
                    stages.push_back (stage.get ());
                    stage.release ();
                }
            }
        }

        Pipeline::Pipeline (
                ui32 stageCount,
                const std::string &stageName,
                JobQueue::Type stageType,
                ui32 stageWorkerCount,
                i32 stageWorkerPriority,
                StagePriorityAdjustment stagePriorityAdjustment,
                i32 stagePriorityAdjustmentDelta,
                ui32 stageWorkerAffinity) {
            assert (stageCount > 0);
            for (ui32 i = 0; i < stageCount; ++i) {
                std::string stageName_;
                if (!stageName.empty ()) {
                    stageName_ = util::FormatString ("%s-%u", stageName.c_str (), i);
                }
                JobQueue::UniquePtr stage (
                    new JobQueue (
                        stageName_,
                        stageType,
                        stageWorkerCount,
                        stageWorkerPriority,
                        stageWorkerAffinity));
            #if !defined (TOOLCHAIN_OS_Windows)
                switch (stagePriorityAdjustment) {
                    case NoAdjustment:
                        break;
                    case Inc:
                        stageWorkerPriority = IncPriority (stageWorkerPriority);
                        break;
                    case Add:
                        stageWorkerPriority =
                            AddPriority (stageWorkerPriority, stagePriorityAdjustmentDelta);
                        break;
                    case Dec:
                        stageWorkerPriority = DecPriority (stageWorkerPriority);
                        break;
                    case Sub:
                        stageWorkerPriority =
                            SubPriority (stageWorkerPriority, stagePriorityAdjustmentDelta);
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

        void Pipeline::Enq (
                Job::UniquePtr job,
                std::size_t stage) {
            if (job.get () != 0) {
                if (stage < stages.size ()) {
                    stages[stage]->Enq (JobQueue::Job::UniquePtr (job.release ()));
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void Pipeline::Flush () {
            for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                stages[i]->CancelAll ();
            }
        }

        void Pipeline::GetStats (std::vector<JobQueue::Stats> &stats) {
            stats.resize (stages.size ());
            for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                stats[i] = stages[i]->GetStats ();
            }
        }

        void Pipeline::WaitForIdle () {
            for (std::size_t i = 0, count = stages.size (); i < count; ++i) {
                stages[i]->WaitForIdle ();
            }
        }

    } // namespace util
} // namespace thekogans
