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
#include "thekogans/util/Config.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/WorkerPool.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX (WorkerPool::Worker, SpinLock, 5)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX (WorkerPool::WorkerPtr, SpinLock, 5)

        WorkerPool::WorkerPool (
                ui32 minWorkers_,
                ui32 maxWorkers_,
                const std::string &name_,
                RunLoop::Type type_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                ui32 maxPendingJobs_,
                RunLoop::WorkerCallback *workerCallback_) :
                minWorkers (minWorkers_),
                maxWorkers (maxWorkers_),
                name (name_),
                type (type_),
                workerCount (workerCount_),
                workerPriority (workerPriority_),
                workerAffinity (workerAffinity_),
                maxPendingJobs (maxPendingJobs_),
                workerCallback (workerCallback_),
                activeWorkerCount (0) {
            assert (minWorkers > 0);
            assert (maxWorkers >= minWorkers);
            for (ui32 i = 0; i < minWorkers; ++i) {
                std::string workerName;
                if (!name.empty ()) {
                    workerName = FormatString ("%s-%u", name.c_str (), i);
                }
                workers.push_back (
                    new Worker (
                        workerName,
                        type,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        maxPendingJobs,
                        workerCallback));
                ++activeWorkerCount;
            }
        }

        WorkerPool::~WorkerPool () {
            struct Callback : public WorkerList::Callback {
                typedef WorkerList::Callback::result_type result_type;
                typedef WorkerList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type worker) {
                    delete worker;
                    return true;
                }
            } callback;
            workers.clear (callback);
        }

        WorkerPool::WorkerPtr::WorkerPtr (
                WorkerPool &workerPool_,
                ui32 retries,
                const TimeSpec &timeSpec) :
                workerPool (workerPool_),
                worker (workerPool.GetWorker ()) {
            while (worker == 0 && retries-- != 0) {
                Sleep (timeSpec);
                worker = workerPool.GetWorker ();
            }
            if (worker == 0) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unable to acquire a worker from %s pool.",
                    workerPool.name.c_str ());
            }
        }

        WorkerPool::WorkerPtr::~WorkerPtr () {
            assert (worker != 0);
            workerPool.ReleaseWorker (worker);
        }

        WorkerPool::Worker *WorkerPool::GetWorker () {
            Worker *worker = 0;
            {
                LockGuard<SpinLock> guard (spinLock);
                if (!workers.empty ()) {
                    // Borrow a worker from the front of the queue.
                    // This combined with ReleaseWorker putting
                    // returned workers at the front should guarantee
                    // the best cache utilization.
                    worker = workers.pop_front ();
                }
                else if (activeWorkerCount < maxWorkers) {
                    std::string workerName;
                    if (!name.empty ()) {
                        workerName = FormatString ("%s-%u", name.c_str (), activeWorkerCount);
                    }
                    worker = new Worker (
                        workerName,
                        type,
                        workerCount,
                        workerPriority,
                        workerAffinity,
                        maxPendingJobs,
                        workerCallback);
                    ++activeWorkerCount;
                }
            }
            return worker;
        }

        void WorkerPool::ReleaseWorker (Worker *worker) {
            if (worker != 0) {
                LockGuard<SpinLock> guard (spinLock);
                if (activeWorkerCount > minWorkers) {
                    delete worker;
                    --activeWorkerCount;
                }
                else {
                    // Put the recently used worker at the front of
                    // the queue. With any luck the next time a worker
                    // is borrowed from this pool, it will be the last
                    // one used, and it's cache will be nice and warm.
                    workers.push_front (worker);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        ui32 GlobalWorkerPoolCreateInstance::minWorkers = SystemInfo::Instance ().GetCPUCount ();
        ui32 GlobalWorkerPoolCreateInstance::maxWorkers = SystemInfo::Instance ().GetCPUCount () * 2;
        std::string GlobalWorkerPoolCreateInstance::name = std::string ();
        RunLoop::Type GlobalWorkerPoolCreateInstance::type = RunLoop::TYPE_FIFO;
        ui32 GlobalWorkerPoolCreateInstance::workerCount = 1;
        i32 GlobalWorkerPoolCreateInstance::workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY;
        ui32 GlobalWorkerPoolCreateInstance::workerAffinity = UI32_MAX;
        ui32 GlobalWorkerPoolCreateInstance::maxPendingJobs = UI32_MAX;
        RunLoop::WorkerCallback *GlobalWorkerPoolCreateInstance::workerCallback = 0;

        void GlobalWorkerPoolCreateInstance::Parameterize (
                ui32 minWorkers_,
                ui32 maxWorkers_,
                const std::string &name_,
                RunLoop::Type type_,
                ui32 workerCount_,
                i32 workerPriority_,
                ui32 workerAffinity_,
                ui32 maxPendingJobs_,
                RunLoop::WorkerCallback *workerCallback_) {
            minWorkers = minWorkers_;
            maxWorkers = maxWorkers_;
            name = name_;
            type = type_;
            workerCount = workerCount_;
            workerPriority = workerPriority_;
            workerAffinity = workerAffinity_;
            maxPendingJobs = maxPendingJobs_;
            workerCallback = workerCallback_;
        }

        WorkerPool *GlobalWorkerPoolCreateInstance::operator () () {
            return new WorkerPool (
                minWorkers,
                maxWorkers,
                name,
                type,
                workerCount,
                workerPriority,
                workerAffinity,
                maxPendingJobs,
                workerCallback);
        }

    } // namespace util
} // namespace thekogans
