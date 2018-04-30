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

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
    #include <objbase.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Heap.h"

namespace thekogans {
    namespace util {

        /// \struct RunLoop RunLoop.h thekogans/util/RunLoop.h
        ///
        /// \brief
        /// RunLoop is an abstract base for \see{JobQueue}, \see{DefaultRunLoop} and
        /// \see{SystemRunLoop}. RunLoop allows you to schedule jobs to be executed on
        /// the thread that's running the run loop.

        struct _LIB_THEKOGANS_UTIL_DECL RunLoop {
            /// \brief
            /// Convenient typedef for std::unique_ptr<RunLoop>.
            typedef std::unique_ptr<RunLoop> Ptr;

            /// \enum
            /// The order in which pending jobs are processed;
            /// First In, First Out (TYPE_FIFO), or Last In,
            /// First Out (TYPE_LIFO).
            enum Type {
                /// \brief
                /// First in first out.
                TYPE_FIFO,
                /// \brief
                /// Last in first out.
                TYPE_LIFO,
            };

            /// \brief
            /// Forward declaration of Job.
            struct Job;
            enum {
                /// \brief
                /// JobList ID.
                JOB_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<Job, JOB_LIST_ID>.
            typedef IntrusiveList<Job, JOB_LIST_ID> JobList;

        #if defined (_MSC_VER)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (_MSC_VER)
            /// \struct RunLoop::Job RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// A RunLoop::Job must, at least, implement the Execute method.
            struct _LIB_THEKOGANS_UTIL_DECL Job :
                    public ThreadSafeRefCounted,
                    public JobList::Node {
                /// \brief
                /// Convenient typedef for ThreadSafeRefCounted::Ptr<Job>.
                typedef ThreadSafeRefCounted::Ptr<Job> Ptr;

                /// \brief
                /// Convenient typedef for std::string.
                typedef std::string Id;

                /// \brief
                /// Job id.
                const Id id;
                /// \brief
                /// Call Cancel () to set this flag. Monitor this
                /// flag in Execute () to respond to cancellation
                /// requests.
                volatile bool cancelled;
                /// \brief
                /// Flag used internally to wait for synchronous jobs.
                volatile bool finished;

                /// \brief
                /// ctor.
                /// \param[in] id_ Job id.
                Job (const Id &id_ = GUID::FromRandom ().ToString ()) :
                    id (id_),
                    cancelled (false),
                    finished (false) {}
                /// \brief
                /// dtor.
                virtual ~Job () {}

                /// \brief
                /// Call this method on a running job to cancel execution.
                /// Monitor this flag in Execute () to respond to cancellation
                /// requests.
                inline void Cancel () {
                    cancelled = true;
                }

                /// \brief
                /// Return a unique identifier for this
                /// job. Used to cancel a pending job.
                /// \return unique id for the job.
                inline Id GetId () const throw () {
                    return id;
                }

                // See VERY IMPORTANT comment in Stop below.
                /// \brief
                /// Called before the job is executed. Allows
                /// the job to perform one time initialization.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Prologue (volatile const bool & /*done*/) throw () {}
                /// \brief
                /// Called to execute the job. This is the only api
                /// the job MUST implement.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Execute (volatile const bool & /*done*/) throw () = 0;
                /// \brief
                /// Called after the job is executed. Allows
                /// the job to perform one time cleanup.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Epilogue (volatile const bool & /*done*/) throw () {}

                /// \brief
                /// Return true if the job should stop what it's doing and exit.
                /// Use this method in your implementation of Execute to keep the
                /// RunLoop responsive.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                /// \return true == Job should stop what it's doing and exit.
                inline bool ShouldStop (volatile const bool &done) const {
                    return done || cancelled;
                }
                /// \brief
                /// Return true if job is still running.
                /// \return true == Job has not been cancelled and is not finished.
                inline bool Running () const {
                    return !cancelled && !finished;
                }
                /// \brief
                /// Return true if job completed successfully.
                /// \return true == Job has not been cancelled and is finished.
                inline bool Completed () const {
                    return !cancelled && finished;
                }
            };
        #if defined (_MSC_VER)
            #pragma warning (pop)
        #endif // defined (_MSC_VER)

            /// \struct RunLoop::Stats RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// RunLoop statistics.\n
            /// jobCount - Number of pending jobs.\n
            /// totalJobs - Number of retired (executed) jobs.\n
            /// totalJobTime - Amount of time spent executing jobs.\n
            /// last - Last job.\n
            /// min - Fastest job.\n
            /// max - Slowest job.\n
            struct _LIB_THEKOGANS_UTIL_DECL Stats {
                /// \brief
                /// Pending job count.
                ui32 jobCount;
                /// \brief
                /// Total jobs processed.
                ui32 totalJobs;
                /// \brief
                /// Total time taken to process totalJobs.
                ui64 totalJobTime;
                /// \struct RunLoop::Stats::Job RunLoop.h thekogans/util/RunLoop.h
                ///
                /// \brief
                /// Job stats.
                struct _LIB_THEKOGANS_UTIL_DECL Job {
                    /// \brief
                    /// Job id.
                    RunLoop::Job::Id id;
                    /// \brief
                    /// Job start time.
                    ui64 startTime;
                    /// \brief
                    /// Job end time.
                    ui64 endTime;
                    /// \brief
                    /// Job total execution time.
                    ui64 totalTime;

