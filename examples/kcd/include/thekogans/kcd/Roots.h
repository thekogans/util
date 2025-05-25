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

#if !defined (__thekogans_kcd_Roots_h)
#define __thekogans_kcd_Roots_h

#include <string>
#include <vector>
#include "thekogans/util/BufferedFile.h"
#include "thekogans/util/Serializable.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/Producer.h"
#include "thekogans/kcd/Root.h"
#include "thekogans/kcd/IgnoreList.h"

namespace thekogans {
    namespace kcd {

        struct Roots;

        struct RootsEvents {
            virtual ~RootsEvents () {}

            virtual void OnRootsRootCreated (
                util::RefCounted::SharedPtr<Roots> /*roots*/,
                Root::SharedPtr /*root*/) throw () {}
            virtual void OnRootsRootEnabled (
                util::RefCounted::SharedPtr<Roots> /*roots*/,
                Root::SharedPtr /*root*/) throw () {}
            virtual void OnRootsRootDisabled (
                util::RefCounted::SharedPtr<Roots> /*roots*/,
                Root::SharedPtr /*root*/) throw () {}
            virtual void OnRootsRootDeleted (
                util::RefCounted::SharedPtr<Roots> /*roots*/,
                Root::SharedPtr /*root*/) throw () {}
        };

        struct Roots :
                public util::ArrayValue<Root::SharedPtr>,
                public util::Producer<RootsEvents>,
                public util::Subscriber<RootEvents> {
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (Roots)

            void ScanRoot (
                util::BufferedFile::Transaction::SharedPtr transaction,
                const std::string &path,
                IgnoreList::SharedPtr ignoreList);
            bool EnableRoot (const std::string &path);
            bool DisableRoot (const std::string &path);
            bool DeleteRoot (const std::string &path);

            void Find (
                const std::string &pattern,
                bool ignoreCase,
                bool ordered,
                std::vector<std::string> &results);

            virtual void Init () override;

            virtual void OnRootScanPath (
                Root::SharedPtr /*root*/,
                const std::string &path) throw () override;
        };

    } // namespace kcd
} // namespace thekogans

#endif // !defined (__thekogans_kcd_Roots_h)
