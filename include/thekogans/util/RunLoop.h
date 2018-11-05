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
#include <list>
#if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
    #include <pugixml.hpp>
#endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Condition.h"
#include "thekogans/util/Event.h"

namespace thekogans {
    namespace util {

        /// \struct RunLoop RunLoop.h thekogans/util/RunLoop.h
        ///
        /// \brief
        /// RunLoop is an abstract base for \see{JobQueue}, \see{DefaultRunLoop} and
        /// \see{SystemRunLoop}. RunLoop allows you to schedule jobs (RunLoop::Job)
        /// to be executed on the thread that's running the run loop.

        struct _LIB_THEKOGANS_UTIL_DECL RunLoop : public ThreadSafeRefCounted {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<RunLoop>.
            typedef ThreadSafeRefCounted::Ptr<RunLoop> Ptr;

            /// \brief
            /// Convenient typedef for std::string.
            typedef std::string Id;

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
            /// \brief
            /// Job list id.
            enum {
                /// \brief
                /// JobList ID.
                JOB_LIST_ID,
                /// \brief
                /// Use this sentinel to create your own job lists (\see{Pipeline}).
                LAST_JOB_LIST_ID
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

                /// \enum
                /// Job states.
                enum State {
                    /// \brief
                    /// Job is pending execution.
                    Pending,
                    /// \brief
                    /// Job is currently running.
                    Running,
                    /// \brief
                    /// Job has completed execution.
                    Completed
                };

                /// \enum
                /// Job execution disposition.
                enum Disposition {
                    /// \brief
                    /// Unknown.
                    Unknown,
                    /// \brief
                    /// Job was cancelled.
                    Cancelled,
                    /// \brief
                    /// Job failed execution.
                    Failed,
                    /// \brief
                    /// Job succeeded execution.
                    Succeeded
                };

            protected:
                /// \brief
                /// Job id.
                const Id id;
                /// \brief
                /// RunLoop id on which the job is running.
                RunLoop::Id runLoopId;
                /// \brief
                /// Job state.
                volatile State state;
                /// \brief
                /// Job disposition.
                volatile Disposition disposition;
                /// \brief
                /// If IsFailed, exception will hold the reason.
                Exception exception;
                /// \brief
                /// Provides interruptable sleep (see Sleep below).
                Event sleeping;
                /// \brief
                /// Set when job completes execution.
                Event completed;

            public:
                /// \brief
                /// ctor.
                /// \param[in] id_ Job id.
                Job (const Id &id_ = GUID::FromRandom ().ToString ()) :
                    id (id_),
                    state (Completed),
                    disposition (Unknown),
                    sleeping (false) {}
                /// \brief
                /// dtor.
                virtual ~Job () {}

                /// \brief
                /// Return the job id.
                /// \return Job id.
                inline const Id &GetId () const {
                    return id;
                }
                /// \brief
                /// Return the job RunLoop id.
                /// \return RunLoop id.
                inline const Id &GetRunLoopId () const {
                    return runLoopId;
                }
                /// \brief
                /// Return the job state.
                /// \return Job state.
                inline State GetState () const {
                    return state;
                }
                /// \brief
                /// Return true if job is pending.
                /// \return true == Job is pending.
                inline bool IsPending () const {
                    return state == Pending;
                }
                /// \brief
                /// Return true if job is running.
                /// \return true == Job is running.
                inline bool IsRunning () const {
                    return state == Running;
                }
                /// \brief
                /// Return true if job completed.
                /// \return true == Job has completed.
                inline bool IsCompleted () const {
                    return state == Completed;
                }

                /// \brief
                /// Return the job disposition.
                /// \return Job disposition.
                inline Disposition GetDisposition () const {
                    return disposition;
                }
                /// \brief
                /// Return true if the job was cancelled.
                /// \return true == the job was cancelled.
                inline bool IsCancelled () const {
                    return disposition == Cancelled;
                }
                /// \brief
                /// Return true if the job has failed.
                /// \return true == the job has failed.
                inline bool IsFailed () const {
                    return disposition == Failed;
                }
                /// \brief
                /// Return true if the job has succeeded.
                /// \return true == the job has succeeded.
                inline bool IsSucceeded () const {
                    return disposition == Succeeded;
                }