                    /// \brief
                    /// ctor.
                    Job () :
                        startTime (0),
                        endTime (0),
                        totalTime (0) {}
                    /// \brief
                    /// ctor.
                    /// \param[in] id_ Job id.
                    /// \param[in] startTime_ Job start time.
                    /// \param[in] endTime_ Job end time.
                    /// \param[in] totalTime_ Job total execution time.
                    Job (
                        RunLoop::Job::Id id_,
                        ui64 startTime_,
                        ui64 endTime_,
                        ui64 totalTime_) :
                        id (id_),
                        startTime (startTime_),
                        endTime (endTime_),
                        totalTime (totalTime_) {}

                    /// \brief
                    /// Return the XML representation of the Job stats.
                    /// \param[in] name Job stats name (last, min, max).
                    /// \param[in] indentationLevel Pretty print parameter. If
                    /// the resulting tag is to be included in a larger structure
                    /// you might want to provide a value that will embed it in
                    /// the structure.
                    /// \return The XML reprentation of the Job stats.
                    std::string ToString (
                        const std::string &name,
                        ui32 indentationLevel) const;
                };
                /// \brief
                /// Last job stats.
                Job lastJob;
                /// \brief
                /// Minimum job stats.
                Job minJob;
                /// \brief
                /// Maximum job stats.
                Job maxJob;

                /// \brief
                /// ctor.
                Stats () :
                    jobCount (0),
                    totalJobs (0),
                    totalJobTime (0) {}

                /// \brief
                /// After completion of each job, used to update the stats.
                /// \param[in] jobId Id of completed job.
                /// \param[in] start Job start time.
                /// \param[in] end Job end time.
                void Update (
                    const RunLoop::Job::Id &jobId,
                    ui64 start,
                    ui64 end);

                /// \brief
                /// Return the XML representation of the Stats.
                /// \param[in] name RunLoop name.
                /// \param[in] indentationLevel Pretty print parameter. If
                /// the resulting tag is to be included in a larger structure
                /// you might want to provide a value that will embed it in
                /// the structure.
                /// \return The XML reprentation of the Stats.
                std::string ToString (
                    const std::string &name,
                    ui32 indentationLevel) const;
            };

            /// \struct RunLoop::WorkerCallback RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Gives the RunLoop owner a chance to initialize/uninitialize the worker threads.
            struct _LIB_THEKOGANS_UTIL_DECL WorkerCallback {
                /// \brief
                /// dtor.
                virtual ~WorkerCallback () {}

                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw () {}
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw () {}
            };

        #if defined (TOOLCHAIN_OS_Windows)
            /// \struct RunLoop::COMInitializer RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Initialize the Windows COM library.
            struct _LIB_THEKOGANS_UTIL_DECL COMInitializer : public WorkerCallback {
                /// \brief
                /// CoInitializeEx concurrency model.
                DWORD dwCoInit;

                /// \brief
                /// ctor.
                /// \param[in] dwCoInit_ CoInitializeEx concurrency model.
                COMInitializer (DWORD dwCoInit_ = COINIT_MULTITHREADED) :
                    dwCoInit (dwCoInit_) {}

                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw ();
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw ();
            };

            /// \struct RunLoop::OLEInitializer RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Initialize the Windows OLE library.
            struct _LIB_THEKOGANS_UTIL_DECL OLEInitializer : public WorkerCallback {
                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw ();
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw ();
            };
        #endif // defined (TOOLCHAIN_OS_Windows)

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
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            virtual void Stop (bool /*cancelPendingJobs*/ = true) = 0;

            /// \brief
            /// Enqueue a job to be performed on the run loop thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// NOTE: Same constraint applies to Enq as Stop. Namely, you can't call Enq
            /// from the same thread that called Start.
            virtual void EnqJob (
                Job::Ptr /*job*/,
                bool /*wait*/ = false) = 0;

            /// \struct RunLoop::EqualityTest RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Equality test to use to determine which jobs to wait for and cancel.
            struct EqualityTest {
                /// \brief
                /// dtor.
                virtual ~EqualityTest () {}

                /// \brief
                /// Reimplement this function to test for equality.
                /// \param[in] job Instance to test for equality.
                /// \return true == equal.
                virtual bool operator () (const Job & /*job*/) const throw () = 0;
            };

