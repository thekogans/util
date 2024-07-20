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

#include "thekogans/util/Environment.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsHeader.h"
    #include <objbase.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include <string>
#include <list>
#include <functional>
#include <atomic>
#include "pugixml/pugixml.hpp"
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/SizeT.h"
#include "thekogans/util/Serializable.h"
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
        /// RunLoop is an abstract base for \see{JobQueue}, \see{ThreadRunLoop} and
        /// \see{SystemRunLoop}. RunLoop allows you to schedule jobs (RunLoop::Job)
        /// and c++ closures (lambdas) to be executed on the thread that's running
        /// the run loop.

        struct _LIB_THEKOGANS_UTIL_DECL RunLoop : public virtual RefCounted {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (RunLoop)

            /// \brief
            /// Convenient typedef for std::string.
            typedef std::string Id;

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

        #if defined (TOOLCHAIN_COMPILER_cl)
            #pragma warning (push)
            #pragma warning (disable : 4275)
        #endif // defined (TOOLCHAIN_COMPILER_cl)
            /// \struct RunLoop::Job RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// A RunLoop::Job must, at least, implement the Execute method.
            struct _LIB_THEKOGANS_UTIL_DECL Job :
                    public virtual RefCounted,
                    public JobList::Node {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Job)

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
                void Succeed (const std::atomic<bool> &done);

                /// \brief
                /// Return true if the job should stop what it's doing and exit.
                /// Use this method in your implementation of Execute to keep the
                /// RunLoop responsive.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                /// \return true == Job should stop what it's doing and exit.
                inline bool ShouldStop (const std::atomic<bool> &done) const {
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
                virtual void Prologue (const std::atomic<bool> & /*done*/) throw () {}
                /// \brief
                /// Called to execute the job. This is the only api
                /// the job MUST implement.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Execute (const std::atomic<bool> & /*done*/) throw () = 0;
                /// \brief
                /// Called after the job is executed. Allows
                /// the job to perform one time cleanup.
                /// \param[in] done If true, this flag indicates that
                /// the job should stop what it's doing, and exit.
                virtual void Epilogue (const std::atomic<bool> & /*done*/) throw () {}

                /// \brief
                /// RunLoop needs acces to protected members.
                friend struct RunLoop;
                /// \brief
                /// ThreadRunLoop needs acces to protected members.
                friend struct ThreadRunLoop;
                /// \brief
                /// SystemRunLoop needs acces to protected members.
                template<typename OSRunLoop>
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
        #if defined (TOOLCHAIN_COMPILER_cl)
            #pragma warning (pop)
        #endif // defined (TOOLCHAIN_COMPILER_cl)

            /// \struct RunLoop::LambdaJob RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// A helper class used to execute lambda (function) jobs.

            struct _LIB_THEKOGANS_UTIL_DECL LambdaJob : public Job {
                /// \brief
                /// Convenient typedef for std::function<void (Job & /*job*/,
                /// const std::atomic<bool> & /*done*/)>.
                /// \param[in] job Job that is executing the lambda.
                /// \param[in] done Call job.ShouldStop (done) to respond to
                /// cancel requests and termination events.
                typedef std::function<void (
                    const LambdaJob & /*job*/,
                    const std::atomic<bool> & /*done*/)> Function;

            private:
                /// \brief
                /// Lambda to execute.
                Function function;

            public:
                /// \brief
                /// ctor.
                /// \param[in] function_ Lambda to execute.
                explicit LambdaJob (const Function &function_) :
                        function (function_) {
                    if (function == 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }

                /// \brief
                /// If our run loop is still running, execute the lambda function.
                /// \param[in] done true == The run loop is done and nothing can be executed on it.
                virtual void Execute (const std::atomic<bool> &done) throw () override {
                    if (!ShouldStop (done)) {
                        function (*this, done);
                    }
                }

                /// \ brief
                /// This method exposes the ShoulStop machinery to the lambda.
                /// \param[in] done true == The run loop is done and nothing can be executed on it.
                /// \return true == Continue executing, false == Stop and return.
                inline bool IsRunning (const std::atomic<bool> &done) const {
                    return !ShouldStop (done);
                }
            };

            struct State;

            /// \struct RunLoop::JobExecutionPolicy RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// JobExecutionPolicy allows the user of the run loop to
            /// specify the order in which jobs will be executed.
            /// **********************************************************
            /// IMPORTANT: JobExecutionPolicy api is designed to take
            /// a RunLoop::State and not a job list on which to enqueue
            /// (or from which to dequeue) the given job. This is done
            /// so that the various methods can access RunLoop state if
            /// they need it. It's important that they enqueue/dequeue
            /// these jobs to the pendingJobs list of the given RunLoop
            /// as other RunLoop apis rely on this list to contain the
            /// pending jobs. The various policies are welcome to maintain
            /// a map in to this list to speed up location and retrieval
            /// of the next job to dequeue.
            /// **********************************************************
            struct _LIB_THEKOGANS_UTIL_DECL JobExecutionPolicy : public virtual RefCounted {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (JobExecutionPolicy)

                /// \brief
                /// Max pending run loop jobs.
                const std::size_t maxJobs;

                /// \brief
                /// ctor.
                /// \param[in] maxJobs_ Max pending run loop jobs.
                JobExecutionPolicy (std::size_t maxJobs_ = SIZE_T_MAX);
                /// \brief
                /// dtor.
                virtual ~JobExecutionPolicy () {}

                /// \brief
                /// Enqueue a job on the given RunLoops pendingJobs to be performed
                /// on the run loop thread.
                /// \param[in] runLoop RunLoop on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJob (
                    State &state,
                    Job *job) = 0;
                /// \brief
                /// Enqueue a job on the given RunLoops pendingJobs to be performed
                /// next on the run loop thread.
                /// \param[in] runLoop RunLoop on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJobFront (
                    State &state,
                    Job *job) = 0;
                /// \brief
                /// Dequeue the next job to be executed on the run loop thread.
                /// \param[in] runLoop RunLoop from which to dequeue the next job.
                /// \return The next job to execute (0 if no more pending jobs).
                virtual Job *DeqJob (State &state) = 0;
            };

            /// \struct RunLoop::FIFOJobExecutionPolicy RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// First In, First Out execution policy.
            struct _LIB_THEKOGANS_UTIL_DECL FIFOJobExecutionPolicy : public JobExecutionPolicy {
                /// \brief
                /// ctor.
                /// \param[in] maxJobs Max pending run loop jobs.
                FIFOJobExecutionPolicy (std::size_t maxJobs = SIZE_T_MAX) :
                    JobExecutionPolicy (maxJobs) {}

                /// \brief
                /// Enqueue a job on the given RunLoops pendingJobs to be performed
                /// on the run loop thread.
                /// \param[in] runLoop RunLoop on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJob (
                    State &state,
                    Job *job) override;
                /// \brief
                /// Enqueue a job on the given RunLoops pendingJobs to be performed
                /// next on the run loop thread.
                /// \param[in] runLoop RunLoop on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJobFront (
                    State &state,
                    Job *job) override;
                /// \brief
                /// Dequeue the next job to be executed on the run loop thread.
                /// \param[in] runLoop RunLoop from which to dequeue the next job.
                /// \return The next job to execute (0 if no more pending jobs).
                virtual Job *DeqJob (State &state) override;
            };

            /// \struct RunLoop::LIFOJobExecutionPolicy RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Last In, First Out execution policy.
            struct _LIB_THEKOGANS_UTIL_DECL LIFOJobExecutionPolicy : public JobExecutionPolicy {
                /// \brief
                /// ctor.
                /// \param[in] maxJobs Max pending run loop jobs.
                LIFOJobExecutionPolicy (std::size_t maxJobs = SIZE_T_MAX) :
                    JobExecutionPolicy (maxJobs) {}

                /// \brief
                /// Enqueue a job on the given RunLoops pendingJobs to be performed
                /// on the run loop thread.
                /// \param[in] runLoop RunLoop on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJob (
                    State &state,
                    Job *job) override;
                /// \brief
                /// Enqueue a job on the given RunLoops pendingJobs to be performed
                /// next on the run loop thread.
                /// \param[in] runLoop RunLoop on which to enqueue the given job.
                /// \param[in] job Job to enqueue.
                virtual void EnqJobFront (
                    State &state,
                    Job *job) override;
                /// \brief
                /// Dequeue the next job to be executed on the run loop thread.
                /// \param[in] runLoop RunLoop from which to dequeue the next job.
                /// \return The next job to execute (0 if no more pending jobs).
                virtual Job *DeqJob (State &state) override;
            };

            /// \brief
            /// Convenient typedef for std::list<Job::SharedPtr>.
            typedef std::list<Job::SharedPtr> UserJobList;

            /// \struct RunLoop::Stats RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// RunLoop statistics.\n
            /// totalJobs - Number of retired (completed) jobs.\n
            /// totalJobTime - Amount of time spent executing jobs.\n
            /// last - Last job.\n
            /// min - Fastest job.\n
            /// max - Slowest job.\n
            struct _LIB_THEKOGANS_UTIL_DECL Stats : public Serializable {
                /// \brief
                /// RunLoop::Stats is a \see{Serializable}.
                THEKOGANS_UTIL_DECLARE_SERIALIZABLE (Stats)

                /// \brief
                /// Run loop id.
                RunLoop::Id id;
                /// \brief
                /// Run loop name.
                std::string name;
                /// \brief
                /// Total jobs processed.
                SizeT totalJobs;
                /// \brief
                /// Total time taken to process totalJobs.
                ui64 totalJobTime;
                /// \struct RunLoop::Stats::Job RunLoop.h thekogans/util/RunLoop.h
                ///
                /// \brief
                /// Job stats.
                struct _LIB_THEKOGANS_UTIL_DECL Job : public Serializable {
                    /// \brief
                    /// RunLoop::Stats::Job is a \see{Serializable}.
                    THEKOGANS_UTIL_DECLARE_SERIALIZABLE (Job)

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
                    /// ctor.
                    /// \param[in] job Job to copy.
                    Job (const Job &job) :
                        id (job.id),
                        startTime (job.startTime),
                        endTime (job.endTime),
                        totalTime (job.totalTime) {}

                    /// \brief
                    /// Assignment operator.
                    /// \param[in] job Job to assign.
                    /// \retunr *this.
                    Job &operator = (const Job &job);

                    /// \brief
                    /// Reset the job stats.
                    void Reset ();

                    // Serializable
                    /// \brief
                    /// Return the serialized key size.
                    /// \return Serialized key size.
                    virtual std::size_t Size () const override;

                    /// \brief
                    /// Read the key from the given serializer.
                    /// \param[in] header \see{Serializable::BinHeader}.
                    /// \param[in] serializer \see{Serializer} to read the key from.
                    virtual void Read (
                        const BinHeader & /*header*/,
                        Serializer &serializer) override;
                    /// \brief
                    /// Write the key to the given serializer.
                    /// \param[out] serializer \see{Serializer} to write the key to.
                    virtual void Write (Serializer &serializer) const override;

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
                    /// Read the Serializable from an XML DOM.
                    /// \param[in] header \see{Serializable::TextHeader}.
                    /// \param[in] node XML DOM representation of a Serializable.
                    virtual void Read (
                        const TextHeader & /*header*/,
                        const pugi::xml_node &node) override;
                    /// \brief
                    /// Write the Serializable to the XML DOM.
                    /// \param[out] node Parent node.
                    virtual void Write (pugi::xml_node &node) const override;

                    /// \brief
                    /// Read a Serializable from an JSON DOM.
                    /// \param[in] node JSON DOM representation of a Serializable.
                    virtual void Read (
                        const TextHeader & /*header*/,
                        const JSON::Object &object) override;
                    /// \brief
                    /// Write a Serializable to the JSON DOM.
                    /// \param[out] node Parent node.
                    virtual void Write (JSON::Object &object) const override;
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
                /// \param[in] id_ Run loop id.
                /// \param[in] name_ Run loop name.
                Stats (
                    const RunLoop::Id &id_ = RunLoop::Id (),
                    const std::string &name_ = std::string ()) :
                    id (id_),
                    name (name_),
                    totalJobs (0),
                    totalJobTime (0) {}
                /// brief
                /// ctor.
                /// \parma[in] stats Stats to copy.
                Stats (const Stats &stats) :
                    id (stats.id),
                    name (stats.name),
                    totalJobs (stats.totalJobs),
                    totalJobTime (stats.totalJobTime),
                    lastJob (stats.lastJob),
                    minJob (stats.minJob),
                    maxJob (stats.maxJob) {}

                /// \brief
                /// Assignment operator.
                /// \param[in] stats Stats to assign.
                /// \retunr *this.
                Stats &operator = (const Stats &stats);

                /// \brief
                /// Reset the RunLoop stats.
                void Reset ();

                // Serializable
                /// \brief
                /// Return the serialized key size.
                /// \return Serialized key size.
                virtual std::size_t Size () const override;

                /// \brief
                /// Read the key from the given serializer.
                /// \param[in] header \see{Serializable::BinHeader}.
                /// \param[in] serializer \see{Serializer} to read the key from.
                virtual void Read (
                    const BinHeader & /*header*/,
                    Serializer &serializer) override;
                /// \brief
                /// Write the key to the given serializer.
                /// \param[out] serializer \see{Serializer} to write the key to.
                virtual void Write (Serializer &serializer) const override;

                /// \brief
                /// "RunLoop"
                static const char * const TAG_RUN_LOOP;
                /// \brief
                /// "Id"
                static const char * const ATTR_ID;
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
                /// Read the Serializable from an XML DOM.
                /// \param[in] header \see{Serializable::TextHeader}.
                /// \param[in] node XML DOM representation of a Serializable.
                virtual void Read (
                    const TextHeader & /*header*/,
                    const pugi::xml_node &node) override;
                /// \brief
                /// Write the Serializable to the XML DOM.
                /// \param[out] node Parent node.
                virtual void Write (pugi::xml_node &node) const override;

                /// \brief
                /// Read a Serializable from an JSON DOM.
                /// \param[in] node JSON DOM representation of a Serializable.
                virtual void Read (
                    const TextHeader & /*header*/,
                    const JSON::Object &object) override;
                /// \brief
                /// Write a Serializable to the JSON DOM.
                /// \param[out] node Parent node.
                virtual void Write (JSON::Object &object) const override;

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

                // WorkerCallback
                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw () override;
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw () override;
            };

            /// \struct RunLoop::OLEInitializer RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Initialize the Windows OLE library.
            struct _LIB_THEKOGANS_UTIL_DECL OLEInitializer : public WorkerCallback {
                // WorkerCallback
                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw () override;
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw () override;
            };
        #elif defined (TOOLCHAIN_OS_Linux)
            /// \struct RunLoop::XlibInitializer RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Initialize the Linux X library.
            struct _LIB_THEKOGANS_UTIL_DECL XlibInitializer : public WorkerCallback {
                // WorkerCallback
                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw () override;
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw () override;
            };
        #elif defined (TOOLCHAIN_OS_OSX)
            /// \struct RunLoop::NSApplicationInitializer RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Initialize the OS X NSApplication framework.
            struct _LIB_THEKOGANS_UTIL_DECL NSApplicationInitializer : public WorkerCallback {
                // WorkerCallback
                /// \brief
                /// Called by the worker before entering the job execution loop.
                virtual void InitializeWorker () throw () override;
                /// \brief
                /// Called by the worker before exiting the thread.
                virtual void UninitializeWorker () throw () override;
            };
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \struct RunLoop::WorkerInitializer RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Instantiate one of these in your RunLoop thread to initialize/uninitialize the worker.
            /// This is a convenience class meant to be instatiated on the stack of the worker thread.
            /// It's ctor will call WorkerCallback::InitializeWorker and it's dtor will call
            /// WorkerCallback::UninitializeWorker.
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

            /// \struct RunLoop::State RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Shared RunLoop state. RunLoop lifetime is unpredictable. RunLoops can
            /// be embedded in other objects making them difficult to use in multi-threaded
            /// environment (worker threads). A RunLoop state will always be allocated on
            /// the heap making managing it's lifetime a lot easier.
            struct _LIB_THEKOGANS_UTIL_DECL State : public virtual RefCounted {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (State)

                /// \brief
                /// State has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                /// \brief
                /// RunLoop id.
                const Id id;
                /// \brief
                /// RunLoop name.
                const std::string name;
                /// \brief
                /// RunLoop \see{JobExecutionPolicy}.
                JobExecutionPolicy::SharedPtr jobExecutionPolicy;
                /// \brief
                /// Flag to signal the worker thread(s).
                std::atomic<bool> done;
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

                /// \brief
                /// ctor.
                /// \param[in] name_ RunLoop name.
                /// \param[in] jobExecutionPolicy_ RunLoop \see{JobExecutionPolicy}.
                State (
                    const std::string &name_ = std::string (),
                    JobExecutionPolicy::SharedPtr jobExecutionPolicy_ = new FIFOJobExecutionPolicy);
                /// \brief
                /// dtor.
                virtual ~State ();

                /// \brief
                /// Used internally by worker(s) to get the next job.
                /// \param[in] wait true == Wait until a job becomes available.
                /// \return The next job to execute.
                Job *DeqJob (bool wait = true);
                /// \brief
                /// Called by worker(s) after each job is completed.
                /// Used to update state and \see{RunLoop::Stats}.
                /// \param[in] job Completed job.
                /// \param[in] start Completed job start time.
                /// \param[in] end Completed job end time.
                void FinishedJob (
                    Job *job,
                    ui64 start,
                    ui64 end);
            };

        protected:
            /// \brief
            /// Shared RunLoop state. The idea behind state is
            /// to get around potential lifetime issues that exist
            /// between run loop worker threads and the RunLoop object
            /// itself. Since RunLoop can be instantiated anywhere
            /// (including on the stack and as an aggregate of another
            /// class) it's important that the run loop workers are
            /// able to access run loop state even though the run loop
            /// itself could be gone. To that end RunLoop::State is
            /// always allocated on the heap and it's lifetime is
            /// controlled by the reference count encoded in it's
            /// State::SharedPtr. To allow the last worker to release
            /// the state, pass the State::SharedPtr to worker threads.
            State::SharedPtr state;

        public:
            /// \brief
            /// ctor.
            /// \param[in] name RunLoop name.
            /// \param[in] jobExecutionPolicy RunLoop \see{JobExecutionPolicy}.
            RunLoop (
                const std::string &name = std::string (),
                JobExecutionPolicy::SharedPtr jobExecutionPolicy = new FIFOJobExecutionPolicy) :
                state (new State (name, jobExecutionPolicy)) {}
            /// \brief
            /// ctor.
            /// \param[in] state_ Shared RunLoop state.
            explicit RunLoop (State::SharedPtr state_) :
                    state (state_) {
                if (state.Get () == 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }
            /// \brief
            /// dtor.
            virtual ~RunLoop () {}

            /// \brief
            /// Return RunLoop id.
            /// \return RunLoop id.
            inline const Id &GetId () const {
                return state->id;
            }
            /// \brief
            /// Return RunLoop name.
            /// \return RunLoop name.
            inline const std::string &GetName () const {
                return state->name;
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
            /// Pause run loop execution. Currently running jobs are allowed to finish,
            /// but no other pending jobs are executed until Continue is called. If the
            /// run loop is paused, noop.
            /// VERY IMPORTANT: A paused run loop does NOT imply idle. If you pause a
            /// run loop that has pending jobs, \see{IsIdle} (below) will return false.
            /// \param[in] cancelRunningJobs true == Cancel running jobs.
            /// \param[in] timeSpec How long to wait for the run loop to pause.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == Run loop paused. false == timed out.
            virtual bool Pause (
                bool cancelRunningJobs = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
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
            /// \param[in] cancelRunningJobs true == Cancel all running jobs.
            /// \param[in] cancelPendingJobs true == Cancel all pending jobs.
            virtual void Stop (
                bool /*cancelRunningJobs*/ = true,
                bool /*cancelPendingJobs*/ = true) = 0;
            /// \brief
            /// Return true if the run loop is running (Start was called).
            /// \return true if the run loop is running (Start was called).
            virtual bool IsRunning () = 0;

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
                Job::SharedPtr job,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Enqueue a lambda (function) to be performed on the run loop thread.
            /// \param[in] function Lambda to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// NOTE: Same constraint applies to EnqJob as Stop. Namely, you can't call EnqJob
            /// from the same thread that called Start.
            /// \return std::pair<Job::SharedPtr, bool> containing the LambdaJob and the EnqJob return.
            std::pair<Job::SharedPtr, bool> EnqJob (
                const LambdaJob::Function &function,
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
                Job::SharedPtr job,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Enqueue a lambda (function) to be performed next on the run loop thread.
            /// \param[in] function Lambda to enqueue.
            /// \param[in] wait Wait for job to finish. Used for synchronous job execution.
            /// \param[in] timeSpec How long to wait for the job to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// NOTE: Same constraint applies to EnqJob as Stop. Namely, you can't call EnqJob
            /// from the same thread that called Start.
            /// \return std::pair<Job::SharedPtr, bool> containing the LambdaJob and the EnqJobFront return.
            std::pair<Job::SharedPtr, bool> EnqJobFront (
                const LambdaJob::Function &function,
                bool wait = false,
                const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \struct RunLoop::EqualityTest RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// Equality test to use to determine which jobs to wait for and cancel.
            struct _LIB_THEKOGANS_UTIL_DECL EqualityTest {
                /// \brief
                /// dtor.
                virtual ~EqualityTest () {}

                /// \brief
                /// Reimplement this function to test for equality.
                /// \param[in] job Instance to test for equality.
                /// \return true == equal.
                virtual bool operator () (const Job & /*job*/) const throw () = 0;
            };

            /// \struct RunLoop::LambdaEqualityTest RunLoop.h thekogans/util/RunLoop.h
            ///
            /// \brief
            /// A helper class used to execute lambda (function) jobs.

            struct _LIB_THEKOGANS_UTIL_DECL LambdaEqualityTest : public EqualityTest {
                /// \brief
                /// Convenient typedef for;
                /// std::function<bool (const EqualityTest & /*equalityTest*/, const Job & /*job*/)>.
                /// \param[in] equalityTest EqualityTest that is executing the lambda.
                /// \param[in] job Job to compare for equality.
                /// \return true == equal, false == not equal.
                typedef std::function<bool (
                    const EqualityTest & /*equalityTest*/,
                    const Job & /*job*/)> Function;

            private:
                /// \brief
                /// Lambda to execute.
                Function function;

            public:
                /// \brief
                /// ctor.
                /// \param[in] function_ Lambda to execute.
                explicit LambdaEqualityTest (const Function &function_) :
                        function (function_) {
                    if (function == 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                    }
                }

                /// \brief
                /// Reimplement this function to test for equality.
                /// \param[in] job Instance to test for equality.
                /// \return true == equal.
                virtual bool operator () (const Job &job) const throw () override {
                    return function (*this, job);
                }
            };

            /// \brief
            /// Get a running or a pending job with the given id.
            /// \param[in] jobId Id of job to retrieve.
            /// \return Job matching the given id.
            virtual Job::SharedPtr GetJob (const Job::Id &jobId);
            /// \brief
            /// Get all running and pending jobs matching the given equality test.
            /// \param[in] equalityTest \see{EqualityTest} to query to determine the matching jobs.
            /// \param[out] jobs \see{UserJobList} containing the matching jobs.
            virtual void GetJobs (
                const EqualityTest &equalityTest,
                UserJobList &jobs);
            /// \brief
            /// Get all running and pending jobs matching the given equality test.
            /// \param[in] function \see{LambdaEqualityTest::Function} to query to determine the matching jobs.
            /// \param[out] jobs \see{UserJobList} containing the matching jobs.
            virtual void GetJobs (
                    const LambdaEqualityTest::Function &function,
                    UserJobList &jobs) {
                GetJobs (LambdaEqualityTest (function), jobs);
            }
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
                Job::SharedPtr job,
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
            /// \return true == All jobs completed before the deadline,
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
            /// Wait for all running and pending jobs matching the given equality test to complete.
            /// \param[in] function \see{LambdaEqualityTest::Function} to query to determine which jobs to wait on.
            /// \param[in] timeSpec How long to wait for the jobs to complete.
            /// IMPORTANT: timeSpec is a relative value.
            /// \return true == All jobs satisfying the equalityTest completed,
            /// false == One or more matching jobs timed out.
            virtual bool WaitForJobs (
                    const LambdaEqualityTest::Function &function,
                    const TimeSpec &timeSpec = TimeSpec::Infinite) {
                return WaitForJobs (LambdaEqualityTest (function), timeSpec);
            }
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
            /// Cancel all running and pending jobs matching the given equality test.
            /// \param[in] function \see{LambdaEqualityTest::Function} to query to determine which jobs to cancel.
            virtual void CancelJobs (const LambdaEqualityTest::Function &function) {
                CancelJobs (LambdaEqualityTest (function));
            }
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
            /// Return true if there are no running or pending jobs.
            /// IMPORTANT: See VERY IMPORTANT comment in \see{Pause} (above).
            /// \return true == idle, false == busy.
            virtual bool IsIdle ();
        };

        /// \brief
        /// Implement RunLoop::Stats::Job extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS (RunLoop::Stats::Job)

        /// \brief
        /// Implement RunLoop::Stats::Job value parser.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VALUE_PARSER (RunLoop::Stats::Job)

        /// \brief
        /// Implement RunLoop::Stats extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS (RunLoop::Stats)

        /// \brief
        /// Implement RunLoop::Stats value parser.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VALUE_PARSER (RunLoop::Stats)

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_RunLoop_h)