                /// \brief
                /// Call this method on a running job to cancel execution.
                /// Monitor disposition (ShouldStop () below) in Execute ()
                /// to respond to cancellation requests.
                void Cancel ();

                /// \brief
                /// Wait for the job to complete.
                /// \param[in] timeSpec How long to wait for the job to complete.
                /// IMPORTANT: timeSpec is a relative value.
                /// \return true == Wait completed successfully, false == Timed out.
                inline bool Wait (const TimeSpec &timeSpec = TimeSpec::Infinite) {
                    return completed.Wait (timeSpec);
                }

            protected:
                /// \brief
                /// Used internally by RunLoop to set the RunLoop id and reset
                /// state, disposition and completed.
                /// \param[in] runLoopId_ RunLoop id to which this job belongs.
                virtual void Reset (const RunLoop::Id &runLoopId_);
                /// \brief
                /// Used internally by RunLoop and it's derivatives to set the
                /// job state.
                /// \param[in] state_ New job state.
                virtual void SetState (State state_);

                /// \brief
                /// Used internally by RunLoop and it's derivatives to mark the
                /// job as failed execution.
                void Fail (const Exception &exception_);
                /// \brief
                /// Used internally by RunLoop and it's derivatives to mark the
                /// job as succeeded execution.
                /// \param[in] done false == job completed successfully, otherwise
                /// job was forced to exit Execute because the run loop was stopped.
                void Succeed (const THEKOGANS_UTIL_ATOMIC<bool> &done);

                /// \brief
                /// Return true if the job should stop what it's doing and exit.
                /// Use this method in your implementation of Execute to keep the
                /// RunLoop responsive.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                /// \return true == Job should stop what it's doing and exit.
                inline bool ShouldStop (const THEKOGANS_UTIL_ATOMIC<bool> &done) const {
                    return done || IsCancelled () || IsFailed ();
                }

                /// \brief
                /// Use this method when your job needs to sleep a while during execution.
                /// It provides interruptable sleep in case of cancellation.
                /// \param[in] timeSpec How long to sleep.
                /// IMPORTANT: timeSpec is a relative value.
                /// \return true == Slept successfully, false == Interrupted by Cancel.
                inline bool Sleep (const TimeSpec &timeSpec = TimeSpec::Infinite) {
                    return !sleeping.Wait (timeSpec);
                }

                /// \brief
                /// Called before the job is executed. Allows
                /// the job to perform one time initialization.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Prologue (const THEKOGANS_UTIL_ATOMIC<bool> & /*done*/) throw () {}
                /// \brief
                /// Called to execute the job. This is the only api
                /// the job MUST implement.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Execute (const THEKOGANS_UTIL_ATOMIC<bool> & /*done*/) throw () = 0;
                /// \brief
                /// Called after the job is executed. Allows
                /// the job to perform one time cleanup.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Epilogue (const THEKOGANS_UTIL_ATOMIC<bool> & /*done*/) throw () {}

                /// \brief
                /// RunLoop needs acces to protected members.
                friend struct RunLoop;
                /// \brief
                /// DefaultRunLoop needs acces to protected members.
                friend struct DefaultRunLoop;
                /// \brief
                /// SystemRunLoop needs acces to protected members.
                friend struct SystemRunLoop;
                /// \brief
                /// JobQueue needs acces to protected members.
                friend struct JobQueue;
                /// \brief
                /// Pipeline needs acces to protected members.
                friend struct Pipeline;
                /// \brief
                /// Scheduler needs acces to protected members.
                friend struct Scheduler;
            };
        #if defined (_MSC_VER)
            #pragma warning (pop)
        #endif // defined (_MSC_VER)

