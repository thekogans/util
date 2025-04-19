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
#if defined (TOOLCHAIN_OS_Windows)
    #if defined (TOOLCHAIN_COMPILER_gcc)
        #include "thekogans/util/os/windows/WindowsHeader.h"
        #include <ioapiset.h>
    #endif // defined (TOOLCHAIN_COMPILER_gcc)
    #include <direct.h>
#elif defined (TOOLCHAIN_OS_Linux)
    #include <cstdio>
    #include <sys/types.h>
    #include <sys/ioctl.h>
    #include <sys/epoll.h>
    #include <fcntl.h>
    #include <unistd.h>
#elif defined (TOOLCHAIN_OS_OSX)
    #include <sys/types.h>
    #include <sys/event.h>
    #include <sys/time.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cstdlib>
#include <cassert>
#include <list>
#include <set>
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/os/windows/WindowsUtils.h"
#elif defined (TOOLCHAIN_OS_Linux)
    #include "thekogans/util/os/linux/LinuxUtils.h"
#elif defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/os/osx/OSXUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/Flags.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/File.h"
#include "thekogans/util/Directory.h"

namespace thekogans {
    namespace util {

        struct Directory::Watcher::Watch {
            THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

            using UniquePtr = std::unique_ptr<Watch>;

            std::string directory;
            Directory::Watcher::EventSink &eventSink;
            THEKOGANS_UTIL_HANDLE handle;
        #if defined (TOOLCHAIN_OS_Windows)
            OVERLAPPED overlapped;
            enum {
                BUFFER_SIZE = 8 * 1024
            };
            ui8 buffer[BUFFER_SIZE];
        #elif defined (TOOLCHAIN_OS_OSX)
            struct Entry {
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                using UniquePtr = std::unique_ptr<Entry>;

                Directory::Entry entry;
                THEKOGANS_UTIL_HANDLE handle;

                explicit Entry (const Directory::Entry &entry_) :
                    entry (entry_),
                    handle (THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {}
                ~Entry () {
                    if (handle != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                        close (handle);
                    }
                }

                void Register (const Directory::Watcher::Watch &watch) {
                    if (entry.type != Directory::Entry::Folder) {
                        std::string path = MakePath (watch.directory, entry.name);
                        handle = open (path.c_str (), O_RDONLY);
                        if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
                        }
                        keventStruct event;
                        keventSet (&event, handle, EVFILT_VNODE, EV_ADD | EV_ENABLE,
                            NOTE_DELETE | NOTE_EXTEND | NOTE_WRITE | NOTE_ATTRIB, 0, watch.handle);
                        if (keventFunc (Directory::Watcher::Instance ()->handle,
                                &event, 1, 0, 0, 0) < 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                THEKOGANS_UTIL_OS_ERROR_CODE);
                        }
                    }
                }
            };
            using Entries = OwnerMap<std::string, Entry>;
            Entries entries;
        #endif // defined (TOOLCHAIN_OS_Windows)

