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

#if !defined (__thekogans_kcd_Database_h)
#define __thekogans_kcd_Database_h

#include "thekogans/util/Singleton.h"
#include "thekogans/util/BufferedFile.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/FileAllocatorRegistry.h"
#include "thekogans/kcd/Options.h"

namespace thekogans {
    namespace kcd {

        struct Database :
                public util::RefCountedSingleton<Database>,
                public util::FileAllocator {
            struct Guard : public util::BufferedFile::Transaction::Guard {
                Guard () : util::BufferedFile::Transaction::Guard (*Database::Instance ()) {
                    Database::Instance ()->transaction = transaction;
                }
                ~Guard () {
                    Database::Instance ()->transaction.Reset ();
                }

            };
            util::BufferedFile::Transaction::SharedPtr transaction;

            Database () :
                util::FileAllocator (Options::Instance ()->dbPath) {}

            inline util::BufferedFile::Transaction::SharedPtr GetTransaction () const {
                return transaction;
            }
        };

    } // namespace kcd
} // namespace thekogans

#endif // !defined (__thekogans_kcd_Database_h)