            /// \brief
            /// Convenient typedef for std::list<Job::Ptr>.
            typedef std::list<Job::Ptr> UserJobList;

            /// \struct RunLoop::Stats RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// RunLoop statistics.\n
            /// totalJobs - Number of retired (completed) jobs.\n
            /// totalJobTime - Amount of time spent executing jobs.\n
            /// last - Last job.\n
            /// min - Fastest job.\n
            /// max - Slowest job.\n
            struct _LIB_THEKOGANS_UTIL_DECL Stats {
                /// \brief
                /// "RunLoop"
                static const char * const TAG_RUN_LOOP;
                /// \brief
                /// "Name"
                static const char * const ATTR_NAME;
                /// \brief
                /// "TotalJobs"
                static const char * const ATTR_TOTAL_JOBS;
                /// \brief
                /// "TotalJobTime"
                static const char * const ATTR_TOTAL_JOB_TIME;
                /// \brief
                /// "LastJob"
                static const char * const TAG_LAST_JOB;
                /// \brief
                /// "MinJob"
                static const char * const TAG_MIN_JOB;
                /// \brief
                /// "MaxJob"
                static const char * const TAG_MAX_JOB;

                /// \brief
                /// Run loop name.
                std::string name;
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
                    /// "Job"
                    static const char * const TAG_JOB;
                    /// \brief
                    /// "Id"
                    static const char * const ATTR_ID;
                    /// \brief
                    /// "StartTime"
                    static const char * const ATTR_START_TIME;
                    /// \brief
                    /// "EndTime"
                    static const char * const ATTR_END_TIME;
                    /// \brief
                    /// "TotalTime"
                    static const char * const ATTR_TOTAL_TIME;

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
                    /// Reset the job stats.
                    void Reset ();

                #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
                    /// \brief
                    /// Parse a job from an xml dom that looks like this;
                    /// <Job Id = ""
                    ///      StartTime = ""
                    ///      EndTime = ""
                    ///      TotalTime = ""/>
                    /// \param[in] node DOM representation of an job.
                    void Parse (const pugi::xml_node &node);
                #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)
                    /// \brief
                    /// Return the XML representation of the Job stats.
                    /// \param[in] indentationLevel Pretty print parameter. If
                    /// the resulting tag is to be included in a larger structure
                    /// you might want to provide a value that will embed it in
                    /// the structure.
                    /// \param[in] tagName Job stats name.
                    /// \return The XML reprentation of the Job stats.
                    std::string ToString (
                        std::size_t indentationLevel,
                        const char *tagName = TAG_JOB) const;
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
                /// \param[in] name_ Run loop name.
                explicit Stats (const std::string &name_) :
                    name (name_),
                    totalJobs (0),
                    totalJobTime (0) {}

                /// \brief
                /// Reset the RunLoop stats.
                void Reset ();

            #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
                /// \brief
                /// Parse run loop stats from an xml dom that looks like this;
                /// <RunLoop Name = ""
                ///          TotalJobs = ""
                ///          TotalJobTime = "">
                ///     <LastJob .../>
                ///     <MinJob .../>
                ///     <MaxJob .../>
                /// </RunLoop>
                /// \param[in] node DOM representation of run loop stats.
                void Parse (const pugi::xml_node &node);
            #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)
                /// \brief
                /// Return the XML representation of the Stats.
                /// \param[in] indentationLevel Pretty print parameter. If
                /// the resulting tag is to be included in a larger structure
                /// you might want to provide a value that will embed it in
                /// the structure.
                /// \param[in] tagName Root tag name.
                /// \return The XML reprentation of the Stats.
                std::string ToString (
                    std::size_t indentationLevel,
                    const char *tagName = TAG_RUN_LOOP) const;

            private:
                /// \brief
                /// After completion of each job, used to update the stats.
                /// \param[in] job Completed job.
                /// \param[in] start Job start time.
                /// \param[in] end Job end time.
                void Update (
                    RunLoop::Job *job,
                    ui64 start,
                    ui64 end);