            Watch (const std::string &directory_,
                    Directory::Watcher::EventSink &eventSink_) :
                    directory (directory_),
                    eventSink (eventSink_),
                #if defined (TOOLCHAIN_OS_Windows)
                    handle (CreateFileW (os::windows::UTF8ToUTF16 (directory).c_str (),
                        FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ |
                        FILE_SHARE_WRITE |
                        FILE_SHARE_DELETE,
                        0, OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, 0)) {
                #elif defined (TOOLCHAIN_OS_Linux)
                    handle (inotify_add_watch (
                        Directory::Watcher::Instance ()->handle, directory.c_str (),
                        IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_MOVED_FROM | IN_DELETE)) {
                #elif defined (TOOLCHAIN_OS_OSX)
                    handle (open (directory.c_str (), O_RDONLY)) {
                #endif // defined (TOOLCHAIN_OS_Windows)
                if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", directory.c_str ());
                }
            #if defined (TOOLCHAIN_OS_Windows)
                if (CreateIoCompletionPort (handle,
                        Directory::Watcher::Instance ()->handle,
                        (ULONG_PTR)handle, 0) == 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                ReadDirectoryChanges ();
            #elif defined (TOOLCHAIN_OS_OSX)
                keventStruct event;
                keventSet (&event, handle, EVFILT_VNODE, EV_ADD | EV_ENABLE,
                    NOTE_DELETE | NOTE_EXTEND | NOTE_WRITE | NOTE_ATTRIB, 0, handle);
                if (keventFunc (Directory::Watcher::Instance ()->handle, &event, 1, 0, 0, 0) < 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                Scan (entries, true);
            #endif // defined (TOOLCHAIN_OS_OSX)
            }

            ~Watch () {
            #if defined (TOOLCHAIN_OS_Windows)
                CloseHandle (handle);
            #elif defined (TOOLCHAIN_OS_Linux)
                inotify_rm_watch (Directory::Watcher::Instance ()->handle, handle);
            #elif defined (TOOLCHAIN_OS_OSX)
                close (handle);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }

        #if defined (TOOLCHAIN_OS_Windows)
            void ReadDirectoryChanges () {
                memset (&overlapped, 0, sizeof (overlapped));
                if (!ReadDirectoryChangesW (
                        handle, buffer, sizeof (buffer), FALSE,
                        FILE_NOTIFY_CHANGE_CREATION |
                        FILE_NOTIFY_CHANGE_SIZE |
                        FILE_NOTIFY_CHANGE_FILE_NAME |
                        FILE_NOTIFY_CHANGE_DIR_NAME, 0, &overlapped, 0)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
        #elif defined (TOOLCHAIN_OS_OSX)
            void Scan (
                    Entries &entries,
                    bool doRegister) {
                Directory scanDirectory (directory);
                Directory::Entry directoryEntry;
                for (bool gotEntry = scanDirectory.GetFirstEntry (directoryEntry);
                        gotEntry; gotEntry = scanDirectory.GetNextEntry (directoryEntry)) {
                    if (!IsDotOrDotDot (directoryEntry.name.c_str ())) {
                        Entry::UniquePtr entry (new Entry (directoryEntry));
                        if (doRegister) {
                            entry->Register (*this);
                        }
                        entries.insert (Entries::value_type (directoryEntry.name, entry.get ()));
                        entry.release ();
                    }
                }
            }

            void Update (
                    std::list<Directory::Entry> &added,
                    std::list<Directory::Entry> &deleted,
                    std::list<Directory::Entry> &modified) {
                Entries current;
                Scan (current, false);
                Watch::Entries::iterator originalIt = entries.begin ();
                Watch::Entries::iterator originalEnd = entries.end ();
                Watch::Entries::iterator currentIt = current.begin ();
                Watch::Entries::iterator currentEnd = current.end ();
                while (originalIt != originalEnd && currentIt != currentEnd) {
                    if (originalIt->second->entry.name < currentIt->second->entry.name) {
                        deleted.push_back (originalIt->second->entry);
                        originalIt = entries.deleteAndErase (originalIt);
                    }
                    else if (currentIt->second->entry.name < originalIt->second->entry.name) {
                        Entry *entry = currentIt->second;
                        entry->Register (*this);
                        currentIt = current.erase (currentIt);
                        entries.insert (Entries::value_type (entry->entry.name, entry));
                        added.push_back (entry->entry);
                    }
                    else if (originalIt->second->entry != currentIt->second->entry) {
                        originalIt->second->entry = currentIt->second->entry;
                        modified.push_back (originalIt->second->entry);
                        ++originalIt;
                        ++currentIt;
                    }
                    else {
                        ++originalIt;
                        ++currentIt;
                    }
                }
                assert (originalIt == originalEnd || currentIt == currentEnd);
                while (originalIt != originalEnd) {
                    deleted.push_back (originalIt->second->entry);
                    originalIt = entries.deleteAndErase (originalIt);
                }
                while (currentIt != currentEnd) {
                    Entry *entry = currentIt->second;
                    entry->Register (*this);
                    currentIt = current.erase (currentIt);
                    entries.insert (Entries::value_type (entry->entry.name, entry));
                    added.push_back (currentIt->second->entry);
                }
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        };

    #if defined (TOOLCHAIN_OS_OSX)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (Directory::Watcher::Watch::Entry)
    #endif // defined (TOOLCHAIN_OS_OSX)

        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (Directory::Watcher::Watch)

        Directory::Watcher::Watcher () :
            Thread ("Watcher"),
            #if defined (TOOLCHAIN_OS_Windows)
                handle (CreateIoCompletionPort (
                    THEKOGANS_UTIL_INVALID_HANDLE_VALUE, 0, 0, 1)) {
            #elif defined (TOOLCHAIN_OS_Linux)
                handle (inotify_init1 (IN_NONBLOCK)),
                epollHandle (epoll_create (5)) {
            #elif defined (TOOLCHAIN_OS_OSX)
                handle (kqueue ()) {
            #endif // defined (TOOLCHAIN_OS_Windows)
            if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #if defined (TOOLCHAIN_OS_Linux)
            if (epollHandle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            epoll_event epollEvent = {0};
            epollEvent.events = EPOLLIN;
            epollEvent.data.fd = handle;
            if (epoll_ctl (epollHandle, EPOLL_CTL_ADD, handle, &epollEvent) < 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #endif // defined (TOOLCHAIN_OS_Linux)
            Create (THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY);
        }

        Directory::Watcher::~Watcher () {
        #if defined (TOOLCHAIN_OS_Windows)
            CloseHandle (handle);
        #elif defined (TOOLCHAIN_OS_Linux)
            close (handle);
            close (epollHandle);
        #elif defined (TOOLCHAIN_OS_OSX)
            close (handle);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Directory::Watcher::WatchId Directory::Watcher::AddWatch (
                const std::string &directory,
                EventSink &evenSink) {
            LockGuard<SpinLock> guard (spinLock);
            Watch::UniquePtr watch (new Watch (directory, evenSink));
            assert (watch != nullptr);
            if (watch == nullptr) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Unable to create a watch for: %s", directory.c_str ());
            }
            watches.insert (Watches::value_type (watch->handle, watch.get ()));
            return watch.release ()->handle;
        }

        std::string Directory::Watcher::GetDirectory (WatchId watchId) {
            LockGuard<SpinLock> guard (spinLock);
            Watches::iterator it = watches.find (watchId);
            return it != watches.end () ? it->second->directory : std::string ();
        }

        void Directory::Watcher::RemoveWatch (WatchId watchId) {
            LockGuard<SpinLock> guard (spinLock);
            Watches::iterator it = watches.find (watchId);
            if (it != watches.end ()) {
                watches.deleteAndErase (it);
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Can't find the watch for id: %x", watchId);
            }
        }

        void Directory::Watcher::Run () noexcept {
            while (1) {
                THEKOGANS_UTIL_TRY {
                #if defined (TOOLCHAIN_OS_Windows)
                    static const ui32 maxEvents = 256;
                    OVERLAPPED_ENTRY iocpEvents[maxEvents];
                    ULONG count = 0;
                    if (GetQueuedCompletionStatusEx (handle, iocpEvents,
                            maxEvents, &count, INFINITE, FALSE) && count > 0) {
                        for (ULONG i = 0; i < count; ++i) {
                            std::string directory;
                            EventSink *eventSink = nullptr;
                            WatchId watchId = 0;
                            ui8 buffer[Watch::BUFFER_SIZE];
                            {
                                LockGuard<SpinLock> guard (spinLock);
                                Watches::iterator it = watches.find (
                                    (WatchId)iocpEvents[i].lpCompletionKey);
                                if (it != watches.end ()) {
                                    directory = it->second->directory;
                                    eventSink = &it->second->eventSink;
                                    watchId = it->second->handle;
                                    memcpy (buffer, it->second->buffer, Watch::BUFFER_SIZE);
                                    it->second->ReadDirectoryChanges ();
                                }
                            }
                            if (eventSink != nullptr) {
                                PFILE_NOTIFY_INFORMATION notify = nullptr;
                                size_t offset = 0;
                                do {
                                    notify = (PFILE_NOTIFY_INFORMATION)&buffer[offset];
                                    offset += notify->NextEntryOffset;
                                    std::string entryName =
                                        os::windows::UTF16ToUTF8 (
                                            notify->FileName,
                                            (std::size_t)(notify->FileNameLength / sizeof (WCHAR)));
                                    switch (notify->Action) {
                                        case FILE_ACTION_RENAMED_NEW_NAME:
                                        case FILE_ACTION_ADDED: {
                                            eventSink->HandleAdd (watchId, directory,
                                                Directory::Entry (
                                                    MakePath (directory, entryName)));
                                            break;
                                        }
                                        case FILE_ACTION_RENAMED_OLD_NAME:
                                        case FILE_ACTION_REMOVED: {
                                            Directory::Entry entry;
                                            entry.name = entryName;
                                            eventSink->HandleDelete (watchId, directory, entry);
                                            break;
                                        }
                                        case FILE_ACTION_MODIFIED: {
                                            eventSink->HandleModified (watchId, directory,
                                                Directory::Entry (
                                                    MakePath (directory, entryName)));
                                            break;
                                        }
                                    }
                                } while (notify->NextEntryOffset != 0);
                            }
                        }
                    }
                #elif defined (TOOLCHAIN_OS_Linux)
                    epoll_event epollEvent = {0};
                    int count = epoll_wait (epollHandle, &epollEvent, 1, -1);
                    if (count < 0) {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        if (errorCode != EINTR) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                    }
                    else if (count == 1 && epollEvent.data.fd == handle) {
                        unsigned long length = 0;
                        ioctl (handle, FIONREAD, &length);
                        if (length > 0) {
                            std::vector<char> buffer (length);
                            if (read (handle, buffer.data (), length) != (int)length) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE);
                            }
                            for (ui32 i = 0; i < length;) {
                                inotify_event *event = (inotify_event *)&buffer[i];
                                std::string directory;
                                EventSink *eventSink = nullptr;
                                WatchId watchId = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                                {
                                    LockGuard<SpinLock> guard (spinLock);
                                    Watches::iterator it = watches.find (event->wd);
                                    if (it != watches.end ()) {
                                        directory = it->second->directory;
                                        eventSink = &it->second->eventSink;
                                        watchId = it->second->handle;
                                    }
                                }
                                if (eventSink != nullptr) {
                                    if (Flags32 (event->mask).Test (IN_MOVED_TO) ||
                                            Flags32 (event->mask).Test (IN_CREATE)) {
                                        eventSink->HandleAdd (watchId, directory,
                                            Directory::Entry (MakePath (directory, event->name)));
                                    }
                                    if (Flags32 (event->mask).Test (IN_MOVED_FROM) ||
                                            Flags32 (event->mask).Test (IN_DELETE)) {
                                        Directory::Entry entry;
                                        entry.name = event->name;
                                        eventSink->HandleDelete (watchId, directory, entry);
                                    }
                                    if (Flags32 (event->mask).Test (IN_CLOSE_WRITE)) {
                                        eventSink->HandleModified (watchId, directory,
                                            Directory::Entry (MakePath (directory, event->name)));
                                    }
                                }
                                i += sizeof (inotify_event) + event->len;
                            }
                        }
                    }
                #elif defined (TOOLCHAIN_OS_OSX)
                    static const ui32 maxEvents = 256;
                    keventStruct kqueueEvents[maxEvents];
                    int count = keventFunc (handle, 0, 0, kqueueEvents, maxEvents, 0);
                    if (count < 0) {
                        THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                        if (errorCode != EINTR) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                        }
                    }
                    else {
                        std::set<WatchId> watches;
                        for (int i = 0; i < count; ++i) {
                            if (kqueueEvents[i].flags & EV_ERROR) {
                                std::string directory;
                                EventSink *eventSink = nullptr;
                                WatchId watchId = 0;
                                {
                                    LockGuard<SpinLock> guard (spinLock);
                                    Watches::iterator jt = this->watches.find (
                                        (WatchId)kqueueEvents[i].udata);
                                    if (jt != this->watches.end ()) {
                                        directory = jt->second->directory;
                                        eventSink = &jt->second->eventSink;
                                        watchId = jt->second->handle;
                                    }
                                }
                                if (eventSink != nullptr) {
                                    eventSink->HandleError (watchId, directory,
                                        THEKOGANS_UTIL_ERROR_CODE_EXCEPTION (
                                            (THEKOGANS_UTIL_ERROR_CODE)kqueueEvents[i].data));
                                }
                            }
                            else {
                                watches.insert ((WatchId)kqueueEvents[i].udata);
                            }
                        }
                        for (std::set<WatchId>::const_iterator
                                it = watches.begin (),
                                end = watches.end (); it != end; ++it) {
                            std::string directory;
                            EventSink *eventSink = nullptr;
                            WatchId watchId = 0;
                            std::list<Directory::Entry> added;
                            std::list<Directory::Entry> deleted;
                            std::list<Directory::Entry> modified;
                            {
                                LockGuard<SpinLock> guard (spinLock);
                                Watches::iterator jt = this->watches.find (*it);
                                if (jt != this->watches.end ()) {
                                    directory = jt->second->directory;
                                    eventSink = &jt->second->eventSink;
                                    watchId = jt->second->handle;
                                    jt->second->Update (added, deleted, modified);
                                }
                            }
                            if (eventSink != nullptr) {
                                for (std::list<Directory::Entry>::const_iterator
                                        it = added.begin (), end = added.end ();
                                        it != end; ++it) {
                                    eventSink->HandleAdd (watchId, directory, *it);
                                }
                                for (std::list<Directory::Entry>::const_iterator
                                        it = deleted.begin (), end = deleted.end ();
                                        it != end; ++it) {
                                    eventSink->HandleDelete (watchId, directory, *it);
                                }
                                for (std::list<Directory::Entry>::const_iterator
                                        it = modified.begin (), end = modified.end ();
                                        it != end; ++it) {
                                    eventSink->HandleModified (watchId, directory, *it);
                                }
                            }
                        }
                    }
                #endif // defined (TOOLCHAIN_OS_Windows)
                }
                THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
            }
        }

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (thekogans::util::Directory::Entry, 1, "")

    #if defined (TOOLCHAIN_OS_Windows)
        namespace {
            inline ui64 DWORDDWORDToui64 (
                    DWORD nFileSizeLow,
                    DWORD nFileSizeHigh) {
                ULARGE_INTEGER ull;
                ull.LowPart = nFileSizeLow;
                ull.HighPart = nFileSizeHigh;
                return ull.QuadPart;
            }

            namespace {
                using FlagsDWORD = Flags<DWORD>;

                inline bool IsLink (DWORD dwFileAttributes) {
                    return FlagsDWORD (dwFileAttributes).Test (FILE_ATTRIBUTE_REPARSE_POINT);
                }
                inline bool IsDirectory (DWORD dwFileAttributes) {
                    return FlagsDWORD (dwFileAttributes).Test (FILE_ATTRIBUTE_DIRECTORY);
                }
                inline bool IsFile (DWORD dwFileAttributes) {
                    return
                        FlagsDWORD (dwFileAttributes).TestAny (
                            FILE_ATTRIBUTE_ARCHIVE |
                            FILE_ATTRIBUTE_COMPRESSED |
                            FILE_ATTRIBUTE_ENCRYPTED |
                            FILE_ATTRIBUTE_HIDDEN |
                            FILE_ATTRIBUTE_NORMAL |
                            FILE_ATTRIBUTE_NOT_CONTENT_INDEXED |
                            FILE_ATTRIBUTE_READONLY);
                }
            }

            ui8 systemTotype (DWORD dwFileAttributes) {
                return
                    IsLink (dwFileAttributes) ? Directory::Entry::Link :
                    IsDirectory (dwFileAttributes) ? Directory::Entry::Folder :
                    IsFile (dwFileAttributes) ? Directory::Entry::File : Directory::Entry::Invalid;
            }
        }

        Directory::Entry::Entry (const std::string &path) {
            WIN32_FILE_ATTRIBUTE_DATA attributeData;
            if (GetFileAttributesExW (os::windows::UTF8ToUTF16 (path).c_str (),
                    GetFileExInfoStandard, &attributeData) == TRUE) {
                fileSystem = Windows;
                type = systemTotype (attributeData.dwFileAttributes);
                name = Path (path).GetFullFileName ();
                attributes = attributeData.dwFileAttributes;
                creationDate = os::windows::FILETIMEToi64 (attributeData.ftCreationTime);
                lastAccessedDate = os::windows::FILETIMEToi64 (attributeData.ftLastAccessTime);
                lastModifiedDate = os::windows::FILETIMEToi64 (attributeData.ftLastWriteTime);
                size = DWORDDWORDToui64 (attributeData.nFileSizeLow, attributeData.nFileSizeHigh);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        }
    #else // defined (TOOLCHAIN_OS_Windows)
        namespace {
            ui8 systemTotype (mode_t st_mode) {
                return S_ISDIR (st_mode) ? Directory::Entry::Folder :
                    S_ISREG (st_mode) ? Directory::Entry::File :
                    S_ISLNK (st_mode) ? Directory::Entry::Link : Directory::Entry::Invalid;
            }
        }

        Directory::Entry::Entry (const std::string &path) {
            STAT_STRUCT buf;
            if (LSTAT_FUNC (path.c_str (), &buf) == 0) {
                fileSystem = POSIX;
                type = systemTotype (buf.st_mode);
                name = Path (path).GetFullFileName ();
                mode = buf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
                lastStatusDate = buf.st_ctime;
                lastAccessedDate = buf.st_atime;
                lastModifiedDate = buf.st_mtime;
                size = buf.st_size;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        Directory::Entry::Entry (const Entry &entry) {
            fileSystem = entry.fileSystem;
            type = entry.type;
            name = entry.name;
            if (fileSystem == Windows) {
                attributes = entry.attributes;
                creationDate = entry.creationDate;
            }
            else {
                mode = entry.mode;
                lastStatusDate = entry.lastStatusDate;
            }
            lastAccessedDate = entry.lastAccessedDate;
            lastModifiedDate = entry.lastModifiedDate;
            size = entry.size;
        }

        Directory::Entry &Directory::Entry::operator = (const Entry &entry) {
            if (&entry != this) {
                fileSystem = entry.fileSystem;
                type = entry.type;
                name = entry.name;
                if (fileSystem == Windows) {
                    attributes = entry.attributes;
                    creationDate = entry.creationDate;
                }
                else {
                    mode = entry.mode;
                    lastStatusDate = entry.lastStatusDate;
                }
                lastAccessedDate = entry.lastAccessedDate;
                lastModifiedDate = entry.lastModifiedDate;
                size = entry.size;
            }
            return *this;
        }

        namespace {
            const char * const ATTR_FILE_SYSTEM = "FileSystem";
            const char * const VALUE_WINDOWS = "Windows";
            const char * const VALUE_POSIX = "POSIX";
            const char * const ATTR_TYPE = "Type";
            const char * const VALUE_INVALID = "invalid";
            const char * const VALUE_FILE = "file";
            const char * const VALUE_FOLDER = "folder";
            const char * const VALUE_LINK = "link";
            const char * const ATTR_NAME = "Name";
            const char * const ATTR_ATTRIBUTES = "Attributes";
            const char * const ATTR_CREATION_DATE = "CreationDate";
            const char * const ATTR_MODE = "Mode";
            const char * const ATTR_LAST_STATUS_DATE = "LastStatusDate";
            const char * const ATTR_LAST_ACCESSED_DATE = "LastAccessedDate";
            const char * const ATTR_LAST_MODIFIED_DATE = "LastModifiedDate";
            const char * const ATTR_SIZE = "Size";
        }

        std::string Directory::Entry::fileSystemTostring (ui8 fileSystem) {
            return fileSystem == Windows ? VALUE_WINDOWS : VALUE_POSIX;
        }

        ui8 Directory::Entry::stringTofileSystem (const std::string &fileSystem) {
            return fileSystem == VALUE_WINDOWS ? Windows : POSIX;
        }

        std::string Directory::Entry::typeTostring (ui8 type) {
            return type == File ? VALUE_FILE :
                type == Folder ? VALUE_FOLDER :
                type == Link ? VALUE_LINK : VALUE_INVALID;
        }

        ui8 Directory::Entry::stringTotype (const std::string &type) {
            return type == VALUE_FILE ? File :
                type == VALUE_FOLDER ? Folder :
                type == VALUE_LINK ? Link : Invalid;
        }

        std::size_t Directory::Entry::Size () const noexcept {
            return
                Serializer::Size (fileSystem) +
                Serializer::Size (type) +
                Serializer::Size (name) +
                (fileSystem == Windows ?
                    Serializer::Size (attributes) +
                    Serializer::Size (creationDate) :
                    Serializer::Size (mode) +
                    Serializer::Size (lastStatusDate)) +
                Serializer::Size (lastAccessedDate) +
                Serializer::Size (lastModifiedDate) +
                Serializer::Size (size);
        }

        void Directory::Entry::Read (
                const Header & /*header*/,
                Serializer &serializer) {
            serializer >> fileSystem >> type >> name;
            if (fileSystem == Windows) {
                serializer >> attributes >> creationDate;
            }
            else {
                serializer >> mode >> lastStatusDate;
            }
            serializer >> lastAccessedDate >> lastModifiedDate >> size;
        }

        void Directory::Entry::Write (Serializer &serializer) const {
            serializer << fileSystem << type << name;
            if (fileSystem == Windows) {
                serializer << attributes << creationDate;
            }
            else {
                serializer << mode << lastStatusDate;
            }
            serializer << lastAccessedDate << lastModifiedDate << size;
        }

        void Directory::Entry::ReadXML (
                const Header & /*header*/,
                const pugi::xml_node &node) {
            fileSystem = stringTofileSystem (node.attribute (ATTR_FILE_SYSTEM).value ());
            type = stringTotype (node.attribute (ATTR_TYPE).value ());
            name = Decodestring (node.attribute (ATTR_NAME).value ());
            if (fileSystem == Windows) {
                attributes = stringToui32 (node.attribute (ATTR_ATTRIBUTES).value ());
                creationDate = stringToi64 (node.attribute (ATTR_CREATION_DATE).value ());
            }
            else {
                mode = stringToi32 (node.attribute (ATTR_MODE).value ());
                lastStatusDate = stringToi64 (node.attribute (ATTR_LAST_STATUS_DATE).value ());
            }
            lastAccessedDate = stringToi64 (node.attribute (ATTR_LAST_ACCESSED_DATE).value ());
            lastModifiedDate = stringToi64 (node.attribute (ATTR_LAST_MODIFIED_DATE).value ());
            size = stringToui64 (node.attribute (ATTR_SIZE).value ());
        }

        void Directory::Entry::WriteXML (pugi::xml_node &node) const {
            node.append_attribute (
                ATTR_FILE_SYSTEM).set_value (fileSystemTostring (fileSystem).c_str ());
            node.append_attribute (ATTR_TYPE).set_value (typeTostring (type).c_str ());
            node.append_attribute (ATTR_NAME).set_value (Encodestring (name).c_str ());
            if (fileSystem == Windows) {
                node.append_attribute (
                    ATTR_ATTRIBUTES).set_value (ui32Tostring (attributes).c_str ());
                node.append_attribute (
                    ATTR_CREATION_DATE).set_value (i64Tostring (creationDate).c_str ());
            }
            else {
                node.append_attribute (ATTR_MODE).set_value (i32Tostring (mode).c_str ());
                node.append_attribute (
                    ATTR_LAST_STATUS_DATE).set_value (i64Tostring (lastStatusDate).c_str ());
            }
            node.append_attribute (
                ATTR_LAST_ACCESSED_DATE).set_value (i64Tostring (lastAccessedDate).c_str ());
            node.append_attribute (
                ATTR_LAST_MODIFIED_DATE).set_value (i64Tostring (lastModifiedDate).c_str ());
            node.append_attribute (ATTR_SIZE).set_value (ui64Tostring (size).c_str ());
        }

        void Directory::Entry::ReadJSON (
                const Header & /*header*/,
                const JSON::Object &object) {
            fileSystem = stringTofileSystem (object.Get<JSON::String> (ATTR_FILE_SYSTEM)->value);
            type = stringTotype (object.Get<JSON::String> (ATTR_TYPE)->value);
            name = object.Get<JSON::String> (ATTR_NAME)->value;
            if (fileSystem == Windows) {
                attributes = object.Get<JSON::Number> (ATTR_ATTRIBUTES)->To<ui32> ();
                creationDate = object.Get<JSON::Number> (ATTR_CREATION_DATE)->To<i64> ();
            }
            else {
                mode = object.Get<JSON::Number> (ATTR_MODE)->To<i32> ();
                lastStatusDate = object.Get<JSON::Number> (ATTR_LAST_STATUS_DATE)->To<i64> ();
            }
            lastAccessedDate = object.Get<JSON::Number> (ATTR_LAST_ACCESSED_DATE)->To<i64> ();
            lastModifiedDate = object.Get<JSON::Number> (ATTR_LAST_MODIFIED_DATE)->To<i64> ();
            size = object.Get<JSON::Number> (ATTR_SIZE)->To<ui64> ();
        }

        void Directory::Entry::WriteJSON (JSON::Object &object) const {
            object.Add<const std::string &> (ATTR_FILE_SYSTEM, fileSystemTostring (fileSystem));
            object.Add<const std::string &> (ATTR_TYPE, typeTostring (type));
            object.Add<const std::string &> (ATTR_NAME, name);
            if (fileSystem == Windows) {
                object.Add (ATTR_ATTRIBUTES, attributes);
                object.Add (ATTR_CREATION_DATE, creationDate);
            }
            else {
                object.Add (ATTR_MODE, mode);
                object.Add (ATTR_LAST_STATUS_DATE, lastStatusDate);
            }
            object.Add (ATTR_LAST_ACCESSED_DATE, lastAccessedDate);
            object.Add (ATTR_LAST_MODIFIED_DATE, lastModifiedDate);
            object.Add<const SizeT &> (ATTR_SIZE, size);
        }

    #if defined (TOOLCHAIN_OS_Windows)
        Directory::Directory (const std::string &path_) :
                path (path_),
                handle (THEKOGANS_UTIL_INVALID_HANDLE_VALUE),
                attributes (0),
                creationDate (-1),
                lastAccessedDate (-1),
                lastModifiedDate (-1) {
            WIN32_FILE_ATTRIBUTE_DATA attributeData;
            if (GetFileAttributesExW (os::windows::UTF8ToUTF16 (path).c_str (),
                    GetFileExInfoStandard, &attributeData) == TRUE) {
                attributes = attributeData.dwFileAttributes;
                creationDate = os::windows::FILETIMEToi64 (attributeData.ftCreationTime);
                lastAccessedDate = os::windows::FILETIMEToi64 (attributeData.ftLastAccessTime);
                lastModifiedDate = os::windows::FILETIMEToi64 (attributeData.ftLastWriteTime);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        }

        Directory::~Directory () {
            THEKOGANS_UTIL_TRY {
                Close ();
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        bool Directory::GetFirstEntry (Entry &entry) {
            Close ();
            std::string findPath = MakePath (path, "*");
            WIN32_FIND_DATAW findData;
            handle = FindFirstFileW (os::windows::UTF8ToUTF16 (findPath).c_str (), &findData);
            if (handle != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                GetEntry (findData, entry);
                return true;
            }
            else {
                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                if (errorCode != ERROR_FILE_NOT_FOUND) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            }
            return false;
        }

        bool Directory::GetNextEntry (Entry &entry) {
            if (handle != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                WIN32_FIND_DATAW findData;
                if (FindNextFileW (handle, &findData)) {
                    GetEntry (findData, entry);
                    return true;
                }
                else {
                    THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                    if (errorCode != ERROR_NO_MORE_FILES) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                    }
                }
            }
            return false;
        }

        void Directory::Close () {
            if (handle != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                if (FindClose (handle)) {
                    handle = THEKOGANS_UTIL_INVALID_HANDLE_VALUE;
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
        }

        void Directory::GetEntry (
                const WIN32_FIND_DATAW &findData,
                Entry &entry) const {
            entry.fileSystem = Entry::Windows;
            entry.type = systemTotype (findData.dwFileAttributes);
            entry.name = os::windows::UTF16ToUTF8 (findData.cFileName, wcslen (findData.cFileName));
            entry.attributes = findData.dwFileAttributes;
            entry.creationDate = os::windows::FILETIMEToi64 (findData.ftCreationTime);
            entry.lastAccessedDate = os::windows::FILETIMEToi64 (findData.ftLastAccessTime);
            entry.lastModifiedDate = os::windows::FILETIMEToi64 (findData.ftLastWriteTime);
            entry.size = DWORDDWORDToui64 (findData.nFileSizeLow, findData.nFileSizeHigh);
        }

        void Directory::Create (
                const std::string &path,
                bool createAncestry,
                LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
            if (!Path (path).Exists ()) {
                if (createAncestry) {
                    std::list<std::string> components;
                    {
                        if (!Path (path).IsAbsolute ()) {
                            Path (MakePath (Path::GetCurrDirectory (), path)).GetComponents (
                                components);
                        }
                        else {
                            Path (path).GetComponents (components);
                        }
                    }
                    std::list<std::string> ancestors;
                    for (std::list<std::string>::const_iterator it = components.begin (),
                            end = components.end (); it != end; ++it) {
                        if (*it == ".") {
                            continue;
                        }
                        else if (*it == "..") {
                            if (!ancestors.empty ()) {
                                ancestors.pop_back ();
                            }
                            else {
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                    "Invalid path: %s", path.c_str ());
                            }
                        }
                        else {
                            ancestors.push_back (*it);
                            std::string currenPath = MakePath (ancestors, true);
                            if (!Path (currenPath).Exists () &&
                                    !CreateDirectoryW (
                                        os::windows::UTF8ToUTF16 (currenPath).c_str (),
                                        lpSecurityAttributes)) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", currenPath.c_str ());
                            }
                        }
                    }
                }
                else if (!CreateDirectoryW (
                        os::windows::UTF8ToUTF16 (path).c_str (), lpSecurityAttributes)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
                }
            }
        }
    #else // defined (TOOLCHAIN_OS_Windows)
        Directory::Directory (const std::string &path_) :
                path (path_),
                dir (0),
                mode (0),
                lastStatusDate (-1),
                lastAccessedDate (-1),
                lastModifiedDate (-1) {
            STAT_STRUCT buf;
            if (STAT_FUNC (path.c_str (), &buf) == 0) {
                assert (systemTotype (buf.st_mode) == Entry::Folder);
                mode = buf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
                lastStatusDate = buf.st_ctime;
                lastAccessedDate = buf.st_atime;
                lastModifiedDate = buf.st_mtime;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        }

        Directory::~Directory () {
            THEKOGANS_UTIL_TRY {
                Close ();
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        bool Directory::GetFirstEntry (Entry &entry) {
            Close ();
            dir = opendir (path.c_str ());
            if (dir != nullptr) {
                char buffer[sizeof (dirent) + NAME_MAX + 1];
                dirent *dirEnt = nullptr;
                if (readdir_r (dir, (dirent *)buffer, &dirEnt) == 0) {
                    if (dirEnt != nullptr) {
                        GetEntry (*dirEnt, entry);
                        return true;
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            else {
                THEKOGANS_UTIL_ERROR_CODE errorCode = THEKOGANS_UTIL_OS_ERROR_CODE;
                if (errorCode != ENOENT && errorCode != ENOTDIR) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (errorCode);
                }
            }
            return false;
        }

        bool Directory::GetNextEntry (Entry &entry) {
            if (dir != nullptr) {
                char buffer[sizeof (dirent) + NAME_MAX + 1];
                dirent *dirEnt = nullptr;
                if (readdir_r (dir, (dirent *)buffer, &dirEnt) == 0) {
                    if (dirEnt != nullptr) {
                        GetEntry (*dirEnt, entry);
                        return true;
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            return false;
        }

        void Directory::Create (
                const std::string &path,
                bool createAncestry,
                mode_t mode) {
            if (!Path (path).Exists ()) {
                if (createAncestry) {
                    std::list<std::string> components;
                    {
                        if (!Path (path).IsAbsolute ()) {
                            Path (MakePath (Path::GetCurrDirectory (), path)).GetComponents (
                                components);
                        }
                        else {
                            Path (path).GetComponents (components);
                        }
                    }
                    std::list<std::string> ancestors;
                    for (std::list<std::string>::const_iterator it = components.begin (),
                            end = components.end (); it != end; ++it) {
                        if (*it == ".") {
                            continue;
                        }
                        else if (*it == "..") {
                            if (!ancestors.empty ()) {
                                ancestors.pop_back ();
                            }
                            else {
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                    "Invalid path: %s", path.c_str ());
                            }
                        }
                        else {
                            ancestors.push_back (*it);
                            std::string currenPath = MakePath (ancestors, true);
                            if (!Path (currenPath).Exists () &&
                                    mkdir (currenPath.c_str (), mode) != 0) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", currenPath.c_str ());
                            }
                        }
                    }
                }
                else if (mkdir (path.c_str (), mode) != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
                }
            }
        }

        void Directory::Close () {
            if (dir != nullptr) {
                int result;
                do {
                    result = closedir (dir);
                    if (result < 0 && THEKOGANS_UTIL_OS_ERROR_CODE != EINTR) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                            THEKOGANS_UTIL_OS_ERROR_CODE);
                    }
                } while (result < 0);
                dir = nullptr;
            }
        }

        void Directory::GetEntry (
                const dirent &dirEnt,
                Entry &entry) const {
            std::string pathName = MakePath (path, dirEnt.d_name);
            STAT_STRUCT buf;
            if (LSTAT_FUNC (pathName.c_str (), &buf) == 0) {
                entry.fileSystem = Entry::POSIX;
                entry.type = systemTotype (buf.st_mode);
                entry.name = dirEnt.d_name;
                entry.mode = buf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
                entry.lastStatusDate = buf.st_ctime;
                entry.lastAccessedDate = buf.st_atime;
                entry.lastModifiedDate = buf.st_mtime;
                entry.size = buf.st_size;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", pathName.c_str ());
            }
        }
    #endif // defined (TOOLCHAIN_OS_Windows)

        void Directory::Delete (
                const std::string &path,
                bool recursive) {
            if (recursive) {
                Directory directory (path);
                Directory::Entry entry;
                for (bool gotEntry = directory.GetFirstEntry (entry);
                        gotEntry; gotEntry = directory.GetNextEntry (entry)) {
                    if (!entry.name.empty ()) {
                        std::string entryPath = MakePath (path, entry.name);
                        if (entry.type == Directory::Entry::Folder) {
                            if (!IsDotOrDotDot (entry.name.c_str ())) {
                                Delete (entryPath, recursive);
                            }
                        }
                        else if (entry.type == Directory::Entry::File ||
                                entry.type == Directory::Entry::Link) {
                            File::Delete (entryPath);
                        }
                        else {
                            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                "Unknown entry type (%x): %s",
                                entry.type, entryPath.c_str ());
                        }
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "%s", "Empty directory entry");
                    }
                }
            }
        #if defined (TOOLCHAIN_OS_Windows)
            if (RemoveDirectoryW (os::windows::UTF8ToUTF16 (path).c_str ()) == 0) {
        #else // defined (TOOLCHAIN_OS_Windows)
            if (rmdir (path.c_str ()) == -1) {
        #endif // defined (TOOLCHAIN_OS_Windows)
                THEKOGANS_UTIL_THROW_ERROR_CODE_AND_MESSAGE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE, " (%s)", path.c_str ());
            }
        }

    } // namespace util
} // namespace thekogans
