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

#include "thekogans/util/Environment.h"
#if !defined (TOOLCHAIN_OS_Windows)
    #include <signal.h>
    #include <errno.h>
    #include <sys/wait.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <pwd.h>
    #if defined (TOOLCHAIN_OS_OSX)
        #include <libproc.h>
        #include <grp.h>
    #endif // defined (TOOLCHAIN_OS_OSX)
#endif // !defined (TOOLCHAIN_OS_Windows)
#include <cassert>
#include <cstdlib>
#include <vector>
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Flags.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/File.h"
#include "thekogans/util/JobQueue.h"
#include "thekogans/util/ChildProcess.h"

namespace thekogans {
    namespace util {

        ChildProcess::StdIO::StdIO (std::size_t hookStdIO_) :
                hookStdIO (hookStdIO_) {
            {
                inPipe[0] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                inPipe[1] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                if (Flags<std::size_t> (hookStdIO).Test (ChildProcess::HOOK_STDIN)) {
                    if (pipe (inPipe) < 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                #if defined (TOOLCHAIN_OS_Windows)
                    if (!SetHandleInformation (inPipe[1],
                            HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                #endif // defined (TOOLCHAIN_OS_Windows)
                }
            }
            {
                outPipe[0] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                outPipe[1] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                if (Flags<std::size_t> (hookStdIO).Test (ChildProcess::HOOK_STDOUT)) {
                    if (pipe (outPipe) < 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                #if defined (TOOLCHAIN_OS_Windows)
                    if (!SetHandleInformation (outPipe[0],
                            HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                #endif // defined (TOOLCHAIN_OS_Windows)
                }
            }
            {
                errPipe[0] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                errPipe[1] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                if (Flags<std::size_t> (hookStdIO).Test (ChildProcess::HOOK_STDERR)) {
                    if (pipe (errPipe) < 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                #if defined (TOOLCHAIN_OS_Windows)
                    if (!SetHandleInformation (errPipe[0],
                            HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                #endif // defined (TOOLCHAIN_OS_Windows)
                }
            }
        }

    #if !defined (TOOLCHAIN_OS_Windows)
        void ChildProcess::StdIO::SetupParent () {
            if (Flags<std::size_t> (hookStdIO).Test (ChildProcess::HOOK_STDIN)) {
                // inPipe[0]
                {
                    assert (inPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    close (inPipe[0]);
                    inPipe[0] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                }
                // inPipe[1]
                {
                    assert (inPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    // Used by parent to write to child's stdin.
                }
            }
            if (Flags<std::size_t> (hookStdIO).Test (ChildProcess::HOOK_STDOUT)) {
                // outPipe[0]
                {
                    assert (outPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    // Used by parent to listen for child's stdout.
                }
                // outPipe[1]
                {
                    assert (outPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    close (outPipe[1]);
                    outPipe[1] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                }
            }
            if (Flags<std::size_t> (hookStdIO).Test (ChildProcess::HOOK_STDERR)) {
                // errPipe[0]
                {
                    assert (errPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    // Used by parent to listen for child's stderr.
                }
                // errPipe[1]
                {
                    assert (errPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    close (errPipe[1]);
                    errPipe[1] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                }
            }
        }

        void ChildProcess::StdIO::SetupChild () {
            if (Flags<std::size_t> (hookStdIO).Test (ChildProcess::HOOK_STDIN)) {
                // inPipe[0]
                {
                    assert (inPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    if (dup2 (inPipe[0], STDIN_FILENO) < 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                    inPipe[0] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                }
                // inPipe[1]
                {
                    assert (inPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    close (inPipe[1]);
                    inPipe[1] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                }
            }
            if (Flags<std::size_t> (hookStdIO).Test (ChildProcess::HOOK_STDOUT)) {
                // outPipe[0]
                {
                    assert (outPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    close (outPipe[0]);
                    outPipe[0] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                }
                // outPipe[1]
                {
                    assert (outPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    if (dup2 (outPipe[1], STDOUT_FILENO) < 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                    outPipe[1] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                }
            }
            if (Flags<std::size_t> (hookStdIO).Test (ChildProcess::HOOK_STDERR)) {
                // errPipe[0]
                {
                    assert (errPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    close (errPipe[0]);
                    errPipe[0] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                }
                // errPipe[1]
                {
                    assert (errPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
                    if (dup2 (errPipe[1], STDERR_FILENO) < 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                    errPipe[1] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                }
            }
        }
    #endif // !defined (TOOLCHAIN_OS_Windows)

        ChildProcess::StdIO::~StdIO () {
        #if defined (TOOLCHAIN_OS_Windows)
            {
                if (inPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    CloseHandle (inPipe[0]);
                }
                if (inPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    CloseHandle (inPipe[1]);
                }
            }
            {
                if (outPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    CloseHandle (outPipe[0]);
                }
                if (outPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    CloseHandle (outPipe[1]);
                }
            }
            {
                if (errPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    CloseHandle (errPipe[0]);
                }
                if (errPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    CloseHandle (errPipe[1]);
                }
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            {
                if (inPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    close (inPipe[0]);
                }
                if (inPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    close (inPipe[1]);
                }
            }
            {
                if (outPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    close (outPipe[0]);
                }
                if (outPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    close (outPipe[1]);
                }
            }
            {
                if (errPipe[0] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    close (errPipe[0]);
                }
                if (errPipe[1] != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    close (errPipe[1]);
                }
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        ///////////////////////////////////////////////////////////////////////////////////

        namespace {
        #if defined (TOOLCHAIN_OS_Windows)
            void ClearProcessInformation (PROCESS_INFORMATION &processInformation) {
                ZeroMemory (&processInformation, sizeof (PROCESS_INFORMATION));
                processInformation.hProcess = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                processInformation.hThread = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                processInformation.dwProcessId = THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE;
                processInformation.dwThreadId = THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE;
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            void ClearProcessInformation (
                    pid_t &pid,
                    i32 &returnCode) {
                pid = THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE;
                returnCode = -1;
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        ChildProcess::ChildProcess (
                const std::string &path_,
                std::size_t hookStdIO_) :
                path (path_),
                hookStdIO (hookStdIO_) {
        #if defined (TOOLCHAIN_OS_Windows)
            ClearProcessInformation (processInformation);
        #else // defined (TOOLCHAIN_OS_Windows)
            ClearProcessInformation (pid, returnCode);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    #if defined (TOOLCHAIN_OS_Windows)
        ChildProcess::~ChildProcess () {
            if (processInformation.hProcess != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                CloseHandle (processInformation.hProcess);
            }
            if (processInformation.hThread != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                CloseHandle (processInformation.hThread);
            }
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        void ChildProcess::AddArgument (const std::string &argument) {
            if (!argument.empty ()) {
                arguments.push_back (argument);
            }
        }

        void ChildProcess::AddEnvironmentVariable (
                const std::string &environmentVariable) {
            if (!environmentVariable.empty ()) {
                environmentVariables.push_back (environmentVariable);
            }
        }

        std::string ChildProcess::BuildCommandLine () const {
            std::string commandLine = "\"" + path + "\"";
            for (std::list<std::string>::const_iterator
                    it = arguments.begin (),
                    end = arguments.end (); it != end; ++it) {
                commandLine += " " + *it;
            }
            return commandLine;
        }

        THEKOGANS_UTIL_PROCESS_ID ChildProcess::Spawn (bool detached) {
        #if defined (TOOLCHAIN_OS_Windows)
            if (path.empty ()) {
                return THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE;
            }
            commandLine = "\"" + path + "\"";
            for (std::list<std::string>::const_iterator it = arguments.begin (),
                    end = arguments.end (); it != end; ++it) {
                commandLine += " " + *it;
            }
            environment.clear ();
            if (!environmentVariables.empty ()) {
                for (std::list<std::string>::const_iterator
                        it = environmentVariables.begin (),
                        end = environmentVariables.end (); it != end; ++it) {
                    environment += *it + '\0';
                }
                environment += '\0';
            }
            STARTUPINFOW startInfo;
            ZeroMemory (&startInfo, sizeof (STARTUPINFOW));
            startInfo.cb = sizeof (STARTUPINFOW);
            if (hookStdIO != HOOK_NONE) {
                stdIO.reset (new StdIO (hookStdIO));
                startInfo.dwFlags |= STARTF_USESTDHANDLES;
                startInfo.hStdInput = stdIO->inPipe[0];
                startInfo.hStdOutput = stdIO->outPipe[1];
                startInfo.hStdError = stdIO->errPipe[1];
            }
            ClearProcessInformation (processInformation);
            std::wstring wcommandLine = UTF8ToUTF16 (commandLine);
            std::wstring wenvironment = UTF8ToUTF16 (environment);
            if (!CreateProcessW (0, &wcommandLine[0], 0, 0, TRUE,
                    CREATE_UNICODE_ENVIRONMENT | (detached ? DETACHED_PROCESS : 0),
                    !wenvironment.empty () ? &wenvironment[0] : 0,
                    !startupDirectory.empty () ? UTF8ToUTF16 (startupDirectory).c_str () : 0,
                    &startInfo,
                    &processInformation)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return processInformation.dwProcessId;
        #else // defined (TOOLCHAIN_OS_Windows)
            if (hookStdIO != HOOK_NONE) {
                stdIO.reset (new StdIO (hookStdIO));
            }
            std::vector<const char *> argv;
            argv.push_back (path.c_str ());
            for (std::list<std::string>::const_iterator
                    it = arguments.begin (),
                    end = arguments.end (); it != end; ++it) {
                argv.push_back ((*it).c_str ());
            }
            argv.push_back (0);
            std::vector<const char *> envp;
            if (!environmentVariables.empty ()) {
                for (std::list<std::string>::const_iterator
                        it = environmentVariables.begin (),
                        end = environmentVariables.end (); it != end; ++it) {
                    envp.push_back ((*it).c_str ());
                }
                envp.push_back (0);
            }
            ClearProcessInformation (pid, returnCode);
            // We can't use dup with vfork.
        #if defined (__APPLE__)
            pid = fork ();
        #else // defined (__APPLE__)
            pid = hookStdIO != HOOK_NONE ? fork () : vfork ();
        #endif // defined (__APPLE__)
            if (pid < 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            if (pid > 0) {
                // parent process
                if (hookStdIO != HOOK_NONE) {
                    assert (stdIO.get () != 0);
                    stdIO->SetupParent ();
                }
            }
            else {
                // child process
                THEKOGANS_UTIL_TRY {
                    if (detached) {
                        setsid ();
                    }
                    if (!startupDirectory.empty () && chdir (startupDirectory.c_str ()) < 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                    if (hookStdIO != HOOK_NONE) {
                        assert (stdIO.get () != 0);
                        stdIO->SetupChild ();
                    }
                    if (!envp.empty ()) {
                    #if defined (TOOLCHAIN_OS_OSX)
                        if (execve (
                                argv[0],
                                (char * const *)argv.data (),
                                (char * const *)envp.data ()) < 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE);
                        }
                    #else // defined (TOOLCHAIN_OS_OSX)
                        if (execvpe (
                                argv[0],
                                (char * const *)argv.data (),
                                (char * const *)envp.data ()) < 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE);
                        }
                    #endif // defined (TOOLCHAIN_OS_OSX)
                    }
                    else {
                        if (execvp (argv[0], (char * const *)argv.data ()) < 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE);
                        }
                    }
                }
                THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
                // If we got here that means execv... failed.
                // The best we can do here is log the error
                // and return it to the parent.
                exit (THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return pid;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    #if !defined (TOOLCHAIN_OS_Windows)
        namespace {
            struct Waiter : public Thread {
                pid_t pid;
                int *status;
                int options;
                Mutex mutex;
                Condition condition;

                Waiter (pid_t pid_,
                        int *status_,
                        int options_) :
                        pid (pid_),
                        status (status_),
                        options (options_),
                        condition (mutex) {
                    mutex.Acquire ();
                }
                ~Waiter () {
                    mutex.Release ();
                }

                inline bool IsExited () const {
                    return exited;
                }

                inline void Cancel () {
                    pthread_cancel (thread);
                }

                // Thread
                virtual void Run () throw () {
                    pid = waitpid (pid, status, options);
                    {
                        LockGuard<Mutex> guard (mutex);
                        condition.Signal ();
                    }
                }
            };

            pid_t waitpid_timed (
                    pid_t pid,
                    int *status,
                    int options,
                    const TimeSpec &timeSpec) {
                if (status != 0) {
                    Waiter waiter (pid, status, options);
                    waiter.Create (THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY);
                    if (!waiter.condition.Wait (timeSpec)) {
                        *status = ETIMEDOUT;
                        waiter.Cancel ();
                    }
                    // The following two lines will prevent a
                    // potential deadlock that can result if the above
                    // waiter.condition.Wait returns before Run had a
                    // chance to execute.
                    waiter.mutex.TryAcquire ();
                    waiter.mutex.Release ();
                    waiter.Wait ();
                    return waiter.pid;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                }
            }

            #define WIFTIMEDOUT(status) ((status & 0xff) == ETIMEDOUT)
        }
    #endif // !defined (TOOLCHAIN_OS_Windows)

        ChildProcess::ChildStatus ChildProcess::Wait (const TimeSpec &timeSpec) {
        #if defined (TOOLCHAIN_OS_Windows)
            DWORD result = WaitForSingleObject (
                processInformation.hProcess,
                (DWORD)timeSpec.ToMilliseconds ());;
            return
                result == WAIT_OBJECT_0 ? Finished :
                result == WAIT_TIMEOUT ? TimedOut : Failed;
        #else // defined (TOOLCHAIN_OS_Windows)
            if (pid != THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE) {
                int status;
                pid_t wpid = timeSpec == TimeSpec::Infinite ?
                    waitpid (pid, &status, WUNTRACED) :
                    waitpid_timed (pid, &status, WUNTRACED, GetCurrentTime () + timeSpec);
                pid = THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE;
                returnCode = -1;
                if (wpid == THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE) {
                    return Failed;
                }
                if (WIFEXITED (status)) {
                    returnCode = WEXITSTATUS (status);
                    return Finished;
                }
                if (WIFTIMEDOUT (status)) {
                    return TimedOut;
                }
                if (WIFSIGNALED (status)) {
                    return Killed;
                }
            }
            return Failed;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        ChildProcess::ChildStatus ChildProcess::Exec (const TimeSpec &timeSpec) {
            return Spawn () != THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE ? Wait (timeSpec) : Failed;
        }

        void ChildProcess::Kill (int sig) {
        #if defined (TOOLCHAIN_OS_Windows)
            if (!TerminateProcess (processInformation.hProcess, 0)) {
        #else // defined (TOOLCHAIN_OS_Windows)
            if (kill (pid, sig) < 0) {
        #endif // defined (TOOLCHAIN_OS_Windows)
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        }

        i32 ChildProcess::GetReturnCode () const {
        #if defined (TOOLCHAIN_OS_Windows)
            DWORD exitCode = 0;
            if (!GetExitCodeProcess (processInformation.hProcess, &exitCode)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return (i32)exitCode;
        #else // defined (TOOLCHAIN_OS_Windows)
            return returnCode;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        RunLoop::Job::SharedPtr ChildProcess::CreateSpawnJob (bool detached) {
            struct SpawnJob : public RunLoop::Job {
                ChildProcess &childProcess;
                bool detached;
                SpawnJob (
                    ChildProcess &childProcess_,
                    bool detached_) :
                    childProcess (childProcess_),
                    detached (detached_) {}
                // RunLoop::Job
                virtual void Execute (const std::atomic<bool> &done) throw () {
                    if (!ShouldStop (done)) {
                        THEKOGANS_UTIL_TRY {
                            childProcess.Spawn (detached);
                        }
                        THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
                    }
                }
            };
            return RunLoop::Job::SharedPtr (new SpawnJob (*this, detached));
        }

        RunLoop::Job::SharedPtr ChildProcess::CreateExecJob (ChildStatus &status) {
            struct ExecJob : public RunLoop::Job {
                ChildProcess &childProcess;
                ChildStatus &status;
                ExecJob (
                    ChildProcess &childProcess_,
                    ChildStatus &status_) :
                    childProcess (childProcess_),
                    status (status_) {}
                // RunLoop::Job
                virtual void Execute (const std::atomic<bool> &done) throw () {
                    if (!ShouldStop (done)) {
                        THEKOGANS_UTIL_TRY {
                            status = childProcess.Exec ();
                        }
                        THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
                    }
                }
            };
            return RunLoop::Job::SharedPtr (new ExecJob (*this, status));
        }

        Buffer ChildProcess::CollectOutput (
                THEKOGANS_UTIL_HANDLE handle,
                std::size_t chunkSize,
                bool reap,
                const TimeSpec &timeSpec) {
            Buffer buffer (HostEndian, chunkSize);
            TenantFile stdOut (HostEndian, handle, std::string ());
            while (buffer.AdvanceWriteOffset (
                    stdOut.Read (buffer.GetWritePtr (), buffer.GetDataAvailableForWriting ())) > 0) {
                if (buffer.GetDataAvailableForWriting () == 0) {
                    buffer.Resize (buffer.GetDataAvailableForReading () + chunkSize);
                }
            }
            if (reap) {
                // reap the child process
                Wait (timeSpec);
            }
            return buffer;
        }

        LockFile::LockFile (const std::string &path_) :
                path (path_) {
            if (!path.IsEmpty ()) {
                if (!path.Exists ()) {
                    File::Touch (path_);
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Lock file (%s) found.\n", path_.c_str ());
                }
            }
        }

        LockFile::~LockFile () {
            if (path.Exists ()) {
                path.Delete ();
            }
        }

    #if !defined (TOOLCHAIN_OS_Windows)
        namespace {
            // If you're looking at this and your first reaction is WTF!?!
            // Fear not, there is a reason. An intermittent crash in the
            // parent of the child (daemon) process was reported after the
            // child process sends a SIGUSR1 signal to the parent to let it
            // know all is well. The crash is random and after doing extensive
            // digging and research the only thing I can surmise is that there's
            // a race in the run time cleanup code. If any memory management is
            // performed by the static object dtors we sometimes get;
            // BUG IN CLIENT OF LIBPLATFORM: Trying to recursively lock an os_unfair_lock.
            // This hoop jumping is to avoid any and all memory management in the
            // context of the signal handler.
            struct exitJob : public RunLoop::Job {
                int exitCode;
                explicit exitJob (int exitCode_) :
                        exitCode (exitCode_) {
                    // Take out a reference on ourselves to avoid
                    // RunLoop::FinishedJob calling Release causing
                    // memory management.
                    AddRef ();
                    // Pre-allocate the string to avoid memory
                    // management in Reset (below).
                    runLoopId = GUID::FromRandom ().ToString ();
                }
            protected:
                // This duplicates RunLoop::Job::Reset with a minor
                // difference. We avoid std::string copy (and
                // potential malloc/free) by copying the RunLoop::Id
                // in place.
                virtual void Reset (const RunLoop::Id &runLoopId_) {
                    strncpy (&runLoopId[0], &runLoopId_[0], runLoopId.size ());
                    SetState (Pending);
                    disposition = Unknown;
                    completed.Reset ();
                }
                virtual void Execute (const std::atomic<bool> & /*done*/) throw () {
                    exit (exitCode);
                }
            };
            RunLoop::Job *failureExit = new exitJob (EXIT_FAILURE);
            RunLoop::Job *successExit = new exitJob (EXIT_SUCCESS);

            void SignalHandlerSIGCHLD (int /*signum*/) {
                GlobalJobQueue::Instance ().EnqJob (RunLoop::Job::SharedPtr (failureExit));
            }
            void SignalHandlerSIGALRM (int /*signum*/) {
                GlobalJobQueue::Instance ().EnqJob (RunLoop::Job::SharedPtr (failureExit));
            }
            void SignalHandlerSIGUSR1 (int /*signum*/) {
                GlobalJobQueue::Instance ().EnqJob (RunLoop::Job::SharedPtr (successExit));
            }
        }

        // This function was adapted from: http://www.itp.uzh.ch/~dpotter/howto/daemonize
        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API Daemonize (
                const char *userName,
                const char *directory,
                const char *lockFilePath,
                i32 waitForChild) {
            // Already a daemon.
            if (getppid () == 1) {
                return;
            }
            // Create the lock file as the current user.
            if (lockFilePath != 0) {
                THEKOGANS_UTIL_HANDLE lockFile = open (lockFilePath, O_RDWR | O_CREAT, 0640);
                if (lockFile < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            // Drop user if there is one, and we were run as root.
            if (userName != 0 && (getuid () == 0 || geteuid () == 0)) {
                passwd *pw = getpwnam (userName);
                if (pw == 0 || setuid (pw->pw_uid) < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            // Trap signals that we expect to recieve.
            signal (SIGCHLD, SignalHandlerSIGCHLD);
            signal (SIGALRM, SignalHandlerSIGALRM);
            signal (SIGUSR1, SignalHandlerSIGUSR1);
            // This hack is necessary to make sure no allocation is done in the signal handler.
            GlobalJobQueue::CreateInstance ();
            // Fork off the parent process.
            pid_t pid = fork ();
            if (pid < 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            // If we got a good PID, then we can exit the parent process.
            if (pid > 0) {
                // Wait for confirmation from the child via SIGTERM or SIGCHLD, or
                // for waitForChild seconds to elapse (SIGALRM). pause () should
                // not return.
                alarm (waitForChild);
                pause ();
                exit (EXIT_FAILURE);
            }
            // At this point we are executing as the child process.
            // Cancel certain signals.
            signal (SIGCHLD, SIG_DFL); // A child process dies.
            signal (SIGTSTP, SIG_IGN); // Various TTY signals.
            signal (SIGTTOU, SIG_IGN);
            signal (SIGTTIN, SIG_IGN);
            signal (SIGHUP, SIG_IGN); // Ignore hangup signal.
            signal (SIGTERM, SIG_DFL); // Die on SIGTERM.
            // Change the file mode mask.
            umask (0);
            // Create a new SID for the child process.
            if (setsid () < 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            // Change the current working directory. This prevents the current
            // directory from being locked; hence not being able to remove it.
            if (directory == 0) {
                directory = "/";
            }
            if (chdir (directory) < 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            // Redirect standard io to /dev/null.
            if (freopen ("/dev/null", "r", stdin) != 0 &&
                    freopen ("/dev/null", "w", stdout) != 0 &&
                    freopen ("/dev/null", "w", stderr) != 0) {
                // Tell the parent process that we are A-okay.
                kill (getppid (), SIGUSR1);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            // From here we're in the daemon...
        }
    #endif // !defined (TOOLCHAIN_OS_Windows)

    #if defined (TOOLCHAIN_OS_Windows)
        namespace {
            struct ProcessHandle {
            private:
                HANDLE handle;

            public:
                explicit ProcessHandle (THEKOGANS_UTIL_PROCESS_ID processId) :
                        handle (
                            OpenProcess (
                                PROCESS_QUERY_LIMITED_INFORMATION,
                                FALSE,
                                processId)) {
                    if (handle == 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                }
                ~ProcessHandle () {
                    if (handle != 0) {
                        CloseHandle (handle);
                    }
                }

                HANDLE Get () const {
                    return handle;
                }
            };

            struct ProcessToken {
            private:
                HANDLE token;

            public:
                ProcessToken (
                        HANDLE processHandle,
                        DWORD desiredAccess = TOKEN_QUERY) :
                        token (0) {
                    if (!OpenProcessToken (processHandle, desiredAccess, &token)) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                }
                ~ProcessToken () {
                    if (token != 0) {
                        CloseHandle (token);
                    }
                }

                HANDLE Get () const {
                    return token;
                }
            };

            struct AdminSID {
            private:
                PSID adminSID;

            public:
                AdminSID () : adminSID (0) {
                    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
                    if (!AllocateAndInitializeSid (
                            &NtAuthority,
                            2,
                            SECURITY_BUILTIN_DOMAIN_RID,
                            DOMAIN_ALIAS_RID_ADMINS,
                            0, 0, 0, 0, 0, 0,
                            &adminSID)) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                }
                ~AdminSID () {
                    if (adminSID != 0) {
                        FreeSid (adminSID);
                    }
                }

                PSID Get () const {
                    return adminSID;
                }
            };
        }
    #endif // defined (TOOLCHAIN_OS_Windows)
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API GetProcessPath (
                THEKOGANS_UTIL_PROCESS_ID processId) {
        #if defined (TOOLCHAIN_OS_Windows)
            ProcessHandle processHandle (processId);
            wchar_t path[MAX_PATH] = {};
            DWORD length = MAX_PATH;
            if (QueryFullProcessImageNameW (processHandle.Get (), 0, path, &length)) {
                return UTF16ToUTF8 (path, length, WC_ERR_INVALID_CHARS);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #elif defined (TOOLCHAIN_OS_Linux)
            char path[PATH_MAX + 1] = {0};
            ssize_t length = readlink (
                FormatString ("/proc/%d/exe", processId).c_str (), path, sizeof (path));
            if (length != -1) {
                path[length] = '\0';
                return path;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #elif defined (TOOLCHAIN_OS_OSX)
            char path[PROC_PIDPATHINFO_MAXSIZE] = {};
            if (proc_pidpath (processId, path, sizeof (path)) != -1) {
                return path;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API IsAdminProcess (
                THEKOGANS_UTIL_PROCESS_ID processId) {
        #if defined (TOOLCHAIN_OS_Windows)
            ProcessHandle processHandle (processId);
            ProcessToken processToken (processHandle.Get ());
            TOKEN_ELEVATION tokenElevation = {0};
            DWORD length = 0;
            if (GetTokenInformation (
                    processToken.Get (),
                    TokenElevation,
                    &tokenElevation,
                    sizeof (tokenElevation),
                    &length)) {
                assert (length == sizeof (tokenElevation));
                return tokenElevation.TokenIsElevated == TRUE;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #elif defined (TOOLCHAIN_OS_Linux)
            // FIXME: implement
            assert (0);
            return false;
        #elif defined (TOOLCHAIN_OS_OSX)
            // A user cannot be member in more than NGROUPS groups,
            // not counting the default group (hence the + 1)
            gid_t groupIDs[NGROUPS + 1];
            int groupCount = 0;
            {
                // ID of user who started the process
                uid_t userID = getuid ();
                // Get user password info for that user
                const struct passwd *pw = getpwuid (userID);
                if (pw != 0) {
                    // Look up groups that user belongs to
                    groupCount = NGROUPS + 1;
                    // getgrouplist returns ints and not gid_t and
                    // both may not necessarily have the same size
                    int intGroupIDs[NGROUPS + 1];
                    getgrouplist (pw->pw_name, pw->pw_gid, intGroupIDs, &groupCount);
                    // Copy them to real array
                    for (int i = 0; i < groupCount; ++i) {
                        groupIDs[i] = intGroupIDs[i];
                    }
                }
                else {
                    // We cannot lookup the user but we can look what groups this process
                    // currently belongs to (which is usually the same group list).
                    groupCount = getgroups (NGROUPS + 1, groupIDs);
                }
            }
            for (int i = 0; i < groupCount; ++i) {
                // Get the group info for each group
                const struct group *group = getgrgid (groupIDs[i]);
                // An admin user is member of the group named "admin"
                if (group != 0 && strcmp (group->gr_name, "admin") == 0) {
                    return true;
                }
            }
            return false;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