                /// \brief
                /// RunLoop needs access to Update.
                friend struct RunLoop;
                /// \brief
                /// Pipeline needs access to Update.
                friend struct Pipeline;
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

            /// \struct RunLoop::WorkerInitializer RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Instantiate one of these in your RunLoop thread to initialize/uninitialize the worker.
            struct _LIB_THEKOGANS_UTIL_DECL WorkerInitializer {
                /// \brief
                /// Worker thread initialization/uninitialization callback.
                WorkerCallback *workerCallback;

                /// \brief
                /// ctor.
                /// \param[in] workerCallback_ Worker thread initialization/uninitialization callback.
                explicit WorkerInitializer (WorkerCallback *workerCallback_);
                /// \brief
                /// dtor.
                ~WorkerInitializer ();
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
                /// \param[in] initialized_ true == worker thread has been initialized.
                COMInitializer (DWORD dwCoInit_ = COINIT_MULTITHREADED) :
                    dwCoInit (dwCoInit_) {}

                // WorkerCallback
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
                // WorkerCallback
                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw ();
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw ();
            };
        #elif defined (TOOLCHAIN_OS_OSX)
            /// \struct RunLoop::CocoaInitializer RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Initialize the OS X Cocoa framework.
            struct _LIB_THEKOGANS_UTIL_DECL CocoaInitializer : public WorkerCallback {
                // WorkerCallback
                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw ();
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw ();
            };
        #endif // defined (TOOLCHAIN_OS_Windows)

        protected:
            /// \brief
            /// RunLoop id.
            const Id id;
            /// \brief
            /// RunLoop name.
            const std::string name;
            /// \brief
            /// RunLoop type (TIPE_FIFO or TYPE_LIFO)
            const Type type;
            /// \brief
            /// Max pending jobs.
            const ui32 maxPendingJobs;
            /// \brief
            /// Flag to signal the worker thread(s).
            THEKOGANS_UTIL_ATOMIC<bool> done;
            /// \brief
            /// Queue of pending jobs.
            JobList pendingJobs;
            /// \brief
            /// List of running jobs.
            JobList runningJobs;
            /// \brief
            /// RunLoop stats.
            Stats stats;
            /// \brief
            /// Synchronization mutex.
            Mutex jobsMutex;
            /// \brief
            /// Synchronization condition variable.
            Condition jobsNotEmpty;
            /// \brief
            /// Synchronization condition variable.
            Condition idle;
            /// \brief
            /// true == run loop is paused.
            bool paused;
            /// \brief
            /// Signal waiting workers that the run loop is not paused.
            Condition notPaused;

        public:
            /// \brief
            /// ctor.
            /// \param[in] name_ RunLoop name.
            /// \param[in] type_ RunLoop type.
            /// \param[in] maxPendingJobs_ Max pending run loop jobs.
            /// \param[in] done_ true == Must call Start.
            RunLoop (
                const std::string &name_ = std::string (),
                Type type_ = TYPE_FIFO,
                ui32 maxPendingJobs_ = UI32_MAX,
                bool done_ = true);
            /// \brief
            /// dtor.
            virtual ~RunLoop ();

            /// \brief
            /// Return RunLoop id.
            /// \return RunLoop id.
            inline const Id &GetId () const {
                return id;
            }
            /// \brief
            /// Return RunLoop name.
            /// \return RunLoop name.
            inline const std::string &GetName () const {
                return name;
            }

            /// \brief
            /// Return the pendig job count.
            /// \return Pendig job count.
            std::size_t GetPendingJobCount ();
            /// \brief
            /// Return the running job count.
            /// \return Running job count.
            std::size_t GetRunningJobCount ();

