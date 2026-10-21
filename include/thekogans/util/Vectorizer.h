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

#if !defined (__thekogans_util_Vectorizer2_h)
#define __thekogans_util_Vectorizer2_h

#include <cstddef>
#include <atomic>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/SystemInfo.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/Thread2.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/Barrier.h"

namespace thekogans {
    namespace util {

        /// \struct Vectorizer Vectorizer.h thekogans/util/Vectorizer.h
        ///
        /// \brief
        /// Implements a very simple and convenient array vectorizer. Adhering to
        /// the age old adage that a picture is worth a thousands words, here is a
        /// canonical example illustrating the intended usage:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// std::vector<blas::Point3> result;
        /// std::vector<blas::Point3> vertices;
        /// blas::Matrix3 xform;
        /// \endcode
        ///
        /// Traditional loop:
        ///
        /// \code{.cpp}
        /// for (std::size_t i = 0, count = vertices.size (); i < count; ++i) {
        ///     result[i] = vertices[i] * xform;
        /// }
        /// \endcode
        ///
        /// Vectorized loop:
        ///
        /// \code{.cpp}
        /// struct XformVerticesJob : public util::Vectorizer::Job {
        /// private:
        ///     std::vector<blas::Point3> &result;
        ///     const std::vector<blas::Point3> &vertices;
        ///     const blas::Matrix3 &xform;
        ///
        /// public:
        ///     XformVerticesJob (
        ///         std::vector<blas::Point3> &result_,
        ///         const std::vector<blas::Point3> &vertices_,
        ///         const blas::Matrix3 &xform_) :
        ///         result (result_),
        ///         vertices (vertices_),
        ///         xform (xform_) {}
        ///
        ///     virtual void Execute (
        ///             std::size_t startIndex,
        ///             std::size_t endIndex,
        ///             std::size_t /*rank*/) noexcept {
        ///         for (; startIndex < endIndex; ++startIndex) {
        ///             result[startIndex] = vertices[startIndex] * xform;
        ///         }
        ///     }
        ///
        ///     virtual std::size_t Size () const {
        ///         return vertices.size ();
        ///     }
        /// } job (result, vertices, xform);
        /// util::Vectorizer::Instance ()->Execute (job);
        /// \endcode
        struct _LIB_THEKOGANS_UTIL_DECL Vectorizer {
            /// \struct Vectorizer::Job Vectorizer.h thekogans/util/Vectorizer.h
            ///
            /// \brief
            /// Vectorizer job.
            /// Vectorizer implements a three step protocol during job execution:
            ///      1. Call Prolog to allow the job to initialize any internal state.
            ///      2. Release all threads which will then call Execute with
            ///         appropriate chunks.
            ///      3. After all threads finish, call Epilog to allow the job
            ///         to cleanup internal state.
            /// VERY IMPORTANT: In order to minimize the time (not to
            /// mention possible exception safety issues) that Vectorizer
            /// spends on overhead, the Vectorizer does not make copies
            /// of the job passed to Vectorizer::Execute (every worker
            /// thread calls Job::Execute with the same this pointer).
            /// This design decision requires that Job::Execute be
            /// thread safe (re-entrant).
            struct Job {
                /// \brief
                /// Virtual dtor.
                virtual ~Job () {}
                /// \brief
                /// Called before the job is vectorized. This api
                /// implements the scatter part of scatter/gather.
                /// Use it to initialize the space where partial
                /// results will be stored by each stage.
                /// \param[in] chunks Number of chunks this job will be broken up in to.
                virtual void Prolog (std::size_t /*chunks*/) noexcept {}
                /// \brief
                /// Called by each worker with appropriate chunk range.
                /// \param[in] startIndex Vector index where execution begins.
                /// \param[in] endIndex Vector index where execution ends.
                /// \param[in] rank Index of the vector slot (use it to stash partial results).
                /// NOTE: That empty throw spec is there for a reason. It's to remind you,
                /// the vectorizer user, that Execute should never leak exceptions.
                virtual void Execute (
                    std::size_t /*startIndex*/,
                    std::size_t /*endIndex*/,
                    std::size_t /*rank*/) noexcept = 0;
                /// \brief
                /// Called after the job is vectorized. This api
                /// implements the gather part of scatter/gather.
                virtual void Epilog () noexcept {}
                /// \brief
                /// Return total size of job (usually std::vector::size ()).
                /// \return Size of job.
                virtual std::size_t Size () const noexcept = 0;
            };

            /// \brief
            /// ctor. Initialize the workers array, and start waiting for jobs.
            /// \param[in] workerCount_ The width of the vector.
            /// \param[in] workerPriority Worker thread priority.
            Vectorizer (
                std::size_t workerCount_ = SystemInfo::Instance ()->GetCPUCount (),
                i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY);
            /// \brief
            /// dtor.
            ~Vectorizer ();

            /// \brief
            /// In order to provide fine grained control over job
            /// chunking (and because applications know the complexity
            /// of their own jobs), Execute takes a chunkSize_ parameter.
            /// This parameter allows the job to hide the Vectorizer
            /// latency by scheduling fewer workers to do more work.
            /// \param[in] job_ Job to parallelize.
            /// \param[in] chunkSize_ Optional worker chunk size.
            /// NOTE: Vectorizer::Execute is synchronous.
            void Execute (
                Job &job_,
                std::size_t chunkSize_ = SIZE_T_MAX);

        private:
            /// \brief
            /// Flag to signal the worker thread.
            std::atomic<bool> done;
            /// \brief
            /// Synchronization lock.
            Mutex mutex;
            /// \brief
            /// Used to synchronize vectorizer workers.
            Barrier barrier;
            /// \brief
            /// Vectorizer workers.
            std::vector<Thread2> workers;
            // These variables hold the state of the vectorized job.
            /// \brief
            /// Currently executed job.
            Job *job;
            /// \brief
            /// Number of workers that should execute this job.
            std::size_t workerCount;
            /// \brief
            /// Chunk size each worker should execute.
            std::size_t chunkSize;
        };

        struct _LIB_THEKOGANS_UTIL_DECL GlobalVectorizer :
                public Vectorizer,
                public Singleton<GlobalVectorizer> {
            /// \brief
            /// ctor. Initialize the workers array, and start waiting for jobs.
            /// \param[in] workerCount The width of the vector.
            /// \param[in] workerPriority Worker thread priority.
            GlobalVectorizer (
                std::size_t workerCount = SystemInfo::Instance ()->GetCPUCount (),
                i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY) :
                Vectorizer (workerCount, workerPriority) {}
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Vectorizer_h)
