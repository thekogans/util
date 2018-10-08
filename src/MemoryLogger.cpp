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

#include "thekogans/util/LockGuard.h"
#include "thekogans/util/MemoryLogger.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (MemoryLogger::Entry, SpinLock)

        MemoryLogger::~MemoryLogger () {
            ClearEntries ();
        }

        void MemoryLogger::Log (
                const std::string &subsystem,
                ui32 level,
                const std::string &header,
                const std::string &message) throw () {
            LockGuard<SpinLock> guard (spinLock);
            entryList.push_back (new Entry (subsystem, level, header, message));
            if (entryList.size () > maxEntries) {
                Entry::UniquePtr entry (entryList.pop_front ());
                // FIXME: callback.
            }
        }

        void MemoryLogger::SaveEntries (
                const std::string &path,
                i32 flags,
                bool clear) {
            LockGuard<SpinLock> guard (spinLock);
            if (!entryList.empty ()) {
                struct WriteEntriesCallback : public EntryList::Callback {
                    typedef EntryList::Callback::result_type result_type;
                    typedef EntryList::Callback::argument_type argument_type;
                    SimpleFile file;
                    WriteEntriesCallback (
                        const std::string &path,
                        i32 flags) :
                        file (HostEndian, path, flags) {}
                    virtual result_type operator () (argument_type entry) {
                        if (!entry->header.empty ()) {
                            file.Write (entry->header.c_str (), entry->header.size ());
                        }
                        if (!entry->message.empty ()) {
                            file.Write (entry->message.c_str (), entry->message.size ());
                        }
                        return true;
                    }
                } writeEntriesCallback (path, flags);
                entryList.for_each (writeEntriesCallback);
                if (clear) {
                    ClearEntries ();
                }
            }
        }

        void MemoryLogger::ClearEntries () {
            LockGuard<SpinLock> guard (spinLock);
            struct DeleteCallback : public EntryList::Callback {
                typedef EntryList::Callback::result_type result_type;
                typedef EntryList::Callback::argument_type argument_type;
                virtual result_type operator () (argument_type entry) {
                    delete entry;
                    return true;
                }
            } deleteCallback;
            entryList.clear (deleteCallback);
        }

    } // namespace util
} // namespace thekogans