            /// \brief
            /// Wait until the given run loop is created and it starts running.
            /// \param[in] runLoop RunLoop to wait for.
            /// \param[in] sleepTimeSpec How long to sleep between tries.
            /// \param[in] waitTimeSpec Total time to wait.
            /// \return true == the given run loop is running.
            /// false == timed out waiting for the run loop to start.
            static bool WaitForStart (
                Ptr &runLoop,
                const TimeSpec &sleepTimeSpec = TimeSpec::FromMilliseconds (50),
                const TimeSpec &waitTimeSpec = TimeSpec::FromSeconds (3));

            /// \brief
            /// Pause run loop execution. Currently running jobs are allowed to finish,
            /// but no other pending jobs are executed until Continue is called. If the
            /// run loop is paused, noop.
            /// VERY IMPORTANT: A paused run loop does NOT imply idle. If you pause a
            /// run loop that has pending jobs, \see{IsIdle} (below) will return false.
            /// \param[in] cancelRunningJobs true == Cancel running jobs.
            virtual void Pause (bool cancelRunningJobs = false);
            /// \brief
            /// Continue the run loop execution. If the run loop is not paused, noop.
            virtual void Continue ();
            /// \brief
            /// Return true if the run loop is paused.
            /// \return true == the run loop is paused.
            virtual bool IsPaused ();

            /// \brief
            /// Start waiting for jobs.
            virtual void Start () = 0;
            /// \brief
            /// Stop the run loop and in all likelihood exit the thread hosting it.
            /// Obviously, this function needs to be called from a different thread
            /// than the one that called Start.
            /// \param[in] cancelRunningJobs true = Cancel all running jobs.
            /// \param[in] cancelPendingJobs true = Cancel all pending jobs.
            virtual void Stop (
                bool /*cancelRunningJobs*/ = true,
                bool /*cancelPendingJobs*/ = true) = 0;

            /// \brief
            /// Enqueue a job to be performed on the run loop thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// NOTE: Same constraint applies to EnqJob as Stop. Namely, you can't call EnqJob
            /// from the same thread that called Start.
            /// \return true == !wait || WaitForJob (...)
            virtual bool EnqJob (
                Job::Ptr job,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Enqueue a job to be performed next on the run loop thread.
            /// \param[in] job Job to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// NOTE: Same constraint applies to EnqJob as Stop. Namely, you can't call EnqJob
            /// from the same thread that called Start.
            /// \return true == !wait || WaitForJob (...)
            virtual bool EnqJobFront (
                Job::Ptr job,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);

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
                virtual bool operator () (Job & /*job*/) const throw () = 0;
            };

            /// \brief
            /// Get a running or a pending job with the given id.
            /// \param[in] jobId Id of job to retrieve.
            /// \return Job matching the given id.
            virtual Job::Ptr GetJob (const Job::Id &jobId);
            /// \brief
            /// Get all running and pending jobs matching the given equality test.
            /// \param[in] equalityTest \see{EqualityTest} to query to determine the matching jobs.
            /// \param[out] jobs \see{UserJobList} containing the matching jobs.
            virtual void GetJobs (
                const EqualityTest &equalityTest,
                UserJobList &jobs);
            /// \brief
            /// Get all pending jobs.
            /// \param[out] pendingJobs \see{UserJobList} containing pending jobs.
            virtual void GetPendingJobs (UserJobList &pendingJobs);
            /// \brief
            /// Get all running jobs.
            /// \param[out] runningJobs \see{UserJobList} containing running jobs.
            virtual void GetRunningJobs (UserJobList &runningJobs);
            /// \brief
            /// Get all running and pending jobs. pendingJobs and runningJobs can be the same
            /// UserJobList. In that case first n jobs will be pending and the final m jobs
            /// will be running.
            /// \param[out] pendingJobs \see{UserJobList} containing pending jobs.
            /// \param[out] runningJobs \see{UserJobList} containing running jobs.
            virtual void GetAllJobs (
                UserJobList &pendingJobs,
                UserJobList &runningJobs);

