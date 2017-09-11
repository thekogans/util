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

#if !defined (__thekogans_util_ChildProcess_h)
#define __thekogans_util_ChildProcess_h

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
#else // defined (TOOLCHAIN_OS_Windows)
    #include <unistd.h>
    #include <signal.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include <string>
#include <vector>
#include <list>
#include "thekogans/util/Config.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/JobQueue.h"
#include "thekogans/util/Buffer.h"

namespace thekogans {
    namespace util {

        /// \struct ChildProcess ChildProcess.h thekogans/util/ChildProcess.h
        ///
        /// \brief
        /// Implements a platform independent child process.
        ///
        /// APIs for command line arguments, and child process
        /// environment variables are provided. Also the parent can
        /// hook the child's stdio to create a pipe to the child.
        /// The parent can wait for the child to finish or detach it,
        /// and let it run, and manage it's own lifetime. Here is an
        /// example of using ffmpeg to extract a representative frame
        /// for a video file:
        ///
        /// \code
        /// thekogans::util::ChildProcess ffmpegProcess (path to ffmpeg(.exe));
        /// #if defined (TOOLCHAIN_CONFIG_Release)
        /// ffmpegProcess.AddArgument ("-loglevel");
        /// ffmpegProcess.AddArgument ("quiet");
        /// #endif // defined (TOOLCHAIN_CONFIG_Release)
        /// ffmpegProcess.AddArgument ("-intra");
        /// ffmpegProcess.AddArgument ("-ss");
        /// ffmpegProcess.AddArgument (
        ///     duration <= 10 ? "00:00:00" :
        ///     duration <= 1800 ? "00:00:10" :
        ///     duration <= 3600 ? "00:00:30" : "00:01:00");
        /// ffmpegProcess.AddArgument ("-vframes");
        /// ffmpegProcess.AddArgument ("1");
        /// ffmpegProcess.AddArgument ("-i");
        /// ffmpegProcess.AddArgument (path to video file);
        /// ffmpegProcess.AddArgument ("-y");
        /// ffmpegProcess.AddArgument ("-f");
        /// ffmpegProcess.AddArgument ("mjpeg");
        /// ffmpegProcess.AddArgument (path to extracted jpeg);
        /// thekogans::util::ChildProcess::ChildStatus childStatus = ffmpegProcess.Exec ();
        /// if (childStatus == thekogans::util::ChildProcess::Failed) {
        ///     report error
        /// }
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL ChildProcess {
            /// \brief
            /// Specify how to hook the child's standard io.
            enum {
                /// \brief
                /// Don't hook anything.
                HOOK_NONE = 0,
                /// \brief
                /// Hook only stdin.
                HOOK_STDIN = 1,
                /// \brief
                /// Hook only stdout.
                HOOK_STDOUT = 2,
                /// \brief
                /// Hook only stderr.
                HOOK_STDERR = 4,
                /// \brief
                /// Hook everything.
                HOOK_ALL = HOOK_STDIN | HOOK_STDOUT | HOOK_STDERR
            };

        protected:
            /// \brief
            /// Path to child process.
            std::string path;
            /// \brief
            /// Type of io to hook.
            ui32 hookStdIO;
            /// \brief
            /// Arguments to the child process.
            std::list<std::string> arguments;
            /// \brief
            /// Child process environment.
            std::list<std::string> environmentVariables;
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows composed child process command string.
            std::string commandLine;
            /// \brief
            /// Windows composed child process environment string.
            std::string environment;
            /// \brief
            /// Windows child process info.
            PROCESS_INFORMATION processInformation;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// POSIX child process id.
            THEKOGANS_UTIL_PROCESS_ID pid;
            /// \brief
            /// Return code for a process that ended normally.
            i32 returnCode;
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \struct ChildProcess::StdIO ChildProcess.h thekogans/util/ChildProcess.h
            ///
            /// \brief
            /// Holds the hooked std io pipes.
            struct _LIB_THEKOGANS_UTIL_DECL StdIO {
                /// \brief
                /// Convenient typedef for std::unique_ptr<StdIO>.
                typedef std::unique_ptr<StdIO> UniquePtr;

                /// \brief
                /// Type of io to hook.
                ui32 hookStdIO;
                /// \brief
                /// Pipes for stdin.
                THEKOGANS_UTIL_HANDLE inPipe[2];
                /// \brief
                /// Pipes for stdout.
                THEKOGANS_UTIL_HANDLE outPipe[2];
                /// \brief
                /// Pipes for stderr.
                THEKOGANS_UTIL_HANDLE errPipe[2];

                /// \brief
                /// ctor.
                /// \param[in] hookStdIO_ Flags to specify what if any stdio to hook.
                explicit StdIO (ui32 hookStdIO_);
                /// \brief
                /// dtor.
                ~StdIO ();

            #if !defined (TOOLCHAIN_OS_Windows)
                /// \brief
                /// After calling this function, the parent can use;\n
                /// - inPipe[1] = to write to child's stdin\n
                /// - outPipe[0] = to listen on child's stdout\n
                /// - errPipe[0] = to listen on child's stderr
                void SetupParent ();
                /// \brief
                /// Setup stdio on the child side.
                void SetupChild ();
            #endif // !defined (TOOLCHAIN_OS_Windows)
            };
            /// \brief
            /// Hooked std io info.
            StdIO::UniquePtr stdIO;

        public:
            /// \brief
            /// ctor.
            /// \param[in] path_ Path of the child process image.
            /// \param[in] hookStdIO_ Flags to specify what if any stdio to hook.
            explicit ChildProcess (
                const std::string &path_,
                ui32 hookStdIO_ = HOOK_NONE);
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// dtor.
            ~ChildProcess ();
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// Get the path associated with this child process.
            /// \return Path associated with this child process.
            inline const std::string &GetPath () const {
                return path;
            }
            /// \brief
            /// Get the type of standard io to hook.
            /// \return Type of standard io to hook.
            inline ui32 GetHookStdIO () const {
                return hookStdIO;
            }
            /// \brief
            /// Get the list of arguments associated with this child process.
            /// \return List of arguments associated with this child process.
            inline const std::list<std::string> &GetArguments () const {
                return arguments;
            }
            /// \brief
            /// Get the list of environment variables associated with this child process.
            /// \return List of environment variables associated with this child process.
            inline const std::list<std::string> &GetEnvironmentVariables () const {
                return environmentVariables;
            }

            /// \brief
            /// Return the stdin pipe to the child process.
            /// \return The pipe handle.\n
            /// NOTE: You can use this handle as a parameter
            /// to \see{TenantFile}.
            inline THEKOGANS_UTIL_HANDLE GetInPipe () const {
                return stdIO.get () != 0 ?
                    stdIO->inPipe[1] : THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
            }
            /// \brief
            /// Return the stdout pipe to the child process.
            /// \return The pipe handle.\n
            /// NOTE: You can use this handle as a parameter
            /// to \see{TenantFile}.
            inline THEKOGANS_UTIL_HANDLE GetOutPipe () const {
                return stdIO.get () != 0 ?
                    stdIO->outPipe[0] : THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
            }
            /// \brief
            /// Return the stderr pipe to the child process.
            /// \return The pipe handle.\n
            /// NOTE: You can use this handle as a parameter
            /// to \see{TenantFile}.
            inline THEKOGANS_UTIL_HANDLE GetErrPipe () const {
                return stdIO.get () != 0 ?
                    stdIO->errPipe[0] : THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
            }

            /// \brief
            /// Add an argument to the child process command line.
            /// \param[in] argument Argument to add.
            void AddArgument (const std::string &argument);
            /// \brief
            /// Set the list of arguments.
            /// \param[in] arguments_ List of arguments to set.
            inline void SetArguments (const std::list<std::string> &arguments_) {
                arguments = arguments_;
            }
            /// \brief
            /// Add an environment variable to the child process.
            /// \param[in] environmentVariable Variable to add.\n
            /// NOTE: Variables should be in the form of name=value
            void AddEnvironmentVariable (const std::string &environmentVariable);
            /// \brief
            /// Set the list of environment variables.
            /// \param[in] environmentVariables_ List of environment variables to set.
            inline void SetEnvironmentVariables (const std::list<std::string> &environmentVariables_) {
                environmentVariables = environmentVariables_;
            }

            /// \brief
            /// Return the command line which will be executed by this child process.
            /// \return Command line which will be executed by this child process.
            std::string GetCommandLine () const;

            /// \brief
            /// Async child process spawn.
            /// \param[in] detached true = detach the child process from the parent,
            /// \return Process id.
            THEKOGANS_UTIL_PROCESS_ID Spawn (bool detached = false);
            /// \enum
            /// Child process return status.
            enum ChildStatus {
                /// \brief
                /// Child process failed.
                Failed = -1,
                /// \brief
                /// Child process exited normally.
                Finished = 0,
            #if !defined (TOOLCHAIN_OS_Windows)
                /// \brief
                /// Child process was killed.
                Killed = 1
            #endif // !defined (TOOLCHAIN_OS_Windows)
            };
            /// \brief
            /// Wait for child process to complete.
            /// \param[in] timeSpec How long to wait for the child to exit.
            /// \return Child exit status.
            ChildStatus Wait (const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Convenience api. Calls Spawn followed by Wait (synchronous).
            /// \param[in] timeSpec How long to wait for the child to exit.
            /// \return Child exit status.
            ChildStatus Exec (const TimeSpec &timeSpec = TimeSpec::Infinite);
            /// \brief
            /// Send a signal to the child.
        #if defined (TOOLCHAIN_OS_Windows)
            /// \param[in] dummy Ignored.
            void Kill (int = 0);
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] sig Signal number to send. Defaults to SIGTERM.
            void Kill (int sig = SIGTERM);
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// Return the process id.
            /// \return Process id.
            inline THEKOGANS_UTIL_PROCESS_ID GetProcessId () const {
            #if defined (TOOLCHAIN_OS_Windows)
                return processInformation.dwProcessId;
            #else // defined (TOOLCHAIN_OS_Windows)
                return pid;
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            /// \brief
            /// Return the process exit code.
            /// \return Process exit code.
            i32 GetReturnCode () const;

            /// \brief
            /// Create a ChildProcess spawn job to be executed on the \see{MainRunLoop}.
            /// VERY IMPORTANT: This (and CreateExecJob bellow) are meant to be used with
            /// the \see{MainRunLoop}. Regardless you need to call Enq (..., true) on the
            /// queue that will execute the returned job. The reason is that the spawn job
            /// returned holds the reference to the child process and does not control
            /// it's lifetime in any way. Please see the code example provided with
            /// CollectOutput bellow.
            /// \param[in] detached true = detach the child process from the parent,
            /// \return JobQueue::Job representing the spawn job.
            JobQueue::Job::Ptr CreateSpawnJob (bool detached = false);
            /// \brief
            /// Create a ChildProcess exec job to be executed on the \see{MainRunLoop}.
            /// \return JobQueue::Job representing the exec job.
            JobQueue::Job::Ptr CreateExecJob (ChildStatus &status);

            enum {
                /// \brief
                /// Default \see{File::Read} chunk size.
                DEFAULT_CHUNK_SIZE = 1024
            };

            /// \brief
            /// Used in conjunction with CreateSpawnJob above for convenient child
            /// process launching and StdIO collection. Here's an example using ls:
            ///
            /// \code
            /// using namespace thekogans;
            ///
            /// util::ChildProcess lsProcess ("ls");
            /// util::MainRunLoop::Instance ().Enq (lsProcess.CreateSpawnJob (), true);
            /// if (lsProcess.GetProcessId () != THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE) {
            ///     util::Buffer::UniquePtr lsOutput = ls.CollectOutput (ls.GetOutPipe ());
            /// }
            /// else {
            ///     // Unable to spawn ls. Handle error.
            /// }
            /// \endcode
            ///
            /// NOTE: This technique works with either stdOut or stdErr. If your
            /// io needs are more involved (bidirectional?), this wont work as this
            /// function blocks until the child process has exited. In that case
            /// you'll need to use async io and use AsyncIoEvent[Queue | Sink] found
            /// in thekogans stream.
            /// \param[in] handle GetOutPipe () or GetErrPipe ().
            /// \param[in] chunkSize \see{File::Read) chunk size.
            /// \param[in] reap Reap the child process after collecting the output.
            /// \param[in] timeSpec If reap == true, indicates how long to wait for
            /// the child process to exit.
            /// \return Buffer containing the child's output.
            Buffer::UniquePtr CollectOutput (
                THEKOGANS_UTIL_HANDLE handle,
                ui32 chunkSize = DEFAULT_CHUNK_SIZE,
                bool reap = true,
                const TimeSpec &timeSpec = TimeSpec::Zero);

            /// \brief
            /// ChildProcess is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (ChildProcess)
        };

    #if !defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// Call this from main to daemonize the process.
        /// After it returns the process is a daemon.
        /// \param[in] userName Optional user name to run the daemon as.
        /// \param[in] directory Optional directory to change to upon daemonization.
        /// \param[in] lockFilePath Optional lock file to limit the daemon process
        /// to a single instance.
        /// \param[in] waitForChild How long should the parent process wait for
        /// the child to become a daemon (in seconds).
        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API Daemonize (
            const char *userName = 0,
            const char *directory = 0,
            const char *lockFilePath = 0,
            i32 waitForChild = 3);
    #endif // !defined (TOOLCHAIN_OS_Windows)

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_ChildProcess_h)
