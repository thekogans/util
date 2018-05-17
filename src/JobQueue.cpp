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

#include "thekogans/util/LockGuard.h"
#include "thekogans/util/HRTimer.h"
#include "thekogans/util/Exception.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/LoggerMgr.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/JobQueue.h"

namespace thekogans {
    namespace util {

        void JobQueue::Worker::Run () throw () {
            struct WorkerInitializer {
                JobQueue &queue;
                explicit WorkerInitializer (JobQueue &queue_) :
                        queue (queue_) {
                    if (queue.workerCallback != 0) {
                        queue.workerCallback->InitializeWorker ();
                    }
                }
                ~WorkerInitializer () {
                    if (queue.workerCallback != 0) {
                        queue.workerCallback->UninitializeWorker ();
                    }
                }
            } workerInitializer (queue);
            while (!queue.done) {
                Job *job = queue.DeqJob ();
                if (job != 0) {
                    ui64 start = 0;
                    ui64 end = 0;
                    // Short circuit cancelled pending jobs.
                    if (job->GetDisposition () == Job::Unknown) {
                        start = HRTimer::Click ();
                        job->SetStatus (Job::Running);
                        job->Prologue (queue.done);
                        job->Execute (queue.done);
                        job->Epilogue (queue.done);
                        job->Finish ();
                        end = HRTimer::Click ();
                    }
                    queue.FinishedJob (job, start, end);
                }
            }
        }

        JobQueue::JobQueue (
                const std::string &name,
                Type type,
                ui32 maxPendingJobs,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                WorkerCallback *workerCallback_) :
                RunLoop (name, type, maxPendingJobs),
                workerCount (workerCount_),
                workerPriority (workerPriority_),
                workerAffinity (workerAffinity_),
                workerCallback (workerCallback_) {
            if (workerCount > 0) {
                Start ();
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void JobQueue::Start () {
            LockGuard<Mutex> guard (workersMutex);
            if (SetDone (false)) {
                for (ui32 i = 0; i < workerCount; ++i) {
                    std::string workerName;
                    if (!name.empty ()) {
                        if (workerCount > 1) {
                            workerName = FormatString ("%s-%u", name.c_str (), i);
                        }
                        else {
                            workerName = name;
                        }
                    }
                    workers.push_back (
                        new Worker (
                            *this,
                            workerName,
                            workerPriority,
                            workerAffinity));
                }
            }
        }

        void JobQueue::Stop (bool cancelPendingJobs) {
            {
                LockGuard<Mutex> guard (workersMutex);
                if (SetDone (true)) {
                    jobsNotEmpty.SignalAll ();
                    struct Callback : public WorkerList::Callback {
                        typedef WorkerList::Callback::result_type result_type;
                        typedef WorkerList::Callback::argument_type argument_type;
                        virtual result_type operator () (argument_type worker) {
                            // Join the worker thread before deleting it to
                            // let it's thread function finish it's tear down.
                            worker->Wait ();
                            delete worker;
                            return true;
                        }
                    } callback;
                    workers.clear (callback);
                    assert (runningJobs.empty ());
                }
            }
            if (cancelPendingJobs) {
                CancelAllJobs ();
            }
        }

        std::string GlobalJobQueueCreateInstance::name = std::string ();
        RunLoop::Type GlobalJobQueueCreateInstance::type = RunLoop::TYPE_FIFO;
        ui32 GlobalJobQueueCreateInstance::maxPendingJobs = UI32_MAX;
        ui32 GlobalJobQueueCreateInstance::workerCount = 1;
        i32 GlobalJobQueueCreateInstance::workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY;
        ui32 GlobalJobQueueCreateInstance::workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY;
        RunLoop::WorkerCallback *GlobalJobQueueCreateInstance::workerCallback = 0;

        void GlobalJobQueueCreateInstance::Parameterize (
                const std::string &name_,
                RunLoop::Type type_,
                ui32 maxPendingJobs_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                RunLoop::WorkerCallback *workerCallback_) {
            name = name_;
            type = type_;
            maxPendingJobs = maxPendingJobs_;
            workerCount = workerCount_;
            workerPriority = workerPriority_;
            workerAffinity = workerAffinity_;
            workerCallback = workerCallback_;
        }

        JobQueue *GlobalJobQueueCreateInstance::operator () () {
            return new JobQueue (
                name,
                type,
                maxPendingJobs,
                workerCount,
                workerPriority,
                workerAffinity,
                workerCallback);
        }

    } // namespace util
} // namespace thekogans