            // NOTE for all Wait* methods below: If threads are waiting on pending jobs
            // indefinitely and another thread calls Stop (..., false) or Pause () then
            // the waiting threads will be blocked until you call Start () or Continue ().
            // This is a feature, not a bug. It allows you to suspend run loop execution
            // temporarily without affecting waiters.

            /// \brief
            /// Wait for a given job to complete.
            /// \param[in] job Job to wait on.
            /// NOTE: The RunLoop will check that the given job was in fact
            /// en-queued on this run loop (Job::runloopId) and will throw
            /// THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL if it doesn't.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == completed, false == timed out.
            virtual bool WaitForJob (
                Job::Ptr job,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Wait for a running or a pending job with a given id to complete.
            /// \param[in] jobId Id of job to wait on.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == completed,
            /// false == job with a given id was not in the run loop or timed out.
            virtual bool WaitForJob (
                const Job::Id &jobId,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Wait for all given running and pending jobs.
            /// \param[in] jobs \see{UserJobList} containing the jobs to wait on.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == All jobs satisfying the equalityTest completed,
            /// false == One or more matching jobs timed out.
            /// NOTE: This is a static method and is designed to allow you to
            /// wait on a collection of jobs without regard as to which run loop
            /// they're running on.
            static bool WaitForJobs (
                const UserJobList &jobs,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Wait for all running and pending jobs matching the given equality test to complete.
            /// \param[in] equalityTest \see{EqualityTest} to query to determine which jobs to wait on.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == All jobs satisfying the equalityTest completed,
            /// false == One or more matching jobs timed out.
            virtual bool WaitForJobs (
                const EqualityTest &equalityTest,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Blocks until paused or all jobs are complete and the run loop is empty.
            /// IMPORTANT: See VERY IMPORTANT comment in \see{Pause} (above).
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == RunLoop is idle, false == Timed out.
            virtual bool WaitForIdle (const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Cancel a running or a pending job with a given id.
            /// \param[in] jobId Id of job to cancel.
            /// \return true if the job was cancelled.
            virtual bool CancelJob (const Job::Id &jobId);
            /// \brief
            /// Cancel the list of given jobs.
            /// \param[in] jobs List of jobs to cancel.
            /// NOTE: This is a static method and is designed to allow you to
            /// cancel a collection of jobs without regard as to which run loop
            /// they're running on.
            static void CancelJobs (const UserJobList &jobs);
            /// \brief
            /// Cancel all running and pending jobs matching the given equality test.
            /// \param[in] equalityTest \see{EqualityTest} to query to determine which jobs to cancel.
            virtual void CancelJobs (const EqualityTest &equalityTest);
            /// \brief
            /// Cancel all pending jobs.
            virtual void CancelPendingJobs ();
            /// \brief
            /// Cancel all running jobs.
            virtual void CancelRunningJobs ();
            /// \brief
            /// Cancel all running and pending jobs.
            virtual void CancelAllJobs ();

            /// \brief
            /// Return a snapshot of the run loop stats.
            /// \return A snapshot of the run loop stats.
            virtual Stats GetStats ();
            /// \brief
            /// Reset the run loop stats.
            virtual void ResetStats ();

            /// \brief
            /// Return true if Start was called.
            /// \return true if Start was called.
            virtual bool IsRunning ();
            /// \brief
            /// Return true if there are no running or pending jobs.
            /// IMPORTANT: See VERY IMPORTANT comment in \see{Pause} (above).
            /// \return true = idle, false = busy.
            virtual bool IsIdle ();

        protected:
            /// \brief
            /// Used internally by worker(s) to get the next job.
            /// \param[in] wait true == Wait until a job becomes available.
            /// \return The next job to execute.
            Job *DeqJob (bool wait = true);
            /// \brief
            /// Called by worker(s) after each job is done.
            /// Used to update state and \see{RunLoop::Stats}.
            /// \param[in] job Completed job.
            /// \param[in] start Completed job start time.
            /// \param[in] end Completed job end time.
            void FinishedJob (
                Job *job,
                ui64 start,
                ui64 end);
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RunLoop_h)
