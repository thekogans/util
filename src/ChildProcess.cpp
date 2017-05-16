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

#if !defined (TOOLCHAIN_OS_Windows)
    #include <signal.h>
    #include <errno.h>
    #include <sys/wait.h>
#endif // !defined (TOOLCHAIN_OS_Windows)
#include <cassert>
#include <cstdlib>
#include <vector>
#include "thekogans/util/Flags.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/File.h"
#include "thekogans/util/internal.h"
#include "thekogans/util/ChildProcess.h"

namespace thekogans {
    namespace util {

        ChildProcess::StdIO::StdIO (ui32 hookStdIO_) :
                hookStdIO (hookStdIO_) {
            {
                inPipe[0] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                inPipe[1] = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                if (Flags32 (hookStdIO).Test (ChildProcess::HOOK_STDIN)) {
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
                if (Flags32 (hookStdIO).Test (ChildProcess::HOOK_STDOUT)) {
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
                if (Flags32 (hookStdIO).Test (ChildProcess::HOOK_STDERR)) {
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
            if (Flags32 (hookStdIO).Test (ChildProcess::HOOK_STDIN)) {
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
            if (Flags32 (hookStdIO).Test (ChildProcess::HOOK_STDOUT)) {
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
            if (Flags32 (hookStdIO).Test (ChildProcess::HOOK_STDERR)) {
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
            if (Flags32 (hookStdIO).Test (ChildProcess::HOOK_STDIN)) {
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
            if (Flags32 (hookStdIO).Test (ChildProcess::HOOK_STDOUT)) {
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
            if (Flags32 (hookStdIO).Test (ChildProcess::HOOK_STDERR)) {
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
                ui32 hookStdIO_) :
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

        std::string ChildProcess::GetCommandLine () const {
            std::string commandLine = "\"" + path + "\"";
            for (std::list<std::string>::const_iterator
                    it = arguments.begin (),
                    end = arguments.end (); it != end; ++it) {
                commandLine += " " + *it;
            }
            return commandLine;
        }

        THEKOGANS_UTIL_PROCESS_ID ChildProcess::Spawn () {
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
            STARTUPINFO startInfo;
            ZeroMemory (&startInfo, sizeof (STARTUPINFO));
            startInfo.cb = sizeof (STARTUPINFO);
            if (hookStdIO != HOOK_NONE) {
                stdIO.reset (new StdIO (hookStdIO));
                startInfo.dwFlags |= STARTF_USESTDHANDLES;
                startInfo.hStdInput = stdIO->inPipe[0];
                startInfo.hStdOutput = stdIO->outPipe[1];
                startInfo.hStdError = stdIO->errPipe[1];
            }
            ClearProcessInformation (processInformation);
            if (!CreateProcess (0, &commandLine[0], 0, 0, TRUE, 0,
                    !environment.empty () ? (LPVOID)&environment[0] : 0,
                    0, &startInfo, &processInformation)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return processInformation.dwProcessId;
        #else // defined (TOOLCHAIN_OS_Windows)
            if (hookStdIO != HOOK_NONE) {
                stdIO.reset (new StdIO (hookStdIO));
            }
            std::vector<const char *> argv;
            std::string name = Path (path).GetFullFileName ();
            argv.push_back (name.c_str ());
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
            pid = hookStdIO != HOOK_NONE ? fork () : vfork ();
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
                if (hookStdIO != HOOK_NONE) {
                    assert (stdIO.get () != 0);
                    stdIO->SetupChild ();
                }
                if (!envp.empty ()) {
                #if defined (TOOLCHAIN_OS_OSX)
                    execve (
                        path.c_str (),
                        (char * const *)&argv[0],
                        (char * const *)&envp[0]);
                #else // defined (TOOLCHAIN_OS_OSX)
                    execvpe (
                        path.c_str (),
                        (char * const *)&argv[0],
                        (char * const *)&envp[0]);
                #endif // defined (TOOLCHAIN_OS_OSX)
                }
                else {
                    execvp (path.c_str (), (char * const *)&argv[0]);
                }
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return pid;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        ChildProcess::ChildStatus ChildProcess::Wait (const TimeSpec &timeSpec) {
        #if defined (TOOLCHAIN_OS_Windows)
            return WaitForSingleObject (
                processInformation.hProcess,
                (DWORD)timeSpec.ToMilliseconds ()) == WAIT_OBJECT_0 ? Finished : Failed;
        #else // defined (TOOLCHAIN_OS_Windows)
            if (pid != THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE) {
                int status;
                pid_t wpid = waitpid (pid, &status, WUNTRACED);
                if (wpid == THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE) {
                    return Failed;
                }
                if (WIFEXITED (status)) {
                    pid = THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE;
                    returnCode = WEXITSTATUS (status);
                    return Finished;
                }
                if (WIFSIGNALED (status)) {
                    pid = THEKOGANS_UTIL_INVALID_PROCESS_ID_VALUE;
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

        JobQueue::Job::UniquePtr ChildProcess::CreateSpawnJob () {
            struct SpawnJob : public JobQueue::Job {
                ChildProcess &childProcess;
                SpawnJob (ChildProcess &childProcess_) :
                    childProcess (childProcess_) {}
                // JobQueue::Job
                virtual void Execute (volatile const bool &done) throw () {
                    if (!done) {
                        THEKOGANS_UTIL_TRY {
                            childProcess.Spawn ();
                        }
                        THEKOGANS_UTIL_CATCH_AND_LOG
                    }
                }
            };
            return JobQueue::Job::UniquePtr (new SpawnJob (*this));
        }

        JobQueue::Job::UniquePtr ChildProcess::CreateExecJob (ChildStatus &status) {
            struct ExecJob : public JobQueue::Job {
                ChildProcess &childProcess;
                ChildStatus &status;
                ExecJob (
                    ChildProcess &childProcess_,
                    ChildStatus &status_) :
                    childProcess (childProcess_),
                    status (status_) {}
                // JobQueue::Job
                virtual void Execute (volatile const bool &done) throw () {
                    if (!done) {
                        THEKOGANS_UTIL_TRY {
                            status = childProcess.Exec ();
                        }
                        THEKOGANS_UTIL_CATCH_AND_LOG
                    }
                }
            };
            return JobQueue::Job::UniquePtr (new ExecJob (*this, status));
        }

        Buffer::UniquePtr ChildProcess::CollectOutput (
                THEKOGANS_UTIL_HANDLE handle,
                ui32 chunkSize,
                bool reap,
                const TimeSpec &timeSpec) {
            Buffer::UniquePtr buffer (new Buffer (HostEndian, chunkSize));
            TenantFile stdOut (HostEndian, handle, std::string ());
            while (buffer->AdvanceWriteOffset (
                    stdOut.Read (buffer->GetWritePtr (), buffer->GetDataAvailableForWriting ())) > 0) {
                if (buffer->GetDataAvailableForWriting () == 0) {
                    buffer->Resize (buffer->GetDataAvailableForReading () + chunkSize);
                }
            }
            if (reap) {
                // reap the child process
                Wait (timeSpec);
            }
            return buffer;
        }

    } // namespace util
} // namespace thekogans
