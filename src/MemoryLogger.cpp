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

#include "thekogans/util/Heap.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/MemoryLogger.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (
            thekogans::util::MemoryLogger,
            Logger::TYPE)
        THEKOGANS_UTIL_IMPLEMENT_HEAP_FUNCTIONS (MemoryLogger::Entry)

        MemoryLogger::~MemoryLogger () {
            ClearEntries ();
        }

        void MemoryLogger::Log (
                const std::string &subsystem,
                ui32 level,
                const std::string &header,
                const std::string &message) noexcept {
            if (level <= this->level && (!header.empty () || !message.empty ())) {
                LockGuard<SpinLock> guard (spinLock);
                entryList.push_back (new Entry (subsystem, level, header, message));
                if (entryList.size () > maxEntries) {
                    Entry::UniquePtr entry (entryList.pop_front ());
                    // FIXME: callback.
                }
            }
        }

        void MemoryLogger::SaveEntries (
                const std::string &path,
                i32 flags,
                bool clear) {
            LockGuard<SpinLock> guard (spinLock);
            if (!entryList.empty ()) {
                SimpleFile file (HostEndian, path, flags);
                entryList.for_each (
                    [&file] (EntryList::Callback::argument_type entry) -> EntryList::Callback::result_type {
                        if (!entry->header.empty ()) {
                            file.Write (entry->header.c_str (), entry->header.size ());
                        }
                        if (!entry->message.empty ()) {
                            file.Write (entry->message.c_str (), entry->message.size ());
                        }
                        return true;
                    }
                );
                if (clear) {
                    ClearEntries ();
                }
            }
        }

        void MemoryLogger::ClearEntries () {
            LockGuard<SpinLock> guard (spinLock);
            entryList.clear (
                [] (EntryList::Callback::argument_type entry) -> EntryList::Callback::result_type {
                    delete entry;
                    return true;
                }
            );
        }

    } // namespace util
} // namespace thekogans
