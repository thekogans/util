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

namespace thekogans {
    namespace util {

        void HeapRegistry::AddHeap (
                const std::string &name,
                Diagnostics *heap) {
            assert (!name.empty ());
            assert (heap != 0);
            LockGuard<SpinLock> guard (spinLock);
            map.insert (Map::value_type (name, heap));
        }

        void HeapRegistry::DeleteHeap (const std::string &name) {
            LockGuard<SpinLock> guard (spinLock);
            Map::iterator it = map.find (name);
            if (it != map.end ()) {
                map.erase (it);
            }
        }

        bool HeapRegistry::IsValidPtr (void *ptr) throw () {
            if (ptr != 0) {
                LockGuard<SpinLock> guard (spinLock);
                for (Map::const_iterator it = map.begin (),
                        end = map.end (); it != end; ++it) {
                    if (it->second->IsValidPtr (ptr)) {
                        return true;
                    }
                }
            }
            return false;
        }

        void HeapRegistry::DumpHeaps (
                const std::string &header,
                std::ostream &stream) {
            LockGuard<SpinLock> guard (spinLock);
            if (!header.empty ()) {
                stream << header << std::endl;
            }
            for (Map::const_iterator it = map.begin (),
                    end = map.end (); it != end; ++it) {
                Diagnostics::Stats::UniquePtr stats = it->second->GetStats ();
                if (stats.get () != 0) {
                    stats->Dump (stream);
                }
            }
            stream.flush ();
        }

    } // namespace util
} // namespace thekogans