            /// \brief
            /// Wait for a queued job with a given id. If the job is not
            /// in the queue (in flight), it is not waited on.
            /// \param[in] jobId Id of job to wait on.
            /// \return true if the job was waited on. false if in flight.
            virtual bool WaitForJob (const Job::Id & /*jobId*/) = 0;
            /// \brief
            /// Wait for all queued job matching the given equality test. Jobs in flight
            /// are not waited on.
            /// \param[in] equalityTest EqualityTest to query to determine which jobs to wait on.
            virtual void WaitForJobs (const EqualityTest & /*equalityTest*/) = 0;
            /// \brief
            /// Blocks until all jobs are complete and the run loop is empty.
            virtual void WaitForIdle () = 0;

            /// \brief
            /// Cancel a queued job with a given id. If the job is not
            /// in the queue (in flight), it is not canceled.
            /// \param[in] jobId Id of job to cancel.
            /// \return true = the job was cancelled. false = in flight or nonexistent.
            virtual bool CancelJob (const Job::Id & /*jobId*/) = 0;
            /// \brief
            /// Cancel all queued job matching the given equality test. Jobs in flight
            /// are not canceled.
            /// \param[in] equalityTest EqualityTest to query to determine which jobs to cancel.
            virtual void CancelJobs (const EqualityTest & /*equalityTest*/) = 0;
            /// \brief
            /// Cancel all queued jobs. Job in flight is unaffected.
            virtual void CancelAllJobs () = 0;

            /// \brief
            /// Return a snapshot of the run loop stats.
            /// \return A snapshot of the run loop stats.
            virtual Stats GetStats () = 0;

            /// \brief
            /// Return true if Start was called.
            /// \return true if Start was called.
            virtual bool IsRunning () = 0;
            /// \brief
            /// Return true if there are no pending jobs.
            /// \return true = no pending jobs, false = jobs pending.
            virtual bool IsEmpty () = 0;
            /// \brief
            /// Return true if there are no pending jobs and the
            /// worker is idle.
            /// \return true = idle, false = busy.
            virtual bool IsIdle () = 0;

            /// \brief
            /// Wait until the given run loop is created the and it starts running.
            /// \param[in] runLoop RunLoop to wait for.
            /// \param[in] sleepTimeSpec How long to sleep between tries.
            /// \param[in] waitTimeSpec Total time to wait.
            /// \return true == the given run loop is running.
            /// false == timed out waiting for the run loop to start.
            static bool WaitForStart (
                Ptr &runLoop,
                const TimeSpec &sleepTimeSpec = TimeSpec::FromMilliseconds (50),
                const TimeSpec &waitTimeSpec = TimeSpec::FromSeconds (3));

        protected:
            /// \brief
            /// Forward declaration of JobProxy.
            struct JobProxy;
            enum {
                /// \brief
                /// JobProxyList ID.
                JOB_PROXY_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<JobProxy, JOB_PROXY_LIST_ID>.
            typedef IntrusiveList<JobProxy, JOB_PROXY_LIST_ID> JobProxyList;

        #if defined (_MSC_VER)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (_MSC_VER)
            /// \struct RunLoop::JobProxy RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// JobProxy is a job wrapper used to hold a list of jobs without
            /// requiring that Job derive from multiple lists.
            struct _LIB_THEKOGANS_UTIL_DECL JobProxy : public JobProxyList::Node {
                /// \brief
                /// A private JobProxy heap to help with memory management.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (JobProxy, SpinLock)

                /// \brief
                /// Proxied job.
                Job::Ptr job;

                /// \brief
                /// ctor.
                /// \param[in] job_ Job to proxy.
                explicit JobProxy (Job *job_) :
                    job (job_) {}
            };
        #if defined (_MSC_VER)
            #pragma warning (pop)
        #endif // defined (_MSC_VER)

            /// \struct RunLoop::ReleaseJobQueue RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// ReleaseJobQueue is used internally by RunLoop derivatives (\see{JobQueue},
            /// \see{SystemRunLoop}, \see{DefaultRunLoop}...) to schedule finished/cancelled
            /// jobs for release. It's use resolves a number of potential deadlocks that can
            /// arrive if Job::Release is called on the worker thread.
            struct _LIB_THEKOGANS_UTIL_DECL ReleaseJobQueue :
                    public Singleton<ReleaseJobQueue, SpinLock>,
                    public Thread {
            private:
                /// \brief
                /// List of jobs waiting to be released.
                JobList jobs;
                /// \brief
                /// Since ReleaseJobQueue is a global singleton, jobs needs to be protected.
                Mutex jobsMutex;
                /// \brief
                /// Signal to our thread that we have jobs for it to release.
                Condition jobsNotEmpty;

            public:
                /// \brief
                /// ctor.
                ReleaseJobQueue ();

                /// \brief
                /// Enqueue a job to be released later.
                /// \param[in] job Job to release later.
                void EnqJob (Job *job);

            private:
                /// \brief
                /// Dequeue the next job to release.
                /// \return Next job to release.
                Job *DeqJob ();

                // Thread
                /// \brief
                /// Job release thread.
                virtual void Run () throw ();
            };
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RunLoop_h)
