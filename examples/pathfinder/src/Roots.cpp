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
#include <list>
#include <iostream>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/pathfinder/Roots.h"

namespace thekogans {
    namespace pathfinder {

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (
            thekogans::pathfinder::Roots,
            1,
            util::BTree::Value::TYPE)

        void Roots::ScanRoot (
                const std::string &path,
                IgnoreList::SharedPtr ignoreList) {
            Root::SharedPtr root;
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i]->GetPath () == path) {
                    root = value[i];
                    break;
                }
            }
            if (root == nullptr) {
                root.Reset (new Root (path));
                util::Subscriber<RootEvents>::Subscribe (*root);
                value.push_back (root);
                Produce (
                    std::bind (
                        &RootsEvents::OnRootsRootCreated,
                        std::placeholders::_1,
                        this,
                        root));
            }
            root->Scan (ignoreList);
        }

        bool Roots::EnableRoot (const std::string &path) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i]->GetPath () == path) {
                    if (!value[i]->IsActive ()) {
                        value[i]->SetActive (true);
                        Produce (
                            std::bind (
                                &RootsEvents::OnRootsRootEnabled,
                                std::placeholders::_1,
                                this,
                                value[i]));
                    }
                    return true;
                }
            }
            return false;
        }

        bool Roots::DisableRoot (const std::string &path) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i]->GetPath () == path) {
                    if (value[i]->IsActive ()) {
                        value[i]->SetActive (false);
                        Produce (
                            std::bind (
                                &RootsEvents::OnRootsRootDisabled,
                                std::placeholders::_1,
                                this,
                                value[i]));
                    }
                    return true;
                }
            }
            return false;
        }

        bool Roots::DeleteRoot (const std::string &path) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i]->GetPath () == path) {
                    Root::SharedPtr root = value[i];
                    value.erase (value.begin () + i);
                    util::Subscriber<RootEvents>::Unsubscribe (*root);
                    root->Delete ();
                    Produce (
                        std::bind (
                            &RootsEvents::OnRootsRootDeleted,
                            std::placeholders::_1,
                            this,
                            root));
                    return true;
                }
            }
            return false;
        }

        void Roots::Find (
                const std::string &pattern,
                bool ignoreCase,
                bool ordered) {
            std::list<std::string> patternComponents;
            util::Path (pattern).GetComponents (patternComponents);
            std::list<std::string>::const_iterator patternBegin = patternComponents.begin ();
            std::list<std::string>::const_iterator patternEnd = patternComponents.end ();
            if (patternBegin != patternEnd) {
                for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                    if (value[i]->IsActive ()) {
                        value[i]->Find (patternBegin, patternEnd, ignoreCase, ordered);
                    }
                }
            }
        }

        void Roots::Init () {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                util::Subscriber<RootEvents>::Subscribe (*value[i]);
            }
        }

        void Roots::OnRootScanPath (
                Root::SharedPtr /*root*/,
                const std::string &path) throw () {
            std::cout << path << "\n";
        }

        void Roots::OnRootFindPath (
                Root::SharedPtr /*root*/,
                const std::string &path) throw () {
            std::cout << path << "\n";
        }

    } // namespace pathfinder
} // namespace thekogans
