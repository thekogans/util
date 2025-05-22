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
#include <vector>
#include <unordered_set>
#include <iostream>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/kcd/Roots.h"

namespace thekogans {
    namespace kcd {

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (Roots, 1, util::BTree::Value::TYPE)

        void Roots::ScanRoot (
                util::FileAllocator::SharedPtr fileAllocator,
                util::BufferedFile::Transaction::SharedPtr transaction,
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
            root->Scan (fileAllocator, transaction, ignoreList);
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

        bool Roots::DeleteRoot (
                util::FileAllocator::SharedPtr fileAllocator,
                const std::string &path) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i]->GetPath () == path) {
                    Root::SharedPtr root = value[i];
                    value.erase (value.begin () + i);
                    util::Subscriber<RootEvents>::Unsubscribe (*root);
                    root->Delete (fileAllocator);
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
                util::FileAllocator::SharedPtr fileAllocator,
                const std::string &pattern,
                bool ignoreCase,
                bool ordered,
                std::vector<std::string> &results) {
            auto ScanPattern = [] (
                    std::list<std::string>::const_iterator pathBegin,
                    std::list<std::string>::const_iterator pathEnd,
                    std::list<std::string>::const_iterator patternBegin,
                    std::list<std::string>::const_iterator patternEnd,
                    bool ignoreCase,
                    bool ordered) -> bool {
                while (patternBegin != patternEnd) {
                    std::list<std::string>::const_iterator it =
                        FindPrefix (pathBegin, pathEnd, *patternBegin, ignoreCase);
                    if (it == pathEnd) {
                        return false;
                    }
                    // To honor ordered flag pattern components
                    // must come in order in the resulting paths.
                    if (ordered) {
                        pathBegin = ++it;
                    }
                    ++patternBegin;
                }
                return true;
            };
            std::list<std::string> patternComponents;
            util::Path (pattern).GetComponents (patternComponents);
            std::list<std::string>::const_iterator patternBegin = patternComponents.begin ();
            std::list<std::string>::const_iterator patternEnd = patternComponents.end ();
            if (patternBegin != patternEnd) {
                std::vector<std::string> paths;
                for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                    if (value[i]->IsActive ()) {
                        value[i]->Find (fileAllocator, *patternBegin, ignoreCase, paths);
                    }
                }
                // If we don't care about order, or there's only one pattern
                // component, skip over the first pattern component because
                // value[i]->Find just found it.
                if (!ordered || std::next (patternBegin) == patternEnd) {
                    ++patternBegin;
                }
                // Multiple different components with the same prefix (Python/Python38-32)
                // can be found in the same path. Make sure we only count it once.
                std::unordered_set<std::string> duplicates;
                if (patternBegin == patternEnd) {
                    for (std::size_t i = 0, count = paths.size (); i < count; ++i) {
                        if (duplicates.insert (paths[i]).second) {
                            results.push_back (paths[i]);
                        }
                    }
                }
                else {
                    for (std::size_t i = 0, count = paths.size (); i < count; ++i) {
                        std::list<std::string> pathComponents;
                        util::Path (paths[i]).GetComponents (pathComponents);
                        if (ScanPattern (
                            #if defined (TOOLCHAIN_OS_Windows)
                                // If on Windows, skip over the drive leter.
                                ++pathComponents.begin (),
                            #else // defined (TOOLCHAIN_OS_Windows)
                                pathComponents.begin (),
                            #endif // defined (TOOLCHAIN_OS_Windows)
                                pathComponents.end (),
                                patternBegin,
                                patternEnd,
                                ignoreCase,
                                ordered)) {
                            if (duplicates.insert (paths[i]).second) {
                                results.push_back (paths[i]);
                            }
                        }
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

    } // namespace kcd
} // namespace thekogans
