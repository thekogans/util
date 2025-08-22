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

#if !defined (__thekogans_pathfinder_Roots_h)
#define __thekogans_pathfinder_Roots_h

#include <string>
#include <vector>
#include "thekogans/util/Serializable.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/Producer.h"
#include "thekogans/pathfinder/Root.h"
#include "thekogans/pathfinder/IgnoreList.h"

namespace thekogans {
    namespace pathfinder {

        /// \brief
        /// Forward declaration of \see{Roots} needed by \see{RootsEvents}.
        struct Roots;

        /// \struct RootsEvents Roots.h thekogans/pathfinder/Roots.h
        ///
        /// \brief
        /// Events fired by \see{Roots}.
        struct RootsEvents {
            /// \brief
            /// dtor.
            virtual ~RootsEvents () {}

            /// \brief
            /// A new \see{Root} was created.
            /// \param[in] roots Pointer to \see{Roots}.
            /// \param[in] root New \see{Root}.
            virtual void OnRootsRootCreated (
                util::RefCounted::SharedPtr<Roots> /*roots*/,
                Root::SharedPtr /*root*/) throw () {}
            /// \brief
            /// A \see{Root} was enabled.
            /// \param[in] roots Pointer to \see{Roots}.
            /// \param[in] root \see{Root} that was enabled.
            virtual void OnRootsRootEnabled (
                util::RefCounted::SharedPtr<Roots> /*roots*/,
                Root::SharedPtr /*root*/) throw () {}
            /// \brief
            /// A \see{Root} was disabled.
            /// \param[in] roots Pointer to \see{Roots}.
            /// \param[in] root \see{Root}. that was disabled.
            virtual void OnRootsRootDisabled (
                util::RefCounted::SharedPtr<Roots> /*roots*/,
                Root::SharedPtr /*root*/) throw () {}
            /// \brief
            /// A \see{Root} was deleted.
            /// \param[in] roots Pointer to \see{Roots}
            /// \param[in] root \see{Root} that was deleted.
            virtual void OnRootsRootDeleted (
                util::RefCounted::SharedPtr<Roots> /*roots*/,
                Root::SharedPtr /*root*/) throw () {}
        };

        /// \struct Roots Roots.h thekogans/pathfinder/Roots.h
        ///
        /// \brief
        /// Roots is an \see{ArrayValue<Root::SharedPtr>}. It
        /// keeps track of all the \see{Root}s.
        struct Roots :
                public util::ArrayValue<Root::SharedPtr>,
                public util::Subscriber<RootEvents>,
                public util::Producer<RootsEvents> {
            /// \brief
            /// Roots is a \see{util::Serializable}.
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE (Roots)

            /// \brief
            /// Given a path, scan the directory tree rooted at it.
            /// \param[in] path The root of the directory tree to scan.
            /// \param[in] ignoreList A list of regular expressions to ignore during the scan.
            void ScanRoot (
                const std::string &path,
                IgnoreList::SharedPtr ignoreList);
            /// \brief
            /// Enable the given root. Enabled roots participate in \see{Find}.
            /// \param[in] path Root to enable.
            /// \return true == enabled. false = not found or already enabled.
            bool EnableRoot (const std::string &path);
            /// \brief
            /// Disable the given root. Disabled roots do not participate in \see{Find}.
            /// \param[in] path Root to disable.
            /// \return true == disabled. false = not found or already disabled.
            bool DisableRoot (const std::string &path);
            /// \brief
            /// Delete the given root.
            /// \param[in] path Root to delete.
            /// \return true == enabled. false = not found.
            bool DeleteRoot (const std::string &path);

            /// \brief
            /// Given a regex pattern of path components, find all matching paths
            /// in all enabled \see{Root}s.
            void Find (
                const std::string &pattern,
                bool ignoreCase,
                bool ordered);

            // util::ArrayValue<Root::SharedPtr>.
            /// \brief
            /// Read the value from the given serializer.
            /// \param[in] header \see{SerializableHeader}.
            /// \param[in] serializer \see{Serializer} to read the value from.
            virtual void Read (
                const SerializableHeader &header,
                Serializer &serializer) override;

            // util::Subscriber<RootEvents>.
            virtual void OnRootScanPath (
                Root::SharedPtr /*root*/,
                const std::string &path) throw () override;
            virtual void OnRootFindPath (
                Root::SharedPtr /*root*/,
                const std::string &path) throw () override;
        };

    } // namespace pathfinder
} // namespace thekogans

#endif // !defined (__thekogans_pathfinder_Roots_h)
