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
#include <memory>
#include <algorithm>
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Vectorizer.h"

namespace thekogans {
    namespace util {

        Vectorizer::Vectorizer (
                std::size_t workerCount_,
                i32 workerPriority) :
                done (false),
                barrier (workerCount_),
                job (0),
                workerCount (0),
                chunkSize (0) {
            if (workerCount_ > 1) {
                // We are the first worker. Create workerCount_ - 1
                // additional worker threads.
                for (std::size_t i = 1; i < workerCount_; ++i) {
                    workers.emplace_back (
                        [this] (std::size_t rank) noexcept {
                            // NOTE: No exception handling here. If we were to wrap
                            // the while loop with try/catch, the first exception
                            // thrown would leave the vectorizer with one (or more)
                            // less workers (leaving it completely broken). If we
                            // instead wrap the contents of the loop, then we are
                            // catching an exception we do not know how to handle
                            // (and which leaves the job in an incomplete state).
                            // It's better to just crash loudly, and let the engineer
                            // fix his/her own code.
                            while (true) {
                                // Wait until Execute releases us.
                                barrier.Wait ();
                                // This test used to be done in the while above. It was a source
                                // of intermittent hangs that were difficult to debug. Turns out
                                // that in a single barrier state transition design all decisions
                                // MUST happen inside the barrier walls (between the waits). It
                                // is the only way to guard against phase leakage.
                                if (done.load (std::memory_order_relaxed)) {
                                    break;
                                }
                                if (job != nullptr && rank < workerCount) {
                                    std::size_t startIndex = rank * chunkSize;
                                    std::size_t endIndex = MIN (startIndex + chunkSize, job->Size ());
                                    // This test is necessary because of integer division in Execute.
                                    // By way of example;
                                    // 1. job size: 28
                                    // 2. workers: 8
                                    // 3. chunk size: 28 / 8 = 3 (integer division!)
                                    // 4. 8 * 3 = 24
                                    // 5. 24 < 28 so bump up chunk size (4)
                                    // 6. 7 * 4 = 28 <---- This means that the 8th worker
                                    // will have nothing to do, and hence the test below.
                                    if (startIndex < endIndex) {
                                        job->Execute (startIndex, endIndex, rank);
                                    }
                                }
                                // Wait for all worker threads to finish with the job.
                                barrier.Wait ();
                            }
                        }, i);
                    workers.back ().SetName (FormatString ("Vectorizer-%u", i));
                    workers.back ().SetPriority (workerPriority);
                }
            }
        }

        Vectorizer::~Vectorizer () {
            done.store (true, std::memory_order_relaxed);
            struct DoneJob : public Job {
                std::size_t workerCount;
                explicit DoneJob (std::size_t workerCount_) :
                    workerCount (workerCount_) {}
                virtual void Execute (
                    std::size_t /*startIndex*/,
                    std::size_t /*endIndex*/,
                    std::size_t /*rank*/) noexcept {}
                virtual std::size_t Size () const noexcept {
                    return workerCount;
                }
            } doneJob (workers.size () + 1);
            Execute (doneJob, 1);
            ThreadReaper2::Instance ()->ReapThreads (std::move (workers));
        }

        void Vectorizer::Execute (
                Job &job_,
                std::size_t chunkSize_) {
            if (job_.Size () > 0) {
                // If we are running on a uni-processor, don't waste time with
                // setup.
                if (!workers.empty ()) {
                    // Vectorizer is, as it's name implies, a global
                    // resource. It is not re-entrant. We therefore need to
                    // serialize access to the one and only entry point.
                    LockGuard<Mutex> guard (mutex);
                    assert (job == nullptr);
                    assert (workerCount == 0);
                    assert (chunkSize == 0);
                    job = &job_;
                    if (chunkSize_ == SIZE_T_MAX ||
                            chunkSize_ * (workers.size () + 1) < job->Size ()) {
                        workerCount = workers.size () + 1;
                        chunkSize = job->Size () / workerCount;
                        if (workerCount * chunkSize < job->Size ()) {
                            // Since we are holding workers constant, we
                            // must update chunkSize to account for the
                            // remainder.
                            ++chunkSize;
                        }
                    }
                    else {
                        chunkSize = MIN (chunkSize_, job->Size ());
                        workerCount = job->Size () / chunkSize_;
                        if (workerCount * chunkSize < job->Size ()) {
                            // Since we are holding chunkSize constant,
                            // we must update workerCount to account for
                            // the remainder.
                            ++workerCount;
                        }
                    }
                    assert (workerCount > 0 && workerCount <= workers.size () + 1);
                    assert (chunkSize > 0 && chunkSize <= job->Size ());
                    // Scatter.
                    job->Prolog (workerCount);
                    // Release the workers.
                    barrier.Wait ();
                    if (!done.load (std::memory_order_relaxed)) {
                        // Work on the first chunk.
                        job->Execute (0, chunkSize, 0);
                        // Wait for the rest of the workers.
                        barrier.Wait ();
                        // Gather.
                        job->Epilog ();
                    }
                    job = nullptr;
                    workerCount = 0;
                    chunkSize = 0;
                }
                else {
                    job_.Prolog (1);
                    job_.Execute (0, job_.Size (), 0);
                    job_.Epilog ();
                }
            }
        }

    } // namespace util
} // namespace thekogans
