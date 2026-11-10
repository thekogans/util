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

#include <string>
#include "thekogans/util/Path.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/File.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        TransactedFile::Transaction::~Transaction () {
            // If the transaction has been committed, these two statements
            // are noop. There will be no dirty pages to abort, and no
            // subscribers to notify.
            {
                LockGuard<SpinLock> guard (file.spinLock);
                file.size = file.GetSize ();
                file.pageMap->Clear (true);
            }
            file.Producer<TransactedFileTransactionEvents>::Produce (
                std::bind (
                    &TransactedFileTransactionEvents::OnTransactedFileTransactionAbort,
                    std::placeholders::_1,
                    &file),
                true);
        }

        void TransactedFile::Transaction::Commit (bool clearCache) {
            std::vector<Producer<TransactedFileTransactionEvents>::SharedSubscriberInfo> totalCommittedObjects;
            totalCommittedObjects.reserve (file.Producer<TransactedFileTransactionEvents>::GetSubscriberCount () * 2);
            // =========================================================================
            // STEP 1: DETERMINISTIC CONVERGENT SETTLEMENT LOOP (PHASE 1 ONLY)
            // =========================================================================
            std::vector<Producer<TransactedFileTransactionEvents>::SharedSubscriberInfo> currentDirtyBatch;
            std::size_t iterations = 0;
            static constexpr std::size_t MAX_CONVERGENCE_WAVES = 32;
            while (file.Producer<TransactedFileTransactionEvents>::GetSubscribers (currentDirtyBatch, true) != 0) {
                // DEFENSIVE CEILING: If we hit this, a cyclic dirty loop has been detected!
                if (THEKOGANS_UTIL_UNLIKELY (++iterations > MAX_CONVERGENCE_WAVES)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_ELOOP); // Crash/Abort before corrupting disk!
                }
                for (const auto &subscriber : currentDirtyBatch) {
                    subscriber.second->DeliverEvent (
                        std::bind (
                            &TransactedFileTransactionEvents::OnTransactedFileTransactionCommit,
                            std::placeholders::_1,
                            &file,
                            COMMIT_PHASE_1),
                        subscriber.first);
                }
                totalCommittedObjects.insert (
                    totalCommittedObjects.end (),
                    std::make_move_iterator (currentDirtyBatch.begin ()),
                    std::make_move_iterator (currentDirtyBatch.end ()));
            }
            // =========================================================================
            // STEP 2: UNIFIED FLUSH PASS (PHASE 2 ONLY)
            // =========================================================================
            for (const auto &subscriber : totalCommittedObjects) {
                subscriber.second->DeliverEvent (
                    std::bind (
                        &TransactedFileTransactionEvents::OnTransactedFileTransactionCommit,
                        std::placeholders::_1,
                        &file,
                        COMMIT_PHASE_2),
                    subscriber.first);
            }
            {
                LockGuard<SpinLock> guard (file.spinLock);
                std::string logPath = GetLogPath (file.path);
                // Dump dirty pages to log.
                {
                    SimpleFile log (
                        file.endianness,
                        logPath,
                        SimpleFile::ReadWrite | SimpleFile::Create | SimpleFile::Truncate);
                    log << (ui32)0 << (ui32)0 << file.size <<
                        (SizeType)file.pageMap->GetPageSize ();
                    std::size_t count = file.pageMap->Log (log);
                    log.Seek (0, SEEK_SET);
                    log << MAGIC32 << (ui32)count;
                }
                // At this point the log is commited and closed.
                // If we can't complete the commit (exception),
                // next time Open (above) will use CommitLog
                // before opening the file.
                file.pageMap->Flush (file, clearCache);
                file.SetSize (file.size);
                file.Flush ();
                File::Delete (logPath);
            }
        }

        void TransactedFile::Transaction::CommitLog (const std::string &path) {
            std::string logPath = GetLogPath (path);
            if (Path (path).Exists () && Path (logPath).Exists ()) {
                {
                    SimpleFile file (HostEndian, path, SimpleFile::ReadWrite);
                    ReadOnlyFile log (HostEndian, logPath);
                    // Magic serves two purposes. Firstly it gives us a quick
                    // check to make sure we're dealing with a log file and second,
                    // it allows us to move logs from little to big endian (and
                    // vise versa) machines for analysis and resolution.
                    ui32 magic;
                    log >> magic;
                    if (magic == MAGIC32) {
                        // Log is host endian.
                    }
                    else if (ByteSwap<GuestEndian, HostEndian> (magic) == MAGIC32) {
                        // Log is guest endian. File endianness doesn't mater as it is
                        // just being patched up with dirty tiles. Obviously it is assumed
                        // to be the same as the log endianness.
                        log.endianness = GuestEndian;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Corrupt log %s",
                            logPath.c_str ());
                    }
                    ui32 count;
                    SizeType size;
                    SizeType pageSize;
                    log >> count >> size >> pageSize;
                    AddressType offset;
                    HostBuffer page (pageSize);
                    while (count-- != 0) {
                        log >> offset;
                        log.Read (page.GetDataPtr (), pageSize);
                        file.Seek (offset, SEEK_SET);
                        file.Write (page.GetDataPtr (), pageSize);
                    }
                    file.SetSize (size);
                    file.Flush ();
                }
                Delete (logPath);
            }
        }

        std::string TransactedFile::Transaction::GetLogPath (const std::string &path) {
            std::string name = Path (path).GetFullFileName ();
            return path + "-" + GUID::FromBuffer (name.data (), name.size ()).ToHexString () + ".log";
        }

    } // namespace util
} // namespace thekogans
