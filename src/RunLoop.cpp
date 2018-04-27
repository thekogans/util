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
#include "thekogans/util/HRTimer.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/RunLoop.h"

namespace thekogans {
    namespace util {

    #if defined (TOOLCHAIN_OS_Windows)
        void RunLoop::COMInitializer::InitializeWorker () throw () {
            THEKOGANS_UTIL_TRY {
                HRESULT result = CoInitializeEx (0, dwCoInit);
                if (result != S_OK) {
                    THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (result);
                }
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void RunLoop::COMInitializer::UninitializeWorker () throw () {
            CoUninitialize ();
        }

        void RunLoop::OLEInitializer::InitializeWorker () throw () {
            THEKOGANS_UTIL_TRY {
                HRESULT result = OleInitialize (0);
                if (result != S_OK) {
                    THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (result);
                }
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void RunLoop::OLEInitializer::UninitializeWorker () throw () {
            OleUninitialize ();
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        void RunLoop::Stats::Update (
                const RunLoop::Job::Id &jobId,
                ui64 start,
                ui64 end) {
            ++totalJobs;
            ui64 ellapsed =
                HRTimer::ComputeElapsedTime (start, end);
            totalJobTime += ellapsed;
            lastJob = Job (jobId, start, end, ellapsed);
            if (totalJobs == 1) {
                minJob = Job (jobId, start, end, ellapsed);
                maxJob = Job (jobId, start, end, ellapsed);
            }
            else if (minJob.totalTime > ellapsed) {
                minJob = Job (jobId, start, end, ellapsed);
            }
            else if (maxJob.totalTime < ellapsed) {
                maxJob = Job (jobId, start, end, ellapsed);
            }
        }

        std::string RunLoop::Stats::Job::ToString (
                const std::string &name,
                ui32 indentationLevel) const {
            assert (!name.empty ());
            Attributes attributes;
            attributes.push_back (Attribute ("id", id));
            attributes.push_back (Attribute ("startTime", ui64Tostring (startTime)));
            attributes.push_back (Attribute ("endTime", ui64Tostring (endTime)));
            attributes.push_back (Attribute ("totalTime", ui64Tostring (totalTime)));
            return OpenTag (indentationLevel, name.c_str (), attributes, true, true);
        }

        std::string RunLoop::Stats::ToString (
                const std::string &name,
                ui32 indentationLevel) const {
            Attributes attributes;
            attributes.push_back (Attribute ("jobCount", ui32Tostring (jobCount)));
            attributes.push_back (Attribute ("totalJobs", ui32Tostring (totalJobs)));
            attributes.push_back (Attribute ("totalJobTime", ui64Tostring (totalJobTime)));
            return
                OpenTag (
                    indentationLevel,
                    !name.empty () ? name.c_str () : "RunLoop",
                    attributes,
                    false,
                    true) +
                lastJob.ToString ("last", indentationLevel + 1) +
                minJob.ToString ("min", indentationLevel + 1) +
                maxJob.ToString ("max", indentationLevel + 1) +
                CloseTag (indentationLevel, !name.empty () ? name.c_str () : "RunLoop");
        }

        bool RunLoop::WaitForStart (
                Ptr &runLoop,
                const TimeSpec &sleepTimeSpec,
                const TimeSpec &waitTimeSpec) {
            TimeSpec now = GetCurrentTime ();
            TimeSpec deadline = now + waitTimeSpec;
            while ((runLoop.get () == 0 || !runLoop->IsRunning ()) && deadline > now) {
                Sleep (sleepTimeSpec);
                now = GetCurrentTime ();
            }
            return runLoop.get () != 0 && runLoop->IsRunning ();
        }

        RunLoop::ReleaseJobQueue::ReleaseJobQueue () :
                Thread ("ReleaseJobQueue"),
                jobsNotEmpty (jobsMutex) {
            Create ();
        }

        void RunLoop::ReleaseJobQueue::EnqJob (Job *job) {
            LockGuard<Mutex> guard (jobsMutex);
            jobs.push_back (job);
            jobsNotEmpty.Signal ();
        }

        RunLoop::Job *RunLoop::ReleaseJobQueue::DeqJob () {
            LockGuard<Mutex> guard (jobsMutex);
            while (jobs.empty ()) {
                jobsNotEmpty.Wait ();
            }
            return jobs.pop_front ();
        }

        void RunLoop::ReleaseJobQueue::Run () throw () {
            while (1) {
                Job *job = DeqJob ();
                if (job != 0) {
                    job->Release ();
                }
            }
        }

    } // namespace util
} // namespace thekogans
