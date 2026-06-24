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

#if !defined (__thekogans_util_TransactedFile_h)
#define __thekogans_util_TransactedFile_h

#include <string>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Flags.h"
#include "thekogans/util/PageMap.h"
#include "thekogans/util/NullLock.h"
#include "thekogans/util/Mutex.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/File.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Producer.h"
#include "thekogans/util/AlignedAllocator.h"
#include "thekogans/util/SecureAllocator.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Forward declaration of \see{TransactedFile} needed by \see{TransactedFileEvents}.
        struct TransactedFile;
        /// \brief
        /// Forward declaration of \see{SimpleTransactedFile} needed by \see{TransactedFile}.
        struct SimpleTransactedFile;

        /// \struct TransactedFileEvents TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        /// Subscribe to TransactedFileEvents to receive transaction notifications.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileEvents {
            /// \brief
            /// dtor.
            virtual ~TransactedFileEvents () {}

            /// \brief
            /// Transaction is committing. Depending on the phase do whatever
            /// is appropriate.
            /// If your object derives from \see{TransactedFile::Object}
            /// all this is done under the hood for you. All you will need
            /// to do is implement Alloc (phase 1) and Flush (phase 2).
            /// \param[in] phase Either COMMIT_PHASE_1 or COMMIT_PHASE_2.
            virtual void OnTransactedFileTransactionCommit (
                RefCounted::SharedPtr<TransactedFile> /*file*/,
                int phase) noexcept {}
            /// \brief
            /// Transaction is aborting.
            virtual void OnTransactedFileTransactionAbort (
                RefCounted::SharedPtr<TransactedFile> /*file*/) noexcept {}
        };

        /// \struct TransactedFile TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        /// TransactedFile is the heart of a structured data management system.
        /// Together with \see{TransactedFile::Allocator}, \see{TransactedFile::Registry},
        /// and \see{TransactedFile::Object} they form the core of an OODBMS engine.
        /// TransactedFile is designed to be used in one of two distinct modes;
        /// 1. Fully structured, taking advantage of the classes mentioned above
        /// and 2. As a very powerful and efficient replacement for \see{File} where
        /// you use the built in page caching machinery to speed up reads and writes
        /// but is otherwise unstructured.
        /// ************************** PLEASE READ ****************************
        /// VERY IMPORTANT: In general, TransactedFile is NOT thread safe. Specifically
        /// the legacy \see{File} API is not. I felt introducing a lock with every API
        /// access would be very costly performance wise. And even if I did, the API
        /// design is such that certain common operations (Seek/Read, Seek/Write) would
        /// still not be safe as they would not execute atomicaly. Instead, a higher
        /// level API (ReadEx, WriteEx, Grow, Shrink) is introduced. Together with
        /// \see{TransactedFile::Range} and it's derivatives and \see{TransactedFile::Allocator}
        /// and it's derivatives the following multi-threaded use case is prescribed;
        ///
        /// main thread;
        ///   open/create file.
        ///   release master threads.
        ///   close file.
        ///
        /// master thread;
        ///   if writting
        ///     start transaction. This will lock the file for exclusive access.
        ///   release worker threads that use \see{TransactedFile::Allocator} to
        ///     allocate blocs and \see{TransactedFile::Range} to read and write them.
        ///     It's highly recommended that you use \see{TransactedFile::TransactionParticipant}
        ///     and \see{TransactedFile::Object}. \see{TransactedFile::Regitry}
        ///     can also be used to store global heap objects based on \see{Serializable}.
        ///   wait for workers to enter a barrier (i.e. signal they're done).
        ///   if writting
        ///     optionally delete cache.
        ///     commit transaction.
        /// *******************************************************************
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFile :
                private File,
                public Producer<TransactedFileEvents> {
            /// \brief
            /// TransactedFile participates in the \see{DynamicCreatable}
            /// dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (TransactedFile)

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

            /// \brief
            /// Commit phase 1 (allocation).
            static const int COMMIT_PHASE_1;
            /// \brief
            /// Commit phase 2 (flush).
            static const int COMMIT_PHASE_2;

            /// \struct TransactedFile::Transaction TransactedFile.h thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// A very simple transaction scope guard.
            /// Call Commit before the end of the
            /// scope to commit the transaction.
            /// NOTE: If you're only reading the file, this is unnecessary.
            struct _LIB_THEKOGANS_UTIL_DECL Transaction {
            private:
                /// \see{TransactedFile} we're guarding.
                TransactedFile &file;
                /// \brief
                /// This guard will serialize all transactions.
                LockGuard<Mutex> guard;

            public:
                /// \brief
                /// ctor.
                /// \param[in] file_ \see{TransactedFile} to transact.
                explicit Transaction (TransactedFile &file_) :
                    file (file_),
                    guard (file.mutex) {}
                /// \brief
                /// dtor.
                ~Transaction ();

                /// \brief
                /// Commit the transaction before the dtor aborts it.
                void Commit (bool clearCache = false);

                /// \brief
                /// Transaction is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Transaction)
                /// \brief
                /// Transaction is neither move constructable, nor move assignable.
                THEKOGANS_UTIL_DISALLOW_MOVE_AND_ASSIGN (Transaction)
            };

            /// \struct TransactedFile::TransactionParticipant TransactedFile.h
            /// thekogans/util/TransactedFile.h
            ///
            /// \brief
            /// TransactionParticipants are objects that listen to
            /// \see{TransactedFileEvents} and are able to flush and reload
            /// themselves to and from a \see{TransactedFile}.
            struct _LIB_THEKOGANS_UTIL_DECL TransactionParticipant :
                    public Subscriber<TransactedFileEvents> {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (TransactionParticipant)

            protected:
                /// \brief
                /// \see{TransactedFile} whose \see{TransactedFileEvents}
                /// we're participants of.
                TransactedFile::SharedPtr file;
                /// \brief
                /// Set if the internal cache is dirty.
                static const ui32 FLAGS_DIRTY = 1;
                /// \brief
                /// Combination of the above flags.
                Flags32 flags;

            public:
                /// \brief
                /// ctor.
                /// \param[in] file_ \see{TransactedFile} we're a transaction participant of.
                TransactionParticipant (TransactedFile::SharedPtr file_) :
                    file (file_),
                    flags (0) {}
                /// \brief
                /// dtor.
                virtual ~TransactionParticipant () {}

                /// \brief
                /// Return the file.
                /// \return file.
                inline TransactedFile::SharedPtr GetFile () const {
                    return file;
                }

                /// \brief
                /// Return dirty.
                /// \return dirty.
                inline bool IsDirty () const {
                    return flags.Test (FLAGS_DIRTY);
                }
                /// \brief
                /// Set the dirty flag and if true, subscribe to \see{TransactedFileEvents}.
                /// \param[in] dirty true == dirty, false == clean.
                virtual void SetDirty (bool dirty);

                /// \brief
                /// TransactionParticipant is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TransactionParticipant)
                /// \brief
                /// TransactionParticipant is neither move constructable, nor move assignable.
                THEKOGANS_UTIL_DISALLOW_MOVE_AND_ASSIGN (TransactionParticipant)
            };

            /// \brief
            /// We do our own locking.
            using PageMapType = PageMap<ui64, NullLock>;

        private:
            /// \brief
            /// File size.
            ui64 size;
            /// \brief
            /// \see{PageMap} used to map file pages.
            PageMapType::SharedPtr pageMap;
            /// \brief
            /// For use by \see{Transaction}.
            Mutex mutex;
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            #include "thekogans/util/TransactedFileRange.h"
            #include "thekogans/util/TransactedFileAllocator.h"
            #include "thekogans/util/TransactedFileRegistry.h"
            #include "thekogans/util/TransactedFileObject.h"

        private:
            /// \brief
            /// \see{Allocator} associated with this file.
            Allocator::SharedPtr allocator;
            /// \brief
            /// \see{Registry} associated with this file.
            Registry::SharedPtr registry;

        public:
            /// \brief
            /// ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            /// \param[in] allocator \see{Allocator} to attach to this file.
            /// \param[in] registry \see{Registry} to attach to this file.
            TransactedFile (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path = std::string (),
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr);
            /// \brief
            /// ctor. Open or create the file.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
        #if defined (TOOLCHAIN_OS_Windows)
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] allocator \see{Allocator} to attach to this file.
            /// \param[in] registry \see{Registry} to attach to this file.
            TransactedFile (
                Endianness endianness,
                const std::string &path,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                DWORD dwCreationDisposition = OPEN_ALWAYS,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL,
            #else // defined (TOOLCHAIN_OS_Windows)
                i32 flags = O_RDWR | O_CREAT,
                i32 mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
            #endif // defined (TOOLCHAIN_OS_Windows)
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr);
            /// \brief
            /// dtor. Close the file.
            virtual ~TransactedFile ();

            /// \brief
            /// Return the file path.
            /// \return File path.
            inline const std::string &GetPath () const {
                return path;
            }

            /// \brief
            /// Return the \see{Allocator} attached to thi file.
            /// Since allocator is constant after Init, this API
            /// is thread safe.
            /// \return allocator.
            inline Allocator::SharedPtr GetAllocator () const {
                return allocator;
            }
            /// \brief
            /// Return the \see{Registry} attached to thi file.
            /// Since registry is constant after Init, this API
            /// is thread safe.
            /// \return registry.
            inline Registry::SharedPtr GetRegistry () const {
                return registry;
            }

            inline std::size_t GetPageSize () const {
                return pageMap->GetPageSize ();
            }

            /// \brief
            /// Open or create the file.
            /// NOT thread safe.
            /// \param[in] path Path to file to open.
        #if defined (TOOLCHAIN_OS_Windows)
            /// \param[in] dwDesiredAccess Windows CreateFile parameter.
            /// \param[in] dwShareMode Windows CreateFile parameter.
            /// \param[in] dwCreationDisposition Windows CreateFile parameter.
            /// \param[in] dwFlagsAndAttributes Windows CreateFile parameter.
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] flags POSIX open parameter.
            /// \param[in] mode POSIX open parameter.
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \param[in] allocator \see{Allocator} to attach to this file.
            /// \param[in] registry \see{Registry} to attach to this file.
            void OpenEx (
                const std::string &path,
            #if defined (TOOLCHAIN_OS_Windows)
                DWORD dwDesiredAccess = GENERIC_READ | GENERIC_WRITE,
                DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE,
                DWORD dwCreationDisposition = OPEN_EXISTING,
                DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL,
            #else // defined (TOOLCHAIN_OS_Windows)
                i32 flags = O_RDWR,
                i32 mode = S_IRUSR | S_IWUSR,
            #endif // defined (TOOLCHAIN_OS_Windows)
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr);
            /// \brief
            /// Close the file.
            /// Thread safe.
            void CloseEx ();

            /// \brief
            /// Thread safe version of Read.
            /// \param[in] offset Start of range.
            /// \param[out] buffer Where to place the bytes.
            /// \param[in] count Number of bytes to read.
            /// \return Number of bytes actually read.
            std::size_t ReadEx (
                ui64 offset,
                void *buffer,
                std::size_t count);
            /// \brief
            /// Thread safe version of Write.
            /// \param[in] offset Start of range.
            /// \param[in] buffer Where bytes come from.
            /// \param[in] count Number of bytes to write.
            /// \return Number of bytes actually written.
            std::size_t WriteEx (
                ui64 offset,
                const void *buffer,
                std::size_t count);

            /// \brief
            /// Grow the file by the given amount.
            /// Thread safe.
            /// \param[in] amount Amount to grow the file by.
            /// \return Old file size.
            ui64 Grow (ui64 amount);
            /// \brief
            /// Shrink the file by the given amount.
            /// Thread safe.
            /// \param[in] amount Amount to shrink the file by.
            /// \return New file size.
            ui64 Shrink (ui64 amount);

            // File
            /// \brief
            /// Return file size in bytes.
            /// Thread safe.
            /// \return File size in bytes.
            inline ui64 GetSizeEx () {
                LockGuard<SpinLock> guard (spinLock);
                return size;
            }

        private:
            /// \brief
            /// Called during file open. If the file is empty and an
            /// \see{Allocator} and an optional \see{Registry} were
            /// provided, they are used to initialize the file (make
            /// is strucured). If the file is not empty and at least
            /// an \see{Allocator} was provided, the file is assumed
            /// to be structured and the \see{Allocator} and an optional
            /// \see{Registry} will come from the file. If no \see{Allocator}
            /// was provided the file is assumed to be unstrucured.
            /// \param[in] allocator_ \see{Allocator} to attach to this file.
            /// If nullptr, use \see{TransactedFileBTreeAllocator}.
            /// \param[in] registry_ \see{Registry} to attach to this file.
            /// If nullptr, use \see{TransactedFileBTreeRegistry}.
            /// NOT thread safe.
            void Init (
                Allocator::SharedPtr allocator_ = nullptr,
                Registry::SharedPtr registry_ = nullptr);
            /// \brief
            /// Used by Open to apply the log before opening the file.
            /// \path path File path.
            static void CommitLog (const std::string &path);
            /// \brief
            /// Commit the current transaction.
            /// Used by \see{Transaction} to commit the current changes.
            /// Thread safe.
            void Commit (bool clearCache = false);
            /// \brief
            /// Abort the current transaction.
            /// Used by \see{Transaction} dtor to abort uncommitted changes.
            /// Thread safe.
            void Abort ();

            /// \brief
            /// This is a wrapper around pageMap.GetPage.
            /// Thread safe.
            /// \param[in] offset Offset whose page to return.
            /// \return Page that covers the neighborhood around the given offset.
            PageMapType::Page::SharedPtr GetPage (ui64 offset);

            /// \brief
            /// Given a file path, use the full file name to create
            /// a GUID to append to the path to act as the log path.
            /// Thread safe.
            /// \param[in] path File path to turn in to log path.
            /// \return Log path based on the passed in file path.
            static std::string GetLogPath (const std::string &path);

            /// \brief
            /// Needs access to allocator and regitry.
            friend struct SimpleTransactedFile;

            /// \brief
            /// TransactedFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TransactedFile)
            /// \brief
            /// TransactedFile is neither move constructable, nor move assignable.
            THEKOGANS_UTIL_DISALLOW_MOVE_AND_ASSIGN (TransactedFile)
        };

        /// \brief
        /// Implement \see{TransactedFile::Allocator} extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS (TransactedFile::Allocator)

        /// \brief
        /// Implement \see{TransactedFile::Registry} extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS (TransactedFile::Registry)

        /// \struct SimpleTransactedFile TransactedFile.h thekogans/util/TransactedFile.h
        ///
        /// \brief
        /// SimpleTransactedFile exposes the basic flags supported by the standard
        /// library open that are portable across Windows, Linux and OS X.
        /// NOTE: On Linux and OS X if a file needs to be created, it's
        /// mode will be 0644. This is fine for most cases but might not
        /// be appropriate for some. If you need to control the mode of
        /// the created file use TransactedFile instead.
        struct _LIB_THEKOGANS_UTIL_DECL SimpleTransactedFile : public TransactedFile {
            /// \brief
            /// Default ctor.
            /// \param[in] endianness File endianness.
            /// \param[in] handle OS file handle.
            /// \param[in] path File path.
            SimpleTransactedFile (
                Endianness endianness = HostEndian,
                THEKOGANS_UTIL_HANDLE handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE,
                const std::string &path = std::string (),
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr) :
                TransactedFile (endianness, handle, path, allocator, registry) {}
            /// \brief
            /// ctor. Abstracts most useful functionality from POSIX open.
            /// \param[in] endianness File endianness.
            /// \param[in] path Path to file to open.
            /// \param[in] flags Most useful POSIX open flags.
            SimpleTransactedFile (
                Endianness endianness,
                const std::string &path,
                Flags32 flags = SimpleFile::ReadWrite | SimpleFile::Create,
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr);

            /// \brief
            /// Open the file.
            /// \param[in] path File path.
            /// \param[in] flags File open flags.
            void SimpleOpen (
                const std::string &path,
                Flags32 flags = SimpleFile::ReadWrite | SimpleFile::Create,
                Allocator::SharedPtr allocator = nullptr,
                Registry::SharedPtr registry = nullptr);

            /// \brief
            /// SimpleTransactedFile is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SimpleTransactedFile)
            /// \brief
            /// SimpleTransactedFile is neither move constructable, nor move assignable.
            THEKOGANS_UTIL_DISALLOW_MOVE_AND_ASSIGN (SimpleTransactedFile)
        };

    } // namespace util
} // namespace thekogans

#endif // __thekogans_util_TransactedFile_h
