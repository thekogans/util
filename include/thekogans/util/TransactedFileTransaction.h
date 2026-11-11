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

/// \brief
/// TransactedFile implements a two phase commit. The
/// first phase (usually called the allocation phase)
/// will have all \see{Object}s allocate the space they
/// need to commit themselves to disk. The space is
/// allocated using an \see{Allocator}. This is also
/// the time when all offset pointers are updated (Again
/// see \see{Object} and \see{ObjectEvents}). The second
/// phase (usually called the commit phase) will have
/// all objects flush themselves to disk. This logic is
/// enshrined in \see{Object}.

/// \struct TransactedFile::Transaction TransactedFile.h thekogans/util/TransactedFile.h
///
/// \brief
/// A very simple transaction scope guard. Call Commit before the end of the
/// scope to commit the transaction. If not, the dtor will abort it.
struct _LIB_THEKOGANS_UTIL_DECL Transaction {
    static constexpr int PHASE_NONE = 0;
    static constexpr int PHASE_IN_PROGRESS = 1;
    static constexpr int PHASE_ALLOC = 2;
    static constexpr int PHASE_FLUSH = 3;
    static constexpr int PHASE_WAL = 4;
    static constexpr int PHASE_ABORT = 5;

    static constexpr const char * const phaseName[] = {
        "PHASE_NONE",
        "PHASE_IN_PROGRESS",
        "PHASE_ALLOC",
        "PHASE_FLUSH",
        "PHASE_WAL",
        "PHASE_ABORT"
    };

    /// \brief
    /// ctor.
    /// \param[in] file_ \see{TransactedFile} to transact.
    explicit Transaction (TransactedFile::SharedPtr file_) :
            file (file_),
            guard (file->mutex) {
        file->phase = PHASE_IN_PROGRESS;
    }
    /// \brief
    /// dtor. Abort an uncommitted transaction.
    ~Transaction ();

    /// \brief
    /// Commit transaction before dtor aborts it.
    /// NOTE: You can call Commit as many times as you like in the
    /// scope of a single transaction. Effectively turning Commit
    /// in to Checkpoint. That's only an illusion since there is
    /// no Rollback.
    /// \param[in] clearCache true == Clear \see{PageMap} cache after committing.
    void Commit (bool clearCache = false);

    /// \brief
    /// Used by Open to apply the log before opening the file.
    /// \path path File path.
    static void CommitLog (const std::string &path);

    /// \brief
    /// Given a file path, use the full file name to create
    /// a GUID to append to the path to act as the log path.
    /// Thread safe.
    /// \param[in] path File path to turn in to log path.
    /// \return Log path based on the passed in file path.
    static std::string GetLogPath (const std::string &path);

private:
    /// \brief
    /// \see{TransactedFile} we're guarding.
    TransactedFile::SharedPtr file;
    /// \brief
    ///
    LockGuard<Mutex> guard;

    /// \brief
    /// Transaction is neither copy or move constructable, nor assignable.
    THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (Transaction)
};
